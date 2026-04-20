#include "tx.h"

static void clearScreenAndExit(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  exit(0);
}

void editorProcessCommandMode() {
  char *cmd = editorPrompt(":%s", NULL);

  if (cmd == NULL)
    return;

  if (strcmp(cmd, "q") == 0) {
    if (E.dirty) {
      editorSetStatusMessage("No write since last change (add ! to override)");
    } else {
      clearScreenAndExit();
    }
  } else if (strcmp(cmd, "q!") == 0) {
    clearScreenAndExit();
  } else if (strcmp(cmd, "w") == 0) {
    disableRawMode();
    editorSave();
  } else if (strcmp(cmd, "wq") == 0) {
    editorSave();
    exit(0);
  } else if (strcmp(cmd, "f") == 0 || strcmp(cmd, "find") == 0) {
    editorFind();
  } else {
    editorSetStatusMessage("Unknown command: %s", cmd);
  }

  free(cmd);
}
