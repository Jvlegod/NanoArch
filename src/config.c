#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AppConfig g_config = {
    .physical_w = 720,
    .physical_h = 1280,
    .rotation = 90,
    .show_fps = 0
};

void config_load(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[Config] File not found: %s, using defaults.\n", filename);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[32], val[32];
        if (sscanf(line, "%[^=]=%s", key, val) == 2) {
            if (strcmp(key, "physical_w") == 0) g_config.physical_w = atoi(val);
            else if (strcmp(key, "physical_h") == 0) g_config.physical_h = atoi(val);
            else if (strcmp(key, "rotation") == 0) g_config.rotation = atoi(val);
        }
    }
    
    fclose(f);
    printf("[Config] Loaded: %dx%d, Rotation: %d\n", 
           g_config.physical_w, g_config.physical_h, g_config.rotation);
}