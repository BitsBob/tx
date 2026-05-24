#ifndef ROW_H
#define ROW_H

typedef struct erow {
    int idx;             /* row's position in the file, used by syntax highlight */
    int size;            /* length of chars */
    int rsize;           /* length of render (tabs expanded) */
    char *chars;         /* raw file bytes */
    char *render;        /* display bytes, tabs replaced with spaces */
    unsigned char *hl;   /* highlight type per render byte, see editorHighlight */
    int hl_open_comment; /* 1 if this row ends inside a multi-line comment */
} erow;

#endif
