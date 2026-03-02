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
#define TX_VERSION "0.0.1"
#define TX_TAB_STOP 8
#define TX_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

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

extern char *YANKED_TEXT;
extern size_t YANKED_LEN;

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
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
void editorFind();
void editorInsertChar(int c);
void editorInsertNewline();
void editorInsertRow(int at, char *s, size_t len);
void editorOpen(char *filename);
void editorPaste();
void editorSave();
void editorUpdateRow(erow *row);
void editorYankSelection();

// output.c
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);
void editorSetFortuneStatusMessage();

// input.c
void editorProcessKeypress();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

#endif

