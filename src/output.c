#include "tx.h"

struct abuf {
  char *b;
  int len;
  int cap;
};

#define ABUF_INIT {NULL, 0, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  if (ab->len + len > ab->cap) {
    int new_cap = ab->cap ? ab->cap * 2 : 256;
    while (new_cap < ab->len + len)
      new_cap *= 2;
    char *new = realloc(ab->b, new_cap);
    if (new == NULL)
      return;
    ab->b = new;
    ab->cap = new_cap;
  }
  memcpy(&ab->b[ab->len], s, len);
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

void editorScroll() {
  CB.rx = CB.cx;
  if (CB.cy < CB.numrows) {
    CB.rx = editorRowCxToRx(&CB.row[CB.cy], CB.cx);
  }
  if (CB.cy < CB.rowoff) {
    CB.rowoff = CB.cy;
  }
  if (CB.cy >= CB.rowoff + E.screenrows) {
    CB.rowoff = CB.cy - E.screenrows + 1;
  }
  if (CB.rx < CB.coloff) {
    CB.coloff = CB.rx;
  }
  if (CB.rx >= CB.coloff + E.screencols - E.gutter_width) {
    CB.coloff = CB.rx - E.screencols + E.gutter_width + 1;
  }
}

int isCharSelected(int x, int y) {
  if (E.mode == MODE_VISUAL_LINE) {
    int start_y = E.vis_start_cy;
    int end_y = CB.cy;
    if (start_y > end_y) { int t = start_y; start_y = end_y; end_y = t; }
    return (y >= start_y && y <= end_y);
  }

  if (E.mode  != MODE_VISUAL || CB.numrows == 0 || y >= CB.numrows) return 0;
  
  int start_y = E.vis_start_cy;
  int end_y = CB.cy;

  if (start_y >= CB.numrows) start_y = CB.numrows - 1;
  if (end_y >= CB.numrows) end_y = CB.numrows - 1;

  int start_x = editorRowCxToRx(&CB.row[start_y], E.vis_start_cx);
  int end_x   = CB.rx;

  if (start_y > end_y ||
     (start_y == end_y && start_x > end_x)) {
    int tmp_y = start_y; start_y = end_y; end_y = tmp_y;
    int tmp_x = start_x; start_x = end_x; end_x = tmp_x;
     }

  if (y < start_y || y > end_y) return 0;
  if (y == start_y && y == end_y) return (x >= start_x && x <= end_x);
  if (y == start_y) return (x >= start_x);
  if (y == end_y) return (x <= end_x);
  return 1;
}

void editorDrawRows(struct abuf *ab) {
  int digits = 1, n= CB.numrows;
  while (n >= 10) {
    digits++;
    n /= 10;
  }

  int gutter = E.settings.CONFIG_NUMBERS ? digits + 1 : 0;
  E.gutter_width = gutter;

  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + CB.rowoff;
    if (filerow >= CB.numrows) {
      if (CB.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "TX editor -- version %s", TX_VERSION);
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;

        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);

        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      if (E.settings.CONFIG_NUMBERS) {
        char linenum[16];
        int ln_len = snprintf(linenum, sizeof(linenum), "%*d ", digits, filerow + 1);
        abAppend(ab, "\x1b[90m", 5);
        abAppend(ab, linenum, ln_len);
        abAppend(ab, "\x1b[m", 3);
      }

      int len = CB.row[filerow].rsize - CB.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols - E.gutter_width) len = E.screencols - E.gutter_width;

      char *render = (len > 0) ? &CB.row[filerow].render[CB.coloff] : NULL;
      unsigned char *hl = (len > 0) ? &CB.row[filerow].hl[CB.coloff] : NULL;
      int in_selection = 0;
      int current_color = -1;

      int j = 0;
      while (j < len) {
        int selected = (E.mode == MODE_VISUAL && isCharSelected(j + CB.coloff, filerow));
        int color = (E.settings.CONFIG_SYNTAX && hl[j] != HL_NORMAL)
              ? editorSyntaxToColor(hl[j])
              : -1;

        if (selected && !in_selection) {
          abAppend(ab, "\x1b[7m", 4);
          in_selection = 1;
        } else if (!selected && in_selection) {
          abAppend(ab, "\x1b[27m", 5);
          in_selection = 0;
        }

        if (color != current_color) {
          current_color = color;
          if (color == -1) {
            abAppend(ab, "\x1b[39m", 5);
          } else {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
        }

        int run_start = j;
        j++;
        while (j < len) {
          int next_selected = (E.mode == MODE_VISUAL && isCharSelected(j + CB.coloff, filerow));
          int next_color = (hl[j] == HL_NORMAL) ? -1 : editorSyntaxToColor(hl[j]);
          if (next_selected != selected || next_color != color)
            break;
          j++;
        }
        abAppend(ab, &render[run_start], j - run_start);
      }

      abAppend(ab, "\x1b[39m", 5);
      if (in_selection) abAppend(ab, "\x1b[m", 3);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

char *editorModeToString() {
  switch (E.mode) {
  case MODE_INSERT:
    return " INSERT ";
  case MODE_COMMAND:
    return " COMMAND ";
  default:
    return " NORMAL ";
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4); // Invert colors

  char *modename = editorModeToString();
  int modelen = strlen(modename);
  abAppend(ab, modename, modelen);

  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), " %.20s - %d lines %s",
                     CB.filename ? CB.filename : "[No Name]", CB.numrows,
                     CB.dirty ? "(modified)" : "");

  int rlen = snprintf(rstatus, sizeof(rstatus), " %s | %d/%d ",
                      CB.syntax ? CB.syntax->filetype : "no ft",
                      CB.cy + 1, CB.numrows);
  
  if (modelen > E.screencols) modelen = E.screencols;

  if (len + modelen > E.screencols) {
    len = E.screencols - modelen;
  }
  abAppend(ab, status, len);

  int used_width = len + modelen;

  while (used_width < E.screencols) {
    if (E.screencols - used_width == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      used_width++;
    }
  }

  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  if (E.mode == MODE_INSERT) {
    abAppend(&ab, "\x1b[5 q", 5);
  } else {
    abAppend(&ab, "\x1b[2 q", 5);
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (CB.cy - CB.rowoff) + 1,
           (CB.rx - CB.coloff) + E.gutter_width + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

void editorSetFortuneStatusMessage() {
  if (!E.settings.CONFIG_STATUS_FORTUNE) {
    editorSetStatusMessage("");
    return;
  }

  FILE *fp = popen("fortune -s 2>/dev/null", "r");
  if (!fp) {
    editorSetStatusMessage("Unfortunate.");
    return;
  }

  char buffer[1024];
  size_t len = fread(buffer, 1, sizeof(buffer) - 1, fp);
  buffer[len] = '\0';

  pclose(fp);

  for (size_t i = 0; i < len; i++) {
    if (buffer[i] == '\n')
      buffer[i] = ' ';
  }

  editorSetStatusMessage("%s", buffer);
}
