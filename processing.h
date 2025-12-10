/*
 * processing.h
 * 
 * Signal processing functions for on-device PPG analysis and HRV calculation.
 * 
 * OVERVIEW:
 * This library provides a complete pipeline for processing raw PPG signals
 * and extracting heart rate variability (HRV) metrics directly on the device.
 * Currently, processing is handled by Firebase cloud functions after BLE
 * transmission, but these functions enable on-device processing if desired.
 * 
 * PROCESSING PIPELINE:
 * 1. Signal preprocessing (filtering, noise removal)
 * 2. Peak/valley detection in PPG waveform
 * 3. RR interval calculation (time between heartbeats)
 * 4. HRV metrics computation (HR, SDNN, RMSSD)
 * 
 * KEY FUNCTIONS:
 * - bandpassFilter: Remove DC offset and high-frequency noise
 * - movingAverageFilter: Smooth signal for better peak detection
 * - thresholdPeakDetection: Find heartbeat peaks in PPG signal
 * - calcRrIntervals: Calculate inter-beat intervals
 * - calculateHRVMetrics: Compute heart rate and HRV metrics
 * 
 * HRV METRICS EXPLAINED:
 * - Heart Rate (HR): Average beats per minute
 * - SDNN: Standard deviation of RR intervals (overall variability)
 * - RMSSD: Root mean square of successive differences (short-term variability)
 * 
 * MEMORY CONSIDERATIONS:
 * These functions use dynamic memory allocation (malloc/free).
 * Ensure sufficient heap memory is available (typically >8KB for processing).
 * Always free() returned pointers after use to prevent memory leaks.
 * 
 * USAGE EXAMPLE:
 * See PPGManager::processPPGData() for complete pipeline implementation.
 * 
 * REFERENCE:
 * Based on processing algorithms from:
 * https://github.com/j-laiti/PPG-affect-classification
 * 
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include <Arduino.h>

// ============================================================================
// FILTERING FUNCTIONS
// ============================================================================

// Apply bandpass filter to remove DC offset and high-frequency noise
// Optimized for heart rate frequencies (0.5-4 Hz)
// Parameters:
//   input: Raw PPG signal (integer array)
//   output: Filtered signal (float array, pre-allocated)
//   size: Number of samples
void bandpassFilter(long* input, float* output, int size);

// Apply moving average filter for signal smoothing
// Reduces noise while preserving signal shape
// Parameters:
//   input: Signal to filter (float array)
//   output: Smoothed signal (float array, pre-allocated)
//   size: Number of samples
//   windowSize: Number of samples to average (typical: 4-8)
void movingAverageFilter(float* input, float* output, int size, int windowSize);

// ============================================================================
// SIGNAL PREPROCESSING
// ============================================================================

// Remove zero values and flat regions from signal
// Returns dynamically allocated array - caller must free()
// Parameters:
//   input: Signal with zeros (integer array)
//   size: Input size
//   newSize: Output parameter - size of returned array
// Returns: Filtered signal (caller must free())
float* removeZero(long* input, int size, int* newSize);

// Assess signal quality (placeholder - to be implemented)
// Returns: Quality score (1 = good, 0 = poor)
int signalQual(float* signal, int fs);

// ============================================================================
// PEAK AND VALLEY DETECTION
// ============================================================================

// Detect valleys (minima) in PPG signal
// Returns dynamically allocated array - caller must free()
// Parameters:
//   dataset: PPG signal (float array)
//   size: Number of samples
//   fs: Sampling frequency (Hz)
//   min_distance: Minimum time between valleys (seconds)
//   valley_count: Output parameter - number of valleys found
// Returns: Array of valley indices (caller must free())
int* valleyDetection(float* dataset, int size, int fs, float min_distance, int* valley_count);

// Pair consecutive valleys for segment analysis
// Returns dynamically allocated 2D array - caller must free()
// Parameters:
//   valleys: Array of valley indices
//   valley_count: Number of valleys
// Returns: Array of [start, end] pairs (caller must free each pair and array)
int** pairValley(int* valleys, int valley_count);

// Detect peaks (maxima) in PPG signal using adaptive thresholding
// Returns dynamically allocated array - caller must free()
// Parameters:
//   dataset: PPG signal (float array)
//   size: Number of samples
//   fs: Sampling frequency (Hz)
//   threshold_factor: Multiplier for mean threshold (typical: 0.8-1.2)
//   min_distance: Minimum time between peaks (seconds, typical: 0.4)
//   peak_count: Output parameter - number of peaks found
// Returns: Array of peak indices (caller must free())
int* thresholdPeakDetection(float* dataset, int size, int fs, float threshold_factor, float min_distance, int* peak_count);

// ============================================================================
// NOISE ELIMINATION (STATISTICAL METHODS)
// ============================================================================

// Calculate statistical metrics for signal segments
// Used for quality assessment and noise detection
void statisticDetection(float* signal, int size, int fs, int** valleys, int valley_count, float* stds, float* kurtosiss, float* skews);

// Determine thresholds for noise elimination based on statistics
void statisticThreshold(float* stds, float* kurtosiss, float* skews, int valley_count, float* ths, float* std_ths, float* kurt_ths, float* skews_ths);

// Eliminate noisy segments using statistical thresholds (basic version)
void eliminateNoiseInTime(float* data, int size, int fs, float* ths, int cycle);

// Eliminate noisy segments using statistical thresholds (advanced version)
// Returns dynamically allocated array - caller must free()
float* eliminateNoiseInTime(float* data, int size, int fs, float* ths, int cycle, int** valleys, int valley_count, int* newSize);

// ============================================================================
// HRV CALCULATION
// ============================================================================

// Calculate RR intervals from detected peaks
// Returns dynamically allocated array - caller must free()
// Parameters:
//   peaks: Array of peak indices
//   peakCount: Number of peaks
//   fs: Sampling frequency (Hz)
//   rrCount: Output parameter - number of valid RR intervals
// Returns: Array of RR intervals in milliseconds (caller must free())
// Note: Only returns intervals in physiologically valid range (300-1500ms)
int* calcRrIntervals(int* peaks, int peakCount, int fs, int* rrCount);

// Calculate HRV metrics from RR intervals
// Returns dynamically allocated array - caller must free()
// Parameters:
//   rr_intervals: Array of RR intervals (milliseconds)
//   rrCount: Number of RR intervals
//   metricsCount: Output parameter - number of metrics (always 3)
// Returns: Array containing [Heart Rate (bpm), SDNN (ms), RMSSD (ms)]
//          Caller must free()
float* calculateHRVMetrics(int* rr_intervals, int rrCount, int* metricsCount);

// Estimate consistency of RR intervals (standard deviation)
// Used for signal quality assessment
// Returns: Standard deviation of RR intervals, or -1 if insufficient data
float estimateRRIntervalConsistency(int* rr_intervals, int rrCount);

#endif
