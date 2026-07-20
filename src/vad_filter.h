#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Forward declarations
struct vad_filter_data;

// Global VAD state variables (for debugging/external access)
extern bool vad_active;
extern float current_rms;

// Function declarations
void register_vad_filter(void);

// Utility functions for external access
bool get_vad_status(void);
float get_current_rms(void);
void set_vad_threshold(float threshold);

#ifdef __cplusplus
}
#endif