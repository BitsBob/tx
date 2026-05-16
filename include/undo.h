#ifndef UNDO_H
#define UNDO_H

void undoBegin(int top, int count);
void undoCommit(void);
void undoUndo(void);
void undoRedo(void);

#endif
