#include "tx.h"

struct editorConfig E = {};

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
  E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetFortuneStatusMessage();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
