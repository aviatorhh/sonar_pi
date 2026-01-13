#pragma once

#include <stdint.h>

// ---------------------- DRIVE FREQUENCY SETTINGS ----------------------
// Sets the output frequency of the ultrasonic transducer
// Uses DRIVE_FREQUENCY directly for R4, uses divider for R3
#define DRIVE_FREQUENCY 40000
#define DRIVE_FREQUENCY_TIMER_DIVIDER ((F_CPU / (2 * DRIVE_FREQUENCY)) - 1)

// ---------------------- BANDPASS FILTER SETTINGS ----------------------
// Sets the digital band-pass filter frequency on the TUSS4470 driver chip
// This should roughly match the transducer drive frequency
// For additional register values, see TUSS4470 datasheet, Table 7.1 (pages 17–18)
#define FILTER_FREQUENCY_REGISTER 0x00 // 40 kHz
// #define FILTER_FREQUENCY_REGISTER 0x09 // 68 kHz
// #define FILTER_FREQUENCY_REGISTER 0x10 // 100 kHz
// #define FILTER_FREQUENCY_REGISTER 0x18 // 151 kHz
// #define FILTER_FREQUENCY_REGISTER 0x1E // 200 kHz

// Number of ADC samples to take per measurement cycle
// Each sample takes approximately 13.2 microseconds
// This value must match the number of samples expected by the Python visualization tool
#define NUM_SAMPLES 600	// MUST BE AN EVEN NUMBER
uint32_t num_samples = NUM_SAMPLES; // for future use

// Threshold level for detecting the bottom echo
// The first echo stronger than this value (after the blind zone) is considered the bottom
#define THRESHOLD_VALUE 0x1f

// ---------------------- DEPTH OVERRIDE ----------------------
// If enabled, software will scan the captured analogValues[] after each
// acquisition and choose the max sample after the blind zone to be 
// the bottom echo, instead of the first sample above the threshold.
#define USE_DEPTH_OVERRIDE

// ---------------------- MAX DEPTH ------------------------------------
// What dpeths are we looking at? Has to be made dynamically changeable.
#define MAX_DEPTH 10 // meter; you can go deeper but do not go below 10. Accuracy issues then

// ---------------------- ADC DIVISION FACTOR ------------------------------------
// Controls the accuracy of the ADC. See datasheet for tweaks
#define ADC_DIVISION_FACTOR 64  // gives 250kHz sampling rate

// ---------------------- ADC 8BIT RESOLUTION ------------------------------------
// Affects the resolution of the ADC values, 10-bit vs 8-bit. 8-bit should be enough
// as 10-bit doubles the amount of data being transferred over the serial line.
#define ADC_8BIT_RESOLUTION
