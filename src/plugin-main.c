/*
Speech Detection For OBS
Copyright (C) 2025 in-c0 nyakoraiden@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include "vad_filter.h"

const char *PLUGIN_NAME = "speech-vad-plugin";
const char *PLUGIN_VERSION = "0.0.1";

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "[%s] Loaded (version %s)", PLUGIN_NAME, PLUGIN_VERSION);

    register_vad_filter();
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "[%s] Unloaded", PLUGIN_NAME);
}
