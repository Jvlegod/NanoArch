// 1. 必须放在第一行，否则 strcasestr 会导致崩溃
#define _GNU_SOURCE 

#include "core_manager.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 2. 确保显式初始化
char** dynamic_list = NULL;
int dynamic_count = 0;

// 定义支持的核心列表
CoreInfo supported_cores[] = {
    {"Nestopia (NES)", ".nes", "nes"},
    {"mGBA (GBA)",     ".gb",  "gb"},
    {"Snes9x (SNES)",  ".smc", "snes"}
};
int CORE_COUNT = 3;

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

    printf("[NanoArch] Scanning directory: %s\n", path);

    DIR* dir = opendir(path);
    if (!dir) {
        printf("[Warning] Directory not found: %s\n", path);
    } else {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
                if (strcasestr(entry->d_name, supported_cores[core_idx].extension)) {
                    char** temp = realloc(dynamic_list, sizeof(char*) * (dynamic_count + 1));
                    if (!temp) {
                        printf("[Error] Memory allocation failed!\n");
                        break;
                    }
                    dynamic_list = temp;
                    dynamic_list[dynamic_count] = strdup(entry->d_name);
                    dynamic_count++;
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
    
    printf("[NanoArch] Found %d items.\n", dynamic_count);
}