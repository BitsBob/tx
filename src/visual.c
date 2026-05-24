#include "tx.h"
#include "editor.h"
#include "input.h"
#include "output.h"
#include "undo.h"
#include "visual.h"

static void normalizeSelection(int *sy, int *sx, int *ey, int *ex) {
  if (*sy > *ey || (*sy == *ey && *sx > *ex)) {
    int ty = *sy; *sy = *ey; *ey = ty;
    int tx = *sx; *sx = *ex; *ex = tx;
  }
}

void editorYankSelection() {
  if (E.mode != MODE_VISUAL)
    return;

  int start_y = E.vis_start_cy, end_y = CB.cy;
  int start_x = E.vis_start_cx, end_x = CB.cx;
  normalizeSelection(&start_y, &start_x, &end_y, &end_x);

  size_t total_len = 0;
  for (int y = start_y; y <= end_y; y++) {
    int row_start = (y == start_y) ? start_x : 0;
    int row_end = (y == end_y) ? end_x : CB.row[y].size;
    total_len += row_end - row_start;
    if (y != end_y)
      total_len += 1;
  }

  free(YANKED_TEXT);
  YANKED_TEXT = malloc(total_len + 1);
  YANKED_LEN = total_len;

  size_t idx = 0;
  for (int y = start_y; y <= end_y; y++) {
    int row_start = (y == start_y) ? start_x : 0;
    int row_end = (y == end_y) ? end_x : CB.row[y].size;

    for (int x = row_start; x < row_end; x++) {
      YANKED_TEXT[idx++] = CB.row[y].chars[x];
    }

    if (y != end_y) {
      YANKED_TEXT[idx++] = '\n';
    }
  }

  YANKED_TEXT[idx] = '\0';
  editorSetStatusMessage("Yanked %zu bytes", YANKED_LEN);

  E.mode = MODE_NORMAL;
}

void editorDeleteSelection() {
  if (E.mode != MODE_VISUAL)
    return;

  int start_y = E.vis_start_cy, end_y = CB.cy;
  int start_x = E.vis_start_cx, end_x = CB.cx;
  normalizeSelection(&start_y, &start_x, &end_y, &end_x);

  editorYankSelection();

  erow *first_row = &CB.row[start_y];
  erow *last_row = &CB.row[end_y];

  /* Copy suffix before editorRowAppendString, which calls realloc on
   * first_row->chars. When start_y == end_y the two rows are the same
   * allocation, so suffix would become a dangling pointer without this copy. */
  int suffix_len = last_row->size - end_x;
  char *suffix = malloc(suffix_len + 1);
  memcpy(suffix, &last_row->chars[end_x], suffix_len);
  suffix[suffix_len] = '\0';

  first_row->size = start_x;
  first_row->chars[start_x] = '\0';
  editorRowAppendString(first_row, suffix, suffix_len);
  free(suffix);

  int rows_to_delete = end_y - start_y;
  while (rows_to_delete > 0) {
    editorDelRow(start_y + 1);
    rows_to_delete--;
  }

  CB.cx = start_x;
  CB.cy = start_y;
  E.mode = MODE_NORMAL;
  CB.dirty++;
  editorSetStatusMessage("Deleted selection");
}

void editorProcessVisualMode(int c) {
  int line_mode = (E.mode == MODE_VISUAL_LINE);
  switch (c) {
    case 'y':
      if (line_mode) {
        editorYankLines();
      } else {
        editorYankSelection();
      }
      break;

    case 'p': {
      int top = CB.cy;
      int count = (CB.cy < CB.numrows) ? 1 : 0;
      undoBegin(top, count);
      editorPaste();
      undoCommit();
    } break;

    case '\x1b':
      E.mode = MODE_NORMAL;
      break;

    case 'G':
      editorJumpToEnd();
      break;

    case 'd':
    case 'x':
    if (line_mode) {
      editorDeleteLines();
    } 
    else {
      int sy = E.vis_start_cy, ey = CB.cy;
      if (sy > ey) { int t = sy; sy = ey; ey = t; }
      int top = sy;
      int count = ey - sy + 1;
      if (top + count > CB.numrows) count = CB.numrows - top;
      if (count < 0) count = 0;
      undoBegin(top, count);
      editorDeleteSelection();
      undoCommit();
    } break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case 'h': editorMoveCursor(ARROW_LEFT); break;
    case 'j': editorMoveCursor(ARROW_DOWN); break;
    case 'k': editorMoveCursor(ARROW_UP); break;
    case 'l': editorMoveCursor(ARROW_RIGHT); break;

    default:
      break;
  }
}
