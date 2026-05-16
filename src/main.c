#include "tx.h"
#include "buffer.h"
#include "config.h"
#include "editor.h"
#include "input.h"
#include "output.h"
#include "terminal.h"

struct editorConfig E = {};

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();

    char config_path[512];
    const char *home = getenv("HOME");
    if (home)
        snprintf(config_path, sizeof(config_path), "%s/.txrc", home);
    else
        snprintf(config_path, sizeof(config_path), ".txrc");

    int cfg_result = editorLoadConfig(config_path, &E.settings);
    if (cfg_result == -1) {
        fprintf(stderr, "Failed to parse config file: %s\n", config_path);
    }

    bufferNew();
    E.buf_current = 0;

    if (argc >= 2) {
        bufferOpen(argv[1]);
    }

    editorSetFortuneStatusMessage();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}