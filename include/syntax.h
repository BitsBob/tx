#ifndef SYNTAX_H
#define SYNTAX_H

#include "row.h"

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
};

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

struct editorSyntax {
    const char *filetype;
    const char **filematch;
    const char **keywords;
    const char *singleline_comment_start;
    const char *multiline_comment_start;
    const char *multiline_comment_end;
    int flags;
};

void editorUpdateSyntax(erow *row);
int  editorSyntaxToColor(int hl);
void editorSelectSyntaxHighlight(void);

#endif
