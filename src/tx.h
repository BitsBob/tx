#ifndef TX_H
#define TX_H
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

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#define MAX_YANKS 10
#define TX_VERSION "0.1.1"
#define TX_TAB_STOP 8
#define TX_QUIT_TIMES 3
#define TX_UNDO_MAX 200

#define CTRL_KEY(k) ((k) & 0x1f)

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
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
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

struct editorConfig {
  char *filename;
  char *yank_buffer;
  char statusmsg[80];
  erow *row;
  int coloff;
  int cx, cy;
  int dirty;
  int last_coloff;
  int last_cx, last_cy;
  int last_rowoff;
  int mode;
  int numrows;
  int rowoff;
  int pendingOp;
  int rx;
  int screencols;
  int screenrows;
  int vis_start_cx, vis_start_cy;
  struct editorSyntax *syntax;
  struct termios orig_termios;
  time_t statusmsg_time;
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

#endif

