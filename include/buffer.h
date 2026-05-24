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
    int readonly;
    int *diags;       /* severity per row: 0=none 1=error 2=warn 3=info 4=hint */
    int  diags_count; /* length of diags array (== numrows at last update)     */
    int  lsp_version; /* incremented on didOpen/didChange                      */
} Buffer;

void bufferClose(int idx);
void bufferFree(int idx);
void bufferOpen(char *filename);
void bufferSwitch(int idx);
int  bufferNew(void);

#endif
