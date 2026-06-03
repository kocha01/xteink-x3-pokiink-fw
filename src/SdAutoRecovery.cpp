#include "SdAutoRecovery.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <string>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_task_wdt.h"

#include "bootloader_common.h"
#include "esp_flash_partitions.h"

#include "network/PokiInkFwMagic.h"

namespace SdAutoRecovery {
namespace {

constexpr size_t MIN_FW_SIZE = 256 * 1024;       // 256 KB sanity floor
constexpr size_t MAX_FW_SIZE = 6 * 1024 * 1024;  // 6 MB — fits app partition
constexpr size_t WRITE_CHUNK = 4096;             // 1 flash sector

bool renameWithSuffix(const char* path, const char* suffix) {
  // Build "<path><suffix>".  If a stale version exists from a previous run,
  // remove it first — Storage.rename refuses to overwrite.  Storage.remove
  // tolerates non-existent paths.
  std::string target = std::string(path) + suffix;
  Storage.remove(target.c_str());
  return Storage.rename(path, target.c_str());
}

// Pre-flight header check on the SD FILE (not the flash partition) so we can
// bail without erasing anything if the file is obviously wrong.
bool checkFileHeader(FsFile& file, const char* path) {
  uint8_t header[48];
  if (!file.seek(0) || file.read(header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    LOG_ERR("RECOV", "Header read failed for %s", path);
    renameWithSuffix(path, ".rejected.readerr");
    return false;
  }
  if (header[0] != 0xE9) {
    LOG_ERR("RECOV", "Bad image magic: 0x%02X (expected 0xE9)", header[0]);
    renameWithSuffix(path, ".rejected.notesp32");
    return false;
  }
  uint32_t appMagic = 0;
  std::memcpy(&appMagic, header + 32, sizeof(appMagic));
  if (appMagic != 0xABCD5432u) {
    LOG_ERR("RECOV", "Bad app_desc magic: 0x%08lX (expected 0xABCD5432)",
            static_cast<unsigned long>(appMagic));
    renameWithSuffix(path, ".rejected.noappdesc");
    return false;
  }
  return true;
}

// Verify the just-flashed partition carries our board's magic marker.
// Mirrors what the network OTA installer does (OtaUpdater.cpp).  Rejecting
// here means we DON'T flip otadata — the bricked / wrong-board firmware
// stays in the inactive slot but never boots.
bool verifyPokiInkMagic(const esp_partition_t* part) {
  constexpr size_t SCAN_SIZE = 256 * 1024;
  const void* mapped = nullptr;
  esp_partition_mmap_handle_t handle = 0;
  esp_err_t err = esp_partition_mmap(part, 0, SCAN_SIZE, ESP_PARTITION_MMAP_DATA, &mapped, &handle);
  if (err != ESP_OK || mapped == nullptr) {
    LOG_ERR("RECOV", "mmap failed for board-tag scan: %s", esp_err_to_name(err));
    return false;
  }
  const uint8_t* haystack = static_cast<const uint8_t*>(mapped);
  const uint8_t needle0 = static_cast<uint8_t>(POKIINK_X3_FW_MAGIC[0]);
  bool found = false;
  // Manual first-byte filter + memcmp.  Avoids depending on memmem (not
  // always present in ESP-IDF's libc) and is fast enough — 256 KB scans in
  // well under a second on the C3's 160 MHz CPU.
  for (size_t i = 0; i + POKIINK_X3_FW_MAGIC_LEN <= SCAN_SIZE; ++i) {
    if (haystack[i] != needle0) continue;
    if (std::memcmp(haystack + i, POKIINK_X3_FW_MAGIC,
                    static_cast<size_t>(POKIINK_X3_FW_MAGIC_LEN)) == 0) {
      found = true;
      break;
    }
  }
  esp_partition_munmap(handle);
  return found;
}

// Update otadata to point at `target`.  Same routine the network OTA
// installer uses — keeps wear leveling (writes to the lower-seq sector),
// allocates a seq number that maps to the right slot, and uses
// ESP_OTA_IMG_UNDEFINED state so main.cpp's first-boot-after-flash detector
// can switch it to VALID on next boot.
bool flipOtadataTo(const esp_partition_t* target) {
  const esp_partition_t* otadata =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!otadata) {
    LOG_ERR("RECOV", "otadata partition not found");
    return false;
  }

  esp_ota_select_entry_t entry[2];
  esp_partition_read(otadata, 0, &entry[0], sizeof(entry[0]));
  esp_partition_read(otadata, 0x1000, &entry[1], sizeof(entry[1]));
  const bool valid0 = bootloader_common_ota_select_valid(&entry[0]);
  const bool valid1 = bootloader_common_ota_select_valid(&entry[1]);

  uint32_t maxSeq = 0;
  if (valid0) maxSeq = entry[0].ota_seq;
  if (valid1 && entry[1].ota_seq > maxSeq) maxSeq = entry[1].ota_seq;

  const int targetSlot = target->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;
  uint32_t newSeq = maxSeq + 1;
  // (seq - 1) % num_partitions must equal slot index — bump if it doesn't.
  if ((newSeq - 1) % 2 != static_cast<uint32_t>(targetSlot)) {
    newSeq++;
  }

  esp_ota_select_entry_t newEntry;
  std::memset(&newEntry, 0xFF, sizeof(newEntry));
  newEntry.ota_seq = newSeq;
  newEntry.ota_state = ESP_OTA_IMG_UNDEFINED;
  newEntry.crc = bootloader_common_ota_select_crc(&newEntry);

  // Pick the staler sector to write into (wear leveling).
  int writeSector;
  if (!valid0) {
    writeSector = 0;
  } else if (!valid1) {
    writeSector = 1;
  } else {
    writeSector = (entry[0].ota_seq <= entry[1].ota_seq) ? 0 : 1;
  }
  const uint32_t writeOffset = static_cast<uint32_t>(writeSector) * 0x1000;

  esp_err_t err = esp_partition_erase_range(otadata, writeOffset, 0x1000);
  if (err != ESP_OK) {
    LOG_ERR("RECOV", "otadata erase failed: %s", esp_err_to_name(err));
    return false;
  }
  err = esp_partition_write(otadata, writeOffset, &newEntry, sizeof(newEntry));
  if (err != ESP_OK) {
    LOG_ERR("RECOV", "otadata write failed: %s", esp_err_to_name(err));
    return false;
  }

  LOG_INF("RECOV", "otadata updated → slot %d (ota_seq=%lu, sector=%d)", targetSlot,
          static_cast<unsigned long>(newSeq), writeSector);
  return true;
}

FlashResult verifyAndFlash(const char* path, bool skipBoardCheck = false) {
  FsFile file;
  if (!Storage.openFileForRead("RECOV", path, file)) {
    LOG_ERR("RECOV", "Open failed for %s", path);
    return FlashResult::OPEN_FAIL;
  }

  const size_t fileSize = file.size();
  if (fileSize < MIN_FW_SIZE || fileSize > MAX_FW_SIZE) {
    LOG_ERR("RECOV", "Size out of range: %u bytes (limits %u..%u)", static_cast<unsigned>(fileSize),
            static_cast<unsigned>(MIN_FW_SIZE), static_cast<unsigned>(MAX_FW_SIZE));
    file.close();
    renameWithSuffix(path, ".rejected.size");
    return FlashResult::SIZE_OUT_OF_RANGE;
  }

  if (!checkFileHeader(file, path)) {
    file.close();
    return FlashResult::BAD_HEADER;
  }

  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (!target) {
    LOG_ERR("RECOV", "No inactive OTA partition found");
    file.close();
    renameWithSuffix(path, ".rejected.nopartition");
    return FlashResult::PARTITION_ERROR;
  }
  if (fileSize > target->size) {
    LOG_ERR("RECOV", "File (%u) larger than partition (%u)", static_cast<unsigned>(fileSize),
            static_cast<unsigned>(target->size));
    file.close();
    renameWithSuffix(path, ".rejected.toolarge");
    return FlashResult::SIZE_OUT_OF_RANGE;
  }

  LOG_INF("RECOV", "Erasing target partition '%s' (offset 0x%lX, size %u)", target->label,
          static_cast<unsigned long>(target->address), static_cast<unsigned>(target->size));
  esp_err_t err = esp_partition_erase_range(target, 0, target->size);
  if (err != ESP_OK) {
    LOG_ERR("RECOV", "Erase failed: %s", esp_err_to_name(err));
    file.close();
    renameWithSuffix(path, ".rejected.eraseerr");
    return FlashResult::PARTITION_ERROR;
  }

  if (!file.seek(0)) {
    LOG_ERR("RECOV", "Seek to 0 failed");
    file.close();
    renameWithSuffix(path, ".rejected.seekerr");
    return FlashResult::WRITE_FAIL;
  }

  uint8_t buf[WRITE_CHUNK];
  size_t written = 0;
  const unsigned long tStart = millis();
  while (written < fileSize) {
    const size_t want = (fileSize - written) > WRITE_CHUNK ? WRITE_CHUNK : (fileSize - written);
    const int n = file.read(buf, want);
    if (n != static_cast<int>(want)) {
      LOG_ERR("RECOV", "Short read at offset %u: got %d, wanted %u", static_cast<unsigned>(written), n,
              static_cast<unsigned>(want));
      file.close();
      renameWithSuffix(path, ".rejected.shortread");
      return FlashResult::WRITE_FAIL;
    }
    err = esp_partition_write(target, written, buf, want);
    if (err != ESP_OK) {
      LOG_ERR("RECOV", "Write failed at offset %u: %s", static_cast<unsigned>(written), esp_err_to_name(err));
      file.close();
      renameWithSuffix(path, ".rejected.writeerr");
      return FlashResult::WRITE_FAIL;
    }
    written += want;
    // Feed the task watchdog.  Writing 6 MB at the X3's SD speed takes
    // 30-60 s end-to-end; without this the 5-second TWDT would reset the
    // chip mid-flash and the otadata flip below would never run, leaving
    // the user on the old slot — the same "OTA looked successful then
    // device rebooted to old version" symptom we hit on the network OTA
    // path.  Same pattern as OtaUpdater.cpp's download loop.
    esp_task_wdt_reset();
    if ((written & 0x3FFFF) == 0) {  // every 256 KB
      LOG_INF("RECOV", "  %u / %u KB", static_cast<unsigned>(written / 1024),
              static_cast<unsigned>(fileSize / 1024));
    }
  }
  file.close();
  const unsigned long writeMs = millis() - tStart;
  LOG_INF("RECOV", "Wrote %u bytes in %lu ms (%lu KB/s)", static_cast<unsigned>(fileSize), writeMs,
          static_cast<unsigned long>(fileSize / (writeMs ? writeMs : 1)));

  if (!skipBoardCheck) {
    if (!verifyPokiInkMagic(target)) {
      LOG_ERR("RECOV", "POKIINK_X3_FW_MAGIC not found — refusing to switch boot target. "
                      "File is likely for a different board (e.g. X4) or not a PokiInk-X3 build. "
                      "Bootloader will keep loading the current slot.");
      renameWithSuffix(path, ".rejected.wrongboard");
      return FlashResult::WRONG_BOARD;
    }
    LOG_INF("RECOV", "PokiInk-X3 board tag verified on flashed partition.");
  } else {
    LOG_INF("RECOV", "skipBoardCheck=true — POKIINK_X3_FW_MAGIC verification skipped at caller's "
                    "explicit request.  If the binary is for a different board (e.g. X4), the "
                    "device will brick on next boot.  Hope you know what you're doing.");
  }

  if (!flipOtadataTo(target)) {
    renameWithSuffix(path, ".rejected.otadata_write");
    return FlashResult::OTADATA_FAIL;
  }

  // Rename the source file so we don't re-flash on every boot.
  renameWithSuffix(path, ".applied");
  return FlashResult::SUCCESS;
}

}  // namespace

namespace {

// Probe in priority order — first hit wins, others are ignored this boot.
constexpr const char* kCandidates[] = {
    "/pokiink-recovery.bin",
    "/pokiink-update.bin",
    "/update.bin",
};

// Returns the path to the first candidate file that exists on SD, or nullptr.
const char* findCandidate() {
  for (const char* path : kCandidates) {
    if (Storage.exists(path)) return path;
  }
  return nullptr;
}

}  // namespace

bool hasMagicInFile(const char* path) {
  // Read first 256 KB of the file in chunks and scan for POKIINK_X3_FW_MAGIC.
  // Same scan range as verifyPokiInkMagic() uses on the flashed partition —
  // staying consistent means a pre-scan PASS guarantees the post-flash
  // verify will also PASS (modulo cosmic-ray-flips-during-write).
  FsFile file;
  if (!Storage.openFileForRead("RECOV", path, file)) {
    LOG_ERR("RECOV", "Magic pre-scan: open failed for %s", path);
    return false;
  }
  if (!file.seek(0)) {
    file.close();
    return false;
  }

  constexpr size_t SCAN_SIZE = 256 * 1024;
  constexpr size_t CHUNK = 4096;
  // 24-byte overlap between chunks so we don't miss a magic that straddles
  // a chunk boundary.  Carry the trailing bytes of the previous chunk into
  // the head of the next.
  constexpr size_t OVERLAP = static_cast<size_t>(POKIINK_X3_FW_MAGIC_LEN) - 1;
  static_assert(OVERLAP < CHUNK, "overlap must be smaller than chunk size");

  uint8_t buf[CHUNK + OVERLAP];
  std::memset(buf, 0, sizeof(buf));
  size_t carry = 0;       // bytes carried over from the previous chunk
  size_t scanned = 0;
  bool found = false;
  const uint8_t needle0 = static_cast<uint8_t>(POKIINK_X3_FW_MAGIC[0]);

  while (scanned < SCAN_SIZE) {
    const size_t want = std::min(CHUNK, SCAN_SIZE - scanned);
    const int n = file.read(buf + carry, want);
    if (n <= 0) break;
    const size_t total = carry + static_cast<size_t>(n);
    for (size_t i = 0; i + POKIINK_X3_FW_MAGIC_LEN <= total; ++i) {
      if (buf[i] != needle0) continue;
      if (std::memcmp(buf + i, POKIINK_X3_FW_MAGIC,
                      static_cast<size_t>(POKIINK_X3_FW_MAGIC_LEN)) == 0) {
        found = true;
        break;
      }
    }
    if (found) break;
    // Carry the last (MAGIC_LEN - 1) bytes into the next iteration.
    if (total >= OVERLAP) {
      std::memmove(buf, buf + total - OVERLAP, OVERLAP);
      carry = OVERLAP;
    } else {
      carry = total;
    }
    scanned += static_cast<size_t>(n);
    esp_task_wdt_reset();
  }
  file.close();
  return found;
}

FlashResult flashFromFile(const char* path, bool skipBoardCheck) {
  // Public wrapper around the private verifyAndFlash helper.  Same machinery,
  // exposed so the Settings → System → Update from SD activity can call it
  // with any user-picked .bin file (not just the auto-discovery filenames).
  // The activity passes skipBoardCheck=true after the user explicitly
  // confirms "Force install" on a previously-rejected wrong-board file.
  return verifyAndFlash(path, skipBoardCheck);
}

void runIfRequested(bool forceRecoveryMode) {
  if (forceRecoveryMode) {
    // ─── Forced recovery mode ─────────────────────────────────────────────
    // Triggered by main.cpp when the user holds UP+POWER at boot — the
    // Xteink OEM bootloader's "insert SD with update.bin" workflow that
    // PokiInk users expect to inherit.  Unlike the silent auto-check, this
    // mode BLOCKS waiting for the user to insert an SD card (or for an
    // already-present file to be detected by the SD layer).
    //
    // 60-second deadline keeps a stuck-button scenario from soft-locking
    // boot — if the deadline expires we fall back to normal boot, where the
    // user's currently installed firmware will run (and presumably let them
    // navigate to Settings → System Update or otherwise try again).
    constexpr unsigned long RECOVERY_TIMEOUT_MS = 60000;
    constexpr unsigned long POLL_INTERVAL_MS    = 500;
    const unsigned long deadline = millis() + RECOVERY_TIMEOUT_MS;
    LOG_INF("RECOV", "Forced recovery mode — polling SD for /update.bin (or /pokiink-update.bin "
                     "/ /pokiink-recovery.bin) every %lu ms for up to %lu s",
            POLL_INTERVAL_MS, RECOVERY_TIMEOUT_MS / 1000);

    unsigned long lastHeartbeatLog = 0;
    while (millis() < deadline) {
      if (const char* path = findCandidate()) {
        LOG_INF("RECOV", "Forced recovery: found %s — verifying", path);
        if (verifyAndFlash(path) == FlashResult::SUCCESS) {
          LOG_INF("RECOV", "Forced recovery SUCCESS — rebooting in 2 seconds");
          delay(2000);
          ESP.restart();
          // unreachable
        }
        LOG_ERR("RECOV", "Forced recovery FAILED for %s — see .rejected.* suffix. "
                          "Continuing to wait in case user drops a corrected file.",
                path);
        // Don't return — let the user fix the file and try again within the
        // 60 s window.  verifyAndFlash already renamed the rejected file so
        // findCandidate() won't re-trigger on the same path.
      }
      // Periodic heartbeat (every 5 s) so Serial monitor users know we're alive.
      const unsigned long now = millis();
      if (now - lastHeartbeatLog >= 5000) {
        LOG_INF("RECOV", "  ...waiting (%lu s remaining)",
                (deadline - now) / 1000);
        lastHeartbeatLog = now;
      }
      delay(POLL_INTERVAL_MS);
    }
    LOG_INF("RECOV", "Forced recovery timeout — continuing normal boot");
    return;
  }

  // ─── Silent auto-recovery (default) ─────────────────────────────────────
  // One-shot check.  If a candidate file is present we attempt flash; if
  // not, we return immediately so the running firmware can boot normally.
  if (const char* path = findCandidate()) {
    LOG_INF("RECOV", "SD auto-recovery: found %s — attempting install", path);
    if (verifyAndFlash(path) == FlashResult::SUCCESS) {
      LOG_INF("RECOV", "SD auto-recovery SUCCESS — rebooting in 2 seconds");
      delay(2000);
      ESP.restart();
      // unreachable
    }
    LOG_ERR("RECOV", "SD auto-recovery FAILED for %s — see .rejected.* suffix on SD for the reason", path);
    // Don't try the next candidate after a failure.  If the user dropped a
    // bad file we'd just overwrite the inactive partition with another bad
    // copy.  Better to surface the rejection clearly and let them retry.
  }
}

}  // namespace SdAutoRecovery
