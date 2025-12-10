/*
 * processing.cpp
 * 
 * Implementation of signal processing functions for PPG analysis.
 * See processing.h for interface documentation.
 * 
 * NOTE: These functions are currently NOT used in the default firmware.
 * Processing is handled by Firebase cloud functions after BLE transmission.
 * To enable on-device processing, see PPGManager::processPPGData().
 * 
 * REFERENCE:
 * Algorithms adapted from: https://github.com/j-laiti/PPG-affect-classification
 */

#include "processing.h"
#include <stdlib.h>  // malloc, free, realloc
#include <math.h>    // sqrt, pow, ceil

// ============================================================================
// BANDPASS FILTER
// ============================================================================
/*
 * Second-order IIR bandpass filter designed for heart rate frequencies.
 * Removes DC offset and high-frequency noise while preserving 0.5-4 Hz band.
 * 
 * Filter coefficients (b, a) were designed using digital filter design tools
 * for optimal heart rate signal extraction from PPG.
 */
void bandpassFilter(long* input, float* output, int size) {
    // Filter coefficients (numerator and denominator)
    float b[] = {0.292893, 0, -0.292893};  // Numerator coefficients
    float a[] = {1, -1.16574, 0.292893};   // Denominator coefficients
    
    for (int i = 0; i < size; i++) {
        if (i < 2) {
            // Initialize first two samples (insufficient history for filtering)
            output[i] = input[i];
        } else {
            // Apply IIR filter equation: y[n] = b[0]*x[n] + b[1]*x[n-1] + b[2]*x[n-2] 
            //                                   - a[1]*y[n-1] - a[2]*y[n-2]
            output[i] = b[0] * input[i] + b[1] * input[i - 1] + b[2] * input[i - 2] -
                        a[1] * output[i - 1] - a[2] * output[i - 2];
        }
    }
}

// ============================================================================
// MOVING AVERAGE FILTER
// ============================================================================
/*
 * Simple moving average filter for signal smoothing.
 * Each output sample is the average of the current and previous windowSize samples.
 */
void movingAverageFilter(float* input, float* output, int size, int windowSize) {
    for (int i = 0; i < size; i++) {
        float sum = 0;
        int count = 0;
        
        // Average current and previous samples within window
        for (int j = 0; j < windowSize; j++) {
            if ((i - j) >= 0) {
                sum += input[i - j];
                count++;
            }
        }
        
        output[i] = sum / count;
    }
}

// ============================================================================
// REMOVE ZERO VALUES
// ============================================================================
/*
 * Removes zero values from signal, which may indicate sensor disconnection
 * or invalid readings. Returns compacted array.
 * 
 * WARNING: Caller must free() the returned pointer.
 */
float* removeZero(long* input, int size, int* newSize) {
    // First pass: Count non-zero elements
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (input[i] != 0) {
            count++;
        }
    }
    
    // Allocate output array
    float* output = (float*)malloc(count * sizeof(float));
    if (output == NULL) {
        Serial.println("ERROR: Memory allocation failed in removeZero()");
        while (1);  // Halt - critical error
    }
    
    // Second pass: Copy non-zero values
    int j = 0;
    for (int i = 0; i < size; i++) {
        if (input[i] != 0) {
            output[j++] = (float)input[i];
        }
    }
    
    *newSize = count;
    return output;
}

// ============================================================================
// SIGNAL QUALITY CHECK (PLACEHOLDER)
// ============================================================================
/*
 * Placeholder for signal quality assessment.
 * TODO: Implement quality metrics based on:
 * - Signal amplitude variance
 * - Presence of clear peaks
 * - RR interval consistency
 * - Motion artifact detection
 */
int signalQual(float* signal, int fs) {
    // Currently returns 1 (good quality) for all signals
    // Implement proper quality assessment as needed
    return 1;
}

// ============================================================================
// NOISE ELIMINATION (SIMPLIFIED VERSION)
// ============================================================================
/*
 * Simplified noise elimination function.
 * TODO: Implement time-domain noise elimination using statistical thresholds.
 * See eliminateNoiseInTime() overload for more advanced version.
 */
void eliminateNoiseInTime(float* data, int size, int fs, float* ths, int cycle) {
    // Placeholder - implement statistical noise removal if needed
}

// ============================================================================
// VALLEY DETECTION
// ============================================================================
/*
 * Detects minima (valleys) in PPG signal.
 * Valleys correspond to diastolic phase (lowest blood volume).
 * 
 * WARNING: Caller must free() the returned pointer.
 */
int* valleyDetection(float* dataset, int size, int fs, float min_distance, int* valley_count) {
    // Calculate minimum sample distance between valleys
    int TH_elapsed = (int)ceil(min_distance * fs);
    
    // Allocate memory for potential valleys (worst case: all samples)
    int* valleyList = (int*)malloc(size * sizeof(int));
    int nvalleys = 0;
    
    // Calculate signal mean (threshold for valley detection)
    float localaverage = 0;
    for (int i = 0; i < size; i++) {
        localaverage += dataset[i];
    }
    localaverage /= size;
    
    // Temporary window to track consecutive below-average samples
    int* window = (int*)malloc(size * sizeof(int));
    int window_size = 0;
    
    // Scan signal for valleys (regions below average)
    for (int i = 0; i < size; i++) {
        if (dataset[i] > localaverage && window_size < 1) {
            // Above average, no valley in progress
            continue;
        } else if (dataset[i] <= localaverage) {
            // Below average - add to current valley window
            window[window_size++] = i;
        } else {
            // End of valley - find minimum within window
            if (window_size > 0) {
                int min_index = 0;
                for (int j = 1; j < window_size; j++) {
                    if (dataset[window[j]] < dataset[window[min_index]]) {
                        min_index = j;
                    }
                }
                valleyList[nvalleys++] = window[min_index];
                window_size = 0;
            }
        }
    }
    
    // Filter valleys that are too close together
    int* valleyArray = (int*)malloc(nvalleys * sizeof(int));
    int valid_valleys = 0;
    
    for (int i = 0; i < nvalleys; i++) {
        if (valid_valleys == 0 || (valleyList[i] - valleyArray[valid_valleys - 1]) > TH_elapsed) {
            valleyArray[valid_valleys++] = valleyList[i];
        }
    }
    
    // Cleanup
    free(window);
    free(valleyList);
    
    *valley_count = valid_valleys;
    return valleyArray;
}

// ============================================================================
// PAIR CONSECUTIVE VALLEYS
// ============================================================================
/*
 * Pairs consecutive valleys to define signal segments for analysis.
 * Each segment represents one cardiac cycle.
 * 
 * WARNING: Caller must free() both the returned array and each pair.
 */
int** pairValley(int* valleys, int valley_count) {
    // Allocate array of pairs
    int** pairedValleys = (int**)malloc((valley_count - 1) * sizeof(int*));
    
    // Create pairs of consecutive valleys
    for (int i = 0; i < valley_count - 1; i++) {
        pairedValleys[i] = (int*)malloc(2 * sizeof(int));
        pairedValleys[i][0] = valleys[i];      // Start of segment
        pairedValleys[i][1] = valleys[i + 1];  // End of segment
    }
    
    return pairedValleys;
}

// ============================================================================
// STATISTICAL DETECTION
// ============================================================================
/*
 * Calculate statistical metrics for each signal segment.
 * Used to identify and remove noisy segments based on:
 * - Standard deviation (signal variability)
 * - Kurtosis (peakedness/tailedness)
 * - Skewness (asymmetry)
 */
void statisticDetection(float* signal, int size, int fs, int** valleys, int valley_count, 
                        float* stds, float* kurtosiss, float* skews) {
    for (int i = 0; i < valley_count; i++) {
        int start = valleys[i][0];
        int end = valleys[i][1];
        int length = end - start + 1;
        
        // Calculate mean
        float sum = 0;
        for (int j = start; j <= end; j++) {
            sum += signal[j];
        }
        float mean = sum / length;
        
        // Calculate variance (for standard deviation)
        float variance = 0;
        for (int j = start; j <= end; j++) {
            variance += pow(signal[j] - mean, 2);
        }
        variance /= length;
        stds[i] = sqrt(variance);
        
        // Calculate kurtosis (measure of outliers)
        float fourth_moment = 0;
        for (int j = start; j <= end; j++) {
            fourth_moment += pow(signal[j] - mean, 4);
        }
        kurtosiss[i] = (fourth_moment / length) / (variance * variance) - 3;
        
        // Calculate skewness (measure of asymmetry)
        float third_moment = 0;
        for (int j = start; j <= end; j++) {
            third_moment += pow(signal[j] - mean, 3);
        }
        skews[i] = (third_moment / length) / pow(sqrt(variance), 3);
    }
}

// ============================================================================
// STATISTICAL THRESHOLD CALCULATION
// ============================================================================
/*
 * Calculate adaptive thresholds for noise elimination based on
 * mean statistics of all segments plus configured offsets.
 */
void statisticThreshold(float* stds, float* kurtosiss, float* skews, int valley_count, 
                        float* ths, float* std_ths, float* kurt_ths, float* skews_ths) {
    // Calculate mean statistics across all segments
    float std_sum = 0, kurt_sum = 0, skew_sum = 0;
    for (int i = 0; i < valley_count; i++) {
        std_sum += stds[i];
        kurt_sum += kurtosiss[i];
        skew_sum += skews[i];
    }
    
    // Calculate thresholds with configured offsets
    *std_ths = (std_sum / valley_count) + ths[0];
    *kurt_ths = (kurt_sum / valley_count) + ths[1];
    skews_ths[0] = (skew_sum / valley_count) - ths[2];  // Lower bound
    skews_ths[1] = (skew_sum / valley_count) + ths[3];  // Upper bound
}

// ============================================================================
// NOISE ELIMINATION (ADVANCED VERSION)
// ============================================================================
/*
 * Remove noisy segments using statistical thresholds.
 * Segments with excessive variance, unusual distributions, or asymmetry
 * are considered corrupted and removed.
 * 
 * WARNING: Caller must free() the returned pointer.
 */
float* eliminateNoiseInTime(float* data, int size, int fs, float* ths, int cycle, 
                            int** valleys, int valley_count, int* newSize) {
    // Allocate arrays for statistics
    float* stds = (float*)malloc(valley_count * sizeof(float));
    float* kurtosiss = (float*)malloc(valley_count * sizeof(float));
    float* skews = (float*)malloc(valley_count * sizeof(float));
    
    // Step 1: Calculate statistics for each segment
    statisticDetection(data, size, fs, valleys, valley_count, stds, kurtosiss, skews);
    
    // Step 2: Calculate adaptive thresholds
    float std_ths, kurt_ths, skews_ths[2];
    statisticThreshold(stds, kurtosiss, skews, valley_count, ths, &std_ths, &kurt_ths, skews_ths);
    
    // Step 3: Identify valid segments (within thresholds)
    int* valid_indices = (int*)malloc(valley_count * sizeof(int));
    int valid_count = 0;
    
    for (int i = 0; i < valley_count; i++) {
        if (stds[i] < std_ths && 
            kurtosiss[i] < kurt_ths && 
            skews[i] > skews_ths[0] && 
            skews[i] < skews_ths[1]) {
            valid_indices[valid_count++] = i;
        }
    }
    
    // Step 4: Calculate size of filtered data
    int final_size = 0;
    for (int i = 0; i < valid_count; i++) {
        final_size += valleys[valid_indices[i]][1] - valleys[valid_indices[i]][0] + 1;
    }
    
    // Step 5: Copy valid segments to output array
    float* filtered_data = (float*)malloc(final_size * sizeof(float));
    int idx = 0;
    for (int i = 0; i < valid_count; i++) {
        for (int j = valleys[valid_indices[i]][0]; j <= valleys[valid_indices[i]][1]; j++) {
            filtered_data[idx++] = data[j];
        }
    }
    
    // Cleanup
    free(stds);
    free(kurtosiss);
    free(skews);
    free(valid_indices);
    
    *newSize = final_size;
    return filtered_data;
}

// ============================================================================
// THRESHOLD PEAK DETECTION
// ============================================================================
/*
 * Detect peaks (maxima) in PPG signal using adaptive threshold.
 * Peaks correspond to systolic phase (maximum blood volume).
 * 
 * Algorithm:
 * 1. Calculate mean signal amplitude
 * 2. Apply threshold factor to mean (typically 0.9)
 * 3. Find local maxima above threshold
 * 4. Enforce minimum distance between peaks
 * 
 * WARNING: Caller must free() the returned pointer.
 */
int* thresholdPeakDetection(float* dataset, int size, int fs, float threshold_factor, 
                            float min_distance, int* peak_count) {
    // Estimate maximum number of peaks (one per 10 samples is generous)
    int max_peaks = size / 10;
    int* peakList = (int*)malloc(max_peaks * sizeof(int));
    int peak_idx = 0;
    
    // Calculate threshold as fraction of mean amplitude
    float local_average = 0;
    for (int i = 0; i < size; i++) {
        local_average += dataset[i];
    }
    local_average /= size;
    local_average *= threshold_factor;  // Apply threshold factor
    
    // Convert minimum distance from seconds to samples
    int TH_elapsed = (int)ceil(min_distance * fs);
    
    // Scan for peaks (local maxima above threshold)
    for (int i = 1; i < size - 1; i++) {
        // Check if current sample is:
        // 1. Above threshold
        // 2. Greater than previous sample
        // 3. Greater than next sample
        if (dataset[i] >= local_average && 
            dataset[i] > dataset[i - 1] && 
            dataset[i] > dataset[i + 1]) {
            
            // Ensure minimum distance from previous peak
            if (peak_idx == 0 || (i - peakList[peak_idx - 1]) > TH_elapsed) {
                peakList[peak_idx++] = i;
            }
        }
    }
    
    // Reallocate to exact size
    peakList = (int*)realloc(peakList, peak_idx * sizeof(int));
    *peak_count = peak_idx;
    return peakList;
}

// ============================================================================
// CALCULATE RR INTERVALS
// ============================================================================
/*
 * Calculate inter-beat intervals (RR intervals) from detected peaks.
 * RR intervals are the time between consecutive heartbeats.
 * 
 * Only physiologically valid intervals (300-1500ms) are retained.
 * This corresponds to heart rates of 40-200 bpm.
 * 
 * WARNING: Caller must free() the returned pointer.
 */
int* calcRrIntervals(int* peaks, int peakCount, int fs, int* rrCount) {
    if (peakCount < 2) {
        *rrCount = 0;
        return NULL;  // Need at least 2 peaks to calculate an interval
    }
    
    // Allocate for maximum possible intervals
    int* rr_intervals = (int*)malloc((peakCount - 1) * sizeof(int));
    int rr_idx = 0;
    
    // Calculate intervals between consecutive peaks
    for (int i = 1; i < peakCount; i++) {
        // Convert sample difference to milliseconds
        int rr_interval = (peaks[i] - peaks[i - 1]) * 1000 / fs;
        
        // Keep only physiologically valid intervals
        // 300ms = 200 bpm (max reasonable HR)
        // 1500ms = 40 bpm (min reasonable HR)
        if (rr_interval >= 300 && rr_interval <= 1500) {
            rr_intervals[rr_idx++] = rr_interval;
        }
    }
    
    // Reallocate to exact size
    rr_intervals = (int*)realloc(rr_intervals, rr_idx * sizeof(int));
    *rrCount = rr_idx;
    return rr_intervals;
}

// ============================================================================
// CALCULATE HRV METRICS
// ============================================================================
/*
 * Calculate heart rate variability metrics from RR intervals.
 * 
 * Returns array with three metrics:
 * - metrics[0]: Heart Rate (bpm) - average beats per minute
 * - metrics[1]: SDNN (ms) - standard deviation of RR intervals
 * - metrics[2]: RMSSD (ms) - root mean square of successive differences
 * 
 * SDNN reflects overall HRV (long-term variability)
 * RMSSD reflects parasympathetic activity (short-term variability)
 * 
 * WARNING: Caller must free() the returned pointer.
 */
float* calculateHRVMetrics(int* rr_intervals, int rrCount, int* metricsCount) {
    if (rrCount == 0) {
        *metricsCount = 0;
        return NULL;
    }
    
    *metricsCount = 3;
    float* metrics = (float*)malloc(3 * sizeof(float));
    
    // Calculate average RR interval
    float sumRR = 0;
    for (int i = 0; i < rrCount; i++) {
        sumRR += rr_intervals[i];
    }
    float avgRR = sumRR / rrCount;
    
    // Calculate heart rate from average RR interval
    // HR (bpm) = 60000ms / avgRR(ms)
    float heartRate = 60000.0 / avgRR;
    metrics[0] = heartRate;
    
    // Calculate SDNN (standard deviation of RR intervals)
    float varianceRR = 0;
    for (int i = 0; i < rrCount; i++) {
        varianceRR += pow(rr_intervals[i] - avgRR, 2);
    }
    float sdnn = sqrt(varianceRR / rrCount);
    metrics[1] = sdnn;
    
    // Calculate RMSSD (root mean square of successive differences)
    float sumOfSquares = 0;
    for (int i = 1; i < rrCount; i++) {
        float diff = rr_intervals[i] - rr_intervals[i - 1];
        sumOfSquares += diff * diff;
    }
    float rmssd = sqrt(sumOfSquares / (rrCount - 1));
    metrics[2] = rmssd;
    
    return metrics;
}

// ============================================================================
// ESTIMATE RR INTERVAL CONSISTENCY
// ============================================================================
/*
 * Calculate standard deviation of RR intervals as a quality metric.
 * Lower values indicate more consistent heart rate (better signal quality).
 * 
 * Note: This is essentially the same as SDNN from calculateHRVMetrics(),
 * but provided as a standalone function for signal quality assessment.
 */
float estimateRRIntervalConsistency(int* rr_intervals, int rrCount) {
    if (rrCount < 2) return -1;
    
    // Calculate mean RR interval
    float meanRR = 0;
    for (int i = 0; i < rrCount; i++) {
        meanRR += rr_intervals[i];
    }
    meanRR /= rrCount;
    
    // Calculate variance
    float varianceRR = 0;
    for (int i = 0; i < rrCount; i++) {
        varianceRR += pow(rr_intervals[i] - meanRR, 2);
    }
    
    // Return standard deviation
    return sqrt(varianceRR / rrCount);
}
