#include "tx.h"

static void normalizeSelection(int *sy, int *sx, int *ey, int *ex) {
  if (*sy > *ey || (*sy == *ey && *sx > *ex)) {
    int ty = *sy; *sy = *ey; *ey = ty;
    int tx = *sx; *sx = *ex; *ex = tx;
  }
}

void editorYankSelection() {
  if (E.mode != MODE_VISUAL)
    return;

  int start_y = E.vis_start_cy, end_y = E.cy;
  int start_x = E.vis_start_cx, end_x = E.cx;
  normalizeSelection(&start_y, &start_x, &end_y, &end_x);

  size_t total_len = 0;
  for (int y = start_y; y <= end_y; y++) {
    int row_start = (y == start_y) ? start_x : 0;
    int row_end = (y == end_y) ? end_x : E.row[y].size;
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
    int row_end = (y == end_y) ? end_x : E.row[y].size;

    for (int x = row_start; x < row_end; x++) {
      YANKED_TEXT[idx++] = E.row[y].chars[x];
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

  int start_y = E.vis_start_cy, end_y = E.cy;
  int start_x = E.vis_start_cx, end_x = E.cx;
  normalizeSelection(&start_y, &start_x, &end_y, &end_x);

  editorYankSelection();

  erow *first_row = &E.row[start_y];
  erow *last_row = &E.row[end_y];

  char *suffix = &last_row->chars[end_x];
  int suffix_len = last_row->size - end_x;

  first_row->size = start_x;
  editorRowAppendString(first_row, suffix, suffix_len);

  int rows_to_delete = end_y - start_y;
  while (rows_to_delete > 0) {
    editorDelRow(start_y + 1);
    rows_to_delete--;
  }

  E.cx = start_x;
  E.cy = start_y;
  E.mode = MODE_NORMAL;
  E.dirty++;
  editorSetStatusMessage("Deleted selection");
}

void editorProcessVisualMode(int c) {
  switch (c) {
    case 'y':
      editorYankSelection();
      break;

    case 'p': {
      int top = E.cy;
      int count = (E.cy < E.numrows) ? 1 : 0;
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
    case 'x': {
      int sy = E.vis_start_cy, ey = E.cy;
      if (sy > ey) { int t = sy; sy = ey; ey = t; }
      int top = sy;
      int count = ey - sy + 1;
      if (top + count > E.numrows) count = E.numrows - top;
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
