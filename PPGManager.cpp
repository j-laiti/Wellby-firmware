/*
 * PPGManager.cpp
 * 
 * Implementation of MAX30105 PPG sensor management and data collection.
 * See PPGManager.h for interface documentation.
 */

#include "PPGManager.h"
#include "BluetoothManager.h"
#include <vector>

// ============================================================================
// Constructor
// ============================================================================

PPGManager::PPGManager(BluetoothManager& bluetoothManager)
    : bluetoothManager(bluetoothManager), PPGindex(0), recordingInProgress(false) {
    // Initialize member variables
    // Sensor initialization happens in setUpSensor()
}

// ============================================================================
// Sensor Initialization and Configuration
// ============================================================================

void PPGManager::setUpSensor() {
    // Attempt to initialize MAX30105 sensor via I2C
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("ERROR: MAX30105 sensor not found!");
        Serial.println("Please check:");
        Serial.println("  - I2C wiring (SDA/SCL connections)");
        Serial.println("  - Power supply to sensor");
        Serial.println("  - I2C address (0x57 default)");
        while (1);  // Halt execution - sensor is critical
    }
    
    Serial.println("MAX30105 sensor initialized successfully");
    
    // Configure sensor parameters:
    // setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange)
    // see MAX30105 sensor details for more setup options

    // Parameters explained:
    // - ledBrightness: 255 = maximum brightness (0-255)
    // - sampleAverage: 8 = average 8 samples per reading (reduces noise)
    // - ledMode: 3 = Red + IR + Green LEDs enabled
    // - sampleRate: 200 = 200 samples per second
    // - pulseWidth: 411 = 411 microseconds pulse width (affects resolution)
    // - adcRange: 2048 = ADC range in nanoamps (2048nA, lower = more sensitive)
    particleSensor.setup(255, SAMPLING_AVERAGE, 3, SAMPLING_RATE, 411, 2048);
    
    // Disable Red and IR LEDs initially (we only use Green for PPG)
    // Green LED is optimal for heart rate detection through skin
    particleSensor.setPulseAmplitudeRed(0);    // Red LED off
    particleSensor.setPulseAmplitudeIR(0);     // IR LED off
    // Green LED brightness is set by setup() function above
    
    Serial.println("Sensor configuration:");
    Serial.print("  Sampling rate: "); Serial.print(SAMPLING_RATE); Serial.println(" Hz");
    Serial.print("  Sample averaging: "); Serial.println(SAMPLING_AVERAGE);
    Serial.println("  Active LED: Green (optimal for heart rate)");
}

// ============================================================================
// Real-Time Recording Control
// ============================================================================

void PPGManager::startRealTimePPGRecording() {
    Serial.println("Starting real-time PPG recording");
    
    // Reset data buffer for new recording
    resetPPGArray();
    
    // Ensure sensor is powered on and ready
    turnOnSensor();
    
    // Record start time for automatic timeout
    recordingStartTime = millis();
    
    // Set recording flag
    recordingInProgress = true;
    
    Serial.println("Recording active - data streaming to BLE");
}

void PPGManager::stopRealTimePPGRecording() {
    Serial.println("Stopping real-time PPG recording");
    
    // Clear recording flag
    recordingInProgress = false;
    
    // Power down sensor to conserve battery
    shutDownSensor();
    
    Serial.println("Recording stopped - sensor powered down");
}

void PPGManager::realTimePPGRec() {
    // Only proceed if recording is active
    if (recordingInProgress) {
        // Check if we're still within the 60-second recording window
        if (millis() - recordingStartTime < COLLECTION_TIME) {
            // Collect and transmit one PPG sample
            collectPPGData();
        } else {
            // Recording duration exceeded - auto-stop
            Serial.println("Recording timeout (60s) - stopping automatically");
            stopRealTimePPGRecording();
        }
    }
}

// ============================================================================
// Data Collection and Transmission
// ============================================================================

void PPGManager::collectPPGData() {
    // Read raw PPG value from green LED channel
    // Green LED provides best signal quality for heart rate through skin
    // Alternative: particleSensor.getRed() or particleSensor.getIR()
    uint32_t ppgRaw = particleSensor.getGreen();
    
    // Debug output (comment out for production to reduce serial overhead)
    Serial.println(ppgRaw);
    
    // Batch data for efficient BLE transmission
    batchPPGData(ppgRaw);
}

void PPGManager::batchPPGData(uint32_t ppgSignal) {
    // Static vector persists between function calls to accumulate samples
    static std::vector<uint8_t> dataBatch;
    
    // Protocol constants
    const uint8_t delimiter = 0xFE;    // Sample delimiter for parsing
    const int packetSize = 18;         // BLE MTU-optimized packet size
    
    // Convert 32-bit sensor reading to 16-bit for transmission
    // This reduces bandwidth while maintaining sufficient resolution
    int16_t scaledSignal = static_cast<int16_t>(ppgSignal);
    
    // Debug output (disable in production for performance)
    Serial.print("PPG Signal: "); Serial.println(ppgSignal);
    Serial.print("Scaled Signal: "); Serial.println(scaledSignal);
    
    // Pack 16-bit value as two bytes (big-endian)
    dataBatch.push_back((scaledSignal >> 8) & 0xFF);  // High byte
    dataBatch.push_back(scaledSignal & 0xFF);         // Low byte
    dataBatch.push_back(delimiter);                   // Sample delimiter
    
    // Once we have enough data, transmit packet
    if (dataBatch.size() >= packetSize) {
        // Debug output (disable in production)
        Serial.print("Transmitting batch: ");
        for (auto byte : dataBatch) {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        // Transmit via BLE
        bluetoothManager.sendRawPpgData(dataBatch.data(), dataBatch.size());
        
        // Clear batch for next packet
        dataBatch.clear();
    }
}

// ============================================================================
// Proximity Detection (Wear Status)
// ============================================================================

bool PPGManager::proximityCheck() {
    Serial.println("Checking proximity (wear status)...");
    
    unsigned long startTime = millis();
    float greenTotal = 0;
    int sampleCount = 0;
    
    // Collect samples over 3-second window
    while (millis() - startTime < SAMPLE_WINDOW) {
        float greenValue = particleSensor.getGreen();
        greenTotal += greenValue;
        sampleCount++;
        delay(100);  // Sample every 100ms (10 Hz)
    }
    
    // Calculate average green LED intensity
    float avgGreen = greenTotal / sampleCount;
    
    Serial.print("Average Green LED reading: ");
    Serial.println(avgGreen);
    
    // When sensor is touching skin, light is reflected back to detector
    // Higher values indicate contact, lower values indicate no contact
    bool isWorn = avgGreen > PROXIMITY_THRESHOLD;
    
    Serial.print("Sensor status: ");
    Serial.println(isWorn ? "WORN (contact detected)" : "NOT WORN (no contact)");
    
    return isWorn;
}

// ============================================================================
// OPTIONAL: Motion Detection Using IMU
// ============================================================================
/*
 * Uncomment this function if your device includes an LSM6DS3 IMU sensor
 * and you want to detect excessive motion before recording.
 * 
 * Motion artifacts can corrupt PPG signals, so checking for stillness
 * before recording can improve data quality.
 */

// bool PPGManager::motionCheck() {
//     Serial.println("Checking motion level...");
//     
//     unsigned long startTime = millis();
//     float gyroXTotal = 0, gyroYTotal = 0, gyroZTotal = 0;
//     int count = 0;
//     
//     // Collect gyroscope samples over 3-second window
//     while (millis() - startTime < SAMPLE_WINDOW) {
//         // Read 3-axis gyroscope data (angular velocity)
//         float gx = myIMU.readFloatGyroX();
//         float gy = myIMU.readFloatGyroY();
//         float gz = myIMU.readFloatGyroZ();
//         
//         // Accumulate for averaging
//         gyroXTotal += gx;
//         gyroYTotal += gy;
//         gyroZTotal += gz;
//         count++;
//         
//         delay(100);  // Sample every 100ms
//     }
//     
//     // Calculate average angular velocity on each axis
//     float avgGyroX = gyroXTotal / count;
//     float avgGyroY = gyroYTotal / count;
//     float avgGyroZ = gyroZTotal / count;
//     
//     // Calculate magnitude of motion vector
//     float gyroMag = sqrt(avgGyroX * avgGyroX + 
//                          avgGyroY * avgGyroY + 
//                          avgGyroZ * avgGyroZ);
//     
//     Serial.print("Average gyroscope magnitude: ");
//     Serial.println(gyroMag);
//     
//     // Check if motion is below acceptable threshold
//     bool isStill = gyroMag < GYRO_THRESHOLD;
//     
//     Serial.print("Motion status: ");
//     Serial.println(isStill ? "STILL (OK to record)" : "MOVING (wait for stillness)");
//     
//     return isStill;
// }

// ============================================================================
// OPTIONAL: On-Device Signal Processing
// ============================================================================
/*
 * This function performs on-device PPG signal processing and HRV calculation.
 * Currently commented out because processing is handled by Firebase function
 * after BLE transmission to mobile app.
 * 
 * To enable on-device processing:
 * 1. Uncomment this function
 * 2. Implement/include StorageManager for saving results
 * 3. Call this function after collecting BUFFER_SIZE samples
 * 4. See processing.h for available signal processing functions
 * 
 * Processing pipeline:
 * 1. Bandpass filter (remove DC offset and high-frequency noise)
 * 2. Trim edge artifacts from filtering
 * 3. Moving average smoothing
 * 4. Peak detection in PPG signal
 * 5. Calculate RR intervals (time between peaks)
 * 6. Calculate HRV metrics (HR, SDNN, RMSSD)
 */

// void PPGManager::processPPGData(StorageManager& storage) {
//     Serial.println("=== Starting On-Device PPG Processing ===");
//     
//     // Step 1: Apply bandpass filter to remove DC offset and noise
//     // Filter parameters optimized for heart rate frequencies (0.5-4 Hz)
//     Serial.println("Applying bandpass filter...");
//     bandpassFilter(ppgRawData, filteredData, BUFFER_SIZE);
//     
//     // Step 2: Trim edge samples affected by filter initialization
//     Serial.println("Trimming edge artifacts...");
//     for (int i = 0; i < BUFFER_SIZE - 2 * IGNORE_EDGE_SAMPLES; i++) {
//         trimmedData[i] = filteredData[i + IGNORE_EDGE_SAMPLES];
//     }
//     
//     // Step 3: Apply moving average filter for additional smoothing
//     Serial.println("Applying moving average filter...");
//     movingAverageFilter(trimmedData, smoothedData, 
//                         BUFFER_SIZE - 2 * IGNORE_EDGE_SAMPLES, 6);
//     
//     // Step 4: Detect peaks in smoothed signal (corresponds to heartbeats)
//     Serial.println("Detecting peaks...");
//     int peakCount;
//     int* peaks = thresholdPeakDetection(
//         smoothedData + 15,                              // Skip first 15 samples
//         BUFFER_SIZE - 2 * IGNORE_EDGE_SAMPLES - 15,    // Adjusted size
//         SAMPLING_RATE / SAMPLING_AVERAGE,               // Effective sampling rate
//         0.9,                                            // Threshold factor
//         0.4,                                            // Minimum distance factor
//         &peakCount
//     );
//     
//     // Debug: Print detected peak locations
//     Serial.print("Detected "); Serial.print(peakCount); Serial.println(" peaks");
//     for (int i = 0; i < peakCount; i++) {
//         Serial.print("  Peak "); Serial.print(i);
//         Serial.print(" at index: "); Serial.println(peaks[i] + 15);
//     }
//     
//     // Step 5: Calculate RR intervals from peak locations
//     Serial.println("Calculating RR intervals...");
//     int rrCount;
//     int* rr_intervals = calcRrIntervals(
//         peaks, 
//         peakCount, 
//         SAMPLING_RATE / SAMPLING_AVERAGE, 
//         &rrCount
//     );
//     
//     // Step 6: Calculate HRV metrics from RR intervals
//     Serial.println("Calculating HRV metrics...");
//     int metricsCount;
//     float* metrics = calculateHRVMetrics(rr_intervals, rrCount, &metricsCount);
//     
//     // Step 7: Store results if calculation successful
//     if (metrics != NULL) {
//         unsigned long timestamp = millis();
//         Serial.println("HRV Metrics calculated:");
//         Serial.print("  Heart Rate: "); Serial.print(metrics[0]); Serial.println(" bpm");
//         Serial.print("  SDNN: "); Serial.print(metrics[1]); Serial.println(" ms");
//         Serial.print("  RMSSD: "); Serial.print(metrics[2]); Serial.println(" ms");
//         
//         // Store to internal storage (requires StorageManager)
//         storage.storeMetrics(timestamp, metrics[0], metrics[1], metrics[2]);
//         
//         free(metrics);
//     } else {
//         Serial.println("ERROR: HRV calculation failed");
//     }
//     
//     // Cleanup dynamically allocated memory
//     free(peaks);
//     free(rr_intervals);
//     
//     Serial.println("=== Processing Complete ===");
// }

// ============================================================================
// Utility Functions
// ============================================================================

void PPGManager::resetPPGArray() {
    // Clear data buffer and reset index
    memset(ppgRawData, 0, sizeof(ppgRawData));
    PPGindex = 0;
    Serial.println("PPG data buffer reset");
}

void PPGManager::shutDownSensor() {
    // Put MAX30105 into low-power shutdown mode
    // Consumes <1µA in this state vs ~600µA when active
    particleSensor.shutDown();
    Serial.println("MAX30105 sensor powered down");
}

void PPGManager::turnOnSensor() {
    // Wake sensor from shutdown mode
    // Sensor returns to previous configuration
    particleSensor.wakeUp();
    Serial.println("MAX30105 sensor powered on");
}

bool PPGManager::isDataCollected() {
    // Check if enough data has been collected for processing
    // Useful for buffered/batch recording mode
    return PPGindex >= BUFFER_SIZE;
}