#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static bool parse_bool(const char *val) {
    return (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
}

int editorLoadConfig(char *path, struct configOptions *cfg) {
    /* Set defaults first so they apply even when no config file exists. */
    cfg->CONFIG_TAB_STOP              = 4;
    cfg->CONFIG_UNDO_MAX              = 100;
    cfg->CONFIG_SYNTAX                = false;
    cfg->CONFIG_SEARCH_CASE_SENSITIVE = false;
    cfg->CONFIG_SEARCH_HIGHLIGHT      = true;
    cfg->CONFIG_STATUS_FORTUNE        = true;
    cfg->CONFIG_NUMBERS               = false;
    cfg->CONFIG_LSP_ENABLE            = false;
    cfg->CONFIG_AUTOPAIR              = true;
    strncpy(cfg->CONFIG_LSP_CMD, "clangd", sizeof(cfg->CONFIG_LSP_CMD) - 1);
    cfg->CONFIG_LSP_CMD[sizeof(cfg->CONFIG_LSP_CMD) - 1] = '\0';

    FILE *fp = fopen(path, "r");
    if (!fp)
        return (errno == ENOENT) ? -1 : -2;

    char line[256];
    int  lineno = 0;

    while (fgets(line, sizeof line, fp)) {
        lineno++;
        char *p = trim(line);

        if (*p == '\0' || *p == '#') continue;

        if (strncmp(p, "set", 3) != 0) {
            fprintf(stderr, "config:%d: expected 'set': %s", lineno, line);
            continue;
        }
        p += 3;
        while (isspace((unsigned char)*p)) p++;

        char *key = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) { *p++ = '\0'; while (isspace((unsigned char)*p)) p++; }
        char *value = p;

        if      (strcmp(key, "tabstop")          == 0) cfg->CONFIG_TAB_STOP              = atoi(value);
        else if (strcmp(key, "undo_max")          == 0) cfg->CONFIG_UNDO_MAX              = atoi(value);
        else if (strcmp(key, "syntax")            == 0) cfg->CONFIG_SYNTAX                = parse_bool(value);
        else if (strcmp(key, "search_sensitive")  == 0) cfg->CONFIG_SEARCH_CASE_SENSITIVE = parse_bool(value);
        else if (strcmp(key, "search_highlight")  == 0) cfg->CONFIG_SEARCH_HIGHLIGHT      = parse_bool(value);
        else if (strcmp(key, "status_fortune")    == 0) cfg->CONFIG_STATUS_FORTUNE        = parse_bool(value);
        else if (strcmp(key, "numbers")           == 0) cfg->CONFIG_NUMBERS               = parse_bool(value);
        else if (strcmp(key, "lsp_enable")        == 0) cfg->CONFIG_LSP_ENABLE            = parse_bool(value);
        else if (strcmp(key, "autopair")          == 0) cfg->CONFIG_AUTOPAIR              = parse_bool(value);
        else if (strcmp(key, "lsp_cmd")           == 0) {
            strncpy(cfg->CONFIG_LSP_CMD, value, sizeof(cfg->CONFIG_LSP_CMD) - 1);
            cfg->CONFIG_LSP_CMD[sizeof(cfg->CONFIG_LSP_CMD) - 1] = '\0';
        }
        else fprintf(stderr, "config:%d: unknown key '%s'\n", lineno, key);
    }

    fclose(fp);
    return 0;
}