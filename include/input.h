#ifndef INPUT_H
#define INPUT_H

void  editorProcessKeypress(void);
void  editorProcessNormalMode(int c);
void  editorProcessInsertMode(int c);
void  editorMoveCursor(int key);
void  editorJumpToTop(void);
void  editorJumpToEnd(void);
char *editorPrompt(char *prompt, void (*callback)(char *, int));

#endif
