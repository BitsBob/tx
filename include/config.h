#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

struct configOptions {
    int  CONFIG_TAB_STOP;
    int  CONFIG_UNDO_MAX;
    bool CONFIG_NUMBERS;
    bool CONFIG_SYNTAX;
    bool CONFIG_SEARCH_CASE_SENSITIVE;
    bool CONFIG_SEARCH_HIGHLIGHT;
    bool CONFIG_STATUS_FORTUNE;
    bool CONFIG_LSP_ENABLE;
    bool CONFIG_AUTOPAIR;
};

int editorLoadConfig(char *path, struct configOptions *cfg);

#endif
