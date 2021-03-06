/**
 * @file smallsh.c
 * @author Cody Ray <rayc2@oregonstate.edu>
 * @version 1.0
 * @section DESCRIPTION
 *
 * For OSU CS 344
 * Assignment 3
 * 
 * Source file for smallsh, 
 * a shell that implements a subset of features of well-known shells
 */

#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "smallsh.h"

// Declared as a global variable because it is needed in the SIGTSTP handler
// but cannot be passed as a parameter to `void (*sa_handler)(int)`
bool allow_bg = true;

int main()
{
    // Initialize sigaction struct for SIGTSTP (Ctrl-Z)
    struct sigaction sa_SIGTSTP = {0};
    sa_SIGTSTP.sa_handler = &handle_SIGTSTP; // Register handler to toggle foreground-only mode
    sigfillset(&sa_SIGTSTP.sa_mask);
    sa_SIGTSTP.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_SIGTSTP, NULL);

    // Initialize sigaction struct for SIGINT (Ctrl-C)
    struct sigaction sa_SIGINT = {0};
    sa_SIGINT.sa_handler = SIG_IGN; // Ignore signal for now, will be set back to default later on
    sigfillset(&sa_SIGINT.sa_mask);
    sa_SIGINT.sa_flags = 0;
    sigaction(SIGINT, &sa_SIGINT, NULL);

    // Create new struct to store command info
    struct command cmd;
    cmd.args = malloc(sizeof(char *) * MAX_ARGS + 1);

    // Create new struct to store info about background processes
    struct background bg;

    // To store exit status or terminating signal of the last fg process
    int lastExit = 0;

    // To determine when to exit the shell
    bool cont = true;

    do
    {
        // Display prompt
        printf(": ");
        fflush(stdout);

        // Zero out members of the command struct so we can store the next command
        reset_command(&cmd);

        // Read current command, parse it, and store it in cmd
        parse_command(read_command(), &cmd);

        // Execute the current command
        cont = exec_cmd(&cmd, &lastExit, sa_SIGINT, &bg);

        // Check on background processes
        bg = run_bg_census(bg, lastExit);

        // Free memory allocated for input and output files
        free(cmd.inFile);
        free(cmd.outFile);
    } while (cont);

    kill_zombies(bg);
}

void handle_SIGTSTP(int signo)
{
    // Clear stdout and stdin
    fflush(stdout);
    clearerr(stdin);

    if (allow_bg) // Not currently in foreground-only mode
    {
        // Switch to foreground-only mode and print a message about it
        allow_bg = false;
        char *msg = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, 50);
    }
    else // Currently in foreground-only mode
    {
        // Exit foreground-only mode and print a message about it
        allow_bg = true;
        char *msg = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, msg, 30);
    }

    // Give the prompt again and clear stdout
    printf(": ");
    fflush(stdout);
}

void reset_command(struct command *cmd)
{
    // Zero out args so we can fill it again
    memset((*cmd).args, 0, MAX_ARGS + 1);
    (*cmd).nargs = 0;

    // By default, don't run in background
    (*cmd).run_in_bg = false;

    // Don't carry over i/o redirects
    (*cmd).inFile = NULL;
    (*cmd).outFile = NULL;
}

char *read_command(void)
{
    size_t buffer_size = 0;
    char *line = NULL;
    getline(&line, &buffer_size, stdin); // Read command
    return line;                         // Return command string so it can be parsed
}

void parse_command(char *cmd_str, struct command *cmd)
{
    // Allocate enough space for 512 arguments
    char **tokens = malloc(sizeof(char *) * MAX_INPUT_LEN + 1);

    // Get the first argument
    char *token = strtok(cmd_str, TOKEN_DELIMITER);

    // Create a string to hold the PID for variable expansion
    char *pid_str = malloc(sizeof(char) * MAX_PID_LEN + 1);
    sprintf(pid_str, "%d", getpid());

    // Iterate through all arguments in command
    while (token != NULL)
    {

        if (strmatch(token, "<")) // Input redirect encountered
        {
            token = strtok(NULL, TOKEN_DELIMITER);                       // Move to the next argument
            (*cmd).inFile = malloc(sizeof(char) * MAX_FILENAME_LEN + 1); // Allocate enough space for a file name
            strcpy((*cmd).inFile, token);                                // Copy the token into the command struct
        }
        else if (strmatch(token, ">")) // Output redirect encountered
        {
            token = strtok(NULL, TOKEN_DELIMITER);                        // Move to the next token
            (*cmd).outFile = malloc(sizeof(char) * MAX_FILENAME_LEN + 1); // Allocate enough space for a file name
            strcpy((*cmd).outFile, token);                                // Copy the token into the command struct
        }
        else // Not a redirect character
        {
            if (strstr(token, "$$") != NULL) // Token has at least one instance of "$$"
            {
                tokens[(*cmd).nargs] = malloc((sizeof(char) * ((strlen(token)) / 2) + 1) * strlen(pid_str) + 1); // Allocate enough space for a token with only '$' characters
                expand_pid(tokens[(*cmd).nargs], token, pid_str);                                                // Replace all instances of "$$" with the PID and store result in tokens
            }
            else                              // It's a regular argument
                tokens[(*cmd).nargs] = token; // Store it as it is

            (*cmd).nargs++; // Increment the number of arguments
        }

        token = strtok(NULL, TOKEN_DELIMITER); // Get the next token
    }

    free(pid_str);

    // Check if the last argument is the background character
    if ((*cmd).nargs > 1 && strmatch(tokens[(*cmd).nargs - 1], "&"))
    {
        if (allow_bg)                // Not in foreground-only mode
            (*cmd).run_in_bg = true; // Indicate the command should run in the background

        // Remove the character
        (*cmd).nargs--;
        tokens[(*cmd).nargs] = NULL;
    }

    (*cmd).args = tokens; // Store the arguments in the command struct
}

bool strmatch(const char *str1, const char *str2)
{
    if (strcmp(str1, str2) == 0)
        return true; // Strings match
    return false;    // Strings don't match
}

void expand_pid(char *result, char *token, char *pid)
{
    // Create a buffer large enough to hold a token consisting entirely of $ characters
    // i.e. half of the string length * the max number of digits in a PID
    char *buf = malloc(sizeof(char) * (((strlen(token) / 2) + 1) * MAX_PID_LEN) + 1);

    char *pos = buf;   // The position to insert at
    char *tmp = token; // Temp variable to store chunks of the token

    while (true)
    {
        char *substr = strstr(tmp, "$$"); // Get the substring starting at the first occurrence of "$$"
        if (substr != NULL)
        {
            // Copy over the chunk before substr
            memcpy(pos, tmp, substr - tmp);
            pos += substr - tmp;

            // Copy over the PID value
            memcpy(pos, pid, strlen(pid));
            pos += strlen(pid);

            tmp = substr + 2; // Move past "$$"
        }
        else // We've gone beyond the last instance of "$$""
        {
            strcpy(pos, tmp); // Copy the remaining part of the string
            break;
        }
    }

    strcpy(result, buf); // Copy buffer into result
}

bool exec_cmd(struct command *cmd, int *lastExit, struct sigaction sa_SIGINT, struct background *bg)
{
    // Pull out the first and second arguments to make syntax less busy
    char *firstArg = (*cmd).args[0];
    char *secondArg = (*cmd).args[1];

    // Check if first command is a built-in function
    if (firstArg == NULL || (strncmp(firstArg, "#", 1)) == 0) // For empty line or comment, do nothing
        return true;
    else if (strmatch(firstArg, "exit")) // For "exit", return false to exit the main loop
        return false;
    else if (strmatch(firstArg, "cd")) // For "cd", change the directory based on the second argument
        return smallsh_cd(secondArg);
    else if (strmatch(firstArg, "status")) // For "status", print the exit/termination value of the last fg process
        return smallsh_status(*lastExit);

    // Command is not built in, outsource execution
    run_non_builtin(cmd, lastExit, sa_SIGINT, bg);
    return true;
}

bool smallsh_cd(char *dir)
{
    if (dir == NULL) // "cd" is the only argument
    {
        const char *homeDir = getenv("HOME");
        if (chdir(homeDir) == -1)                                 // Change to HOME directory
            fprintf(stderr, "directory %s not found\n", homeDir); // Directory wasn't found, so print an error
    }
    else if (chdir(dir) == -1)                   // A path was specified, change the working directory to that path
        printf("directory %s not found\n", dir); // Directory wasn't found, so print an error

    fflush(stderr);
    return true; // Always continue shell execution after "cd"
}

bool smallsh_status(int lastExit)
{
    if (WIFEXITED(lastExit))                                     // Last foreground process exited on its own
        printf("exit value %d\n", WEXITSTATUS(lastExit));        // Print the exit value
    else                                                         // The last foreground process was terminated
        printf("terminated by signal %d\n", WTERMSIG(lastExit)); // Print the terminating signal
    return true;                                                 // Always continue shell execution after "cd"
}

void run_non_builtin(struct command *cmd, int *lastExit, struct sigaction sa_SIGINT, struct background *bg)
{
    // A lot of this code comes from the Module 4 explorations

    pid_t pid = fork(); // Create a new process
    if (pid < 0)        // The process failed, print an error and exit
    {
        fprintf(stderr, "out of resources\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) // Child process
    {
        if ((*cmd).run_in_bg) // Process is running in the background
        {
            if ((*cmd).inFile == NULL)
                handle_redirect("/dev/null", "in"); // No input redirect, use /dev/null
            if ((*cmd).outFile == NULL)
                handle_redirect("/dev/null", "out"); // No output redirect, use /dev/null
        }
        else // Process is not running in the background
        {
            // Set the SIGINT action to its default behavior
            sa_SIGINT.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sa_SIGINT, NULL);
        }

        if (((*cmd).inFile != NULL))
            handle_redirect((*cmd).inFile, "in"); // Command has an input redirect, so redirect to specified file
        if (((*cmd).outFile != NULL))
            handle_redirect((*cmd).outFile, "out"); // Command has an output redirect, so redirect to specified file

        // Execute command
        if (execvp((*cmd).args[0], (*cmd).args))
        {
            // An error occurred, print an error and exit
            fprintf(stderr, "%s: no such file or directory\n", (*cmd).args[0]);
            exit(EXIT_FAILURE);
        }
    }
    else // Parent process
    {
        if (!((*cmd).run_in_bg)) // Child process not running in background, so wait until it completes
        {
            // Wait until the process exits on its own or a terminating signal is detected
            do
            {
                waitpid(pid, lastExit, 0);
            } while (!WIFEXITED(*lastExit) && !WIFSIGNALED(*lastExit));

            // If the process was terminated by a signal, print a message about the signal
            if (WIFSIGNALED(*lastExit))
                printf("terminated by signal %d\n", WTERMSIG(*lastExit));
        }
        else // Child process running in the background, don't wait
        {
            // Print a message about the background PID
            printf("background pid: %d\n", pid);

            // Add the background process' PID to the background struct so we can keep an eye on them
            (*bg).pids[(*bg).size++] = pid;
        }
    }
}

void handle_redirect(char *file, const char *direction)
{
    // This code takes from "Exploration: Processes and I/O"

    int fd;      // File descriptor
    int dup_res; // Dup result

    if (strmatch(direction, "in")) // Input redirect
    {
        fd = open(file, O_RDONLY); // Open the file only for reading
        if (fd == -1)
        {
            // Error opening file; print error and exit
            fprintf(stderr, "cannot open file %s for input\n", file);
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(fd, 0); // Redirect to stdin
        if (dup_res == -1)
        {
            // Error using dup2; print error and exit
            fprintf(stderr, "cannot redirect input to file %s\n", file);
            exit(EXIT_FAILURE);
        }
    }
    else // Output redirect
    {
        // Open the file only for writing; create if doesn't exit; truncate if it does
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            // Error opening file; print error and exit
            fprintf(stderr, "cannot open file %s for output\n", file);
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(fd, 1);
        if (dup_res == -1)
        {
            // Error using dup2; print error and exit
            fprintf(stderr, "cannot redirect output to file %s\n", file);
            exit(EXIT_FAILURE);
        }
    }
    close(fd); // Close the file
}

struct background run_bg_census(struct background bg, int lastExit)
{
    pid_t pid;
    struct background still_running = {}; // Struct to hold info about all of the still-running processes
    for (int i = 0; i < bg.size; i++)     // Walk through all of the processes that were running in the background after the last command
    {
        pid = waitpid(bg.pids[i], &lastExit, WNOHANG); // Check on it
        if (pid > 0)                                   // The process has finished
        {
            // Print a message about the process finishing
            printf("background pid %d is done: ", pid);
            smallsh_status(lastExit);
        }
        else if (pid == 0)                                         // The process is still running
            still_running.pids[still_running.size++] = bg.pids[i]; // Store its PID in the new background struct
    }
    fflush(stdout);
    return still_running; // Return the new background struct, with only info about still-running background processes
}

void kill_zombies(struct background bg)
{
    pid_t pid;
    int exitVal;
    for (int i = 0; i < bg.size; i++) // Walk through all processes that were running in background after last command
    {
        pid = waitpid(bg.pids[i], &exitVal, WNOHANG); // Check on it
        if (pid == 0)                                 // The process is still running
            kill(bg.pids[i], SIGKILL);                // So kill it
    }
}