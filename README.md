# Wellby-firmware
Android code for the Wellby wearable device hardware

An open-source wearable device for photoplethysmography (PPG) based health monitoring with Bluetooth Low Energy connectivity for real-time data streaming to mobile applications.

## HARDWARE COMPATIBILITY:
- Wellby v1: Module-based design using Seeed Studio XIAO nRF52840
- Wellby v2: Custom PCB design with nRF52840 microcontroller

## REQUIRED HARDWARE:
- nRF52840 microcontroller (Seeed XIAO nRF52840 or custom PCB)
- MAX30105 PPG sensor (I2C interface)
- Push button connected to pin D7
- RGB LED (common cathode) for status indication
- LiPo battery with charging circuit

## PIN CONFIGURATION:
- D7: User button input (active LOW with internal pullup) + wake-up pin
- I2C: MAX30105 sensor communication (SDA/SCL)
- LED_GREEN, LED_RED, LED_BLUE: Status indication LEDs
- See PowerManager.h for battery management pins

## DEVICE OPERATION MODES:
1. IDLE: Default state, green LED, sensor shutdown, ready for button input
    - Can enable periodic proximity checks or routine metrics collection
2. BLE: Bluetooth enabled, blue LED, real-time PPG streaming to connected app
    - Activated by double-press from IDLE mode
    - Recording initiated from mobile app via BLE characteristic write
3. SLEEP: Low power mode, all LEDs off, wake on button press
    - Activated by long-press (>800ms) from any mode
    - Uses nRF52 SYSTEMOFF mode for minimal power consumption

## USER INTERACTIONS:
 - Double press: Toggle between IDLE and BLE modes
 - Long press (>800ms): Enter sleep mode
 - Button press from sleep: Wake device to IDLE mode
