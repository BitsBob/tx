#include "tx.h"
#include "editor.h"
#include "input.h"
#include "search.h"

/* editorFindCallback is called by editorPrompt on every keystroke while the
 * search bar is open. Static locals track the current match and direction so
 * arrow keys cycle through results without restarting the scan each time. */
void editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction  = 1;

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction  = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction  = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;

    for (int i = 0; i < CB.numrows; i++) {
        current += direction;
        if (current == -1)        current = CB.numrows - 1;
        else if (current == CB.numrows) current = 0;

        erow *row   = &CB.row[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current;
            CB.cy      = current;
            CB.cx      = editorRowRxToCx(row, match - row->render);
            CB.rowoff  = CB.numrows;
            break;
        }
    }
}

void editorFind(void) {
    int saved_cx     = CB.cx;
    int saved_cy     = CB.cy;
    int saved_coloff = CB.coloff;
    int saved_rowoff = CB.rowoff;

    char *query = editorPrompt("Search: %s (ESC to cancel)", editorFindCallback);
    if (query) {
        free(query);
    } else {
        CB.cx     = saved_cx;
        CB.cy     = saved_cy;
        CB.coloff = saved_coloff;
        CB.rowoff = saved_rowoff;
    }
}