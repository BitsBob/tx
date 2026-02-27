#ifndef TX_H
#define TX_H

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TX_VERSION "0.0.1"
#define TX_TAB_STOP 8
#define TX_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorMode {
  MODE_NORMAL,
  MODE_INSERT,
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

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  int mode;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
};

extern struct editorConfig E;

// terminal.c
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getWindowSize(int *rows, int *cols);
void die(const char *s);

// editor.c
void editorOpen(char *filename);
void editorSave();
void editorInsertChar(int c);
void editorDelChar();
void editorUpdateRow(erow *row);
void editorInsertNewline();
void editorInsertRow(int at, char *s, size_t len);
char *editorRowsToString(int *buflen);
int editorRowCxToRx(erow *row, int rx);

// output.c
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);

// input.c
void editorProcessKeypress();
char *editorPrompt(char *prompt);

#endif
