#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define DEFAULT_N_READ_BYTES 1024
#define DATA_FILE_PATH "/home/enferex/.config/clipass/"
#define DATA_FILE      DATA_FILE_PATH  "entropy"
#define ENTROPY_SRC    "/dev/urandom"

static _Bool get_value(FILE *fp, size_t offset, size_t count)
{
    return false;
}

static void usage(const char *execname)
{
    printf("Usage: %s <index> [-c count] [-h]\n"
           "       <index>:     Index into your %s file.\n"
           "       -c count: Number of bytes to obtain. (Default: %d)\n"
           "       -h:       This help message.\n"
           "       -g:       Generate an ASCII entropy file (-c bytes long)\n"
           "\n"
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
           "* (Or use this to learn how the X clipboard works (selections)).\n"
           "* THE USER IS RESPONSIBLE FOR WHERE THEY PASTE THE PASSWORD\n"
           "* AND FOR THE CONTENTS OF THE PASSWORD.\n",
           execname, DATA_FILE, DEFAULT_N_READ_BYTES, DATA_FILE);
}

static void generate_entropy_file(size_t n_bytes)
{
    size_t i;
    FILE *en, *fp;
   
    if (!(fp = fopen(DATA_FILE, "w"))) {
        fprintf(stderr, "Could not create entropy file: %s\n"
                "Check that this path exists: %s\n",
                strerror(errno), DATA_FILE_PATH);
        exit(EXIT_FAILURE);
    }

    /* Open entropy source and read in data */
    if (!(en = fopen(ENTROPY_SRC, "r"))) {
        fprintf(stderr, "Could not open entropy source: %s\n", ENTROPY_SRC);
        exit(EXIT_FAILURE);
    }

    /* Read in data (one byte at a time, slower than a block) but we can
     * constantly churn the pool. I don't know what is more pseudorandom,
     * reading a block or reading a single character.  Presumably it would be
     * the same since (I would imagine) that the requested length is less than
     * what is in the entropy pool.
     */
    printf("Obtaining entropy: ");
    for (i=0; i<n_bytes; ++i) {
        int c = fgetc(en);
        if (c == -1) {
            fprintf(stderr, "Error reading %zu bytes of entropy.\n", n_bytes);
            exit(EXIT_FAILURE);
        }

        /* Ensure the data is printable, since I think most passwords assume
         * that.
         */
        c = (c % (126-32)) + 32;
    
        /* Save data */
        if (fputc(c, fp) == -1) {
            fprintf(stderr, "Error writing %zu bytes of entropy.\n", n_bytes);
            exit(EXIT_FAILURE);
        }

        putc('.', stdout);
        fflush(NULL);
    }
    printf("\n%zu bytes of (printable) entropy saved to: %s\n", i, DATA_FILE);

    /* chmod 0400 */
    printf("Marking file as read-only by the user: 0400\n");
    if (chmod(DATA_FILE, S_IRUSR) == -1) {
        fprintf(stderr, "Error setting permission to %s\n", DATA_FILE);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    fclose(en);
}

int main(int argc, char **argv)
{
    int i, fd, opt, n_bytes;
    Display *disp;
    Window win;
    ssize_t code_index;
    struct stat file_info;
    _Bool do_gen;
    const unsigned char *str = NULL;

    /* Args */
    do_gen  = false;
    n_bytes = DEFAULT_N_READ_BYTES;
    while ((opt = getopt(argc, argv, "gc:h")) != -1) {
        switch (opt) {
        case 'g': do_gen = true; break;
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
    if (code_index < 0 && !do_gen) {
        fprintf(stderr, "An index must be specified (see help: -h)\n");
        exit(EXIT_FAILURE);
    }

    /* Generate a new entropy file */
    if (do_gen)
      generate_entropy_file(n_bytes);

    /* Check entropy file existence and readability */
    if ((fd = open(DATA_FILE, O_RDONLY)) == -1) {
        fprintf(stderr, "Error opening entropy file: %s (see -h)\n", DATA_FILE);
        exit(EXIT_FAILURE);
    }
    if (fstat(fd, &file_info) != 0) {
        fprintf(stderr, "Error obtainit stats on entropy file: %s (see -h)\n",
                DATA_FILE);
        exit(EXIT_FAILURE);
    }
    if (file_info.st_mode == S_IRUSR) {
        fprintf(stderr, "Error: Entropy file must be "
                "user-read-only (see -h)\n");
        exit(EXIT_FAILURE);
    }

    /* Check that the index and length are within range of the file */

    printf("Copying password to primary buffer.\n");
    
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
                            str, strlen((const char *)str));
            XSendEvent(event.xselectionrequest.display,
                       event.xselectionrequest.requestor,
                       False, None, (XEvent *)&sel);
        }
    }

    /* Cleanup and exit */
    XCloseDisplay(disp);
    return 0;
}
