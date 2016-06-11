#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define DEFAULT_N_READ_BYTES 64
#define DATA_FILE "~/.config/clipass/entropy"

static size_t measure_entropy(const char *fname)
{
}

static _Bool get_value(FILE *, size_t offset, size_t count);

static void usage(const char *execname)
{
    printf("Usage: %s <index> [-c count] [-h]\n"
           "       <index>:     Index into your %s file.\n"
           "       -c count: Number of bytes to obtain. (Default: %d)\n"
           "       -h:       This help message."
           "\n\n"
           "CliPass is a simple utility that can be used to\n"
           "conviently generate large passwords that do not\n"
           "have to be remembered by the user.\n"
           "\nIn general passwords are a weak form of protection, but until\n"
           "the day where there are better alternatives, typically the\n"
           "larger the password the better, and that is the motivating idea\n"
           "behind this tool.\n"
           "\n"
           "User's don't want to have to remember or manually enter a password.\n"
           "CliPass reads data from the user's read-only file (%s),\n"
           "copies '-c' bytes of that data to the clipboard and then\n"
           "the user has 5 seconds to paste that into theinput field\n"
           "of their choice.  After 5 seconds the clipboard is cleared.\n"
           "\n"
           "===== DISCLAIMER =====\n"
           "* THIS IS FREE SOFTWARE: NO WARRANTY OR GUARANTEE.\n"
           "* USE AT YOUR OWN DISCRETION.\n"
           "* (Or use this to learn how the X clipboard words).\n"
           "* THE USER IS RESPONSIBLE FOR WHERE THEY PASTE THE PASSWORD\n"
           "* AND FOR THE CONTENTS OF THE PASSWORD.\n",
           execname, DATA_FILE, DEFAULT_N_READ_BYTES, DATA_FILE);
}

int main(int argc, char **argv)
{
    int i, err, opt, n_bytes;
    Display *disp;
    Window win;
    char errtxt[1024], *buf;
    ssize_t code_index;
    struct stat file_info;
    const char *str;

    /* Args */
    n_bytes = DEFAULT_N_READ_BYTES;
    while ((opt = getopt(argc, argv, "c:h")) != -1) {
        switch (opt) {
        case 'h': usage(argv[0]); exit(EXIT_SUCCESS);
        case 'c': n_bytes = atoi(optarg); break;
        default: exit(EXIT_FAILURE);
        } 
    }

    /* Argument describing the index into .data file */
    code_index = -1;
    for (i=optind; i<argc; ++i) {
        if (code_index != -1) {
            fprintf(stderr,
                    "Error: multiple <index> arguments passed (see -h)\n");
            exit(EXIT_FAILURE);
        }
        code_index = strtoll(argv[i], NULL, 10);
    }
    if (code_index < 0) {
        fprintf(stderr, "Invalid <index>: %ld\n", code_index);
        exit(EXIT_FAILURE);
    }

    /* Check .data file existence and readability */
    if (lstat(DATA_FILE, &file_info) != 0) {
        fprintf(stderr, "Error opening entropy file: %s (see -h)\n", DATA_FILE);
        exit(EXIT_FAILURE);
    }
    if (file_info.st_mode == S_IRUSR) {
        fprintf(stderr, "Error: Entropy file must be "
                "user-read-only (see -h)\n");
        exit(EXIT_FAILURE);
    }

    printf("Copying string to primary buffer: %s\n", str);
    
    /* Create a display and get the root window */
    if (!(disp = XOpenDisplay(NULL))) {
        fprintf(stderr, "Could not open display\n");
        exit(EXIT_FAILURE);
    }
    win = XDefaultRootWindow(disp);

    /* Claim we own the primary selection */
    XSetSelectionOwner(disp, XA_PRIMARY, win, CurrentTime);
    
    /* Loop and wait for events from applications that want to read from the primary
     * selection or timeout and exit.
     */
    for ( ;; ) {
        XEvent event;
        XNextEvent(disp, &event);
        if (event.type == SelectionRequest) {
            XSelectionEvent sel = {0};
            sel.type = SelectionNotify;
            sel.display = disp;
            sel.requestor = event.xselectionrequest.requestor;
            sel.selection = XA_PRIMARY;
            sel.target = event.xselectionrequest.target;
            sel.property = event.xselectionrequest.property;
            sel.time = event.xselectionrequest.time;

            /* Update window property, apparantly this is how
             * selection data is passed.
             */
            XChangeProperty(event.xselectionrequest.display,
                            event.xselectionrequest.requestor,
                            event.xselectionrequest.property,
                            XA_STRING, 8, PropModeReplace,
                            str, strlen(str));
            XSendEvent(event.xselectionrequest.display,
                       event.xselectionrequest.requestor,
                       False, None, (XEvent *)&sel);
        }
    }

    /* Cleanup and exit */
    XCloseDisplay(disp);
    return 0;
}
