/*
 * ButtonManager.cpp
 * 
 * Implementation of button handling with debouncing and pattern detection.
 * See ButtonManager.h for interface documentation.
 */

#include "buttonManager.h"
#include <Arduino.h>

// ============================================================================
// Constructor - Initialize button pin and LED pins
// ============================================================================

ButtonManager::ButtonManager(int pin) {
    buttonPin = pin;
    pinMode(buttonPin, INPUT_PULLUP);  // Button is active LOW
    
    // Initialize state variables
    isPressed = false;
    isSecondPress = false;
    longPressDetected = false;
    doublePressDetected = false;
    lastDebounceTime = 0;
    pressStartTime = 0;
    lastPressTime = 0;
    lockOut = false;
    lastButtonState = HIGH;  // Unpressed state (pullup)

    // Configure LED pins for status feedback
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    
    // Initialize LEDs to OFF state (active LOW, so write HIGH)
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
}

// ============================================================================
// Main button handling - Call repeatedly in loop()
// ============================================================================

void ButtonManager::handleButton() {
    // Handle lockout period after detecting a press pattern
    // Prevents false triggers from contact bounce or rapid presses
    if (lockOut) {
        if (millis() - lastDebounceTime > lockOutDelay) {
            lockOut = false;  // Lockout expired, resume normal operation
        } else {
            return;  // Still in lockout, ignore button
        }
    }

    // Read current button state
    int currentButtonState = digitalRead(buttonPin);
    
    // -------------------------------------------------------------------------
    // BUTTON PRESS DETECTED (HIGH → LOW transition)
    // -------------------------------------------------------------------------
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        pressStartTime = millis(); 
        isPressed = true;
    }
    
    // -------------------------------------------------------------------------
    // BUTTON RELEASE DETECTED (LOW → HIGH transition)
    // -------------------------------------------------------------------------
    if (currentButtonState == HIGH && lastButtonState == LOW && isPressed) {
        unsigned long pressDuration = millis() - pressStartTime;
        isPressed = false;

        // Long press detection: press held for >800ms
        if (pressDuration > 800) {
            longPressDetected = true;
        } 
        // Short press: could be single press or part of double press
        else {
            if (!isSecondPress) {
                // First press in potential double press sequence
                lastPressTime = millis();
                isSecondPress = true;  // Wait for possible second press
            } else {
                // Second press detected - check timing
                if (millis() - lastPressTime <= 500) {
                    // Valid double press (within 500ms window)
                    doublePressDetected = true;
                    isSecondPress = false;
                }
            }
        }
    } 
    // -------------------------------------------------------------------------
    // DOUBLE PRESS TIMEOUT - Second press didn't arrive in time
    // -------------------------------------------------------------------------
    else if (isSecondPress && (millis() - lastPressTime > 500)) {
        // Timeout expired, reset double press detection
        // (Could implement single-press action here if needed)
        isSecondPress = false;
    }

    // Update state for next iteration
    lastButtonState = currentButtonState;
}

// ============================================================================
// Event query functions - Return true once per event, then clear flag
// ============================================================================

bool ButtonManager::isLongPress() {
    if (longPressDetected) {
        longPressDetected = false;  // Clear flag after reading
        return true;
    }
    return false;
}

bool ButtonManager::isDoublePress() {
    if (doublePressDetected) {
        doublePressDetected = false;  // Clear flag after reading
        return true;
    }
    return false;
}

// ============================================================================
// LED Control - Visual feedback for device state
// ============================================================================

void ButtonManager::setLEDs(bool green, bool red, bool blue) {
    // Common cathode RGB LED: LOW = ON, HIGH = OFF
    digitalWrite(LED_GREEN, green ? LOW : HIGH);
    digitalWrite(LED_RED, red ? LOW : HIGH);
    digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}