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

// Color payloads
uint8_t payload_white[] = {0x33, 0x05, 0x0d, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc4};
uint8_t payload_pink[] = {0x33, 0x05, 0x0d, 0xff, 0x15, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09};

// Track handled devices
std::set<String> written_devices;

// Function prototypes
bool nameMatches(const char* name);
void bleScanAndControl(uint8_t* colorPayload);
void processUartCommand();
void blinkLED();

void setup() {
  Serial.begin(115200);
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  
  // Blink LED 3 times on startup
  blinkLED();
  
  // Initialize BLE
  BLEDevice::init("");
  
  Serial.println("READY. Send WHITE or PINK on UART to begin. Script stops after 1 scans with no new devices.");
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

void processUartCommand() {
  static String inputBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        inputBuffer.toUpperCase();
        inputBuffer.trim();
        
        if (inputBuffer == "WHITE") {
          written_devices.clear(); // Clear previous devices to allow re-control
          bleScanAndControl(payload_white);
          Serial.println("READY for next command.");
        } else if (inputBuffer == "PINK") {
          written_devices.clear(); // Clear previous devices to allow re-control
          bleScanAndControl(payload_pink);
          Serial.println("READY for next command.");
        } else {
          Serial.println("ERR");
        }
        
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}
