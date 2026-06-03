#include "tx.h"

#include "editor.h"
#include "lsp_diag.h"
#include "output.h"
#include "syntax.h"

struct abuf {
  char *b;
  int len;
  int cap;
};

#define ABUF_INIT {NULL, 0, 0}

static void abAppend(struct abuf *ab, const char *s, int len) {
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

static void abFree(struct abuf *ab) { free(ab->b); }

static void editorScroll(void) {
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

static int isCharSelected(int x, int y) {
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

/* Advance (y,x) one char position in direction dir (+1/-1), skipping over line boundaries (and empty lines). Returns 0 when the buffer edge is hit. */
static int braceStep(int *y, int *x, int dir) {
  *x += dir;
  for (;;) {
    if (*y < 0 || *y >= CB.numrows) return 0;
    if (*x < 0) {
      if (--(*y) < 0) return 0;
      *x = CB.row[*y].size - 1;
    } else if (*x >= CB.row[*y].size) {
      if (++(*y) >= CB.numrows) return 0;
      *x = 0;
    } else {
      return 1;
    }
  }
}

static int isBraceChar(char c) {
  return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

/* If the cursor sits on a bracket -- or directly in front of one (the
 * insert-mode caret rests just after it) -- find its match. On success the
 * source bracket's char coordinates are written to by/bx and its pair to
 * my/mx, and 1 is returned. Braces inside strings and comments are skipped so
 * they don't throw off the nesting count. */
static int editorFindMatchingBrace(int *by, int *bx, int *my, int *mx) {
  if (CB.cy < 0 || CB.cy >= CB.numrows) return 0;
  erow *row = &CB.row[CB.cy];

  /* Prefer the char under the cursor, then the one immediately before it. */
  int cx = -1;
  if (CB.cx >= 0 && CB.cx < row->size && isBraceChar(row->chars[CB.cx])) {
    cx = CB.cx;
  } else if (CB.cx - 1 >= 0 && CB.cx - 1 < row->size && isBraceChar(row->chars[CB.cx - 1])) {
    cx = CB.cx - 1;
  }
  if (cx < 0) return 0;

  char c = row->chars[cx];
  const char *opens = "([{", *closes = ")]}";
  const char *p;
  int dir;
  char self = c, other;
  if ((p = strchr(opens, c)) != NULL && *p) {
    dir = 1;  other = closes[p - opens];
  } else {
    p = strchr(closes, c);
    dir = -1; other = opens[p - closes];
  }

  int y = CB.cy, x = cx, balance = 0;
  for (;;) {
    erow *r = &CB.row[y];
    int skip = 0;
    if (!(y == CB.cy && x == cx)) {
      int rx = editorRowCxToRx(r, x);
      if (rx < r->rsize) {
        unsigned char h = r->hl[rx];
        if (h == HL_STRING || h == HL_COMMENT || h == HL_MLCOMMENT) skip = 1;
      }
    }
    if (!skip) {
      char ch = r->chars[x];
      if (ch == self) balance++;
      else if (ch == other) balance--;
      if (balance == 0) {
        *by = CB.cy; *bx = cx;
        *my = y; *mx = x;
        return 1;
      }
    }
    if (!braceStep(&y, &x, dir)) return 0;
  }
}

static void editorDrawRows(struct abuf *ab) {
  int digits = 1, n= CB.numrows;
  while (n >= 10) {
    digits++;
    n /= 10;
  }

  int lsp_active = lspIsReady();
  int gutter = (E.settings.CONFIG_NUMBERS ? digits + 1 : 0) + (lsp_active ? 2 : 0);
  E.gutter_width = gutter;

  /* Matching-brace highlight: the bracket under the cursor and its pair, stored in render coordinates so they line up with the draw loop. */
  int brace_active = 0;
  int b1_row = -1, b1_rx = -1, b2_row = -1, b2_rx = -1;
  {
    int by, bx, my, mx;
    if (editorFindMatchingBrace(&by, &bx, &my, &mx)) {
      brace_active = 1;
      b1_row = by; b1_rx = editorRowCxToRx(&CB.row[by], bx);
      b2_row = my; b2_rx = editorRowCxToRx(&CB.row[my], mx);
    }
  }

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
      if (lsp_active) {
        int sev = (CB.diags && filerow < CB.diags_count) ? CB.diags[filerow] : 0;
        const char *color;
        const char *marker;
        switch (sev) {
          case 1:  color = "\x1b[31m"; marker = "E"; break; /* error   - red    */
          case 2:  color = "\x1b[33m"; marker = "W"; break; /* warning - yellow */
          case 3:  color = "\x1b[36m"; marker = "I"; break; /* info    - cyan   */
          case 4:  color = "\x1b[90m"; marker = "H"; break; /* hint    - dim    */
          default: color = "\x1b[90m"; marker = " "; break; /* none             */
        }
        abAppend(ab, color, 5);
        abAppend(ab, marker, 1);
        abAppend(ab, " \x1b[m", 4);
      }
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
      int in_brace = 0;
      int current_color = -1;

#define IS_BRACE_CELL(absrx)                                  \
  (brace_active && (((filerow) == b1_row && (absrx) == b1_rx) || \
                    ((filerow) == b2_row && (absrx) == b2_rx)))

      int j = 0;
      while (j < len) {
        int absrx = j + CB.coloff;
        int selected = (E.mode == MODE_VISUAL || E.mode == MODE_VISUAL_LINE) && isCharSelected(absrx, filerow);
        int brace = IS_BRACE_CELL(absrx);
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

        if (brace && !in_brace) {
          abAppend(ab, "\x1b[4m", 4);
          in_brace = 1;
        } else if (!brace && in_brace) {
          abAppend(ab, "\x1b[24m", 5);
          in_brace = 0;
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
          int next_absrx = j + CB.coloff;
          int next_selected = (E.mode == MODE_VISUAL || E.mode == MODE_VISUAL_LINE) && isCharSelected(next_absrx, filerow);
          int next_brace = IS_BRACE_CELL(next_absrx);
          int next_color = (hl[j] == HL_NORMAL) ? -1 : editorSyntaxToColor(hl[j]);
          if (next_selected != selected || next_brace != brace || next_color != color)
            break;
          j++;
        }
        abAppend(ab, &render[run_start], j - run_start);
      }

#undef IS_BRACE_CELL

      if (in_brace) abAppend(ab, "\x1b[24m", 5);
      abAppend(ab, "\x1b[39m", 5);
      if (in_selection) abAppend(ab, "\x1b[m", 3);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

static char *editorModeToString(void) {
  switch (E.mode) {
  case MODE_INSERT:      return " INSERT ";
  case MODE_COMMAND:     return " COMMAND ";
  case MODE_VISUAL:      return " VISUAL ";
  case MODE_VISUAL_LINE: return " V-LINE ";
  default:               return " NORMAL ";
  }
}

static void editorDrawStatusBar(struct abuf *ab) {
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

static void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  lspPoll();
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
