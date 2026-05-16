#ifndef TERMINAL_H
#define TERMINAL_H

int  editorReadKey(void);
int  getWindowSize(int *rows, int *cols);
void die(const char *s);
void disableRawMode(void);
void enableRawMode(void);

#endif
