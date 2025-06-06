/*** includes ***/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios og_termios;

/*** terminal ***/

void die(const char *s)
{
    perror(s);
    exit(1);
}

void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(void)
{
    if (tcgetattr(STDIN_FILENO, &og_termios))
        die("tcgetattr");
    atexit(disableRawMode); // from stdlib, callbacks in C!!

    struct termios raw = og_termios;
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

/*** init ***/

int main(void)
{
    enableRawMode();
    while (1)
    {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno == EAGAIN)
            die("read");
        if (iscntrl(c))
        {
            // need to add \r as well as terminal automatically adds that but we disabled this post
            // output processing feature of terminal using OPOST flag. It is carriage return(from typewriter days)
            // it takes the cursor back to the first line and \n brings it to the next line
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == CTRL_KEY('q'))
            break;
    }

    return 0;
}
