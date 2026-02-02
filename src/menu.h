#ifndef MENU_H
#define MENU_H

typedef enum {
    PAGE_MAIN,
    PAGE_CORES,
    PAGE_SETTINGS,
    PAGE_VERSION
} MenuPage;

typedef struct {
    MenuPage currentPage;
    int cursor;
} MenuState;

#endif // MENU_H