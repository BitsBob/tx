#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "syntax.h"

/* Keywords ending with '|' are highlighted as secondary keywords (types).
 * The trailing pipe is stripped before matching; it is just a marker. */
static const char *C_HL_extensions[] = {".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", NULL};
static const char *C_HL_keywords[] = {
    "switch",   "if",       "while",    "for",      "break",    "continue",
    "return",   "else",     "struct",   "union",    "typedef",  "static",
    "enum",     "class",    "case",     "sizeof",   "volatile", "extern",
    "do",       "goto",     "default",  "inline",   "restrict", "register",
    "nullptr",  "template", "typename", "namespace","public",   "private",
    "protected","virtual",  "override", "new",      "delete",   "try",
    "catch",    "throw",

    "int|",     "long|",    "double|",  "float|",   "char|",    "unsigned|",
    "signed|",  "void|",    "short|",   "auto|",    "const|",   "bool|",
    "size_t|",  "ssize_t|", "uint8_t|", "uint16_t|","uint32_t|","uint64_t|",
    "int8_t|",  "int16_t|", "int32_t|", "int64_t|", "wchar_t|", NULL
};

static const char *PY_HL_extensions[] = {".py", ".pyw", NULL};
static const char *PY_HL_keywords[] = {
    "and",      "as",       "assert",   "async",    "await",    "break",
    "class",    "continue", "def",      "del",      "elif",     "else",
    "except",   "finally",  "for",      "from",     "global",   "if",
    "import",   "in",       "is",       "lambda",   "nonlocal", "not",
    "or",       "pass",     "raise",    "return",   "try",      "while",
    "with",     "yield",    "match",    "case",

    "True|",    "False|",   "None|",    "int|",     "str|",     "float|",
    "bool|",    "list|",    "dict|",    "tuple|",   "set|",     "bytes|",
    "type|",    "object|",  NULL
};

static const char *JS_HL_extensions[] = {".js", ".jsx", ".ts", ".tsx", ".mjs", NULL};
static const char *JS_HL_keywords[] = {
    "break",    "case",     "catch",    "class",    "const",    "continue",
    "debugger", "default",  "delete",   "do",       "else",     "export",
    "extends",  "finally",  "for",      "function", "if",       "import",
    "in",       "instanceof","let",     "new",      "of",       "return",
    "static",   "super",    "switch",   "this",     "throw",    "try",
    "typeof",   "var",      "void",     "while",    "with",     "yield",
    "async",    "await",    "from",     "as",

    "true|",    "false|",   "null|",    "undefined|","number|", "string|",
    "boolean|", "symbol|",  "object|",  "any|",     "void|",    "never|",
    "enum|",    "interface|","type|",   NULL
};

static const char *SH_HL_extensions[] = {".sh", ".bash", ".zsh", NULL};
static const char *SH_HL_keywords[] = {
    "if",       "then",     "else",     "elif",     "fi",       "for",
    "while",    "do",       "done",     "case",     "esac",     "in",
    "function", "return",   "exit",     "local",    "export",   "readonly",
    "shift",    "break",    "continue", "select",   "until",    "trap",

    "true|",    "false|",   NULL
};

static const char *RS_HL_extensions[] = {".rs", NULL};
static const char *RS_HL_keywords[] = {
    "as",       "break",    "const",    "continue", "crate",    "else",
    "enum",     "extern",   "fn",       "for",      "if",       "impl",
    "in",       "let",      "loop",     "match",    "mod",      "move",
    "mut",      "pub",      "ref",      "return",   "self",     "Self",
    "static",   "struct",   "super",    "trait",    "type",     "unsafe",
    "use",      "where",    "while",    "async",    "await",    "dyn",

    "i8|",      "i16|",     "i32|",     "i64|",     "i128|",    "isize|",
    "u8|",      "u16|",     "u32|",     "u64|",     "u128|",    "usize|",
    "f32|",     "f64|",     "bool|",    "char|",    "str|",     "String|",
    "Vec|",     "Option|",  "Result|",  "Box|",     "Rc|",      "Arc|",
    "true|",    "false|",   NULL
};

static const char *GO_HL_extensions[] = {".go", NULL};
static const char *GO_HL_keywords[] = {
    "break",    "case",     "chan",      "const",    "continue", "default",
    "defer",    "else",     "fallthrough","for",     "func",     "go",
    "goto",     "if",       "import",   "interface","map",      "package",
    "range",    "return",   "select",   "struct",   "switch",   "type",
    "var",

    "int|",     "int8|",    "int16|",   "int32|",   "int64|",   "uint|",
    "uint8|",   "uint16|",  "uint32|",  "uint64|",  "uintptr|", "float32|",
    "float64|", "complex64|","complex128|","bool|",  "byte|",    "rune|",
    "string|",  "error|",   "true|",    "false|",   "nil|",     NULL
};

static const char *MAKE_HL_extensions[] = {"Makefile", "makefile", ".mk", NULL};
static const char *MAKE_HL_keywords[] = {
    "ifeq",     "ifneq",    "ifdef",    "ifndef",   "else",     "endif",
    "define",   "endef",    "include",  "override", "export",   "unexport",
    NULL
};

static struct editorSyntax HLDB[] = {
    { "c",          C_HL_extensions,    C_HL_keywords,
      "//", "/*", "*/",   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "python",     PY_HL_extensions,   PY_HL_keywords,
      "#",  NULL, NULL,   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "javascript", JS_HL_extensions,   JS_HL_keywords,
      "//", "/*", "*/",   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "shell",      SH_HL_extensions,   SH_HL_keywords,
      "#",  NULL, NULL,   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "rust",       RS_HL_extensions,   RS_HL_keywords,
      "//", "/*", "*/",   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "go",         GO_HL_extensions,   GO_HL_keywords,
      "//", "/*", "*/",   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },

    { "make",       MAKE_HL_extensions, MAKE_HL_keywords,
      "#",  NULL, NULL,   0 },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

static int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if (CB.syntax == NULL) return;

    const char **keywords = CB.syntax->keywords;
    const char *scs = CB.syntax->singleline_comment_start;
    const char *mcs = CB.syntax->multiline_comment_start;
    const char *mce = CB.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep   = 1;
    int in_string  = 0;
    int in_comment = (row->idx > 0 && CB.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else { i++; continue; }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (CB.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else if (c == '"' || c == '\'') {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }

        if (CB.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2  = keywords[j][klen - 1] == '|';
                if (kw2) klen--;

                if (!strncmp(&row->render[i], keywords[j], klen) &&
                    is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) { prev_sep = 0; continue; }
        }

        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < CB.numrows)
        editorUpdateSyntax(&CB.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT: return 36;
        case HL_KEYWORD1:  return 32;
        case HL_KEYWORD2:  return 33;
        case HL_STRING:    return 35;
        case HL_NUMBER:    return 31;
        case HL_MATCH:     return 34;
        default:           return 37;
    }
}

void editorSelectSyntaxHighlight(void) {
    CB.syntax = NULL;
    if (CB.filename == NULL) return;

    char *ext = strrchr(CB.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(CB.filename, s->filematch[i]))) {
                CB.syntax = s;
                for (int filerow = 0; filerow < CB.numrows; filerow++)
                    editorUpdateSyntax(&CB.row[filerow]);
                return;
            }
            i++;
        }
    }
}
