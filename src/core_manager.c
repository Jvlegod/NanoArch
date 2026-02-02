#define _GNU_SOURCE 
#include "core_manager.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char** dynamic_list = NULL;
int dynamic_count = 0;

CoreInfo supported_cores[] = {
    {"Nintendo (NES)", ".nes", "nes", "./cores/InfoNES"},
    {"Game Boy (GB)", ".gb", "gb", "./cores/light-gb"},
};

int CORE_COUNT = sizeof(supported_cores) / sizeof(CoreInfo);

void clear_dynamic_list() {
    if (dynamic_list) {
        for (int i = 0; i < dynamic_count; i++) {
            if (dynamic_list[i]) free(dynamic_list[i]);
        }
        free(dynamic_list);
        dynamic_list = NULL;
    }
    dynamic_count = 0;
}

void scan_roms(int core_idx) {
    clear_dynamic_list();
    char path[256];
    snprintf(path, sizeof(path), "roms/%s", supported_cores[core_idx].sub_dir);
    
    DIR* dir = opendir(path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
                if (strcasestr(entry->d_name, supported_cores[core_idx].extension)) {
                    char** temp = realloc(dynamic_list, sizeof(char*) * (dynamic_count + 1));
                    if (temp) {
                        dynamic_list = temp;
                        dynamic_list[dynamic_count] = strdup(entry->d_name);
                        dynamic_count++;
                    }
                }
            }
        }
        closedir(dir);
    }
    char** temp = realloc(dynamic_list, sizeof(char*) * (dynamic_count + 1));
    if (temp) {
        dynamic_list = temp;
        dynamic_list[dynamic_count] = strdup("<-- Back");
        dynamic_count++;
    }
}