#include <obs-module.h>
#include "vad_filter.h"

static const char *get_name(void *unused) {
    return "Speech VAD Filter";
}

static void *create_filter(obs_data_t *settings, obs_source_t *source) {
    return source;
}

static void destroy_filter(void *data) {}

static struct obs_audio_data *filter_audio(void *data, struct obs_audio_data *audio) {
    // TODO: Add VAD logic and UDP sending here
    return audio;
}

obs_properties_t *get_props(void *data) {
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_bool(props, "enabled", "Enable VAD");
    return props;
}

static struct obs_source_info vad_filter = {
    .id = "speech_vad_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = get_name,
    .create = create_filter,
    .destroy = destroy_filter,
    .filter_audio = filter_audio,
    .get_properties = get_props,
};

void register_vad_filter() {
    obs_register_source(&vad_filter);
}
