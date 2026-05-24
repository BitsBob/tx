#include "tx.h"
#include "buffer.h"
#include "command.h"
#include "editor.h"
#include "help.h"
#include "input.h"
#include "output.h"
#include "search.h"

static void clearScreenAndExit(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
}

void editorProcessCommandMode() {
    char *cmd = editorPrompt(":%s", NULL);
    E.mode = MODE_NORMAL;
    if (cmd == NULL)
        return;

    if (strcmp(cmd, "q") == 0) {
        if (CB.dirty) {
            editorSetStatusMessage("No write since last change (add ! to override)");
        } else {
            bufferClose(E.buf_current);
            if (E.buf_count == 0)
                clearScreenAndExit();
        }
    } else if (strcmp(cmd, "q!") == 0) {
        bufferFree(E.buf_current);
        if (E.buf_count == 0)
            clearScreenAndExit();

    } else if (strcmp(cmd, "w") == 0) {
        editorSave();

    } else if (strcmp(cmd, "wq") == 0) {
        editorSave();
        bufferFree(E.buf_current);
        if (E.buf_count == 0)
            clearScreenAndExit();

    } else if (strncmp(cmd, "e ", 2) == 0) {
        bufferOpen(cmd + 2);

    } else if (strcmp(cmd, "bn") == 0) {
        bufferSwitch((E.buf_current + 1) % E.buf_count);

    } else if (strcmp(cmd, "bp") == 0) {
        bufferSwitch((E.buf_current - 1 + E.buf_count) % E.buf_count);

    } else if (strcmp(cmd, "bd") == 0) {
        bufferClose(E.buf_current);
        if (E.buf_count == 0)
            clearScreenAndExit();

    } else if (strcmp(cmd, "bd!") == 0) {
        bufferFree(E.buf_current);
        if (E.buf_count == 0)
            clearScreenAndExit();

    } else if (strcmp(cmd, "ls") == 0) {
        char list[256] = {0};
        int pos = 0;
        for (int i = 0; i < E.buf_count && pos < (int)sizeof(list) - 1; i++) {
            pos += snprintf(list + pos, sizeof(list) - pos, "%s%d:%s%s ",
                            i == E.buf_current ? "[" : "",
                            i + 1,
                            E.buffers[i].filename ? E.buffers[i].filename : "[No Name]",
                            i == E.buf_current ? "]" : "");
        }
        editorSetStatusMessage("%s", list);

    } else if (strcmp(cmd, "f") == 0 || strcmp(cmd, "find") == 0) {
        editorFind();

    } else if (strncmp(cmd, "h", 1) == 0 && (cmd[1] == '\0' || cmd[1] == ' ')) {
        const char *topic = (cmd[1] == ' ') ? cmd + 2 : "";
        editorOpenHelp(topic);

    } else {
        editorSetStatusMessage("Unknown command: %s", cmd);
    }

    free(cmd);
}