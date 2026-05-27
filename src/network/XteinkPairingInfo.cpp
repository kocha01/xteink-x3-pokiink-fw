#include "XteinkPairingInfo.h"

#include <Logging.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <mbedtls/aes.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "dev"
#endif

namespace {
constexpr const char* kPairingUrlBase = "https://www.xteink.com/pages/xteink-apps?v=";
constexpr const char* kOfficialPairBrand = "xteink_ble";
constexpr const char* kOfficialDeviceType = "ESP32C3_X3";
constexpr std::array<uint8_t, 16> kOfficialQrKey = {
    0xD4, 0xC3, 0xB1, 0x8B, 0xE0, 0x47, 0x39, 0x22,
    0x6C, 0x05, 0x9D, 0x1F, 0x5A, 0xFE, 0x76, 0xA8,
};

std::string hexEncodeLower(const uint8_t* data, const size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";

  std::string encoded;
  encoded.resize(len * 2);
  for (size_t i = 0; i < len; i++) {
    encoded[i * 2] = kHex[(data[i] >> 4) & 0x0F];
    encoded[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return encoded;
}

std::string buildOfficialPairingJson(const XteinkPairingInfo& info) {
  std::string json;
  json.reserve(160);
  json += "{\"brand\":\"";
  json += kOfficialPairBrand;
  json += "\",\"device_type\":\"";
  json += kOfficialDeviceType;
  json += "\",\"version\":\"";
  json += info.version;
  json += "\",\"device_id\":\"";
  json += info.deviceId;
  json += "\",\"ble_name\":\"";
  json += info.bleName;
  json += "\",\"ble_mac\":\"";
  json += info.bleMac;
  json += "\"}";
  return json;
}

std::string encryptOfficialPairingJson(const std::string& plaintext) {
  if (plaintext.empty()) {
    return "";
  }

  const size_t padding = 16 - (plaintext.size() % 16);
  std::vector<uint8_t> padded(plaintext.begin(), plaintext.end());
  padded.insert(padded.end(), padding == 0 ? 16 : padding, static_cast<uint8_t>(padding == 0 ? 16 : padding));

  uint8_t iv[16];
  uint8_t ivWorking[16];
  esp_fill_random(iv, sizeof(iv));
  memcpy(ivWorking, iv, sizeof(iv));

  std::vector<uint8_t> cipherText(padded.size());

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const int setKeyResult = mbedtls_aes_setkey_enc(&aes, kOfficialQrKey.data(), 128);
  if (setKeyResult != 0) {
    LOG_ERR("XTBLE", "Failed to set QR AES key (%d)", setKeyResult);
    mbedtls_aes_free(&aes);
    return "";
  }

  const int encryptResult =
      mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded.size(), ivWorking, padded.data(), cipherText.data());
  mbedtls_aes_free(&aes);
  if (encryptResult != 0) {
    LOG_ERR("XTBLE", "Failed to encrypt QR payload (%d)", encryptResult);
    return "";
  }

  std::string encoded = hexEncodeLower(iv, sizeof(iv));
  encoded += hexEncodeLower(cipherText.data(), cipherText.size());
  return encoded;
}
}  // namespace

bool readXteinkPairingMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
    return true;
  }

  WiFi.macAddress(mac);
  for (uint8_t value : std::array<uint8_t, 6>{mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]}) {
    if (value != 0) {
      return true;
    }
  }

  return false;
}

std::string formatXteinkDeviceId(const uint8_t mac[6]) {
  // The stock X3 firmware appears to derive an 8-digit decimal-ish device ID
  // from hardware bytes rather than exposing the full MAC directly.
  const uint32_t numericId =
      (static_cast<uint32_t>(mac[3]) << 16) | (static_cast<uint32_t>(mac[4]) << 8) | static_cast<uint32_t>(mac[5]);
  return std::to_string(numericId);
}

std::string formatXteinkBleName(const uint8_t mac[6]) {
  char buffer[15];
  snprintf(buffer, sizeof(buffer), "XTEPD_BLE_%02X%02X", mac[4], mac[5]);
  return buffer;
}

std::string formatXteinkBleMac(const uint8_t mac[6]) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buffer;
}

XteinkPairingInfo buildXteinkPairingInfo(const uint8_t mac[6]) {
  XteinkPairingInfo info;
  info.deviceId = formatXteinkDeviceId(mac);
  info.bleName = formatXteinkBleName(mac);
  info.bleMac = formatXteinkBleMac(mac);
  info.version = CROSSPOINT_VERSION;
  info.pairingJson = buildOfficialPairingJson(info);

  const std::string encryptedPayload = encryptOfficialPairingJson(info.pairingJson);
  info.pairingUrl = std::string(kPairingUrlBase) + (encryptedPayload.empty() ? info.deviceId : encryptedPayload);
  return info;
}

XteinkPairingInfo buildXteinkPairingInfo() {
  uint8_t mac[6] = {};
  readXteinkPairingMac(mac);
  return buildXteinkPairingInfo(mac);
}
