#include "tx.h"

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();

    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback)
          callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback)
      callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0)
        E.cx--;
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size)
        E.cx++;
      break;
    case ARROW_UP:
      if (E.cy != 0)
        E.cy--;
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows)
        E.cy++;
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorJumpToTop() {
  E.cy = 0;
  E.cx = 0;
  E.rowoff = 0;
}

void editorJumpToEnd() {
  if (E.numrows > 0) {
    E.cy = E.numrows - 1;
    E.cx = 0;
  }
}

void editorProcessNormalMode(int c) {
  switch (E.pendingOp) {
    case OP_DELETE:
      if (c == 'd') {
        undoBegin(E.cy, 1);
        editorDeleteLine();
        undoCommit();
      } else if (c == 'w') {
        undoBegin(E.cy, 1);
        editorDeleteWord();
        undoCommit();
      }
      break;

    case OP_JUMP_TO_TOP:
      if (c == 'g') {
        editorJumpToTop();
      }
      break;

    case OP_YANK:
      if (c == 'y') {
        editorYankLine();
      }
      break;

    case OP_INSERT:
      if (c == 'c') {
        undoBegin(E.cy, 1);
        editorDeleteLine();
        editorInsertNewline();
        undoCommit();
        E.mode = MODE_INSERT;
        E.pendingOp = OP_NONE;
      } else if (c == 'w') {
        undoBegin(E.cy, 1);
        editorDeleteWord();
        undoCommit();
        E.mode = MODE_INSERT;
      }
      break;
  }

  E.pendingOp = OP_NONE;

  switch (c) {
    case 'i':
      E.mode = MODE_INSERT;
      break;

    case 'v':
      E.vis_start_cx = E.cx;
      E.vis_start_cy = E.cy;
      E.mode = MODE_VISUAL;
      break;

    case ':':
      editorProcessCommandMode();
      break;

    case 'g':
      E.pendingOp = OP_JUMP_TO_TOP;
      break;

    case 'G':
      editorJumpToEnd();
      break;

    case 'c':
      E.pendingOp = OP_INSERT;
      break;

    case '/':
      editorFind();
      break;

    case 'd':
      E.pendingOp = OP_DELETE;
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case '\x1b':
      if (E.pendingOp != 0) {
        E.pendingOp = OP_NONE;
        editorSetStatusMessage("");
      }
      break;

    case 'h': editorMoveCursor(ARROW_LEFT); break;
    case 'j': editorMoveCursor(ARROW_DOWN); break;
    case 'k': editorMoveCursor(ARROW_UP); break;
    case 'l': editorMoveCursor(ARROW_RIGHT); break;

    case 'p': {
      int top = E.cy;
      int count = (E.cy < E.numrows) ? 1 : 0;
      undoBegin(top, count);
      editorPaste();
      undoCommit();
    } break;

    case 'x':
      if (E.cy < E.numrows) {
        undoBegin(E.cy, 1);
        editorDelCharUnderCursor();
        undoCommit();
      }
      break;

    case 'u':
      undoUndo();
      break;
    case CTRL_KEY('r'):
      undoRedo();
      break;

    case CTRL_KEY('q'):
      exit(0);
      break;
  }
}

void editorProcessInsertMode(int c) {
  static int quit_times = TX_QUIT_TIMES;

  switch (c) {
    case '\r': {
      int count = (E.cy < E.numrows) ? 1 : 0;
      undoBegin(E.cy, count);
      editorInsertNewline();
      undoCommit();
    } break;

    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                               "Press Ctrl-Q %d more times to quit.",
                               quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY: {
      int top = (c == DEL_KEY) ? E.cy : (E.cy > 0 ? E.cy - 1 : 0);
      int count = E.numrows - top;
      if (count > 2) count = 2;
      if (count < 0) count = 0;
      undoBegin(top, count);
      if (c == DEL_KEY)
        editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      undoCommit();
    } break;

    case PAGE_UP:
    case PAGE_DOWN: {
      if (c == PAGE_UP) {
        E.cy = E.rowoff;
      } else if (c == PAGE_DOWN) {
        E.cy = E.rowoff + E.screenrows - 1;
        if (E.cy > E.numrows)
          E.cy = E.numrows;
      }
      int times = E.screenrows;
      while (times--)
        editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    } break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      E.mode = MODE_NORMAL;
      break;

    case '{':
    case '(':
    case '[':
    case '\"':
    case '\'': {
      char close = (c == '{') ? '}' :
                   (c == '(') ? ')' :
                   (c == '[') ? ']' : c;
      int count = (E.cy < E.numrows) ? 1 : 0;
      undoBegin(E.cy, count);
      editorInsertChar(c);
      editorInsertChar(close);
      editorMoveCursor(ARROW_LEFT);
      undoCommit();
    } break;

    default: {
      int count = (E.cy < E.numrows) ? 1 : 0;
      undoBegin(E.cy, count);
      editorInsertChar(c);
      undoCommit();
    } break;
  }

  quit_times = TX_QUIT_TIMES;
}

void editorProcessKeypress() {
  int c = editorReadKey();

  switch (E.mode) {
    case MODE_NORMAL:
      editorProcessNormalMode(c);
      break;
    case MODE_INSERT:
      editorProcessInsertMode(c);
      break;
    case MODE_VISUAL:
      editorProcessVisualMode(c);
      break;
  }
}
