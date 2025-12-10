/*
 * Wellby Wearable - Main Firmware
 * 
 * An open-source wearable device for photoplethysmography (PPG) based health monitoring
 * with Bluetooth Low Energy connectivity for real-time data streaming to mobile applications.
 * 
 * HARDWARE COMPATIBILITY:
 * - Wellby v1: Module-based design using Seeed Studio XIAO nRF52840
 * - Wellby v2: Custom PCB design with nRF52840 microcontroller
 * 
 * REQUIRED HARDWARE:
 * - nRF52840 microcontroller (Seeed XIAO nRF52840 or custom PCB)
 * - MAX30105 PPG sensor (I2C interface)
 * - Push button connected to pin D7
 * - RGB LED (common cathode) for status indication
 * - LiPo battery with charging circuit
 * 
 * PIN CONFIGURATION:
 * - D7: User button input (active LOW with internal pullup) + wake-up pin
 * - I2C: MAX30105 sensor communication (SDA/SCL)
 * - LED_GREEN, LED_RED, LED_BLUE: Status indication LEDs
 * - See PowerManager.h for battery management pins
 * 
 * DEVICE OPERATION MODES:
 * 1. IDLE: Default state, green LED, sensor shutdown, ready for button input
 *    - Can enable periodic proximity checks or routine metrics collection
 * 2. BLE: Bluetooth enabled, blue LED, real-time PPG streaming to connected app
 *    - Activated by double-press from IDLE mode
 *    - Recording initiated from mobile app via BLE characteristic write
 * 3. SLEEP: Low power mode, all LEDs off, wake on button press
 *    - Activated by long-press (>800ms) from any mode
 *    - Uses nRF52 SYSTEMOFF mode for minimal power consumption
 * 
 * USER INTERACTIONS:
 * - Double press: Toggle between IDLE and BLE modes
 * - Long press (>800ms): Enter sleep mode
 * - Button press from sleep: Wake device to IDLE mode
 * 
 * BLE SERVICES:
 * - Real-time PPG data streaming (200Hz sampling with averaging)
 * - Battery status monitoring
 * - Remote recording control from mobile app
 * - HRV metrics transmission (if on-device processing enabled)
 * 
 * AUTHOR: Justin Laiti (formatted with support from Claude Sonnet 4.5)
 * LICENSE: See repository root for open-source license information
 * REPOSITORY: []
 * 
 * For detailed build instructions and hardware specifications, see the 
 * associated HardwareX publication and repository documentation.
 */

#include "PPGManager.h"
#include "buttonManager.h"
#include "BluetoothManager.h"
#include "PowerManager.h"

// ============================================================================
// CONFIGURATION - Modify these values for your specific device
// ============================================================================

// Device identifier for BLE advertising (change for each device you build)
#define DEVICE_NAME "W"        // Prefix for the wearable name that appears in BLE scanning
#define DEVICE_NUMBER "123"         // Unique identifier for this specific device

// Button and wake-up pin configuration
#define BUTTON_PIN 7                // Physical button connected to D7 (active LOW)
#define WAKEUP_PIN D7               // Same pin used for wake from sleep mode

// ============================================================================
// SYSTEM INITIALIZATION
// ============================================================================

PowerManager powerManager;
ButtonManager buttonManager(BUTTON_PIN);

BluetoothManager bluetoothManager;
PPGManager ppgManager(bluetoothManager);

// System state machine - controls overall device behavior
enum SystemState { 
  IDLE,   // Low power state, sensor off, green LED, ready for commands
  SLEEP,  // Ultra-low power state, can only wake via button
  BLE     // Active state, Bluetooth enabled, real-time data streaming
};
SystemState currentSystemState = IDLE;

// Optional: Idle state sub-modes for autonomous monitoring
// Uncomment and implement in loop() if desired
// enum IdleState { CHECK, RECORD, REST };
// IdleState currentIdleState = REST;

// ============================================================================
// SETUP - Initialize all system components
// ============================================================================

void setup() {
  // Initialize serial communication for debugging
  // Note: Serial output is optional and can be disabled in production
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // Wait up to 3s for serial monitor
  Serial.println("=== Wellby Firmware Initializing ===");
  
  // Initialize Bluetooth manager and configure services
  Serial.println("Initializing Bluetooth...");
  bluetoothManager.begin(DEVICE_NAME, DEVICE_NUMBER);
  
  // Link power manager to BLE for battery status updates
  bluetoothManager.setPowerManager(powerManager);
  
  // Link PPG manager to BLE for remote recording control
  bluetoothManager.setPPGManager(ppgManager);

  // Initialize and configure the MAX30105 PPG sensor
  Serial.println("Initializing PPG sensor...");
  ppgManager.setUpSensor();
  
  // Immediately shut down sensor to conserve power in IDLE state
  // Sensor will be activated when recording is requested
  ppgManager.shutDownSensor();
  
  // Set initial LED state: Green = IDLE mode, ready for commands
  buttonManager.setLEDs(true, false, false);
  
  Serial.println("=== Initialization Complete ===");
  Serial.println("Device in IDLE mode (Green LED)");
  Serial.println("Double-press: Enable BLE | Long-press: Sleep mode");
}

// ============================================================================
// MAIN LOOP - Process button inputs and manage system states
// ============================================================================

void loop() {
  // Continuously check for button press events
  // ButtonManager handles debouncing and press pattern detection
  buttonManager.handleButton();

  // -------------------------------------------------------------------------
  // LONG PRESS: Enter sleep mode from any state
  // -------------------------------------------------------------------------
  if (buttonManager.isLongPress()) {
    Serial.println("Long press detected - entering sleep mode");
    
    // Visual feedback: Blink red LED before sleeping
    buttonManager.setLEDs(false, true, false);
    delay(1000);
    buttonManager.setLEDs(false, false, false);
    
    // Transition to sleep state
    currentSystemState = SLEEP;
  }

  // -------------------------------------------------------------------------
  // DOUBLE PRESS: Toggle between IDLE and BLE modes
  // -------------------------------------------------------------------------
  if (buttonManager.isDoublePress()) {
    
    // Can only enable BLE from IDLE state (safety check)
    if (currentSystemState == IDLE) {
      Serial.println("Double-press detected - activating BLE mode");
      
      // Start Bluetooth advertising so mobile app can discover device
      bluetoothManager.startAdvertising();
      
      // Visual feedback: Blue LED indicates BLE active
      buttonManager.setLEDs(false, false, true);
      
      // Update battery status and transmit to app when connected
      bluetoothManager.updateBatteryStatus(powerManager.getBatteryStatus());
      
      currentSystemState = BLE;
      
    } 
    // If already in BLE mode, return to IDLE
    else if (currentSystemState == BLE) {
      Serial.println("Double-press detected - returning to IDLE mode");
      
      // Stop Bluetooth advertising and disconnect
      bluetoothManager.stopAdvertising();
      
      // Visual feedback: Green LED indicates IDLE
      buttonManager.setLEDs(true, false, false);
      
      currentSystemState = IDLE;
    }
  }

  // -------------------------------------------------------------------------
  // STATE-SPECIFIC BEHAVIOR
  // -------------------------------------------------------------------------
  
  if (currentSystemState == IDLE) {
    // IDLE MODE: Device is on but conserving power
    // 
    // Potential additions for autonomous monitoring:
    // - Periodic proximity checks using ppgManager.proximityCheck()
    // - Scheduled metric collection and storage
    // - Motion detection before recording
    //
    // Currently: Device waits for user input (button press)
    
  } 
  else if (currentSystemState == BLE && bluetoothManager.isConnected()) {
    // BLE MODE: Device connected to mobile app
    // 
    // Real-time PPG recording is managed by the mobile app:
    // 1. App writes 0x01 to recording control characteristic → start recording
    // 2. PPG data streams continuously to app via BLE notifications
    // 3. App writes 0x00 to recording control characteristic → stop recording
    //
    // The realTimePPGRec() function handles this automatically
    ppgManager.realTimePPGRec();
    
  } 
  else if (currentSystemState == SLEEP) {
    // SLEEP MODE: Ultra-low power consumption
    // This function does not return until device wakes via button press
    sleepMode();
  }

  // Note: Recording can also be initiated via button press if desired
  // Add button press detection here and call ppgManager.startRealTimePPGRecording()
}


// ============================================================================
// SLEEP MODE - Ultra-low power consumption state
// ============================================================================

/*
 * Enters nRF52 SYSTEMOFF mode for minimal power consumption.
 * 
 * POWER CONSUMPTION:
 * - Only GPIO sense mechanism remains active
 * - All RAM contents are lost
 * - Device essentially performs a reset on wake
 * 
 * WAKE MECHANISM:
 * - Button press on WAKEUP_PIN (D7) triggers wake
 * - GPIO sense detects HIGH level (button release)
 * - Device resets and runs setup() again
 * 
 */
void sleepMode() {
  Serial.println("Entering SYSTEMOFF sleep mode...");
  Serial.println("WARNING: Device will reset on wake. Press button to wake.");
  Serial.flush();  // Ensure message is transmitted before sleep
  
  // Shut down PPG sensor to eliminate power draw
  ppgManager.shutDownSensor();
  
  // Configure button pin for sense capabilities in SYSTEMOFF mode
  pinMode(WAKEUP_PIN, INPUT_PULLUP_SENSE);
  
  // Enable GPIO sense to wake on HIGH level (button release after press)
  // This is necessary because SYSTEMOFF disables all other wake sources
  NRF_GPIO->PIN_CNF[WAKEUP_PIN] |= (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);
  
  // Enter SYSTEMOFF mode - this is effectively a controlled power-off
  // Device will appear "off" and consume <1μA until button press
  NRF_POWER->SYSTEMOFF = 1;
  
  // Code execution never reaches here - device resets on wake
}