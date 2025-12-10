/*
 * PowerManager.cpp
 * 
 * Implementation of battery monitoring and power management.
 * See PowerManager.h for interface documentation.
 */

#include "PowerManager.h"

// ============================================================================
// PIN DEFINITIONS FOR POWER MANAGEMENT
// ============================================================================
// These pin assignments are for the Seeed XIAO nRF52840 and custom PCB
// Verify pin compatibility if adapting for different hardware

#define PIN_VBAT        (32)    // D32: Battery voltage input (via voltage divider)
#define PIN_VBAT_ENABLE (14)    // D14: Enable battery voltage reading (LOW = enabled)
#define PIN_HICHG       (22)    // D22: Charge current setting (LOW = 100mA, HIGH = 50mA)
#define PIN_CHG         (23)    // D23: Charge status indicator (LOW = charging, HIGH = not charging)

// Note: Pin 13 usage - verify this matches your hardware version
// Some charging circuits use pin 13 for additional current control
#define PIN_CHG_CURRENT (13)    // D13: Additional charge current control (if applicable)

// ============================================================================
// ADC to Voltage Conversion Constants
// ============================================================================
// The battery voltage is read through a voltage divider
// Formula: voltage = (ADC_reading / ADC_MAX) * VREF * divider_ratio
// For XIAO nRF52840: VREF = 3.6V, divider ratio = 2.961, 12-bit ADC (4096 levels)
#define ADC_MAX_VALUE   4096.0
#define ADC_VREF        3.6
#define VOLTAGE_DIVIDER 2.961

// ============================================================================
// Constructor - Initialize with default status
// ============================================================================

PowerManager::PowerManager() : batteryStatus('U'), lowPowerMode(false) {
    initPins();
}

// ============================================================================
// Pin Initialization
// ============================================================================

void PowerManager::initPins() {
    // Configure pin modes
    pinMode(PIN_CHG_CURRENT, OUTPUT);   // Charge current control (verify for your hardware)
    pinMode(PIN_VBAT_ENABLE, OUTPUT);   // Battery voltage reading enable
    pinMode(PIN_HICHG, OUTPUT);         // Charge current setting
    pinMode(PIN_VBAT, INPUT);           // Battery voltage ADC input
    pinMode(PIN_CHG, INPUT);            // Charge status input
    
    // Set initial states
    // Note: Setting HIGH typically selects lower charging current (50mA)
    // for safer charging of smaller batteries
    digitalWrite(PIN_CHG_CURRENT, HIGH);    // Set to 50mA charging current
    digitalWrite(PIN_VBAT_ENABLE, LOW);     // Enable battery voltage reading
    digitalWrite(PIN_HICHG, HIGH);          // Set charge current to 50mA
}

// ============================================================================
// Read Battery Voltage from ADC
// ============================================================================

float PowerManager::readBatteryVoltage() {
    // Read analog value from battery voltage pin
    int adcReading = analogRead(PIN_VBAT);
    
    // Convert ADC reading to actual voltage
    // Formula accounts for voltage divider and ADC reference voltage
    float voltage = (VOLTAGE_DIVIDER * ADC_VREF * adcReading) / ADC_MAX_VALUE;
    
    // Optional: Add averaging for more stable readings
    // You could take multiple samples and average them here
    
    return voltage;
}

// ============================================================================
// Classify Battery Status Based on Voltage
// ============================================================================

char PowerManager::classifyBatteryStatus(float voltage) {
    // Battery status thresholds for LiPo battery
    // These are conservative values to ensure device reliability
    
    if (voltage < 3.5) {
        // RED: Low battery - user should charge soon
        // Below 3.5V, LiPo is getting quite discharged
        return 'R';
    } 
    else if (voltage >= 3.5 && voltage < 3.9) {
        // YELLOW: Medium battery - moderate charge remaining
        // 3.5V-3.9V is typical operating range
        return 'Y';
    } 
    else {
        // GREEN: Good battery - well charged
        // Above 3.9V indicates good charge (approaching 4.2V full charge)
        return 'G';
    }
    
    // Note: You may want to adjust these thresholds based on your specific
    // battery chemistry and discharge characteristics
}

// ============================================================================
// Read and Update Battery Status
// ============================================================================

void PowerManager::readAndSaveBatteryStatus() {
    // Read current voltage
    float voltage = readBatteryVoltage();
    
    // Classify and save status
    char newStatus = classifyBatteryStatus(voltage);
    
    // Log status change if different from previous
    if (newStatus != batteryStatus && Serial) {
        Serial.print("Battery status changed: ");
        Serial.print(batteryStatus);
        Serial.print(" -> ");
        Serial.print(newStatus);
        Serial.print(" (");
        Serial.print(voltage, 2);
        Serial.println("V)");
    }
    
    batteryStatus = newStatus;
}

// ============================================================================
// FUTURE ENHANCEMENTS
// ============================================================================
/*
 * Potential additions for power management:
 * 
 * 1. Low Power Mode Implementation:
 *    - Reduce CPU clock speed when idle
 *    - Disable unused peripherals
 *    - Implement sleep between sensor readings
 * 
 * 2. Charge Detection:
 *    - Monitor PIN_CHG to detect when device is charging
 *    - Adjust behavior during charging (e.g., keep BLE active)
 * 
 * 3. Battery Capacity Estimation:
 *    - Track voltage over time to estimate remaining capacity
 *    - Provide percentage-based battery indicator
 * 
 * 4. Power Consumption Monitoring:
 *    - Log current draw in different modes
 *    - Estimate runtime based on current usage
 * 
 * 5. Adaptive Charging:
 *    - Adjust charging current based on battery temperature
 *    - Implement smart charging profiles
 */