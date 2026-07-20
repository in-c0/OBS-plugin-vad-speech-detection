#include <obs-module.h>
#include <math.h>
#include <string.h>
#include <util/threading.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "vad_filter.h"
#include "vad_debug.h"

// VAD and phoneme detection structures
struct vad_filter_data {
    obs_source_t *source;
    obs_data_t *settings;
    
    // VAD parameters
    bool enabled;
    float threshold;
    float gate_open_threshold;
    float gate_close_threshold;
    
    // Audio processing
    float *audio_buffer;
    size_t buffer_size;
    float current_rms;
    bool vad_active;
    
    // Smoothing and timing
    uint64_t last_voice_time;
    uint64_t voice_timeout;
    float smoothed_rms;
    float alpha; // smoothing factor
    
    // Frequency analysis for phoneme detection
    float *freq_buffer;
    size_t freq_buffer_size;
    float formant_f1, formant_f2, formant_f3;
    
    // UDP communication
    int udp_socket;
    struct sockaddr_in udp_addr;
    char udp_host[64];
    int udp_port;
    bool udp_enabled;
    
    // Threading
    pthread_mutex_t data_mutex;
};

// Phoneme classification based on formant frequencies
typedef struct {
    char phoneme[8];
    float f1_center, f1_range;
    float f2_center, f2_range;
    int mouth_shape; // 0-7 for different mouth positions
} phoneme_data_t;

static phoneme_data_t phoneme_table[] = {
    {"A", 730, 100, 1090, 200, 0},    // /a/ - open mouth
    {"E", 530, 80, 1840, 300, 1},     // /e/ - mid-open
    {"I", 270, 60, 2290, 400, 2},     // /i/ - closed, spread
    {"O", 570, 90, 840, 150, 3},      // /o/ - rounded
    {"U", 300, 70, 870, 200, 4},      // /u/ - rounded, closed
    {"M", 0, 0, 0, 0, 5},             // /m/ - closed lips
    {"P", 0, 0, 0, 0, 6},             // /p/ - closed lips
    {"SIL", 0, 0, 0, 0, 7},           // silence
};

#define PHONEME_COUNT (sizeof(phoneme_table) / sizeof(phoneme_data_t))

// Initialize UDP socket
static bool init_udp_socket(struct vad_filter_data *filter) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        blog(LOG_ERROR, "[VAD] WSAStartup failed");
        return false;
    }
#endif

    filter->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (filter->udp_socket < 0) {
        blog(LOG_ERROR, "[VAD] Failed to create UDP socket");
        return false;
    }

    memset(&filter->udp_addr, 0, sizeof(filter->udp_addr));
    filter->udp_addr.sin_family = AF_INET;
    filter->udp_addr.sin_port = htons(filter->udp_port);
    
    if (inet_pton(AF_INET, filter->udp_host, &filter->udp_addr.sin_addr) <= 0) {
        blog(LOG_ERROR, "[VAD] Invalid UDP host address: %s", filter->udp_host);
        return false;
    }

    blog(LOG_INFO, "[VAD] UDP socket initialized for %s:%d", filter->udp_host, filter->udp_port);
    return true;
}

// Simple formant estimation using peak detection
static void estimate_formants(struct vad_filter_data *filter, float *audio_data, size_t frames) {
    if (!filter->freq_buffer || frames == 0) return;
    
    // Simple spectral analysis - find energy peaks
    // This is a simplified approach; for better results, use FFT and proper formant tracking
    
    float energy_bands[8] = {0}; // Frequency bands for rough formant estimation
    size_t band_size = frames / 8;
    
    for (size_t i = 0; i < frames && i < filter->freq_buffer_size; i++) {
        int band = (i * 8) / frames;
        if (band < 8) {
            energy_bands[band] += audio_data[i] * audio_data[i];
        }
    }
    
    // Map energy bands to approximate formant frequencies
    // Band 0-1: ~200-600Hz (F1)
    // Band 2-4: ~600-1800Hz (F2)  
    // Band 5-7: ~1800-3500Hz (F3)
    
    float f1_energy = energy_bands[0] + energy_bands[1];
    float f2_energy = energy_bands[2] + energy_bands[3] + energy_bands[4];
    float f3_energy = energy_bands[5] + energy_bands[6] + energy_bands[7];
    
    // Estimate formant centers based on energy distribution
    filter->formant_f1 = 400 + (f1_energy * 300); // ~200-700Hz
    filter->formant_f2 = 1200 + (f2_energy * 600); // ~600-1800Hz
    filter->formant_f3 = 2600 + (f3_energy * 900); // ~1700-3500Hz
}

// Classify phoneme based on formants
static int classify_phoneme(struct vad_filter_data *filter) {
    if (!filter->vad_active || filter->current_rms < filter->threshold) {
        return 7; // silence
    }
    
    float best_score = 1000000.0f;
    int best_phoneme = 7; // default to silence
    
    for (int i = 0; i < PHONEME_COUNT - 1; i++) { // exclude silence in loop
        phoneme_data_t *p = &phoneme_table[i];
        
        // Skip consonants that don't have formant data
        if (p->f1_center == 0 && p->f2_center == 0) {
            // For consonants, use RMS-based detection
            if (filter->current_rms > filter->threshold * 1.5f) {
                best_phoneme = p->mouth_shape;
                break;
            }
            continue;
        }
        
        // Calculate distance to phoneme in formant space
        float f1_diff = fabsf(filter->formant_f1 - p->f1_center);
        float f2_diff = fabsf(filter->formant_f2 - p->f2_center);
        
        if (f1_diff < p->f1_range && f2_diff < p->f2_range) {
            float score = f1_diff + f2_diff;
            if (score < best_score) {
                best_score = score;
                best_phoneme = p->mouth_shape;
            }
        }
    }
    
    return best_phoneme;
}

// Send phoneme data via UDP
static void send_phoneme_udp(struct vad_filter_data *filter, int mouth_shape, float intensity) {
    if (!filter->udp_enabled || filter->udp_socket < 0) return;
    
    // Create JSON message for Unity
    char message[256];
    snprintf(message, sizeof(message), 
        "{\"type\":\"phoneme\",\"mouth_shape\":%d,\"intensity\":%.3f,\"vad_active\":%s,\"rms\":%.3f,\"f1\":%.1f,\"f2\":%.1f}",
        mouth_shape, intensity, filter->vad_active ? "true" : "false", 
        filter->current_rms, filter->formant_f1, filter->formant_f2);
    
    size_t sent = sendto(filter->udp_socket, message, strlen(message), 0,
                         (struct sockaddr*)&filter->udp_addr, sizeof(filter->udp_addr));
    
    if (sent < 0) {
        vad_write_debug_text_throttled("UDP send failed");
    }
}

// Calculate RMS (Root Mean Square) for volume detection
static float calculate_rms(float *audio_data, size_t frames) {
    float sum = 0.0f;
    for (size_t i = 0; i < frames; i++) {
        sum += audio_data[i] * audio_data[i];
    }
    return sqrtf(sum / frames);
}

static const char *get_name(void *unused) {
    UNUSED_PARAMETER(unused);
    return "Speech VAD Filter";
}

static void *create_filter(obs_data_t *settings, obs_source_t *source) {
    struct vad_filter_data *filter = bzalloc(sizeof(struct vad_filter_data));
    
    filter->source = source;
    filter->settings = obs_data_create();
    
    // Initialize parameters
    filter->threshold = 0.01f;
    filter->gate_open_threshold = 0.02f;
    filter->gate_close_threshold = 0.005f;
    filter->voice_timeout = 500000000ULL; // 500ms in nanoseconds
    filter->alpha = 0.1f; // smoothing factor
    
    // UDP defaults
    strcpy(filter->udp_host, "127.0.0.1");
    filter->udp_port = 12345;
    filter->udp_enabled = true;
    filter->udp_socket = -1;
    
    // Initialize buffers
    filter->buffer_size = 1024;
    filter->audio_buffer = bmalloc(filter->buffer_size * sizeof(float));
    filter->freq_buffer_size = 512;
    filter->freq_buffer = bmalloc(filter->freq_buffer_size * sizeof(float));
    
    pthread_mutex_init(&filter->data_mutex, NULL);
    
    // Initialize UDP
    if (filter->udp_enabled) {
        init_udp_socket(filter);
    }
    
    blog(LOG_INFO, "[VAD] Filter created");
    return filter;
}

static void destroy_filter(void *data) {
    struct vad_filter_data *filter = data;
    
    if (filter) {
        if (filter->udp_socket >= 0) {
#ifdef _WIN32
            closesocket(filter->udp_socket);
            WSACleanup();
#else
            close(filter->udp_socket);
#endif
        }
        
        pthread_mutex_destroy(&filter->data_mutex);
        
        if (filter->audio_buffer) bfree(filter->audio_buffer);
        if (filter->freq_buffer) bfree(filter->freq_buffer);
        if (filter->settings) obs_data_release(filter->settings);
        
        bfree(filter);
        blog(LOG_INFO, "[VAD] Filter destroyed");
    }
}

static struct obs_audio_data *filter_audio(void *data, struct obs_audio_data *audio) {
    struct vad_filter_data *filter = data;
    
    if (!filter || !filter->enabled || !audio || audio->frames == 0) {
        return audio;
    }
    
    pthread_mutex_lock(&filter->data_mutex);
    
    // Process first channel only for simplicity
    float *audio_data = (float*)audio->data[0];
    size_t frames = audio->frames;
    
    // Calculate RMS
    filter->current_rms = calculate_rms(audio_data, frames);
    
    // Smooth RMS for stability
    filter->smoothed_rms = filter->alpha * filter->current_rms + 
                          (1.0f - filter->alpha) * filter->smoothed_rms;
    
    // VAD logic with hysteresis
    uint64_t now = os_gettime_ns();
    
    if (!filter->vad_active) {
        // Voice activation
        if (filter->smoothed_rms > filter->gate_open_threshold) {
            filter->vad_active = true;
            filter->last_voice_time = now;
            vad_write_debug_text_throttled("Voice detected - VAD ON");
        }
    } else {
        // Voice deactivation
        if (filter->smoothed_rms > filter->gate_close_threshold) {
            filter->last_voice_time = now; // Reset timeout
        } else if ((now - filter->last_voice_time) > filter->voice_timeout) {
            filter->vad_active = false;
            vad_write_debug_text_throttled("Voice timeout - VAD OFF");
        }
    }
    
    // Formant estimation for phoneme detection
    if (filter->vad_active) {
        estimate_formants(filter, audio_data, frames);
        
        // Classify phoneme
        int mouth_shape = classify_phoneme(filter);
        float intensity = fminf(filter->current_rms / filter->threshold, 1.0f);
        
        // Send UDP data
        send_phoneme_udp(filter, mouth_shape, intensity);
        
        vad_write_debug_text_throttled("VAD: ON, RMS: %.3f, Shape: %d, F1: %.0f, F2: %.0f", 
                                     filter->current_rms, mouth_shape, 
                                     filter->formant_f1, filter->formant_f2);
    } else {
        // Send silence
        send_phoneme_udp(filter, 7, 0.0f);
    }
    
    pthread_mutex_unlock(&filter->data_mutex);
    return audio;
}

static void update_filter(void *data, obs_data_t *settings) {
    struct vad_filter_data *filter = data;
    
    pthread_mutex_lock(&filter->data_mutex);
    
    filter->enabled = obs_data_get_bool(settings, "enabled");
    filter->threshold = (float)obs_data_get_double(settings, "threshold");
    filter->gate_open_threshold = (float)obs_data_get_double(settings, "gate_open_threshold");
    filter->gate_close_threshold = (float)obs_data_get_double(settings, "gate_close_threshold");
    
    // UDP settings
    const char *host = obs_data_get_string(settings, "udp_host");
    if (host && strlen(host) > 0) {
        strncpy(filter->udp_host, host, sizeof(filter->udp_host) - 1);
    }
    filter->udp_port = (int)obs_data_get_int(settings, "udp_port");
    bool udp_enabled = obs_data_get_bool(settings, "udp_enabled");
    
    // Reinitialize UDP if settings changed
    if (udp_enabled != filter->udp_enabled || 
        strcmp(filter->udp_host, host) != 0) {
        
        if (filter->udp_socket >= 0) {
#ifdef _WIN32
            closesocket(filter->udp_socket);
#else
            close(filter->udp_socket);
#endif
            filter->udp_socket = -1;
        }
        
        filter->udp_enabled = udp_enabled;
        if (filter->udp_enabled) {
            init_udp_socket(filter);
        }
    }
    
    pthread_mutex_unlock(&filter->data_mutex);
}

static obs_properties_t *get_props(void *data) {
    UNUSED_PARAMETER(data);
    
    obs_properties_t *props = obs_properties_create();
    
    // Basic VAD settings
    obs_properties_add_bool(props, "enabled", "Enable VAD");
    
    obs_properties_add_float_slider(props, "threshold", "Voice Threshold", 
                                   0.001, 0.1, 0.001);
    
    obs_properties_add_float_slider(props, "gate_open_threshold", "Gate Open Threshold", 
                                   0.005, 0.2, 0.005);
    
    obs_properties_add_float_slider(props, "gate_close_threshold", "Gate Close Threshold", 
                                   0.001, 0.05, 0.001);
    
    // UDP settings
    obs_properties_add_bool(props, "udp_enabled", "Enable UDP Output");
    obs_properties_add_text(props, "udp_host", "UDP Host", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "udp_port", "UDP Port", 1024, 65535, 1);
    
    return props;
}

static void get_defaults(obs_data_t *settings) {
    obs_data_set_default_bool(settings, "enabled", true);
    obs_data_set_default_double(settings, "threshold", 0.01);
    obs_data_set_default_double(settings, "gate_open_threshold", 0.02);
    obs_data_set_default_double(settings, "gate_close_threshold", 0.005);
    obs_data_set_default_bool(settings, "udp_enabled", true);
    obs_data_set_default_string(settings, "udp_host", "127.0.0.1");
    obs_data_set_default_int(settings, "udp_port", 12345);
}

static struct obs_source_info vad_filter = {
    .id = "speech_vad_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = get_name,
    .create = create_filter,
    .destroy = destroy_filter,
    .filter_audio = filter_audio,
    .update = update_filter,
    .get_properties = get_props,
    .get_defaults = get_defaults,
};

void register_vad_filter() {
    obs_register_source(&vad_filter);
}