#include <Arduino.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n**************************************");
    Serial.println("*    X3 NEXTGEN FW: BOOT SUCCESS     *");
    Serial.println("**************************************");
    
    uint8_t mac[6];
    char macStr[13];
    WiFi.macAddress(mac);
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    Serial.printf("Device MAC: %s\n", macStr);
    Serial.printf("Pair URL: https://www.xteink.com/pages/xteink-apps?v=%s\n", macStr);
    Serial.println("--------------------------------------");
}

void loop() {
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.println("[System] Status: OK | Mode: NextGen");
        lastHeartbeat = millis();
    }
}
