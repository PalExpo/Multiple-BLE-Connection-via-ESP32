#include <Arduino.h>
#include <NimBLEDevice.h>

// UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pRxCharacteristic = nullptr;

// === Frame Processing ===
void processReceivedFrame(const std::string &frame) {
  Serial.print("Received Frame (HEX): ");
  for (char c : frame) {
    Serial.printf("%02X ", static_cast<uint8_t>(c));
  }
  Serial.println();

  uint8_t ble_command = frame[1];
  Serial.printf("ble_command: 0x%02X\n", ble_command);
}

// === Server Callbacks ===
class MyBLEServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override {
    Serial.print("Client connected: ");
    Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());

    // Adjust connection parameters
    pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);

    // Continue advertising for more clients
    NimBLEDevice::startAdvertising();
  }

  void onDisconnect(NimBLEServer *pServer) override {
    Serial.println("Client disconnected - restarting advertising");
    NimBLEDevice::startAdvertising();
  }

  void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc) override {
    Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
  }

  uint32_t onPassKeyRequest() override {
    Serial.println("Passkey Request");
    return 123456;
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    Serial.printf("Confirm passkey: %u\n", pass_key);
    return true;
  }

  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    if (!desc->sec_state.encrypted) {
      NimBLEDevice::getServer()->disconnect(desc->conn_handle);
      Serial.println("Encryption failed - disconnecting");
    } else {
      Serial.println("Authentication complete");
    }
  }
};

// === Characteristic Callbacks ===
class MyBLECharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    std::string received = pCharacteristic->getValue();

    Serial.print("Received: ");
    for (auto c : received) {
      Serial.printf("%02X ", static_cast<uint8_t>(c));
    }
    Serial.println();

    processReceivedFrame(received);
  }

  void onSubscribe(NimBLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue) override {
    Serial.printf("Client %u subscribed to %s\n",
                  desc->conn_handle, pCharacteristic->getUUID().toString().c_str());
  }
};

// === BLE Setup ===
void setupBLE() {
  Serial.println("Initializing BLE Server...");
  NimBLEDevice::init("Multi-Device over BLE");

  BLEAddress address = NimBLEDevice::getAddress();
  Serial.print("MAC Address: ");
  Serial.println(address.toString().c_str());

  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(103);
  NimBLEDevice::setSecurityAuth(true, true, true);


  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyBLEServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pTxCharacteristic->setCallbacks(new MyBLECharacteristicCallbacks());

  pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_RX_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  pRxCharacteristic->setCallbacks(new MyBLECharacteristicCallbacks());

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinInterval(32);
  pAdvertising->setMaxInterval(64);
  pAdvertising->start();

  Serial.println("BLE Advertising started");
}

// === Arduino Main ===
void setup() {
  Serial.begin(115200);
  setupBLE();
}

void loop() {
  // Main logic
}
