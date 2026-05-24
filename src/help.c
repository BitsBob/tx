#include "tx.h"
#include "buffer.h"
#include "editor.h"
#include "help.h"
#include "output.h"

typedef struct {
    const char *topic;
    const char * const *lines;
} HelpEntry;

/* Index shown for bare :h */
static const char * const help_index[] = {
    "  tx " TX_VERSION " - help",
    "",
    "  Use :h <topic> to open a help page for any key, command, or setting.",
    "  Press :bd to close this buffer.",
    "",
    "  MOTION",
    "    hjkl         cursor movement (arrows also work)   :h hjkl",
    "    w  b         word forward / backward              :h w",
    "    gg  G        jump to top / bottom of file         :h gg",
    "",
    "  OPERATORS  (combine with a motion or double to act on the line)",
    "    d            delete operator                      :h d",
    "    y            yank operator                        :h y",
    "    c            change operator                      :h c",
    "    x            delete character under cursor        :h x",
    "    p            paste yanked text                    :h p",
    "",
    "  MODE SWITCHES",
    "    i            enter insert mode before cursor      :h i",
    "    v  V         visual / visual-line mode            :h v",
    "    :            enter command mode                   :h :",
    "",
    "  UNDO",
    "    u            undo                                 :h u",
    "    Ctrl-r       redo                                 :h u",
    "",
    "  SEARCH",
    "    /            interactive search                   :h /",
    "",
    "  COMMANDS",
    "    :w  :q  :wq  write / quit / both                 :h :w",
    "    :q!          force quit without saving            :h :q!",
    "    :e <file>    open file in new buffer              :h :e",
    "    :bn  :bp     next / previous buffer               :h :bn",
    "    :bd  :bd!    close buffer / force close           :h :bd",
    "    :ls          list open buffers                    :h :ls",
    "    :h <topic>   this help                            :h :h",
    "",
    "  SETTINGS  (~/.txrc, syntax: set <key> <value>)",
    "    tabstop      undo_max     numbers                 :h tabstop",
    "    syntax       autopair     lsp_enable              :h syntax",
    "    lsp_cmd      search_sensitive  search_highlight   :h lsp_cmd",
    "    status_fortune                                    :h status_fortune",
    "",
    NULL
};

/* hjkl */
static const char * const help_hjkl[] = {
    "  hjkl - cursor movement",
    "",
    "  SYNOPSIS",
    "    h    move left one character",
    "    j    move down one line",
    "    k    move up one line",
    "    l    move right one character",
    "",
    "  DESCRIPTION",
    "    Move the cursor in normal and visual mode.",
    "    Arrow keys work in every mode.",
    "    The cursor will not move past the end of a line or past the",
    "    last line of the buffer.",
    "",
    "  SEE ALSO",
    "    :h w   :h gg",
    "",
    NULL
};

/* w - word forward */
static const char * const help_w[] = {
    "  w - move to start of next word",
    "",
    "  SYNOPSIS",
    "    w",
    "",
    "  DESCRIPTION",
    "    Move the cursor forward to the beginning of the next word.",
    "    Words are separated by whitespace.",
    "    Movement is limited to the current line.",
    "",
    "  SEE ALSO",
    "    :h b   :h hjkl",
    "",
    NULL
};

/* b - word backward */
static const char * const help_b[] = {
    "  b - move to start of previous word",
    "",
    "  SYNOPSIS",
    "    b",
    "",
    "  DESCRIPTION",
    "    Move the cursor backward to the beginning of the previous word.",
    "    Words are separated by whitespace.",
    "    Movement is limited to the current line.",
    "",
    "  SEE ALSO",
    "    :h w   :h hjkl",
    "",
    NULL
};

/* gg / G */
static const char * const help_gg[] = {
    "  gg / G - jump to top or bottom of file",
    "",
    "  SYNOPSIS",
    "    gg   jump to the first line",
    "    G    jump to the last line",
    "",
    "  DESCRIPTION",
    "    gg moves the cursor to line 1, column 0.",
    "    G  moves the cursor to the last line, column 0.",
    "",
    "  SEE ALSO",
    "    :h hjkl",
    "",
    NULL
};

/* i - insert mode */
static const char * const help_i[] = {
    "  i - enter insert mode",
    "",
    "  SYNOPSIS",
    "    i",
    "",
    "  DESCRIPTION",
    "    Switch from normal mode to insert mode, placing the cursor",
    "    before the character it was on.",
    "",
    "    In insert mode:",
    "      Enter        insert a newline (with auto-indent)",
    "      Backspace    delete the character before the cursor",
    "      Ctrl-s       save the file",
    "      Ctrl-l       return to normal mode",
    "      Esc          return to normal mode",
    "",
    "    If autopair is enabled, typing {, (, [, \" or ' will",
    "    automatically insert the matching closing character.",
    "",
    "  SEE ALSO",
    "    :h autopair   :h v",
    "",
    NULL
};

/* v / V - visual mode */
static const char * const help_v[] = {
    "  v / V - visual and visual-line mode",
    "",
    "  SYNOPSIS",
    "    v    enter character-wise visual mode",
    "    V    enter line-wise visual mode",
    "",
    "  DESCRIPTION",
    "    Visual mode lets you select a region of text then act on it.",
    "    Move the cursor to extend the selection.",
    "",
    "    Available actions on a selection:",
    "      y    yank the selected text",
    "      d    delete the selected text",
    "      x    delete the selected text",
    "      p    replace the selection with the yank buffer",
    "      G    extend selection to end of file",
    "      Esc  cancel and return to normal mode",
    "",
    "    In visual-line mode (V), whole lines are always selected.",
    "",
    "  SEE ALSO",
    "    :h y   :h d   :h p",
    "",
    NULL
};

/* x */
static const char * const help_x[] = {
    "  x - delete character under cursor",
    "",
    "  SYNOPSIS",
    "    x",
    "",
    "  DESCRIPTION",
    "    Delete the character the cursor is sitting on.",
    "    The deleted character is not placed in the yank buffer.",
    "    The deletion can be undone with u.",
    "",
    "  SEE ALSO",
    "    :h d   :h u",
    "",
    NULL
};

/* d - delete operator */
static const char * const help_d[] = {
    "  d - delete operator",
    "",
    "  SYNOPSIS",
    "    dd      delete current line",
    "    dw      delete from cursor to start of next word",
    "",
    "  DESCRIPTION",
    "    d is an operator that waits for a second key to determine",
    "    what to delete.",
    "",
    "    dd   deletes the entire current line and places it in the",
    "         yank buffer. Can be pasted with p.",
    "",
    "    dw   deletes from the cursor position to the start of the",
    "         next word on the same line.",
    "",
    "    Deletions can be undone with u.",
    "",
    "  SEE ALSO",
    "    :h y   :h p   :h x   :h u",
    "",
    NULL
};

/* y - yank operator */
static const char * const help_y[] = {
    "  y - yank (copy) operator",
    "",
    "  SYNOPSIS",
    "    yy      yank current line",
    "",
    "  DESCRIPTION",
    "    yy copies the current line into the yank buffer without",
    "    deleting it. The yanked line can then be pasted with p.",
    "",
    "    In visual mode, y yanks the selected text.",
    "    In visual-line mode, y yanks the selected lines.",
    "",
    "  SEE ALSO",
    "    :h d   :h p   :h v",
    "",
    NULL
};

/* c - change operator */
static const char * const help_c[] = {
    "  c - change operator",
    "",
    "  SYNOPSIS",
    "    cc      delete current line and enter insert mode",
    "    cw      delete word and enter insert mode",
    "",
    "  DESCRIPTION",
    "    c is like d but switches to insert mode after deleting,",
    "    so you can immediately type replacement text.",
    "",
    "    cc   clears the current line and enters insert mode.",
    "    cw   deletes from the cursor to the next word boundary",
    "         and enters insert mode.",
    "",
    "  SEE ALSO",
    "    :h d   :h i",
    "",
    NULL
};

/* p - paste */
static const char * const help_p[] = {
    "  p - paste",
    "",
    "  SYNOPSIS",
    "    p",
    "",
    "  DESCRIPTION",
    "    Paste the contents of the yank buffer at the current cursor",
    "    position.",
    "",
    "    If the yank buffer holds a full line (yanked with yy or dd),",
    "    it is inserted as a new line below the current one.",
    "    Otherwise the text is inserted inline at the cursor.",
    "",
    "    p can be used in visual and visual-line mode to replace",
    "    the selected text with the yank buffer.",
    "",
    "  SEE ALSO",
    "    :h y   :h d",
    "",
    NULL
};

/* u - undo/redo */
static const char * const help_u[] = {
    "  u / Ctrl-r - undo and redo",
    "",
    "  SYNOPSIS",
    "    u        undo the last change",
    "    Ctrl-r   redo the last undone change",
    "",
    "  DESCRIPTION",
    "    tx keeps a snapshot-based undo history.",
    "    Each discrete edit (insert char, delete line, paste, etc.)",
    "    is a single undo step.",
    "",
    "    The maximum number of undo steps is controlled by the",
    "    undo_max setting (default 100).",
    "",
    "  SEE ALSO",
    "    :h undo_max",
    "",
    NULL
};

/* / - search */
static const char * const help_search[] = {
    "  / - interactive search",
    "",
    "  SYNOPSIS",
    "    /",
    "",
    "  DESCRIPTION",
    "    Opens a search prompt at the bottom of the screen.",
    "    Results highlight as you type.",
    "",
    "    While the prompt is open:",
    "      Arrow Down / Arrow Right   jump to next match",
    "      Arrow Up / Arrow Left      jump to previous match",
    "      Enter                      confirm and stay at match",
    "      Esc                        cancel and return to original position",
    "",
    "  SEE ALSO",
    "    :h search_sensitive   :h search_highlight",
    "",
    NULL
};

/* : */
static const char * const help_colon[] = {
    "  : - command mode",
    "",
    "  SYNOPSIS",
    "    :",
    "",
    "  DESCRIPTION",
    "    Opens the command prompt at the bottom of the screen.",
    "    Type a command and press Enter to execute it.",
    "    Press Esc to cancel.",
    "",
    "    Available commands:",
    "      :w          write the current buffer to disk",
    "      :q          quit (fails if unsaved changes)",
    "      :q!         force quit, discarding changes",
    "      :wq         write then quit",
    "      :e <file>   open file in a new buffer",
    "      :bn         switch to next buffer",
    "      :bp         switch to previous buffer",
    "      :bd         close current buffer",
    "      :bd!        force close current buffer",
    "      :ls         list open buffers",
    "      :h <topic>  open help",
    "",
    "  SEE ALSO",
    "    :h :w   :h :e   :h :bn   :h :ls",
    "",
    NULL
};

/* :w */
static const char * const help_write[] = {
    "  :w - write buffer to disk",
    "",
    "  SYNOPSIS",
    "    :w",
    "",
    "  DESCRIPTION",
    "    Save the current buffer to its file.",
    "    If the buffer has no filename (opened as [No Name]),",
    "    you will be prompted to enter one.",
    "",
    "    :w also triggers an LSP didSave notification if LSP is active.",
    "",
    "  SEE ALSO",
    "    :h :wq   :h :q   :h lsp_enable",
    "",
    NULL
};

/* :q */
static const char * const help_quit[] = {
    "  :q / :q! / :wq - quit commands",
    "",
    "  SYNOPSIS",
    "    :q     quit the current buffer (fails if unsaved changes exist)",
    "    :q!    force quit, discarding any unsaved changes",
    "    :wq    write the buffer then quit",
    "",
    "  DESCRIPTION",
    "    If multiple buffers are open, these commands close the current",
    "    buffer rather than exiting tx. tx exits when the last buffer",
    "    is closed.",
    "",
    "  SEE ALSO",
    "    :h :w   :h :bd",
    "",
    NULL
};

/* :e */
static const char * const help_edit[] = {
    "  :e - open file",
    "",
    "  SYNOPSIS",
    "    :e <filename>",
    "",
    "  DESCRIPTION",
    "    Open the given file in a new buffer and switch to it.",
    "    If the file is already open in another buffer, tx switches",
    "    to that buffer instead of opening a duplicate.",
    "",
    "  SEE ALSO",
    "    :h :bn   :h :ls   :h :bd",
    "",
    NULL
};

/* :bn / :bp */
static const char * const help_bn[] = {
    "  :bn / :bp - switch buffers",
    "",
    "  SYNOPSIS",
    "    :bn    switch to the next buffer",
    "    :bp    switch to the previous buffer",
    "",
    "  DESCRIPTION",
    "    Cycle through open buffers in order.",
    "    The buffer list wraps around at both ends.",
    "",
    "  SEE ALSO",
    "    :h :ls   :h :e   :h :bd",
    "",
    NULL
};

/* :bd */
static const char * const help_bd[] = {
    "  :bd / :bd! - close buffer",
    "",
    "  SYNOPSIS",
    "    :bd    close the current buffer (fails if unsaved changes)",
    "    :bd!   force close the current buffer",
    "",
    "  DESCRIPTION",
    "    Remove the current buffer from the buffer list.",
    "    If it is the last buffer, tx exits.",
    "",
    "  SEE ALSO",
    "    :h :q   :h :ls",
    "",
    NULL
};

/* :ls */
static const char * const help_ls[] = {
    "  :ls - list open buffers",
    "",
    "  SYNOPSIS",
    "    :ls",
    "",
    "  DESCRIPTION",
    "    Display a brief list of all open buffers in the status bar.",
    "    The current buffer is shown in brackets, e.g. [1:filename].",
    "",
    "  SEE ALSO",
    "    :h :bn   :h :e",
    "",
    NULL
};

/* :h */
static const char * const help_help[] = {
    "  :h - help",
    "",
    "  SYNOPSIS",
    "    :h",
    "    :h <topic>",
    "",
    "  DESCRIPTION",
    "    Open a help buffer for the given topic.",
    "    With no argument, shows the main index.",
    "",
    "    Topics can be keys (w, b, gg, v, ...),",
    "    commands (:w, :q, :e, :bn, :bd, :ls, :h),",
    "    or settings (tabstop, syntax, numbers, ...).",
    "",
    "    The help buffer is read-only. Close it with :bd.",
    "",
    NULL
};

/* settings index */
static const char * const help_settings[] = {
    "  settings - ~/.txrc configuration",
    "",
    "  DESCRIPTION",
    "    tx reads ~/.txrc on startup.",
    "    Each line has the form:  set <key> <value>",
    "    Blank lines and lines starting with # are ignored.",
    "",
    "  KEYS",
    "    tabstop          number of spaces per tab stop      :h tabstop",
    "    undo_max         maximum undo history depth         :h undo_max",
    "    numbers          show line numbers                  :h numbers",
    "    syntax           enable syntax highlighting         :h syntax",
    "    autopair         auto-close brackets and quotes     :h autopair",
    "    search_sensitive case-sensitive search              :h search_sensitive",
    "    search_highlight highlight search matches           :h search_highlight",
    "    status_fortune   show a fortune in the status bar   :h status_fortune",
    "    lsp_enable       enable LSP integration             :h lsp_enable",
    "    lsp_cmd          LSP server command                 :h lsp_cmd",
    "",
    "  EXAMPLE  (~/.txrc)",
    "    set tabstop 2",
    "    set numbers true",
    "    set syntax true",
    "    set autopair true",
    "    set lsp_enable true",
    "    set lsp_cmd clangd",
    "",
    NULL
};

/* tabstop */
static const char * const help_tabstop[] = {
    "  tabstop - tab stop width",
    "",
    "  SYNOPSIS",
    "    set tabstop <number>",
    "",
    "  DEFAULT",
    "    4",
    "",
    "  DESCRIPTION",
    "    Number of spaces a tab character renders as.",
    "    Also controls indentation added on newline inside blocks.",
    "",
    "  SEE ALSO",
    "    :h settings",
    "",
    NULL
};

/* undo_max */
static const char * const help_undo_max[] = {
    "  undo_max - undo history depth",
    "",
    "  SYNOPSIS",
    "    set undo_max <number>",
    "",
    "  DEFAULT",
    "    100",
    "",
    "  DESCRIPTION",
    "    Maximum number of undo steps retained per buffer.",
    "    Older steps are discarded when the limit is reached.",
    "    The compile-time ceiling is TX_UNDO_MAX (200).",
    "",
    "  SEE ALSO",
    "    :h u   :h settings",
    "",
    NULL
};

/* numbers */
static const char * const help_numbers[] = {
    "  numbers - line numbers",
    "",
    "  SYNOPSIS",
    "    set numbers true|false",
    "",
    "  DEFAULT",
    "    false",
    "",
    "  DESCRIPTION",
    "    When true, display line numbers in the left gutter.",
    "    Line numbers are shown after the LSP diagnostic marker column.",
    "",
    "  SEE ALSO",
    "    :h lsp_enable   :h settings",
    "",
    NULL
};

/* syntax */
static const char * const help_syntax[] = {
    "  syntax - syntax highlighting",
    "",
    "  SYNOPSIS",
    "    set syntax true|false",
    "",
    "  DEFAULT",
    "    false",
    "",
    "  DESCRIPTION",
    "    When true, tx highlights keywords, strings, comments, and",
    "    numbers for recognised file types (C, C++).",
    "    Highlighting is updated whenever a row is modified.",
    "",
    "  SEE ALSO",
    "    :h settings",
    "",
    NULL
};

/* autopair */
static const char * const help_autopair[] = {
    "  autopair - automatic bracket and quote pairing",
    "",
    "  SYNOPSIS",
    "    set autopair true|false",
    "",
    "  DEFAULT",
    "    true",
    "",
    "  DESCRIPTION",
    "    When true, typing an opening bracket or quote in insert mode",
    "    will automatically insert the matching closing character and",
    "    leave the cursor between them.",
    "",
    "    Pairs:  {  }   (  )   [  ]   \"  \"   '  '",
    "",
    "  SEE ALSO",
    "    :h i   :h settings",
    "",
    NULL
};

/* search_sensitive */
static const char * const help_search_sensitive[] = {
    "  search_sensitive - case-sensitive search",
    "",
    "  SYNOPSIS",
    "    set search_sensitive true|false",
    "",
    "  DEFAULT",
    "    false",
    "",
    "  DESCRIPTION",
    "    When true, the / search treats uppercase and lowercase letters",
    "    as distinct. When false, search is case-insensitive.",
    "",
    "  SEE ALSO",
    "    :h /   :h search_highlight   :h settings",
    "",
    NULL
};

/* search_highlight */
static const char * const help_search_highlight[] = {
    "  search_highlight - highlight search matches",
    "",
    "  SYNOPSIS",
    "    set search_highlight true|false",
    "",
    "  DEFAULT",
    "    true",
    "",
    "  DESCRIPTION",
    "    When true, matching text is highlighted while the search",
    "    prompt is open.",
    "",
    "  SEE ALSO",
    "    :h /   :h search_sensitive   :h settings",
    "",
    NULL
};

/* status_fortune */
static const char * const help_status_fortune[] = {
    "  status_fortune - fortune in the status bar",
    "",
    "  SYNOPSIS",
    "    set status_fortune true|false",
    "",
    "  DEFAULT",
    "    true",
    "",
    "  DESCRIPTION",
    "    When true, the status bar shows a random message when no",
    "    other status message is active.",
    "",
    "  SEE ALSO",
    "    :h settings",
    "",
    NULL
};

/* lsp_enable */
static const char * const help_lsp_enable[] = {
    "  lsp_enable - enable LSP integration",
    "",
    "  SYNOPSIS",
    "    set lsp_enable true|false",
    "",
    "  DEFAULT",
    "    false",
    "",
    "  DESCRIPTION",
    "    When true, tx starts the language server specified by lsp_cmd",
    "    and displays diagnostic markers (E W I H) in the gutter.",
    "",
    "    Diagnostics are updated each time the file is saved.",
    "    The server is started once at launch and stopped on exit.",
    "",
    "    Gutter markers:",
    "      E   error    (red)",
    "      W   warning  (yellow)",
    "      I   info     (cyan)",
    "      H   hint     (dim)",
    "",
    "  SEE ALSO",
    "    :h lsp_cmd   :h :w   :h settings",
    "",
    NULL
};

/* lsp_cmd */
static const char * const help_lsp_cmd[] = {
    "  lsp_cmd - language server command",
    "",
    "  SYNOPSIS",
    "    set lsp_cmd <command>",
    "",
    "  DEFAULT",
    "    clangd",
    "",
    "  DESCRIPTION",
    "    The command used to start the language server.",
    "    Arguments can be included, separated by spaces.",
    "    The server must speak LSP over stdin/stdout.",
    "",
    "    Examples:",
    "      set lsp_cmd clangd",
    "      set lsp_cmd rust-analyzer",
    "      set lsp_cmd pylsp",
    "",
    "  SEE ALSO",
    "    :h lsp_enable   :h settings",
    "",
    NULL
};

static const HelpEntry entries[] = {
    { "",                  help_index           },
    { "index",             help_index           },
    { "hjkl",              help_hjkl            },
    { "h",                 help_hjkl            },
    { "j",                 help_hjkl            },
    { "k",                 help_hjkl            },
    { "l",                 help_hjkl            },
    { "w",                 help_w               },
    { "b",                 help_b               },
    { "gg",                help_gg              },
    { "G",                 help_gg              },
    { "i",                 help_i               },
    { "v",                 help_v               },
    { "V",                 help_v               },
    { "x",                 help_x               },
    { "d",                 help_d               },
    { "dd",                help_d               },
    { "dw",                help_d               },
    { "y",                 help_y               },
    { "yy",                help_y               },
    { "c",                 help_c               },
    { "cc",                help_c               },
    { "cw",                help_c               },
    { "p",                 help_p               },
    { "u",                 help_u               },
    { "/",                 help_search          },
    { ":",                 help_colon           },
    { ":w",                help_write           },
    { ":q",                help_quit            },
    { ":q!",               help_quit            },
    { ":wq",               help_quit            },
    { ":e",                help_edit            },
    { ":bn",               help_bn              },
    { ":bp",               help_bn              },
    { ":bd",               help_bd              },
    { ":bd!",              help_bd              },
    { ":ls",               help_ls              },
    { ":h",                help_help            },
    { "settings",          help_settings        },
    { "tabstop",           help_tabstop         },
    { "tab_stop",          help_tabstop         },
    { "undo_max",          help_undo_max        },
    { "numbers",           help_numbers         },
    { "syntax",            help_syntax          },
    { "autopair",          help_autopair        },
    { "search_sensitive",  help_search_sensitive },
    { "search_highlight",  help_search_highlight },
    { "status_fortune",    help_status_fortune  },
    { "lsp_enable",        help_lsp_enable      },
    { "lsp_cmd",           help_lsp_cmd         },
    { NULL, NULL }
};

void editorOpenHelp(const char *topic) {
    if (!topic) topic = "";

    /* If a help buffer for this topic is already open, switch to it. */
    char title[80];
    snprintf(title, sizeof(title), "[Help: %s]", topic[0] ? topic : "index");

    for (int i = 0; i < E.buf_count; i++) {
        if (E.buffers[i].filename && strcmp(E.buffers[i].filename, title) == 0) {
            bufferSwitch(i);
            return;
        }
    }

    /* Find the matching topic. */
    const char * const *lines = NULL;
    for (int i = 0; entries[i].topic != NULL; i++) {
        if (strcmp(entries[i].topic, topic) == 0) {
            lines = entries[i].lines;
            break;
        }
    }

    if (!lines) {
        editorSetStatusMessage("No help for '%s'  (try :h index)", topic);
        return;
    }

    int idx = bufferNew();
    bufferSwitch(idx);

    CB.filename = strdup(title);
    CB.readonly = 1;

    for (int i = 0; lines[i] != NULL; i++)
        editorInsertRow(CB.numrows, (char *)lines[i], strlen(lines[i]));

    CB.dirty = 0;
    CB.cx    = 0;
    CB.cy    = 0;
}
