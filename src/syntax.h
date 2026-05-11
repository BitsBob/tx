#ifndef SYNTAX_H
#define SYNTAX_H

#include "tx.h"


static char *C_HL_extensions[] = {".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", NULL};
static char *C_HL_keywords[] = {
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

static char *PY_HL_extensions[] = {".py", ".pyw", NULL};
static char *PY_HL_keywords[] = {
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

static char *JS_HL_extensions[] = {".js", ".jsx", ".ts", ".tsx", ".mjs", NULL};
static char *JS_HL_keywords[] = {
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

static char *SH_HL_extensions[] = {".sh", ".bash", ".zsh", NULL};
static char *SH_HL_keywords[] = {
    "if",       "then",     "else",     "elif",     "fi",       "for",
    "while",    "do",       "done",     "case",     "esac",     "in",
    "function", "return",   "exit",     "local",    "export",   "readonly",
    "shift",    "break",    "continue", "select",   "until",    "trap",

    "true|",    "false|",   NULL
};

static char *RS_HL_extensions[] = {".rs", NULL};
static char *RS_HL_keywords[] = {
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

static char *GO_HL_extensions[] = {".go", NULL};
static char *GO_HL_keywords[] = {
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

static char *MAKE_HL_extensions[] = {"Makefile", "makefile", ".mk", NULL};
static char *MAKE_HL_keywords[] = {
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

#endif