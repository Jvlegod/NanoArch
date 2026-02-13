#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_JOYSTICK "/dev/input/event3"

AppConfig g_config = {
    .physical_w = 480,
    .physical_h = 800,
    .rotation = 90,
    .volume = 100,
    .keep_aspect = true,
    .joystick = DEFAULT_JOYSTICK, 
};

void config_load(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[Config] File not found: %s, using defaults.\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == ' ') continue;
        
        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127s", key, val) == 2) {
            if (strcmp(key, "physical_w") == 0) g_config.physical_w = atoi(val);
            else if (strcmp(key, "physical_h") == 0) g_config.physical_h = atoi(val);
            else if (strcmp(key, "rotation") == 0) g_config.rotation = atoi(val);
            else if (strcmp(key, "volume") == 0) g_config.volume = atoi(val);
            else if (strcmp(key, "keep_aspect") == 0) g_config.keep_aspect = (atoi(val) != 0);
            else if (strcmp(key, "joystick") == 0) {
                strncpy(g_config.joystick, val, 127);
            }
        }
    }
    
    fclose(f);
    printf("[Config] Loaded: %dx%d, Rotation: %d, Joystick: %s\n", 
           g_config.physical_w, g_config.physical_h, g_config.rotation, g_config.joystick);
}

void config_save(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[Config] Error: Could not open %s for writing.\n", filename);
        return;
    }

    fprintf(f, "# NanoArch Configuration\n");
    fprintf(f, "physical_w=%d\n", g_config.physical_w);
    fprintf(f, "physical_h=%d\n", g_config.physical_h);
    fprintf(f, "rotation=%d\n", g_config.rotation);
    fprintf(f, "volume=%d\n", g_config.volume);
    fprintf(f, "keep_aspect=%d\n", g_config.keep_aspect ? 1 : 0);
    fprintf(f, "joystick=%s\n", g_config.joystick);

    fclose(f);
    printf("[Config] Saved to %s\n", filename);
}