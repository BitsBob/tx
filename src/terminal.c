#include "tx.h"
#include "editor.h"
#include "terminal.h"

/* Clear the screen and exit with an error message. */
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

/* Restore the terminal to its original state. Registered with atexit. */
void disableRawMode() {
    write(STDOUT_FILENO, "\x1b[m", 3);    /* reset colours */
    write(STDOUT_FILENO, "\x1b[0 q", 5);  /* restore cursor shape */
    write(STDOUT_FILENO, "\x1b[?25h", 6); /* show cursor */
    write(STDOUT_FILENO, "\x1b[2J", 4);   /* clear screen */
    write(STDOUT_FILENO, "\x1b[H", 3);    /* move to top-left */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/* Put the terminal in raw mode: no line buffering, no echo, no signal keys,
 * and a short read timeout so the main loop can poll without blocking. */
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 0;  /* return as soon as any bytes are available */
    raw.c_cc[VTIME] = 1;  /* or after 100ms with nothing */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

/* Read one keypress and return it as an int. Escape sequences (arrow keys,
 * page up/down, home, end, delete) are collapsed into the editorKey enum
 * values defined in editor.h. Plain bytes are returned as-is. */
int editorReadKey() {
    int  nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c != '\x1b') return c;

    /* It is an escape sequence. Read the next two bytes. */
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            /* Three-byte sequence like \x1b[5~ (page up). */
            if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
            if (seq[2] == '~') {
                switch (seq[1]) {
                case '1': return HOME_KEY;
                case '3': return DEL_KEY;
                case '4': return END_KEY;
                case '5': return PAGE_UP;
                case '6': return PAGE_DOWN;
                case '7': return HOME_KEY;
                case '8': return END_KEY;
                }
            }
        } else {
            /* Two-byte sequence like \x1b[A (arrow up). */
            switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        }
    } else if (seq[0] == '0') {
        switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
        }
    }

    return '\x1b';
}

/* Ask the terminal where the cursor is by sending a DSR (device status report)
 * and reading the CPR (cursor position report) response. */
static int getCursorPosition(int *rows, int *cols) {
    char     buf[32];
    unsigned i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

/* Query terminal dimensions via ioctl. Falls back to moving the cursor to
 * an extreme position and reading where it actually lands. */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}
