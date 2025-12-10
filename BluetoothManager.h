/*
 * BluetoothManager.h
 * 
 * Manages Bluetooth Low Energy (BLE) connectivity for the Wellby device.
 * 
 * FEATURES:
 * - Custom BLE service with multiple characteristics
 * - Real-time PPG data streaming
 * - Battery status notifications
 * - Remote recording control from mobile app
 * - HRV metrics transmission (when available)
 * 
 * BLE SERVICE STRUCTURE:
 * - Custom Service UUID: 2ef946af-49fc-43f4-95c1-882a483f0a76
 *   - Raw PPG Data Characteristic (notify): 4aa76196-2777-4205-8260-8e3274beb327
 *   - HRV Metrics Characteristic (notify): 8881ab16-7694-4891-aebe-b0b11c6549d4
 *   - Battery Status Characteristic (notify): a20a1ce0-5f2e-4230-88fe-05eb329dc545
 *   - Recording Control Characteristic (write): 684c8f42-a60c-431c-b8ed-251e966d6a9a
 * 
 * USAGE:
 * 1. Create instance: BluetoothManager bluetoothManager;
 * 2. Initialize: bluetoothManager.begin("W", "142");
 * 3. Set managers: setPowerManager() and setPPGManager()
 * 4. Start advertising: startAdvertising()
 * 5. Data automatically streams when connected
 * 
 * AUTHOR: Justin Laiti
 */

#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <bluefruit.h>

// Forward declarations to avoid circular dependencies
class PPGManager;
class PowerManager;

class BluetoothManager {
public:
    // Initialize BLE with optional device name (format: "W 142")
    // Parameters: prefix (e.g., "W"), number (e.g., "142")
    // If not provided, uses default "W 142"
    void begin(const char* devicePrefix = nullptr, const char* deviceNumber = nullptr);
    
    // Start BLE advertising so device is discoverable
    void startAdvertising();
    
    // Stop BLE advertising and disconnect
    void stopAdvertising();
    
    // Send HRV metrics to connected device
    void sendHrvMetrics(const char* data, int length);
    
    // Send raw PPG data samples to connected device
    void sendRawPpgData(const uint8_t* data, size_t length);
    
    // Update and transmit battery status
    void updateBatteryStatus(const char status);
    
    // Check if device is currently connected to mobile app
    bool isConnected();
    
    // Check if BLE advertising is active
    bool isAdvertising();
    
    // Register callback function to execute on connection
    void setConnectionCallback(void (*callback)(void));
    
    // Link PowerManager for battery status updates
    void setPowerManager(PowerManager& powerMgr);
    
    // Link PPGManager for remote recording control
    void setPPGManager(PPGManager& ppg);

private:
    // BLE service and characteristics
    BLEService customService;
    BLECharacteristic hrvCharacteristic;
    BLECharacteristic batteryStatusCharacteristic;
    BLECharacteristic recControlCharacteristic;
    
    // Static callback functions (required by Bluefruit library)
    static void connectCallback(uint16_t conn_handle);
    static void disconnectCallback(uint16_t conn_handle, uint8_t reason);
    static void recordingStartCallback(uint16_t conn_hdl, BLECharacteristic *chr, uint8_t *data, uint16_t len);
    
    // User-defined connection callback
    static void (*userConnectionCallback)(void);
    
    // Static state tracking (required for callbacks)
    static bool connected;
    static BluetoothManager* instance;
    static PPGManager* ppgManager;
    static PowerManager* powerManager;
};

#endif
