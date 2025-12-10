/*
 * PowerManager.h
 * 
 * Manages battery monitoring and power-related functions for Wellby device.
 * 
 * FEATURES:
 * - Battery voltage reading via ADC
 * - Three-level battery status classification (Green/Yellow/Red)
 * - Charging current control
 * - Charge status monitoring
 * 
 * BATTERY STATUS LEVELS:
 * - 'G' (Green):  ≥3.9V - Good battery level
 * - 'Y' (Yellow): 3.5V-3.9V - Medium battery level
 * - 'R' (Red):    <3.5V - Low battery, charging recommended
 * - 'U' (Unknown): Initial state before first reading
 * 
 * HARDWARE REQUIREMENTS:
 * - LiPo battery connected to charging circuit
 * - Voltage divider for ADC reading (see PIN_VBAT)
 * - Charge control circuitry
 * 
 * PIN CONFIGURATION:
 * - See PowerManager.cpp for specific pin definitions
 * - Pins may vary slightly between Wellby v1 and v2
 * 
 * TYPICAL LIPO VOLTAGE RANGES:
 * - Fully charged: ~4.2V
 * - Nominal: 3.7V
 * - Discharged: 3.0V
 * - Critical: <3.0V (avoid deep discharge)
 * 
 * USAGE:
 * 1. Create instance: PowerManager powerManager;
 * 2. Read status: powerManager.readAndSaveBatteryStatus();
 * 3. Get status: char status = powerManager.getBatteryStatus();
 * 4. Transmit via BLE when needed
 * 
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "BluetoothManager.h"

class PowerManager {
public:
    // Constructor - initializes pins and sets default status
    PowerManager();
    
    // Read current battery voltage and update status classification
    void readAndSaveBatteryStatus();
    
    // Get current battery status ('G', 'Y', 'R', or 'U')
    char getBatteryStatus() const { return batteryStatus; }

private:
    // Current battery status classification
    char batteryStatus;
    
    // Reserved for future low-power mode implementation
    bool lowPowerMode = false;
    
    // Initialize all power management pins
    void initPins();
    
    // Read raw battery voltage from ADC
    float readBatteryVoltage();
    
    // Classify voltage into status category
    char classifyBatteryStatus(float voltage);
};

#endif