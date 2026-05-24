#include "tx.h"
#include "editor.h"
#include "input.h"
#include "lsp_diag.h"
#include "output.h"
#include "syntax.h"
#include "undo.h"

char  *YANKED_TEXT = NULL;
size_t YANKED_LEN  = 0;

int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            rx += (E.settings.CONFIG_TAB_STOP - 1) - (rx % E.settings.CONFIG_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t')
            cur_rx += (E.settings.CONFIG_TAB_STOP - 1) - (cur_rx % E.settings.CONFIG_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx) return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row) {
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs * (E.settings.CONFIG_TAB_STOP - 1) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % E.settings.CONFIG_TAB_STOP != 0)
                row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    if (E.settings.CONFIG_SYNTAX)
        editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > CB.numrows) return;

    CB.row = realloc(CB.row, sizeof(erow) * (CB.numrows + 1));
    if (at < CB.numrows) {
        memmove(&CB.row[at + 1], &CB.row[at], sizeof(erow) * (CB.numrows - at));
        for (int j = at + 1; j <= CB.numrows; j++)
            CB.row[j].idx++;
    }

    CB.row[at].idx            = at;
    CB.row[at].size           = len;
    CB.row[at].chars          = malloc(len + 1);
    memcpy(CB.row[at].chars, s, len);
    CB.row[at].chars[len]     = '\0';
    CB.row[at].rsize          = 0;
    CB.row[at].render         = NULL;
    CB.row[at].hl             = NULL;
    CB.row[at].hl_open_comment = 0;

    CB.numrows++;
    editorUpdateRow(&CB.row[at]);
    CB.dirty++;
}

void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at) {
    if (at < 0 || at >= CB.numrows) return;
    editorFreeRow(&CB.row[at]);
    memmove(&CB.row[at], &CB.row[at + 1], sizeof(erow) * (CB.numrows - at - 1));
    for (int j = at; j < CB.numrows - 1; j++)
        CB.row[j].idx--;
    CB.numrows--;
    CB.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    CB.dirty++;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
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
    if (CB.cy == CB.numrows)
        editorInsertRow(CB.numrows, "", 0);
    editorRowInsertChar(&CB.row[CB.cy], CB.cx, c);
    CB.cx++;
}

void editorDelChar(void) {
    if (CB.cy == CB.numrows) return;
    if (CB.cx == 0 && CB.cy == 0) return;
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

void editorDelCharUnderCursor(void) {
    if (CB.cy >= CB.numrows) return;
    erow *row = &CB.row[CB.cy];
    if (CB.cx >= row->size) return;
    editorRowDelChar(row, CB.cx);
    if (CB.cx > 0 && CB.cx >= row->size)
        CB.cx = row->size - 1;
}

/* Insert a newline at the cursor. Copies the current line's leading whitespace
 * onto the new line. If the cursor is between a matched bracket pair like {}
 * or [], an extra blank line is inserted for the body and the closing bracket
 * is pushed down (split_brace), matching common editor behaviour. */
void editorInsertNewline(void) {
    char *indent    = NULL;
    int   indent_len = 0;
    int   add_extra  = 0;
    int   split_brace = 0;
    char  indent_ch  = ' ';
    int   extra_count = 2;

    if (CB.cy < CB.numrows) {
        erow *row = &CB.row[CB.cy];
        while (indent_len < row->size && indent_len < CB.cx &&
               (row->chars[indent_len] == ' ' || row->chars[indent_len] == '\t'))
            indent_len++;

        if (indent_len > 0) {
            indent = malloc(indent_len);
            memcpy(indent, row->chars, indent_len);
            for (int i = 0; i < indent_len; i++) {
                if (indent[i] == '\t') { indent_ch = '\t'; extra_count = 1; break; }
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
                        (prev == '[' && next == ']'))
                        split_brace = 1;
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
        for (int i = 0; i < indent_len; i++)
            editorRowInsertChar(new_row, CB.cx++, indent[i]);
        if (add_extra)
            for (int i = 0; i < extra_count; i++)
                editorRowInsertChar(new_row, CB.cx++, indent_ch);
    }

    if (split_brace) {
        int cursor_cx = CB.cx;
        int cursor_cy = CB.cy;
        erow *new_row  = &CB.row[CB.cy];
        int   tail_start = CB.cx;
        int   tail_len   = new_row->size - tail_start;
        editorInsertRow(CB.cy + 1, &new_row->chars[tail_start], tail_len);
        new_row = &CB.row[CB.cy];
        new_row->size = tail_start;
        new_row->chars[new_row->size] = '\0';
        editorUpdateRow(new_row);

        erow *close_row = &CB.row[CB.cy + 1];
        for (int i = 0; i < indent_len; i++)
            editorRowInsertChar(close_row, i, indent[i]);

        CB.cx = cursor_cx;
        CB.cy = cursor_cy;
    }

    free(indent);
}

void editorYankLine(void) {
    if (CB.cy >= CB.numrows) return;
    erow *row = &CB.row[CB.cy];
    free(YANKED_TEXT);
    YANKED_TEXT = malloc(row->size + 2);
    memcpy(YANKED_TEXT, row->chars, row->size);
    YANKED_TEXT[row->size]     = '\n';
    YANKED_TEXT[row->size + 1] = '\0';
    YANKED_LEN = row->size + 1;
    editorSetStatusMessage("Yanked 1 line");
}

void editorYankLines(void) {
    int start_y = E.vis_start_cy;
    int end_y   = CB.cy;
    if (start_y > end_y) { int t = start_y; start_y = end_y; end_y = t; }
    if (start_y < 0)            start_y = 0;
    if (end_y >= CB.numrows)    end_y   = CB.numrows - 1;

    size_t total = 0;
    for (int y = start_y; y <= end_y; y++)
        total += CB.row[y].size + 1;

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

void editorDeleteLine(void) {
    if (CB.cy == CB.numrows) return;
    erow *row = &CB.row[CB.cy];
    free(YANKED_TEXT);
    YANKED_TEXT = malloc(row->size + 2);
    memcpy(YANKED_TEXT, row->chars, row->size);
    YANKED_TEXT[row->size]     = '\n';
    YANKED_TEXT[row->size + 1] = '\0';
    YANKED_LEN = row->size + 1;
    editorDelRow(CB.cy);
    if (CB.cy == CB.numrows && CB.cy > 0) CB.cy--;
    editorSetStatusMessage("Deleted 1 line");
}

void editorDeleteLines(void) {
    int start_y = E.vis_start_cy;
    int end_y   = CB.cy;
    if (start_y > end_y) { int t = start_y; start_y = end_y; end_y = t; }
    if (start_y < 0)         start_y = 0;
    if (end_y >= CB.numrows) end_y   = CB.numrows - 1;

    int count = end_y - start_y + 1;
    undoBegin(start_y, count);
    editorYankLines();
    for (int i = 0; i < count; i++) editorDelRow(start_y);

    CB.cy = (start_y >= CB.numrows && CB.numrows > 0) ? CB.numrows - 1 : start_y;
    CB.cx = 0;
    undoCommit();
    E.mode = MODE_NORMAL;
    editorSetStatusMessage("Deleted %d line%s", count, count == 1 ? "" : "s");
}

void editorPaste(void) {
    if (!YANKED_TEXT || YANKED_LEN == 0) return;
    for (size_t i = 0; i < YANKED_LEN; i++) {
        char c = YANKED_TEXT[i];
        if (c == '\n') editorInsertNewline();
        else           editorInsertChar(c);
    }
    editorSetStatusMessage("Pasted %zu bytes", YANKED_LEN);
}

int editorFindWordEnd(erow *row, int start_cx) {
    int i = start_cx;
    if (i < row->size && isspace(row->chars[i])) {
        while (i < row->size && isspace(row->chars[i])) i++;
    } else {
        while (i < row->size && !isspace(row->chars[i])) i++;
        while (i < row->size &&  isspace(row->chars[i])) i++;
    }
    return i;
}

int editorFindWordStart(erow *row, int start_cx) {
    int i = start_cx - 1;
    if (i >= 0 && isspace(row->chars[i])) {
        while (i >= 0 && isspace(row->chars[i])) i--;
    } else {
        while (i >= 0 && !isspace(row->chars[i])) i--;
        while (i >= 0 &&  isspace(row->chars[i])) i--;
    }
    return i + 1;
}

void editorDeleteWord(void) {
    if (CB.cy == CB.numrows) return;
    erow *row = &CB.row[CB.cy];
    if (row->size == 0) return;
    int next = editorFindWordEnd(row, CB.cx);
    int count = next - CB.cx;
    memmove(&row->chars[CB.cx], &row->chars[next], row->size - next);
    row->size -= count;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    CB.dirty++;
}

char *editorRowsToString(int *buflen) {
    int totlen = 0;
    for (int j = 0; j < CB.numrows; j++)
        totlen += CB.row[j].size + 1;
    *buflen = totlen;
    char *buf = malloc(totlen);
    char *p   = buf;
    for (int j = 0; j < CB.numrows; j++) {
        memcpy(p, CB.row[j].chars, CB.row[j].size);
        p += CB.row[j].size;
        *p++ = '\n';
    }
    return buf;
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

    char   *line   = NULL;
    size_t  linecap = 0;
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
    lspNotifyOpen(E.buf_current);
}

void editorSave(void) {
    if (CB.readonly) {
        editorSetStatusMessage("Cannot write to read-only buffer.");
        return;
    }

    if (CB.filename == NULL) {
        CB.filename = editorPrompt("Write buffer as: %s", NULL);
        if (CB.filename == NULL) {
            editorSetStatusMessage("Write aborted.");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int   len;
    char *buf = editorRowsToString(&len);
    int   fd  = open(CB.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                CB.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                lspNotifySave(E.buf_current);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}