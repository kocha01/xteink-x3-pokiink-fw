#pragma once

#include <cstdint>
#include <string>

struct XteinkPairingInfo {
  std::string deviceId;
  std::string bleName;
  std::string bleMac;
  std::string version;
  std::string pairingJson;
  std::string pairingUrl;
};

bool readXteinkPairingMac(uint8_t mac[6]);
std::string formatXteinkDeviceId(const uint8_t mac[6]);
std::string formatXteinkBleName(const uint8_t mac[6]);
std::string formatXteinkBleMac(const uint8_t mac[6]);
XteinkPairingInfo buildXteinkPairingInfo(const uint8_t mac[6]);
XteinkPairingInfo buildXteinkPairingInfo();
