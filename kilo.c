/*** includes ***/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios og_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s)
{
    // Keep this above error printing line, as this clears the screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.og_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(void)
{
    if (tcgetattr(STDIN_FILENO, &E.og_termios))
        die("tcgetattr");
    atexit(disableRawMode); // from stdlib, callbacks in C!!

    struct termios raw = E.og_termios;
    // disable Ctrl+S, Ctrl+Q | disable Ctrl+M(terminal converts this('\r') to new line('\n))
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    // https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html#miscellaneous-flags
    raw.c_cflag |= (CS8);
    // disable logging | turn off canonical mode(now read input byte-by-byte) | disable Ctrl+C, Ctrl+Z, Ctrl+Y(MacOS) | disable Ctrl+V, Ctrl+O(MacOS)
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    // https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html#turn-off-all-output-processing
    raw.c_oflag &= ~OPOST;

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw))
        die("tcsetattr");
}

char editorReadKey(void)
{
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && nread != EAGAIN)
            die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) // get cursor position
        return -1;

    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) == -1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    // https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#window-size-the-hard-way
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        // fallback for if ioctl is not supported by any terminals
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols); // changes the pointers values
    }
    else
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
    return 0;
}

/*** append buffer ***/

struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
    // Extends the memory if available
    // if not assigns a new memory freeing the old one and copies the old data to the new memory
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*** input ***/

void editorProcessKeyPress(void)
{
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** outut ***/

void editorDrawRows(struct abuf *ab)
{
    for (int y = 0; y < E.screenrows; y++)
    {
        abAppend(ab, "\x1b[K", 4);
        abAppend(ab, "~", 1);
        if (y < E.screenrows - 1)
            abAppend(ab, "\r\n", 3);
    }
}

void editorRefreshScreen(void)
{
    // \x1b - escape character
    // [ - start of escape sequence
    // J - command to clear screen
    // 2 - clear entire screen, 1 - clear up until cursor, 0(default) - clear from cursor till end of screen
    // write(STDOUT_FILENO, "\x1b[2J", 4); // Write a 4 byte escape sequence, that will clear the entire screen
    // default args for H escape sequence are 1,1(row, column) counting starts from 1
    // write(STDOUT_FILENO, "\x1b[H", 3); // position the cursor to the 1st row and column same as "\x1b[1;1H"

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Turn off cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6); // Turn on cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor(void)
{
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(void)
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
