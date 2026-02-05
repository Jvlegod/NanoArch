#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AppConfig g_config = {
    .physical_w = 720,
    .physical_h = 1280,
    .rotation = 90,
    .volume = 100,
};

extern AppConfig g_config;

void config_load(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[Config] File not found: %s, using defaults.\n", filename);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char key[64], val[64];
        if (sscanf(line, "%[^=]=%s", key, val) == 2) {
            if (strcmp(key, "physical_w") == 0) g_config.physical_w = atoi(val);
            else if (strcmp(key, "physical_h") == 0) g_config.physical_h = atoi(val);
            else if (strcmp(key, "rotation") == 0) g_config.rotation = atoi(val);
            else if (strcmp(key, "volume") == 0) g_config.volume = atoi(val);
        }
    }
    
    fclose(f);
    printf("[Config] Loaded: %dx%d, Rotation: %d, Volume: %d\n", 
           g_config.physical_w, g_config.physical_h, g_config.rotation, g_config.volume);
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

    fclose(f);
    printf("[Config] Saved to %s\n", filename);
}