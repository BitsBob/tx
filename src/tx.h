#ifndef TX_H
#define TX_H

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#define TX_VERSION "0.1.3"
#define TX_UNDO_MAX 200

#define CTRL_KEY(k) ((k) & 0x1f)

struct configOptions {
  int CONFIG_TAB_STOP;
  int CONFIG_UNDO_MAX;
  bool CONFIG_NUMBERS;
  bool CONFIG_SYNTAX;
  bool CONFIG_SEARCH_CASE_SENSITIVE;
  bool CONFIG_SEARCH_HIGHLIGHT;
  bool CONFIG_STATUS_FORTUNE;
  bool CONFIG_LSP_ENABLE;
};

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

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH,
};

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

struct editorSyntax {
  const char *filetype;
  const char **filematch;
  const char **keywords;
  const char *singleline_comment_start;
  const char *multiline_comment_start;
  const char *multiline_comment_end;
  int flags;
};

extern char *YANKED_TEXT;
extern size_t YANKED_LEN;

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

typedef struct {
  char *filename;
  erow *row;
  int numrows;
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int dirty;
  struct editorSyntax *syntax;
} Buffer;

typedef struct {
  int pid;
  int in_fd;
  int out_fd;
  int active;
  int next_id;
} LspClient;

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

// terminal.c
int editorReadKey();
int getWindowSize(int *rows, int *cols);
void die(const char *s);
void disableRawMode();
void enableRawMode();

// editor.c
char *editorRowsToString(int *buflen);
int editorRowCxToRx(erow *row, int rx);
void editorDelChar();
void editorDelCharUnderCursor();
void editorDelRow(int at);
void editorFind();
void editorFreeRow(erow *row);
void editorInsertChar(int c);
void editorInsertNewline();
void editorInsertRow(int at, char *s, size_t len);
void editorOpen(char *filename);
void editorPaste();
void editorRowAppendString(erow *row, char *s, size_t len);
void editorSave();
void editorUpdateRow(erow *row);
void editorDeleteLine();
void editorYankLine();
void editorDeleteWord();
void editorUpdateSyntax(erow *row);
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int hl);
int editorLoadConfig(char *path, struct configOptions *cfg);


// output.c
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);
void editorSetFortuneStatusMessage();

// input.c
void editorProcessKeypress();
void editorProcessNormalMode(int c);
void editorProcessInsertMode(int c);
void editorMoveCursor(int key);
void editorJumpToTop();
void editorJumpToEnd();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

// visual.c
void editorProcessVisualMode(int c);
void editorYankSelection();
void editorDeleteSelection();

// command.c
void editorProcessCommandMode();

// undo.c
void undoBegin(int top, int count);
void undoCommit(void);
void undoUndo(void);
void undoRedo(void);

// buffer.c
void bufferClose(int idx);
void bufferFree(int idx);
void bufferOpen(char *filename);
void bufferSwitch(int idx);
int bufferNew();

#define CB (E.buffers[E.buf_current])

#endif

