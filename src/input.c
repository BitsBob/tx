#include "tx.h"

void editorProcessInsertMode(int c);
void editorProcessNormalMode(int c);
void handleCommandMode();

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
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) callback(buf, c);
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

    if (callback) callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
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

void editorProcessKeypress() {
  int c = editorReadKey();

  switch (E.mode) {
    case MODE_NORMAL:
      editorProcessNormalMode(c);
      break;
    case MODE_INSERT:
      editorProcessInsertMode(c);
      break;
  }
}

void editorProcessNormalMode(int c) {
  switch (c) {
    case 'i':
      E.mode = MODE_INSERT;
      break;

    case ':':
      handleCommandMode();
      break;

    case '/':
      {
        E.last_cx = E.cx;
        E.last_cy = E.cy;
        
        E.last_rowoff = E.rowoff;
        E.last_coloff = E.coloff;

        char *query = editorPrompt("/%s", editorFindCallback);
        if (query) {
          free(query);
        } else {
          E.cx = E.last_cx;
          E.cy = E.last_cy;
          E.rowoff = E.last_rowoff;
          E.coloff = E.last_coloff; 
        }
      }
      break;


    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case 'h':
    case 'j':
    case 'k':
    case 'l':
      editorMoveCursor(c);
      break;

    case CTRL_KEY('q'):
      exit(0);
      break;

  }
}

void handleCommandMode() {
    char *cmd = editorPrompt(":%s", NULL);

    if (cmd == NULL) return;

    if (strcmp(cmd, "q") == 0) {
        if (E.dirty) {
            editorSetStatusMessage("No write since last change (add ! to override)");
        } else {
            exit(0);
        }
    } else if (strcmp(cmd, "q!") == 0) {
        exit(0);
    } else if (strcmp(cmd, "w") == 0) {
        editorSave();
    } else if (strcmp(cmd, "wq") == 0) {
        editorSave();
        exit(0);
    } else {
        editorSetStatusMessage("Unknown command: %s", cmd);
    }

    free(cmd);
}

void editorProcessInsertMode(int c) {
  static int quit_times = TX_QUIT_TIMES;

  switch (c) {
    case '\r':
      editorInsertNewline();
      break;

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
    case DEL_KEY:
      if (c == DEL_KEY)
        editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN: 
      {
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

    default:
      editorInsertChar(c);
      break;
  }

  quit_times = TX_QUIT_TIMES;
}
