/*
 * ButtonManager.h
 * 
 * Handles user button input with debouncing and multi-press pattern detection.
 * 
 * FEATURES:
 * - Hardware debouncing with configurable delay
 * - Long press detection (>800ms by default)
 * - Double press detection (two presses within 500ms)
 * - LED control for visual feedback
 * - Lockout mechanism to prevent false triggers
 * 
 * HARDWARE REQUIREMENTS:
 * - Push button connected to specified pin (active LOW with internal pullup)
 * - RGB LED for status indication
 * 
 * USAGE:
 * 1. Create instance: ButtonManager buttonManager(BUTTON_PIN);
 * 2. Call handleButton() in main loop
 * 3. Check isLongPress() and isDoublePress() for events
 * 4. Use setLEDs() to provide visual feedback
 * 
 */

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

class ButtonManager {
public:
    ButtonManager(int buttonPin); // Constructor
    void handleButton();          // Call this regularly in the loop
    bool isLongPress();           // Returns true if a long press is detected
    bool isDoublePress();         // Returns true if a double press is detected
    void setLEDs(bool green, bool red, bool blue); // // Note: Assumes common cathode LED (active LOW)

private:
    // Pin assignments
    int buttonPin;
    
    // Timing variables for press detection
    unsigned long pressStartTime;      // When current press began
    unsigned long lastPressTime;       // When previous press occurred
    unsigned long lastDebounceTime;    // For debounce filtering
    
    // State tracking
    bool isPressed;              // Currently being pressed
    bool isSecondPress;          // Waiting for second press in double-press
    bool longPressDetected;      // Long press event flag
    bool doublePressDetected;    // Double press event flag
    bool lockOut;                // Prevents multiple rapid triggers
    bool lastButtonState;        // Previous button reading
    
    // Timing thresholds (in milliseconds)
    static const unsigned long longPressThreshold = 1500;    // Duration for long press (currently unused, see handleButton)
    static const unsigned long doublePressThreshold = 500;   // Max time between double presses
    static const unsigned long debounceDelay = 50;           // Debounce time
    static const unsigned long lockOutDelay = 600;           // Lockout period after event
};

#endif