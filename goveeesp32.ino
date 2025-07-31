#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEScan.h"
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"
#include <set>
#include <string>
#include <vector>

// LED pin for ESP32 (built-in LED)
#define LED_PIN 2

// Target device patterns
const char* name_patterns[] = {"ihoment_H6008", "Govee_H6001"};
const int num_patterns = 2;

// BLE UUIDs
static BLEUUID serviceUUID("00010203-0405-0607-0809-0a0b0c0d1910");
static BLEUUID charUUID("00010203-0405-0607-0809-0a0b0c0d2b11");

// Fixed command payloads
uint8_t power_on[] = {0x33, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33};
uint8_t power_off[] = {0x33, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32};

// Dynamic color payload buffer
uint8_t color_payload[20];

// Track handled devices
std::set<String> written_devices;

// Function prototypes
bool nameMatches(const char* name);
void bleScanAndControl(uint8_t* colorPayload);
void processUartCommand();
void blinkLED();
uint8_t calculateChecksum(uint8_t* data, int length);
void buildColorCommand(uint32_t hexColor);
bool parseHexColor(String input, uint32_t* color);

void setup() {
  Serial.begin(115200);
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  
  // Blink LED 3 times on startup
  blinkLED();
  
  // Initialize BLE
  BLEDevice::init("");
  
  Serial.println("READY. Commands: ON, OFF, or 6-digit hex color (e.g., FFFFFF for white, FF0000 for red).");
}

void loop() {
  processUartCommand();
  delay(50);
}

void blinkLED() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

bool nameMatches(const char* name) {
  if (!name) return false;
  
  for (int i = 0; i < num_patterns; i++) {
    if (strstr(name, name_patterns[i]) != nullptr) {
      return true;
    }
  }
  return false;
}

void bleScanAndControl(uint8_t* colorPayload) {
  int consecutive_no_new = 0;
  
  while (consecutive_no_new < 1) {
    Serial.println("Scanning for BLE devices...");
    
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    BLEScanResults* foundDevices = pBLEScan->start(5, false);
    
    bool found_new = false;
    
    for (int i = 0; i < foundDevices->getCount(); i++) {
      BLEAdvertisedDevice device = foundDevices->getDevice(i);
      String deviceName = device.getName();
      
      if (!nameMatches(deviceName.c_str())) {
        continue;
      }
      
      if (written_devices.find(deviceName) != written_devices.end()) {
        Serial.printf("Already handled %s, skipping.\r\n", deviceName.c_str());
        continue;
      }
      
      Serial.printf("Handling new device %s\r\n", deviceName.c_str());
      
      BLEClient* pClient = BLEDevice::createClient();
      
      if (!pClient->connect(&device)) {
        Serial.println("Connect error");
        continue;
      }
      
      Serial.println("Connected.");
      
      BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
      if (pRemoteService == nullptr) {
        Serial.println("Failed to find service");
        pClient->disconnect();
        continue;
      }
      
      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
      if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic");
        pClient->disconnect();
        continue;
      }
      
      Serial.println("Writing...");
      
      // Write the payload 3 times with delay
      for (int j = 0; j < 3; j++) {
        pRemoteCharacteristic->writeValue(colorPayload, 20);
        delay(200);
      }
      
      Serial.println("Write(s) done");
      written_devices.insert(deviceName);
      found_new = true;
      
      pClient->disconnect();
      Serial.println("Disconnected.");
    }
    
    pBLEScan->clearResults();
    
    if (found_new) {
      consecutive_no_new = 0;
    } else {
      consecutive_no_new++;
      Serial.printf("No new device found. Consecutive unsuccessful scans: %d/1\r\n", consecutive_no_new);
    }
    
    delay(2000);
  }
  
  Serial.println("No new devices found after 1 consecutive scans. Stopping.");
}

uint8_t calculateChecksum(uint8_t* data, int length) {
  uint8_t checksum = 0;
  for (int i = 0; i < length - 1; i++) {  // Exclude the last byte (checksum position)
    checksum ^= data[i];
  }
  return checksum;
}

void buildColorCommand(uint32_t hexColor) {
  // Extract RGB components
  uint8_t r = (hexColor >> 16) & 0xFF;
  uint8_t g = (hexColor >> 8) & 0xFF;
  uint8_t b = hexColor & 0xFF;
  
  // Build the color command: 33050d[RGB][padding]
  color_payload[0] = 0x33;
  color_payload[1] = 0x05;
  color_payload[2] = 0x0d;
  color_payload[3] = r;
  color_payload[4] = g;
  color_payload[5] = b;
  
  // Fill padding with zeros
  for (int i = 6; i < 19; i++) {
    color_payload[i] = 0x00;
  }
  
  // Calculate and set checksum
  color_payload[19] = calculateChecksum(color_payload, 20);
  
  Serial.printf("Built color command for RGB(%02X,%02X,%02X): ", r, g, b);
  for (int i = 0; i < 20; i++) {
    Serial.printf("%02X", color_payload[i]);
  }
  Serial.println();
}

bool parseHexColor(String input, uint32_t* color) {
  // Remove any # prefix
  if (input.startsWith("#")) {
    input = input.substring(1);
  }
  
  // Check if it's exactly 6 hex characters
  if (input.length() != 6) {
    return false;
  }
  
  // Check if all characters are valid hex
  for (int i = 0; i < 6; i++) {
    char c = input.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  
  // Convert to integer
  *color = strtoul(input.c_str(), NULL, 16);
  return true;
}

void processUartCommand() {
  static String inputBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        String originalInput = inputBuffer;
        inputBuffer.toUpperCase();
        inputBuffer.trim();
        
        if (inputBuffer == "ON") {
          Serial.println("Turning lights ON...");
          written_devices.clear();
          bleScanAndControl(power_on);
          Serial.println("READY for next command.");
        } else if (inputBuffer == "OFF") {
          Serial.println("Turning lights OFF...");
          written_devices.clear();
          bleScanAndControl(power_off);
          Serial.println("READY for next command.");
        } else {
          // Try to parse as hex color
          uint32_t color;
          if (parseHexColor(originalInput, &color)) {
            Serial.printf("Setting color to #%06X...\n", (unsigned int)color);
            buildColorCommand(color);
            written_devices.clear();
            bleScanAndControl(color_payload);
            Serial.println("READY for next command.");
          } else {
            Serial.println("ERR - Use ON, OFF, or 6-digit hex color (e.g., FFFFFF, FF0000)");
          }
        }
        
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}
