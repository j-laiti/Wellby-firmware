/*
 * PPGManager.h
 * 
 * Manages the MAX30105 photoplethysmography (PPG) sensor
 * 
 * FEATURES:
 * - Real-time PPG data collection and streaming
 * - Configurable sampling rate and LED settings
 * - Proximity detection for wear status
 * - Optional motion detection via IMU (LSM6DS3)
 * - Optional on-device signal processing (see processing.h)
 * - Power-efficient sensor shutdown when idle
 * 
 * SENSOR CONFIGURATION:
 * - MAX30105: Triple LED (Red, IR, Green) PPG sensor
 * - Sampling rate: 200 Hz (configurable)
 * - Sample averaging: 8 samples per reading
 * - Green LED used for PPG (best for heart rate)
 * - I2C communication at FAST speed (400 kHz)
 * 
 * DATA STREAMING:
 * - Real-time mode: Continuous streaming via BLE
 * - Recording duration: 60 seconds (configurable)
 * - Data format: 16-bit samples with 0xFE delimiter
 * - Packet size: 18 bytes per BLE transmission
 * 
 * OPTIONAL FEATURES:
 * - Proximity check: Detects if sensor is touching skin
 * - Motion check: Uses IMU to detect excessive movement
 * - On-device processing: Signal filtering and HRV calculation
 *   (Currently disabled, see processPPGData() for implementation)
 * 
 * USAGE:
 * 1. Create instance: PPGManager ppgManager(bluetoothManager);
 * 2. Initialize: ppgManager.setUpSensor();
 * 3. Start recording: ppgManager.startRealTimePPGRecording();
 * 4. Stream data: Call ppgManager.realTimePPGRec() in loop
 * 5. Stop recording: ppgManager.stopRealTimePPGRecording();
 * 
 */

#ifndef PPGMANAGER_H
#define PPGMANAGER_H

// Forward declaration to avoid circular dependency
class BluetoothManager;

#include <Wire.h>
#include "MAX30105.h"
#include <math.h>
#include "processing.h"
#include "LSM6DS3.h"

// ============================================================================
// SENSOR CONFIGURATION CONSTANTS
// ============================================================================

#define SAMPLING_RATE 200           // Samples per second (Hz)
#define SAMPLING_AVERAGE 8          // Number of samples averaged per reading
#define COLLECTION_TIME 60000       // Recording duration (milliseconds)
#define REST_TIME 30000             // Rest period between recordings (unused)

// Buffer sizing for on-device processing (if enabled)
#define BUFFER_SIZE ((SAMPLING_RATE / SAMPLING_AVERAGE) * (COLLECTION_TIME / 2000))
#define IGNORE_EDGE_SAMPLES 25      // Edge samples to ignore in filtering

// Proximity and motion detection thresholds
#define PROXIMITY_THRESHOLD 1000    // ADC threshold for skin contact detection
#define GYRO_THRESHOLD 10.0         // Gyroscope magnitude threshold for motion
#define SAMPLE_WINDOW 3000          // Sampling window for checks (milliseconds)

// ============================================================================
// PPGManager Class
// ============================================================================

class PPGManager {
  public:
    // Constructor - requires reference to BluetoothManager for data transmission
    PPGManager(BluetoothManager& bluetoothManager);
    
    // Initialize MAX30105 sensor with default configuration
    void setUpSensor();
    
    // Check if sensor is in contact with skin (returns true if worn)
    bool proximityCheck();
    
    // Collect a single PPG sample and transmit via BLE
    void collectPPGData();
    
    // Clear internal data buffer
    void resetPPGArray();
    
    // Power down the sensor to save battery
    void shutDownSensor();
    
    // Wake up sensor from shutdown state
    void turnOnSensor();
    
    // Check if data collection has completed (for buffered mode)
    bool isDataCollected();
    
    // Start real-time PPG recording session
    void startRealTimePPGRecording();
    
    // Stop real-time PPG recording session
    void stopRealTimePPGRecording();
    
    // Main recording function - call repeatedly in loop during BLE mode
    // Handles automatic 60-second recording timeout
    void realTimePPGRec();
    
  private:
    // Reference to BluetoothManager for data transmission
    BluetoothManager& bluetoothManager;
    
    // Hardware sensor instances
    MAX30105 particleSensor;        // MAX30105 PPG sensor
    LSM6DS3 myIMU;                  // LSM6DS3 IMU (optional, for motion detection)
    
    // Data buffers for on-device processing (if enabled)
    long ppgRawData[BUFFER_SIZE];
    float filteredData[BUFFER_SIZE];
    float trimmedData[BUFFER_SIZE - 2 * IGNORE_EDGE_SAMPLES];
    float smoothedData[BUFFER_SIZE - 2 * IGNORE_EDGE_SAMPLES];
    
    // Recording state management
    int PPGindex;                   // Current buffer index
    unsigned long recordingStartTime;  // Timestamp when recording started
    bool recordingInProgress;       // Flag indicating active recording
    
    // Batch PPG samples for efficient BLE transmission
    // Converts 32-bit sensor reading to 16-bit format with delimiter
    void batchPPGData(uint32_t ppgSignal);
    
    // Optional: Motion detection using IMU
    // Uncomment in .cpp if LSM6DS3 is connected and configured
    // bool motionCheck();
    
    // Optional: On-device signal processing
    // Uncomment in .cpp to enable HRV calculation without cloud processing
    // void processPPGData(StorageManager& storage);
};

#endif