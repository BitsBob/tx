#include "tx.h"

int bufferNew(void) {
    E.buffers = realloc(E.buffers, sizeof(Buffer) * (E.buf_current + 1));
    Buffer *b = &E.buffers[E.buf_count];

    b->filename = NULL;
    b->row = NULL;
    b->numrows = 0;
    b->cx = 0;
    b->cy = 0;
    b->rx = 0;
    b->rowoff = 0;
    b->coloff = 0;
    b->dirty = 0;
    b->syntax = NULL;

    return E.buf_count++;
}

void bufferSwitch(int idx) {
    if (idx < 0 || idx >= E.buf_count)
        return;
    E.buf_current = idx;
    editorSetStatusMessage("Buffer %d/%d: %s", idx + 1, E.buf_count, CB.filename ? CB.filename : "[No Name]");
}

void bufferOpen(char *filename) {
    for (int i=0; i < E.buf_count; i++) {
        if (E.buffers[i].filename && strcmp(E.buffers[i].filename, filename) == 0) {
            bufferSwitch(i);
            return;
        }
    }

    int idx = bufferNew();
    bufferSwitch(idx);
}

void bufferFree(int idx) {
    Buffer *b = &E.buffers[idx];

    free(b->filename);
    for (int i = 0; i < b->numrows; i++) {
        editorFreeRow(&b->row[i]);
    }
    free(b->row);

    memmove(&E.buffers[idx], &E.buffers[idx + 1], sizeof(Buffer) * (E.buf_count - idx - 1)); //remove from array by shifting down
    E.buf_count--;

    if (E.buf_count == 0) {
        bufferNew();
        E.buf_current = 0;
    } else if (E.buf_current >= E.buf_count) {
        E.buf_current = E.buf_count - 1;
    }

    editorSetStatusMessage("Buffer %d/%d: %s", E.buf_current + 1, E.buf_count,
                           CB.filename ? CB.filename : "[No Name]");
}

void bufferClose(int idx) {
    if (idx < 0 || idx >= E.buf_count)
        return;

    if (E.buffers[idx].dirty) {
        editorSetStatusMessage("No write since last change (add ! to override)");
        return;
    }

    bufferFree(idx);
}
