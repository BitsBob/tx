#include "tx.h"

char *YANKED_TEXT = NULL;
size_t YANKED_LEN = 0;

int editorRowRxToCx(erow *row, int rx);
void editorRowAppendString(erow *row, char *s, size_t len);
void editorRowInsertChar(erow *row, int at, int c);
void editorDelRow(int at);

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
    "switch",    "if",      "while",   "for",    "break",    "continue",
    "return",    "else",    "struct",  "union",  "typedef",  "static",
    "enum",      "class",   "case",    "sizeof", "volatile", "extern",

    "int|",      "long|",   "double|", "float|", "char|",    "unsigned|",
    "signed|",   "void|",   "short|",  "auto|",  "const|",   "bool|",
    "size_t|",   "ssize_t|", NULL
};

struct editorSyntax HLDB[] = {
    {"c",
     C_HL_extensions,
     C_HL_keywords,
     "//",
     "/*",
     "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->singleline_comment_start;
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mce_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;

        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
  switch (hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT:
      return 36;
    case HL_KEYWORD1:
      return 32;
    case HL_KEYWORD2:
      return 33;
    case HL_STRING:
      return 35;
    case HL_NUMBER:
      return 31;
    case HL_MATCH:
      return 34;
    default:
      return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL)
    return;

  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;

        for (int filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }

        return;
      }
      i++;
    }
  }
}

void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;
  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }
  if (last_match == -1)
    direction = 1;
  int current = last_match;
  for (int i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;
    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query = editorPrompt("Search: %s (ESC to cancel)", editorFindCallback);
  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

void editorDeleteLine() {
  if (E.cy == E.numrows)
    return;
  erow *row = &E.row[E.cy];
  free(YANKED_TEXT);
  YANKED_TEXT = malloc(row->size + 2);
  memcpy(YANKED_TEXT, row->chars, row->size);
  YANKED_TEXT[row->size] = '\n';
  YANKED_TEXT[row->size + 1] = '\0';
  YANKED_LEN = row->size + 1;

  editorDelRow(E.cy);

  if (E.cy == E.numrows && E.cy > 0)
    E.cy--;
  editorSetStatusMessage("Deleted 1 line");
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Write buffer as: %s", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Write aborted.");
      return;
    }
    editorSelectSyntaxHighlight();
  }
  
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TX_TAB_STOP - 1) - (rx % TX_TAB_STOP);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (TX_TAB_STOP - 1) - (cur_rx % TX_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx)
      return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (TX_TAB_STOP - 1) + 1);
  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TX_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows)
    return;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  if (at < E.numrows) {
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for (int j = at + 1; j <= E.numrows; j++)
      E.row[j].idx++;
  }

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;

  E.numrows++;
  editorUpdateRow(&E.row[at]);

  E.dirty++;
}

void editorInsertNewline() {
  char *indent = NULL;
  int indent_len = 0;
  int add_extra = 0;
  int split_brace = 0;
  char indent_ch = ' ';
  int extra_count = 2;

  if (E.cy < E.numrows) {
    erow *row = &E.row[E.cy];
    while (indent_len < row->size && indent_len < E.cx &&
           (row->chars[indent_len] == ' ' || row->chars[indent_len] == '\t')) {
      indent_len++;
    }
    if (indent_len > 0) {
      indent = malloc(indent_len);
      memcpy(indent, row->chars, indent_len);
      for (int i = 0; i < indent_len; i++) {
        if (indent[i] == '\t') {
          indent_ch = '\t';
          extra_count = 1;
          break;
        }
      }
    }

    if (E.cx > 0 && E.cx <= row->size) {
      char prev = row->chars[E.cx - 1];
      if (prev == '{' || prev == '(' || prev == '[') {
        add_extra = 1;
        if (E.cx < row->size) {
          char next = row->chars[E.cx];
          if ((prev == '{' && next == '}') ||
              (prev == '(' && next == ')') ||
              (prev == '[' && next == ']')) {
            split_brace = 1;
          }
        }
      }
    }
  }

  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;

  if (indent_len > 0 || add_extra) {
    erow *new_row = &E.row[E.cy];
    for (int i = 0; i < indent_len; i++) {
      editorRowInsertChar(new_row, E.cx++, indent[i]);
    }
    if (add_extra) {
      for (int i = 0; i < extra_count; i++) {
        editorRowInsertChar(new_row, E.cx++, indent_ch);
      }
    }
  }

  if (split_brace) {
    int cursor_cx = E.cx;
    int cursor_cy = E.cy;
    erow *new_row = &E.row[E.cy];
    int tail_start = E.cx;
    int tail_len = new_row->size - tail_start;
    editorInsertRow(E.cy + 1, &new_row->chars[tail_start], tail_len);
    new_row = &E.row[E.cy];
    new_row->size = tail_start;
    new_row->chars[new_row->size] = '\0';
    editorUpdateRow(new_row);

    erow *close_row = &E.row[E.cy + 1];
    for (int i = 0; i < indent_len; i++) {
      editorRowInsertChar(close_row, i, indent[i]);
    }

    E.cx = cursor_cx;
    E.cy = cursor_cy;
  }

  free(indent);
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++)
    E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == 0 && E.cy == 0)
    return;
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

void editorDelCharUnderCursor() {
  if (E.cy >= E.numrows)
    return;
  erow *row = &E.row[E.cy];
  if (E.cx >= row->size)
    return;
  editorRowDelChar(row, E.cx);
  if (E.cx > 0 && E.cx >= row->size)
    E.cx = row->size - 1;
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;
  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorYankLine() {
  if (E.cy >= E.numrows)
    return;
  erow *row = &E.row[E.cy];

  free(YANKED_TEXT);
  YANKED_TEXT = malloc(row->size + 2);
  memcpy(YANKED_TEXT, row->chars, row->size);
  YANKED_TEXT[row->size] = '\n';
  YANKED_TEXT[row->size + 1] = '\0';
  YANKED_LEN = row->size + 1;

  editorSetStatusMessage("Yanked 1 line");
}

void editorPaste() {
  if (!YANKED_TEXT || YANKED_LEN == 0)
    return;

  for (size_t i = 0; i < YANKED_LEN; i++) {
    char c = YANKED_TEXT[i];
    if (c == '\n') {
      editorInsertNewline();
    } else {
      editorInsertChar(c);
    }
  }

  editorSetStatusMessage("Pasted %zu bytes", YANKED_LEN);
}

int editorFindWordEnd(erow *row, int start_cx) {
  int i = start_cx;

  if (i < row->size && isspace(row->chars[i])) {
    while (i < row->size && isspace(row->chars[i]))
      i++;
  } else {
    while (i < row->size && !isspace(row->chars[i]))
      i++;
    while (i < row->size && isspace(row->chars[i]))
      i++;
  }

  return i;
}

void editorDeleteWord() {
  if (E.cy == E.numrows)
    return;
  erow *row = &E.row[E.cy];
  if (row->size == 0)
    return;

  int next_word_start = editorFindWordEnd(row, E.cx);
  int count = next_word_start - E.cx;

  memmove(&row->chars[E.cx], &row->chars[next_word_start],
          row->size - next_word_start);
  row->size -= count;
  row->chars[row->size] = '\0';

  editorUpdateRow(row);
  E.dirty++;
}
