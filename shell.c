
/*
 * Burak Aslan
 * Homework 4
 * CCS 3650
 * 02-12-16
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAXLINE 200  /* This is how we declare constants in C */
#define MAXARGS 20

int processID, fileOut, fileIn, background, childIndex, childP;

/* In C, "static" means not visible outside of file.  This is different
 * from the usage of "static" in Java.
 * Note that end_ptr is an output parameter.
 */
static char * getword(char * begin, char **end_ptr)
{
    char * end = begin;

    while (*begin == ' ') {
        begin++;  /* Get rid of leading spaces. */
    }

    end = begin;
    while (*end != '\0' && *end != '\n' && *end != ' ') {
        end++;  /* Keep going. */
    }

    if (end == begin) {
        return NULL;  /* if no more words, return NULL */
    }

    *end = '\0';  /* else put string terminator at end of this word. */
    *end_ptr = end;

    if (begin[0] == '$') { /* if this is a variable to be expanded */
        begin = getenv(begin+1); /* begin+1, to skip past '$' */
    if (begin == NULL) {
        perror("getenv");
        begin = "UNDEFINED";
        }
    }

    return begin; /* This word is now a null-terminated string.  return it. */
}

/* In C, "int" is used instead of "bool", and "0" means false, any
 * non-zero number (traditionally "1") means true.
 */
/* argc is _count_ of args (*argcp == argc); argv is array of arg _values_*/
static void getargs(char cmd[], int *argcp, char *argv[])
{
    char *cmdp = cmd;
    char *end;
    int i = 0;
    processID = 0;
    fileOut = 0;
    fileIn = 0;
    background = 0;
    childIndex = 0;

    /* fgets creates null-terminated string. stdin is pre-defined C constant
     *   for standard intput.  feof(stdin) tests for file:end-of-file.
     */
    if (fgets(cmd, MAXLINE, stdin) == NULL && feof(stdin)) {
        printf("Couldn't read from standard input. End of file? Exiting ...\n");
        exit(1);  /* any non-zero value for exit means failure. */
    }

    while ((cmdp = getword(cmdp, &end)) != NULL && *cmdp != '#') { /* end is output param */

    if (*cmdp == '|') {
        processID = 1;
        childIndex = i;
    }

    if (*cmdp == '>') {
        fileOut = 1;
        childIndex = i;
    }

    if(*cmdp == '<') {
        fileIn = 1;
        childIndex = i;
    }

    if(*cmdp == '&') {
        background = 1;
        childIndex = i; }
        /* getword converts word into null-terminated string */
        argv[i++] = cmdp;
        /* "end" brings us only to the '\0' at end of string */
        cmdp = end + 1;
    }

    if (childIndex) {
        argv[childIndex] = NULL;
    } /* get rid of special character above */

    argv[i] = NULL; /* Create additional null word at end for safety. */
    *argcp = i;
}

static void execute(int argc, char *argv[])
{
    int pipe_fd[2];

    // If the id is not 0
    if (processID != 0) {
        // if the pipe is an error
        if (pipe(pipe_fd) == -1) {
            perror("Running pipe...");
        }
    }

    pid_t childpid = -2;
    pid_t childpid2 = -2;

    childpid = fork(); // Create fork

    // If the process id is he parent and the child exists
    if(processID != 0 && childpid > 0) {
        childpid2 = fork();
    }

    // If we are in the parent
    if (childpid == -1 || childpid2 == -1) {
        perror("fork");
        printf("  (failed to execute command)\n");
    }

    // Inside child process
    if (childpid == 0) {

        // If the process is not an error
        if (processID != 0) {

            // Close the std out
            close(STDOUT_FILENO);
            // If running dup2 is an error
            if (dup2(pipe_fd[1], STDOUT_FILENO) == -1 ) {
                perror("Running dup2...");
            }
            // Close the files
            close(pipe_fd[0]);
            close(pipe_fd[1]);
        }

        // if the next file is not error
        if (fileOut != 0 ) {
            // Close the file
            close(STDOUT_FILENO);
            if(open(argv[childIndex + 1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR) == -1) {
                perror("Writing file...");
            }
        }

        // if the input file is not an error
        if(fileIn != 0) {
            // Close the file
            close(STDIN_FILENO);
            if(open(argv[childIndex + 1], O_RDONLY) == -1) {
                perror("Reading file...");
            }
        }

        // If there is a command we dont know
        if (-1 == execvp(argv[0], argv)) {
            perror("execvp");
            printf("  (couldn't find command)\n");
        }

        /* NOT REACHED unless error occurred */
        exit(1);
    }
    else if (childpid2 == 0) {

        // Add 1 to the index to access next file
        *argv = argv[childIndex + 1];

        // Close the open file
        close(STDIN_FILENO);

        // if we had an error running dup2
        if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
            perror("Running dup2...");
        }

        // Close input and output file
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // If we couldn't find the file
        if( -1 == execvp(argv[0], argv)) {
            perror("execvp");
            printf("  (couldn't find command)\n");
        }
        /* Error occured */
        exit(1);
    }
    else { // In parent

        // If the process id is not the parent process id
        if(processID != 0) {
            // Close file 1 and 2
            close(pipe_fd[0]);
            close(pipe_fd[1]);
        }

        // If the background is not true
        if(background == 0 && childpid > 0) { // wait until child finishes
            // Don't run in background
            waitpid(childpid, NULL, 0);
        }
        else if(background == 0 && childpid2 > 0) { // Wait until child finishes pipe
            waitpid(childpid2, NULL, 0);}
        else {
            // Print out the background process
            printf("[%d] is now a background task\n", childpid);
        }
    }
    return;
}

void interrupt_handler(int signum) {
    // If we have an interuption in the console
    if(signum == SIGINT) {
        printf("Signal Interrupt detected \n");
    }
}

int main(int argc, char *argv[])
{
    char cmd[MAXLINE];
    char *childargv[MAXARGS];
    int childargc;

    // Handle interuption
    signal(SIGINT, interrupt_handler);

    if(argc > 1 && access(argv[1], F_OK) != -1) {
        freopen(argv[1], "r", stdin);
    }

    while (1) {
        printf("%% "); /* printf uses %d, %s, %x, etc.  See 'man 3 printf' */
        fflush(stdout); /* flush from output buffer to terminal itself */
        getargs(cmd, &childargc, childargv); /* childargc and childargv are
            output args; on input they have garbage, but getargs sets them. */
        /* Check first for built-in commands. */
        if (childargc > 0 && strcmp(childargv[0], "exit") == 0) {
            exit(0);
        } else if (childargc > 0 && strcmp(childargv[0], "logout") == 0) {
            exit(0);
        } else {
            execute(childargc, childargv);
        }
    }
    /* NOT REACHED */
}
