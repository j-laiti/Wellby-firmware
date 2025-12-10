/*
 * BluetoothManager.cpp
 * 
 * Implementation of BLE connectivity and data streaming for Wellby device.
 * See BluetoothManager.h for interface documentation.
 */

#include "BluetoothManager.h"
#include "PPGManager.h"
#include "PowerManager.h"

// ============================================================================
// BLE SERVICE AND CHARACTERISTIC UUIDs
// ============================================================================
// Custom service for Wellby device - generated unique UUIDs
// You can generate your own UUIDs at https://www.uuidgenerator.net/

#define CUSTOM_SERVICE_UUID        "2ef946af-49fc-43f4-95c1-882a483f0a76"
#define METRIC_CHARACTERISTIC_UUID "8881ab16-7694-4891-aebe-b0b11c6549d4"
#define BATTERY_CHARACTERISTIC_UUID "a20a1ce0-5f2e-4230-88fe-05eb329dc545"
#define RAW_PPG_CHARACTERISTIC_UUID "4aa76196-2777-4205-8260-8e3274beb327"
#define RECORDING_CONTROL_CHARACTERISTIC_UUID "684c8f42-a60c-431c-b8ed-251e966d6a9a"

// ============================================================================
// Static Member Initialization
// ============================================================================

void (*BluetoothManager::userConnectionCallback)(void) = nullptr;
bool BluetoothManager::connected = false;
BLECharacteristic rawPpgCharacteristic;  // Global for callback access

BluetoothManager* BluetoothManager::instance = nullptr;
PPGManager* BluetoothManager::ppgManager = nullptr;
PowerManager* BluetoothManager::powerManager = nullptr;

// ============================================================================
// Initialize BLE Stack and Configure Services
// ============================================================================

void BluetoothManager::begin(const char* devicePrefix, const char* deviceNumber) {
    instance = this;  // Store instance for static callback access

    // Initialize Bluefruit BLE stack
    Bluefruit.begin();
    
    // Set transmit power (range: -40 to +8 dBm)
    // +4 dBm provides good range without excessive power consumption
    Bluefruit.setTxPower(4);
    
    // Set device name
    // If parameters provided, construct name; otherwise use default
    if (devicePrefix != nullptr && deviceNumber != nullptr) {
        char deviceName[16];
        snprintf(deviceName, sizeof(deviceName), "%s %s", devicePrefix, deviceNumber);
        Bluefruit.setName(deviceName);
        Serial.print("BLE Device Name: ");
        Serial.println(deviceName);
    } else {
        // Default name - update this for your specific device
        Bluefruit.setName("W 123");
        Serial.println("BLE Device Name: W 123 (default)");
    }

    // -------------------------------------------------------------------------
    // Create custom BLE service
    // -------------------------------------------------------------------------
    customService = BLEService(CUSTOM_SERVICE_UUID);
    customService.begin();

    // -------------------------------------------------------------------------
    // Register connection/disconnection callbacks
    // -------------------------------------------------------------------------
    Bluefruit.Periph.setConnectCallback(connectCallback);
    Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

    // -------------------------------------------------------------------------
    // Setup HRV Metrics Characteristic (notify only)
    // -------------------------------------------------------------------------
    // Transmits calculated heart rate variability metrics when available
    hrvCharacteristic = BLECharacteristic(METRIC_CHARACTERISTIC_UUID);
    hrvCharacteristic.setProperties(CHR_PROPS_NOTIFY);
    hrvCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    hrvCharacteristic.setFixedLen(128);  // Fixed length for metric data
    hrvCharacteristic.begin();

    // -------------------------------------------------------------------------
    // Setup Battery Status Characteristic (notify only)
    // -------------------------------------------------------------------------
    // Transmits battery level: 'G' (green/good), 'Y' (yellow/medium), 'R' (red/low)
    batteryStatusCharacteristic = BLECharacteristic(BATTERY_CHARACTERISTIC_UUID);
    batteryStatusCharacteristic.setProperties(CHR_PROPS_NOTIFY);
    batteryStatusCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    batteryStatusCharacteristic.setFixedLen(1);  // Single byte status
    batteryStatusCharacteristic.begin();
    
    // -------------------------------------------------------------------------
    // Setup Raw PPG Data Characteristic (notify only)
    // -------------------------------------------------------------------------
    // Streams real-time photoplethysmography sensor data to mobile app
    rawPpgCharacteristic = BLECharacteristic(RAW_PPG_CHARACTERISTIC_UUID);
    rawPpgCharacteristic.setProperties(CHR_PROPS_NOTIFY);
    rawPpgCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    rawPpgCharacteristic.setFixedLen(20);  // MTU-optimized packet size
    rawPpgCharacteristic.begin();

    // -------------------------------------------------------------------------
    // Setup Recording Control Characteristic (write only)
    // -------------------------------------------------------------------------
    // Allows mobile app to start/stop PPG recording
    // Write 0x01 to start, 0x00 to stop
    recControlCharacteristic = BLECharacteristic(RECORDING_CONTROL_CHARACTERISTIC_UUID);
    recControlCharacteristic.setProperties(CHR_PROPS_WRITE);
    recControlCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    recControlCharacteristic.setWriteCallback(recordingStartCallback);
    recControlCharacteristic.begin();
}

// ============================================================================
// Start BLE Advertising
// ============================================================================

void BluetoothManager::startAdvertising() {
    // Configure advertising packet content
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(customService);
    Bluefruit.Advertising.addName();
    
    // Auto-restart advertising if disconnected
    Bluefruit.Advertising.restartOnDisconnect(true);
    
    // Set advertising intervals (units of 0.625 ms)
    // Interval: 32 = 20ms, 244 = 152.5ms
    Bluefruit.Advertising.setInterval(32, 244);
    
    // Fast timeout: stop fast advertising after 30 seconds
    Bluefruit.Advertising.setFastTimeout(30);
    
    // Start advertising (0 = no timeout, advertise indefinitely)
    Bluefruit.Advertising.start(0);
    
    Serial.println("BLE Advertising started");
}

// ============================================================================
// Data Transmission Functions
// ============================================================================

void BluetoothManager::sendHrvMetrics(const char* data, int length) {
    if (Bluefruit.connected()) {
        hrvCharacteristic.notify((uint8_t*)data, length);
        Serial.println("HRV metrics transmitted");
    }
}

void BluetoothManager::sendRawPpgData(const uint8_t* data, size_t length) {
    if (Bluefruit.connected()) {
        rawPpgCharacteristic.notify(data, length);
        // Note: Avoid excessive Serial prints during high-frequency data streaming
    }
}

void BluetoothManager::updateBatteryStatus(char status) {
    if (Bluefruit.connected()) {
        uint8_t statusByte = static_cast<uint8_t>(status);
        Serial.print("Transmitting battery status: ");
        Serial.println(status);
        batteryStatusCharacteristic.notify(&statusByte, sizeof(statusByte));
    }
}

// ============================================================================
// Advertising Control
// ============================================================================

bool BluetoothManager::isAdvertising() {
    return Bluefruit.Advertising.isRunning();
}

void BluetoothManager::stopAdvertising() {
    if (isAdvertising()) {
        Bluefruit.Advertising.stop();
        Serial.println("BLE Advertising stopped");
    }
}

// ============================================================================
// Manager Linking Functions
// ============================================================================

void BluetoothManager::setPowerManager(PowerManager& powerMgr) {
    powerManager = &powerMgr;
}

void BluetoothManager::setPPGManager(PPGManager& ppg) {
    ppgManager = &ppg;
}

// ============================================================================
// Connection Status and Callbacks
// ============================================================================

bool BluetoothManager::isConnected() {
    return connected;
}

void BluetoothManager::setConnectionCallback(void (*callback)(void)) {
    userConnectionCallback = callback;
}

// ============================================================================
// Static Callback: BLE Connection Event
// ============================================================================

void BluetoothManager::connectCallback(uint16_t conn_handle) {
    if (instance) {
        instance->connected = true;
        Serial.println("BLE Device Connected");

        // Read and transmit current battery status to newly connected device
        if (instance->powerManager) {
            instance->powerManager->readAndSaveBatteryStatus();
            instance->updateBatteryStatus(instance->powerManager->getBatteryStatus());
        }

        // Execute user-defined connection callback if registered
        if (userConnectionCallback) {
            userConnectionCallback();
        }
    }
}

// ============================================================================
// Static Callback: BLE Disconnection Event
// ============================================================================

void BluetoothManager::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
    connected = false;
    Serial.print("BLE Device Disconnected, reason: ");
    Serial.println(reason, HEX);
    
    // Note: Advertising will auto-restart if restartOnDisconnect is enabled
}

// ============================================================================
// Static Callback: Recording Control Characteristic Write
// ============================================================================

void BluetoothManager::recordingStartCallback(uint16_t conn_hdl, BLECharacteristic *chr, uint8_t *data, uint16_t len) {
    // Verify data length and PPGManager availability
    if (len == 1 && ppgManager != nullptr) {
        if (data[0] == 0x01) {
            // Start recording command from mobile app
            Serial.println("Recording START command received from app");
            ppgManager->startRealTimePPGRecording();
        } else if (data[0] == 0x00) {
            // Stop recording command from mobile app
            Serial.println("Recording STOP command received from app");
            ppgManager->stopRealTimePPGRecording();
        } else {
            Serial.print("Unknown recording command: 0x");
            Serial.println(data[0], HEX);
        }
    } else {
        Serial.println("Invalid recording control data received");
    }
}
