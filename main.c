#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define DEFAULT_N_READ_BYTES 64
#define DATA_FILE "~/.config/clipass/entropy"
#define ENTROPY_SRC    "/dev/urandom"
#define ERR(...)                                \
    do {                                        \
        fprintf(stderr, "Error: " __VA_ARGS__); \
        fputc('\n', stderr);                    \
        exit(EXIT_FAILURE);                     \
    } while (0)

static void usage(const char *execname)
{
    printf("Usage: %s <index> [-c count] [-h]\n"
           "       <index>:     Index into your %s file.\n"
           "       -c count: Number of bytes to obtain. (Default: %d)\n"
           "       -h:       This help message.\n"
           "       -g:       Generate an ASCII entropy file (-c bytes long)\n"
           "\n"
           "CliPass is a simple utility that can be used to\n"
           "conveniently generate large passwords that do not\n"
           "have to be remembered by the user.\n"
           "\nIn general passwords are a weak form of protection, but until\n"
           "the day where there are better alternatives, typically the\n"
           "larger the password the better, and that is the motivating idea\n"
           "behind this tool.\n"
           "\n"
           "User's don't want to have to remember or manually enter "
           "a password.\n"
           "CliPass reads data from the user's read-only file (%s),\n"
           "copies '-c' bytes of that data to the clipboard and then\n"
           "the user has 5 seconds to paste that into the input field\n"
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

/* Free this when done */
static unsigned char *get_data(int fd, size_t offset, size_t count)
{
    unsigned char *data = malloc(count + 1);

    if (!data)
      ERR("Not enough memory to allocate for clipboard");
    if (lseek(fd, offset, SEEK_SET) == -1)
      ERR("Could not seek to offset %zu: %s", offset, strerror(errno));
    if (read(fd, data, count) != count)
      ERR("Error reading %zu bytes from entropy file\n", count);

    data[count] = '\0';
    return data;
}

static void generate_entropy_file(const char *data_file, size_t n_bytes)
{
    size_t i;
    FILE *en, *fp;
   
    if (!(fp = fopen(data_file, "w")))
      ERR("Could not create entropy file: %s\n"
          "Check that this path exists: %s",
          strerror(errno), data_file);

    /* Open entropy source and read in data */
    if (!(en = fopen(ENTROPY_SRC, "r")))
        ERR("Could not open entropy source: %s", ENTROPY_SRC);

    /* Read in data (one byte at a time, slower than a block) but we can
     * constantly churn the pool. I don't know what is more pseudorandom,
     * reading a block or reading a single character.  Presumably it would be
     * the same since (I would imagine) that the requested length is less than
     * what is in the entropy pool.
     */
    printf("Obtaining entropy: ");
    for (i=0; i<n_bytes; ++i) {
        int c = fgetc(en);
        if (c == -1)
          ERR("Reading %zu bytes of entropy.", n_bytes);

        /* Ensure the data is printable, since I think most passwords assume
         * that.
         */
        c = (c % (126-32)) + 32;
    
        /* Save data */
        if (fputc(c, fp) == -1)
          ERR("Writing %zu bytes of entropy.", n_bytes);

        putc('.', stdout);
        fflush(NULL);
    }
    printf("\n%zu bytes of (printable) entropy saved to: %s\n", i, data_file);

    /* chmod 0400 */
    printf("Marking file as read-only by the user: 0400\n");
    if (chmod(data_file, S_IRUSR) == -1)
      ERR("Setting permission to %s", data_file);

    fclose(fp);
    fclose(en);
}

/* Free this when done */
static char *generate_data_path_name(void)
{
    size_t len;
    char *path;
    struct passwd *pw = getpwuid(getuid());
  
    if (!pw || !pw->pw_name)
      ERR("User name could not be found: %s", strerror(errno));

    len = strlen("/home/") + strlen(pw->pw_name) + strlen(DATA_FILE) + 1;
    if (!(path = malloc(len)))
      ERR("Not enough memory to allocate a pathname");

    /* +1 to skip past the '~' */
    snprintf(path, len, "/home/%s%s", pw->pw_name, DATA_FILE+1);
    return path;
}

int main(int argc, char **argv)
{
    int i, fd, opt;
    Display *disp;
    Window win;
    size_t n_bytes;
    ssize_t code_index;
    struct stat file_info;
    _Bool do_gen;
    char *data_path;
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
        if (code_index != -1)
          ERR("Multiple <index> arguments passed (see -h)");
        code_index = strtoll(argv[i], NULL, 10);
    }
    if (code_index < 0 && !do_gen)
      ERR("An index must be specified (see help: -h)");

    /* Create the actual data path (expand ~ to the home directory) */
    data_path = generate_data_path_name();

    /* Generate a new entropy file */
    if (do_gen)
      generate_entropy_file(data_path, n_bytes);

    /* Check entropy file existence and readability */
    if ((fd = open(data_path, O_RDONLY)) == -1)
      ERR("Opening entropy file: %s: %s (see -h)\n",data_path, strerror(errno));
    if (fstat(fd, &file_info) != 0)
      ERR("Obtaining stats on entropy file: %s (see -h)", data_path);
    if (file_info.st_mode == S_IRUSR)
      ERR("Entropy file must be user-read-only (see -h)");

    /* Check that the index and length are within range of the file */
    if (n_bytes > file_info.st_size)
      ERR("Requested %zu bytes but the file %s is only %zu bytes",
          n_bytes, data_path, file_info.st_size);

    /* Get data */
    printf("Copying password to primary buffer.\n");
    str = get_data(fd, code_index, n_bytes);

    /* Create a display and get the root window */
    if (!(disp = XOpenDisplay(NULL)))
      ERR("Could not open display");
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

            /* Update window property, apparently this is how
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
