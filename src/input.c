#include "tx.h"
#include "command.h"
#include "editor.h"
#include "input.h"
#include "output.h"
#include "search.h"
#include "terminal.h"
#include "undo.h"
#include "visual.h"

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
  erow *row = (CB.cy >= CB.numrows) ? NULL : &CB.row[CB.cy];
  switch (key) {
    case ARROW_LEFT:
      if (CB.cx != 0)
        CB.cx--;
      break;
    case ARROW_RIGHT:
      if (row && CB.cx < row->size)
        CB.cx++;
      break;
    case ARROW_UP:
      if (CB.cy != 0)
        CB.cy--;
      break;
    case ARROW_DOWN:
      if (CB.cy < CB.numrows)
        CB.cy++;
      break;
  }

  row = (CB.cy >= CB.numrows) ? NULL : &CB.row[CB.cy];
  int rowlen = row ? row->size : 0;
  if (CB.cx > rowlen) {
    CB.cx = rowlen;
  }
}

void editorJumpToTop() {
  CB.cy = 0;
  CB.cx = 0;
  CB.rowoff = 0;
}

void editorJumpToEnd() {
  if (CB.numrows > 0) {
    CB.cy = CB.numrows - 1;
    CB.cx = 0;
  }
}

void editorProcessNormalMode(int c) {
  switch (E.pendingOp) {
    case OP_DELETE:
      if (c == 'd') {
        undoBegin(CB.cy, 1);
        editorDeleteLine();
        undoCommit();
      } else if (c == 'w') {
        undoBegin(CB.cy, 1);
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
        undoBegin(CB.cy, 1);
        editorDeleteLine();
        editorInsertNewline();
        undoCommit();
        E.mode = MODE_INSERT;
      } else if (c == 'w') {
        undoBegin(CB.cy, 1);
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
      E.vis_start_cx = CB.cx;
      E.vis_start_cy = CB.cy;
      E.mode = MODE_VISUAL;
      break;

    case 'V':
      E.vis_start_cx = 0;
      E.vis_start_cy = CB.cy;
      E.mode = MODE_VISUAL_LINE;
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
      int top = CB.cy;
      int count = (CB.cy < CB.numrows) ? 1 : 0;
      undoBegin(top, count);
      editorPaste();
      undoCommit();
    } break;

    case 'x':
      if (CB.cy < CB.numrows) {
        undoBegin(CB.cy, 1);
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
  switch (c) {
    case '\r': {
      int count = (CB.cy < CB.numrows) ? 1 : 0;
      undoBegin(CB.cy, count);
      editorInsertNewline();
      undoCommit();
    } break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      CB.cx = 0;
      break;
    case END_KEY:
      if (CB.cy < CB.numrows)
        CB.cx = CB.row[CB.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY: {
      int top = (c == DEL_KEY) ? CB.cy : (CB.cy > 0 ? CB.cy - 1 : 0);
      int count = CB.numrows - top;
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
        CB.cy = CB.rowoff;
      } else if (c == PAGE_DOWN) {
        CB.cy = CB.rowoff + E.screenrows - 1;
        if (CB.cy > CB.numrows)
          CB.cy = CB.numrows;
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
      if (!E.settings.CONFIG_AUTOPAIR) {
        break;
      }

      char close = (c == '{') ? '}' :
                   (c == '(') ? ')' :
                   (c == '[') ? ']' : c;
      int count = (CB.cy < CB.numrows) ? 1 : 0;
      undoBegin(CB.cy, count);
      editorInsertChar(c);
      editorInsertChar(close);
      editorMoveCursor(ARROW_LEFT);
      undoCommit();
    } break;

    default: {
      int count = (CB.cy < CB.numrows) ? 1 : 0;
      undoBegin(CB.cy, count);
      editorInsertChar(c);
      undoCommit();
    } break;
  }

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
    case MODE_VISUAL_LINE:
      editorProcessVisualMode(c);
      break;
  }
}
