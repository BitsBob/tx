#include "tx.h"
#include "syntax.c"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *YANKED_TEXT = NULL;
size_t YANKED_LEN = 0;

int editorRowRxToCx(erow *row, int rx);
void editorRowAppendString(erow *row, char *s, size_t len);
void editorRowInsertChar(erow *row, int at, int c);
void editorDelRow(int at);

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static bool parse_bool(const char *val) {
    return (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
}

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (CB.syntax == NULL)
    return;

  const char **keywords = CB.syntax->keywords;

  const char *scs = CB.syntax->singleline_comment_start;
  const char *mcs = CB.syntax->multiline_comment_start;
  const char *mce = CB.syntax->multiline_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && CB.row[row->idx - 1].hl_open_comment);

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

    if (CB.syntax->flags & HL_HIGHLIGHT_STRINGS) {
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

    if (CB.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
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
  if (changed && row->idx + 1 < CB.numrows)
    editorUpdateSyntax(&CB.row[row->idx + 1]);
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
  CB.syntax = NULL;
  if (CB.filename == NULL)
    return;

  char *ext = strrchr(CB.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(CB.filename, s->filematch[i]))) {
        CB.syntax = s;

        for (int filerow = 0; filerow < CB.numrows; filerow++) {
          editorUpdateSyntax(&CB.row[filerow]);
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
  for (int i = 0; i < CB.numrows; i++) {
    current += direction;
    if (current == -1)
      current = CB.numrows - 1;
    else if (current == CB.numrows)
      current = 0;
    erow *row = &CB.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      CB.cy = current;
      CB.cx = editorRowRxToCx(row, match - row->render);
      CB.rowoff = CB.numrows;
      break;
    }
  }
}

void editorFind() {
  int saved_cx = CB.cx;
  int saved_cy = CB.cy;
  int saved_coloff = CB.coloff;
  int saved_rowoff = CB.rowoff;

  char *query = editorPrompt("Search: %s (ESC to cancel)", editorFindCallback);
  if (query) {
    free(query);
  } else {
    CB.cx = saved_cx;
    CB.cy = saved_cy;
    CB.coloff = saved_coloff;
    CB.rowoff = saved_rowoff;
  }
}

void editorDeleteLine() {
  if (CB.cy == CB.numrows)
    return;
  erow *row = &CB.row[CB.cy];
  free(YANKED_TEXT);
  YANKED_TEXT = malloc(row->size + 2);
  memcpy(YANKED_TEXT, row->chars, row->size);
  YANKED_TEXT[row->size] = '\n';
  YANKED_TEXT[row->size + 1] = '\0';
  YANKED_LEN = row->size + 1;

  editorDelRow(CB.cy);

  if (CB.cy == CB.numrows && CB.cy > 0)
    CB.cy--;
  editorSetStatusMessage("Deleted 1 line");
}

void editorOpen(char *filename) {
  free(CB.filename);
  CB.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp) {
    editorSetStatusMessage("New file: %s", filename);
    return;
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(CB.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  CB.dirty = 0;
}

int editorLoadConfig(char *path, struct configOptions *cfg) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    perror("path");
    return -1;
  }

  cfg->CONFIG_TAB_STOP              = 4;
  cfg->CONFIG_UNDO_MAX              = 100;
  cfg->CONFIG_SYNTAX                = false;
  cfg->CONFIG_SEARCH_CASE_SENSITIVE = false;
  cfg->CONFIG_SEARCH_HIGHLIGHT      = true;
  cfg->CONFIG_STATUS_FORTUNE        = true;
  cfg->CONFIG_NUMBERS               = false;
  cfg->CONFIG_LSP_ENABLE            = false;
  cfg->CONFIG_AUTOPAIR              = true;

  char line[256];
  int lineno = 0;

  while (fgets(line, sizeof line, fp)) {
    lineno ++;
    char *p = trim(line);

    if (*p == '\0' || *p == '#')
      continue;

    if (strncmp(p, "set", 3) != 0) {
      fprintf(stderr, "Invalid config line %d: %s", lineno, line); // TODO replace with actual entry crash
      continue;
    }

    p += 3;

    while (isspace((unsigned char)*p)) p++;

    char *key = p;
    char *value = NULL;

    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) {
      *p++ = '\0';
      while (isspace((unsigned char)*p))
        p++; 
    }
    value = p;

    if (strcmp(key, "tabstop") == 0)                cfg->CONFIG_TAB_STOP = atoi(value); 
    else if (strcmp(key, "undo_max") == 0)          cfg->CONFIG_UNDO_MAX = atoi(value);
    else if (strcmp(key, "syntax") == 0)            cfg->CONFIG_SYNTAX = parse_bool(value);
    else if (strcmp(key, "search_sensitive") == 0)  cfg->CONFIG_SEARCH_CASE_SENSITIVE = parse_bool(value);
    else if (strcmp(key, "search_highlight") == 0)  cfg->CONFIG_SEARCH_HIGHLIGHT = parse_bool(value);
    else if (strcmp(key, "status_fortune") == 0)    cfg->CONFIG_STATUS_FORTUNE = parse_bool(value);
    else if (strcmp(key, "numbers") == 0)           cfg->CONFIG_NUMBERS = parse_bool(value);
    else if (strcmp(key, "lsp_enable") == 0)        cfg->CONFIG_LSP_ENABLE = parse_bool(value);
    else if (strcmp(key, "autopair") == 0)          cfg->CONFIG_AUTOPAIR = parse_bool(value);
    else fprintf(stderr, "config: unknown key on line %d: %s", lineno, key);
  }

  fclose(fp);
  return 0;
}

void editorSave() {
  if (CB.filename == NULL) {
    CB.filename = editorPrompt("Write buffer as: %s", NULL);
    if (CB.filename == NULL) {
      editorSetStatusMessage("Write aborted.");
      return;
    }
    editorSelectSyntaxHighlight();
  }
  
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(CB.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        CB.dirty = 0;
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
      rx += (E.tab_stop - 1) - (rx % E.tab_stop);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (E.tab_stop - 1) - (cur_rx % E.tab_stop);
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
  row->render = malloc(row->size + tabs * (E.tab_stop - 1) + 1);
  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % E.tab_stop != 0)
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
  if (at < 0 || at > CB.numrows)
    return;

  CB.row = realloc(CB.row, sizeof(erow) * (CB.numrows + 1));

  if (at < CB.numrows) {
    memmove(&CB.row[at + 1], &CB.row[at], sizeof(erow) * (CB.numrows - at));
    for (int j = at + 1; j <= CB.numrows; j++)
      CB.row[j].idx++;
  }

  CB.row[at].idx = at;
  CB.row[at].size = len;
  CB.row[at].chars = malloc(len + 1);
  memcpy(CB.row[at].chars, s, len);
  CB.row[at].chars[len] = '\0';

  CB.row[at].rsize = 0;
  CB.row[at].render = NULL;
  CB.row[at].hl = NULL;
  CB.row[at].hl_open_comment = 0;

  CB.numrows++;
  editorUpdateRow(&CB.row[at]);

  CB.dirty++;
}

void editorInsertNewline() {
  char *indent = NULL;
  int indent_len = 0;
  int add_extra = 0;
  int split_brace = 0;
  char indent_ch = ' ';
  int extra_count = 2;

  if (CB.cy < CB.numrows) {
    erow *row = &CB.row[CB.cy];
    while (indent_len < row->size && indent_len < CB.cx &&
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

    if (CB.cx > 0 && CB.cx <= row->size) {
      char prev = row->chars[CB.cx - 1];
      if (prev == '{' || prev == '(' || prev == '[') {
        add_extra = 1;
        if (CB.cx < row->size) {
          char next = row->chars[CB.cx];
          if ((prev == '{' && next == '}') ||
              (prev == '(' && next == ')') ||
              (prev == '[' && next == ']')) {
            split_brace = 1;
          }
        }
      }
    }
  }

  if (CB.cx == 0) {
    editorInsertRow(CB.cy, "", 0);
  } else {
    erow *row = &CB.row[CB.cy];
    editorInsertRow(CB.cy + 1, &row->chars[CB.cx], row->size - CB.cx);
    row = &CB.row[CB.cy];
    row->size = CB.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  CB.cy++;
  CB.cx = 0;

  if (indent_len > 0 || add_extra) {
    erow *new_row = &CB.row[CB.cy];
    for (int i = 0; i < indent_len; i++) {
      editorRowInsertChar(new_row, CB.cx++, indent[i]);
    }
    if (add_extra) {
      for (int i = 0; i < extra_count; i++) {
        editorRowInsertChar(new_row, CB.cx++, indent_ch);
      }
    }
  }

  if (split_brace) {
    int cursor_cx = CB.cx;
    int cursor_cy = CB.cy;
    erow *new_row = &CB.row[CB.cy];
    int tail_start = CB.cx;
    int tail_len = new_row->size - tail_start;
    editorInsertRow(CB.cy + 1, &new_row->chars[tail_start], tail_len);
    new_row = &CB.row[CB.cy];
    new_row->size = tail_start;
    new_row->chars[new_row->size] = '\0';
    editorUpdateRow(new_row);

    erow *close_row = &CB.row[CB.cy + 1];
    for (int i = 0; i < indent_len; i++) {
      editorRowInsertChar(close_row, i, indent[i]);
    }

    CB.cx = cursor_cx;
    CB.cy = cursor_cy;
  }

  free(indent);
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= CB.numrows)
    return;
  editorFreeRow(&CB.row[at]);
  memmove(&CB.row[at], &CB.row[at + 1], sizeof(erow) * (CB.numrows - at - 1));
  for (int j = at; j < CB.numrows - 1; j++)
    CB.row[j].idx--;
  CB.numrows--;
  CB.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  CB.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  CB.dirty++;
}

void editorInsertChar(int c) {
  if (CB.cy == CB.numrows) {
    editorInsertRow(CB.numrows, "", 0);
  }
  editorRowInsertChar(&CB.row[CB.cy], CB.cx, c);
  CB.cx++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  CB.dirty++;
}

void editorDelChar() {
  if (CB.cy == CB.numrows)
    return;
  if (CB.cx == 0 && CB.cy == 0)
    return;
  erow *row = &CB.row[CB.cy];
  if (CB.cx > 0) {
    editorRowDelChar(row, CB.cx - 1);
    CB.cx--;
  } else {
    CB.cx = CB.row[CB.cy - 1].size;
    editorRowAppendString(&CB.row[CB.cy - 1], row->chars, row->size);
    editorDelRow(CB.cy);
    CB.cy--;
  }
}

void editorDelCharUnderCursor() {
  if (CB.cy >= CB.numrows)
    return;
  erow *row = &CB.row[CB.cy];
  if (CB.cx >= row->size)
    return;
  editorRowDelChar(row, CB.cx);
  if (CB.cx > 0 && CB.cx >= row->size)
    CB.cx = row->size - 1;
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < CB.numrows; j++)
    totlen += CB.row[j].size + 1;
  *buflen = totlen;
  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < CB.numrows; j++) {
    memcpy(p, CB.row[j].chars, CB.row[j].size);
    p += CB.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorYankLine() {
  if (CB.cy >= CB.numrows)
    return;
  erow *row = &CB.row[CB.cy];

  free(YANKED_TEXT);
  YANKED_TEXT = malloc(row->size + 2);
  memcpy(YANKED_TEXT, row->chars, row->size);
  YANKED_TEXT[row->size] = '\n';
  YANKED_TEXT[row->size + 1] = '\0';
  YANKED_LEN = row->size + 1;

  editorSetStatusMessage("Yanked 1 line");
}

void editorYankLines() {
    int start_y = E.vis_start_cy;
    int end_y = CB.cy;
    if (start_y > end_y) { int t = start_y; start_y = end_y; end_y = t; }

    if (start_y < 0) start_y = 0;
    if (end_y >= CB.numrows) end_y = CB.numrows - 1;

    size_t total = 0;
    for (int y = start_y; y <= end_y; y++)
        total += CB.row[y].size + 1;  // +1 for \n

    free(YANKED_TEXT);
    YANKED_TEXT = malloc(total + 1);
    if (!YANKED_TEXT) return;

    size_t idx = 0;
    for (int y = start_y; y <= end_y; y++) {
        memcpy(&YANKED_TEXT[idx], CB.row[y].chars, CB.row[y].size);
        idx += CB.row[y].size;
        YANKED_TEXT[idx++] = '\n';
    }
    YANKED_TEXT[idx] = '\0';
    YANKED_LEN = idx;

    int count = end_y - start_y + 1;
    editorSetStatusMessage("Yanked %d line%s", count, count == 1 ? "" : "s");
    E.mode = MODE_NORMAL;
}

void editorDeleteLines() {
    int start_y = E.vis_start_cy;
    int end_y = CB.cy;
    if (start_y > end_y) { int t = start_y; start_y = end_y; end_y = t; }

    if (start_y < 0) start_y = 0;
    if (end_y >= CB.numrows) end_y = CB.numrows - 1;

    int count = end_y - start_y + 1;

    undoBegin(start_y, count);
    editorYankLines();

    E.mode = MODE_VISUAL_LINE;

    for (int i = 0; i < count; i++)
        editorDelRow(start_y);

    if (start_y >= CB.numrows && CB.numrows > 0)
        CB.cy = CB.numrows - 1;
    else
        CB.cy = start_y;
    CB.cx = 0;

    undoCommit();
    E.mode = MODE_NORMAL;
    editorSetStatusMessage("Deleted %d line%s", count, count == 1 ? "" : "s");
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
  if (CB.cy == CB.numrows)
    return;
  erow *row = &CB.row[CB.cy];
  if (row->size == 0)
    return;

  int next_word_start = editorFindWordEnd(row, CB.cx);
  int count = next_word_start - CB.cx;

  memmove(&row->chars[CB.cx], &row->chars[next_word_start],
          row->size - next_word_start);
  row->size -= count;
  row->chars[row->size] = '\0';

  editorUpdateRow(row);
  CB.dirty++;
}
