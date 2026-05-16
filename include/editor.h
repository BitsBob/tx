#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>
#include <termios.h>
#include <time.h>

#include "buffer.h"
#include "config.h"
#include "row.h"

enum pendingOperations {
    OP_NONE = 0,
    OP_DELETE,
    OP_YANK,
    OP_JUMP_TO_TOP,
    OP_INSERT,
};

enum editorMode {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_VISUAL,
    MODE_VISUAL_LINE,
};

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

struct editorConfig {
    Buffer *buffers;
    int buf_current;
    int buf_count;
    char *yank_buffer;
    char statusmsg[80];
    int last_coloff;
    int last_cx, last_cy;
    int last_rowoff;
    int mode;
    int tab_stop;
    int pendingOp;
    int screencols;
    int screenrows;
    int gutter_width;
    int vis_start_cx, vis_start_cy;
    struct termios orig_termios;
    time_t statusmsg_time;
    struct configOptions settings;
};

extern struct editorConfig E;

#define CB (E.buffers[E.buf_current])

extern char  *YANKED_TEXT;
extern size_t YANKED_LEN;

/* Row helpers */
void  editorUpdateRow(erow *row);
void  editorInsertRow(int at, char *s, size_t len);
void  editorDelRow(int at);
void  editorFreeRow(erow *row);

/* Character-level ops */
void  editorRowInsertChar(erow *row, int at, int c);
void  editorRowDelChar(erow *row, int at);
void  editorRowAppendString(erow *row, char *s, size_t len);
void  editorInsertChar(int c);
void  editorDelChar(void);
void  editorDelCharUnderCursor(void);
void  editorInsertNewline(void);

/* Cursor conversion */
int   editorRowCxToRx(erow *row, int cx);
int   editorRowRxToCx(erow *row, int rx);

/* Yank / paste / delete */
void  editorYankLine(void);
void  editorYankLines(void);
void  editorDeleteLine(void);
void  editorDeleteLines(void);
void  editorDeleteWord(void);
void  editorPaste(void);

/* File I/O */
void  editorOpen(char *filename);
void  editorSave(void);
char *editorRowsToString(int *buflen);

#endif
