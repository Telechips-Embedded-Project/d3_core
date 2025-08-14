#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <cjson/cJSON.h>



void run_piper(const char *text);
void run_piper_with_volume(const char *text, float gain);
