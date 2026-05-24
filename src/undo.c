#include "tx.h"
#include "editor.h"
#include "output.h"
#include "undo.h"

/* Undo works by snapshotting the affected rows before and after each edit.
 * Each entry stores the old text and the new text for a contiguous range
 * starting at `top`. Undoing replays old_lines; redoing replays new_lines.
 * Cursor positions are saved so undo lands the cursor where it was. */
typedef struct u_entry u_entry_T;
struct u_entry {
    u_entry_T *next;
    int top;           /* first row of the affected range */
    int old_count;     /* number of rows in old_lines */
    char **old_lines;
    int new_count;     /* number of rows in new_lines */
    char **new_lines;
    int cx_before, cy_before;  /* cursor before the edit */
    int cx_after,  cy_after;   /* cursor after the edit */
};

static u_entry_T *u_undo_stack = NULL;
static u_entry_T *u_redo_stack = NULL;
static int u_undo_len = 0;

/* Pending state held between undoBegin and undoCommit. */
static int    u_pending = 0;
static int    u_p_top = 0;
static int    u_p_numrows = 0;   /* total row count before the edit */
static int    u_p_old_count = 0;
static char **u_p_old_lines = NULL;
static int    u_p_cx = 0, u_p_cy = 0;

static char **snapshot(int top, int count) {
  if (count <= 0) return NULL;
  char **arr = malloc(sizeof(char *) * count);
  for (int i = 0; i < count; i++) {
    erow *r = &CB.row[top + i];
    arr[i] = malloc(r->size + 1);
    memcpy(arr[i], r->chars, r->size);
    arr[i][r->size] = '\0';
  }
  return arr;
}

static void free_lines(char **lines, int count) {
  if (!lines) return;
  for (int i = 0; i < count; i++) free(lines[i]);
  free(lines);
}

static void free_entry(u_entry_T *e) {
  if (!e) return;
  free_lines(e->old_lines, e->old_count);
  free_lines(e->new_lines, e->new_count);
  free(e);
}

static void clear_stack(u_entry_T **head) {
  while (*head) {
    u_entry_T *e = *head;
    *head = e->next;
    free_entry(e);
  }
}

static int lines_equal(char **a, int ac, char **b, int bc) {
  if (ac != bc) return 0;
  for (int i = 0; i < ac; i++)
    if (strcmp(a[i], b[i]) != 0) return 0;
  return 1;
}

/* Drop oldest entries beyond TX_UNDO_MAX to keep memory bounded. */
static void trim_undo(void) {
  if (u_undo_len <= TX_UNDO_MAX) return;
  u_entry_T *cur = u_undo_stack, *prev = NULL;
  int n = 0;
  while (cur && n < TX_UNDO_MAX) {
    prev = cur;
    cur = cur->next;
    n++;
  }
  if (prev) prev->next = NULL;
  while (cur) {
    u_entry_T *next = cur->next;
    free_entry(cur);
    u_undo_len--;
    cur = next;
  }
}

static void cancel_pending(void) {
  if (!u_pending) return;
  free_lines(u_p_old_lines, u_p_old_count);
  u_p_old_lines = NULL;
  u_p_old_count = 0;
  u_pending = 0;
}

void undoBegin(int top, int count) {
  cancel_pending();

  if (top < 0) top = 0;
  if (top > CB.numrows) top = CB.numrows;
  if (count < 0) count = 0;
  if (top + count > CB.numrows) count = CB.numrows - top;

  u_p_top = top;
  u_p_numrows = CB.numrows;
  u_p_old_count = count;
  u_p_old_lines = snapshot(top, count);
  u_p_cx = CB.cx;
  u_p_cy = CB.cy;
  u_pending = 1;
}

void undoCommit(void) {
  if (!u_pending) return;

  int delta = CB.numrows - u_p_numrows;
  int new_count = u_p_old_count + delta;
  if (new_count < 0) new_count = 0;

  int top = u_p_top;
  if (top > CB.numrows) top = CB.numrows;
  if (top + new_count > CB.numrows) new_count = CB.numrows - top;

  char **new_lines = snapshot(top, new_count);

  if (lines_equal(u_p_old_lines, u_p_old_count, new_lines, new_count) &&
      CB.cx == u_p_cx && CB.cy == u_p_cy) {
    free_lines(new_lines, new_count);
    free_lines(u_p_old_lines, u_p_old_count);
    u_p_old_lines = NULL;
    u_p_old_count = 0;
    u_pending = 0;
    return;
  }

  u_entry_T *e = malloc(sizeof(u_entry_T));
  e->next = u_undo_stack;
  e->top = top;
  e->old_count = u_p_old_count;
  e->old_lines = u_p_old_lines;
  e->new_count = new_count;
  e->new_lines = new_lines;
  e->cx_before = u_p_cx;
  e->cy_before = u_p_cy;
  e->cx_after = CB.cx;
  e->cy_after = CB.cy;

  u_undo_stack = e;
  u_undo_len++;

  clear_stack(&u_redo_stack);
  trim_undo();

  u_p_old_lines = NULL;
  u_p_old_count = 0;
  u_pending = 0;
}

static void clamp_cursor(void) {
  if (CB.cy < 0) CB.cy = 0;
  if (CB.cy > CB.numrows) CB.cy = CB.numrows;
  if (CB.cy < CB.numrows) {
    if (CB.cx > CB.row[CB.cy].size) CB.cx = CB.row[CB.cy].size;
  } else {
    CB.cx = 0;
  }
  if (CB.cx < 0) CB.cx = 0;
}

static void apply_entry(int top, int remove_count,
                        char **insert_lines, int insert_count) {
  for (int i = 0; i < remove_count; i++)
    editorDelRow(top);
  for (int i = 0; i < insert_count; i++) {
    char *s = insert_lines[i];
    editorInsertRow(top + i, s, strlen(s));
  }
}

void undoUndo(void) {
  cancel_pending();
  if (!u_undo_stack) {
    editorSetStatusMessage("Already at oldest change");
    return;
  }
  u_entry_T *e = u_undo_stack;
  u_undo_stack = e->next;
  u_undo_len--;

  apply_entry(e->top, e->new_count, e->old_lines, e->old_count);
  CB.cx = e->cx_before;
  CB.cy = e->cy_before;
  clamp_cursor();

  e->next = u_redo_stack;
  u_redo_stack = e;

  editorSetStatusMessage("Undo");
}

void undoRedo(void) {
  cancel_pending();
  if (!u_redo_stack) {
    editorSetStatusMessage("Already at newest change");
    return;
  }
  u_entry_T *e = u_redo_stack;
  u_redo_stack = e->next;

  apply_entry(e->top, e->old_count, e->new_lines, e->new_count);
  CB.cx = e->cx_after;
  CB.cy = e->cy_after;
  clamp_cursor();

  e->next = u_undo_stack;
  u_undo_stack = e;
  u_undo_len++;

  editorSetStatusMessage("Redo");
}
