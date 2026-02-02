#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

typedef struct {
    const char* display_name;
    const char* extension; 
    const char* sub_dir;
} CoreInfo;

extern CoreInfo supported_cores[];
extern int CORE_COUNT;

extern char** dynamic_list;
extern int dynamic_count;

void scan_roms(int core_idx);
void clear_dynamic_list();

#endif