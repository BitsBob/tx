#ifndef BUFFER_H
#define BUFFER_H

#include "row.h"

struct editorSyntax;

typedef struct {
    char *filename;
    erow *row;
    int numrows;
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int dirty;
    struct editorSyntax *syntax;
} Buffer;

void bufferClose(int idx);
void bufferFree(int idx);
void bufferOpen(char *filename);
void bufferSwitch(int idx);
int  bufferNew(void);

#endif
