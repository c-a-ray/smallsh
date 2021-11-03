/**
 * @file smallsh.h
 * @author Cody Ray <rayc2@oregonstate.edu>
 * @version 1.0
 * @section DESCRIPTION
 *
 * For OSU CS 344
 * Assignment 3
 * 
 * Header file for smallsh, 
 * a shell that implements a subset of features of well-known shells
 */

#ifndef SMALLSH
#define SMALLSH

#define MAX_INPUT_LEN 2048          // Per assignment, max length of a command line
#define MAX_ARGS 512                // Per assignment, max number of arguments
#define MAX_FILENAME_LEN 256        // Per OS spec, max length of a file name
#define MAX_PID_LEN 7               // Per OS spec, max number of digits in a PID
#define BG_CAP 1000                 // Max number of background processes to allow
#define TOKEN_DELIMITER " \r\a\n\t" // Delimiters for splitting command line into args

/** 
 * Stores information about a command entered into smallsh
 */
struct command
{
    char **args;    //< Individual arguments of the command
    int nargs;      //< Number of arguments in the command
    char *inFile;   //< Input redirect file specified in command
    char *outFile;  //< Output redirect file specified in command
    bool run_in_bg; //< Determine if command should run as a background process
};

/** 
 * Stores information about currently running background processes
 */
struct background
{
    pid_t pids[BG_CAP]; //< Array to hold PIDs of all currently running background processes
    int size;           //< The number of currently running background processes
};

/**
 * sa_handler ((*sa_handler)(int)) for SIGTSTP
 * 
 * When SIGTSTP occurs, toggles foreground-only mode.
 * Uses global bool variable allow_bg to accomplish this.
 * 
 * @param signo required but not used
 */
void handle_SIGTSTP(int);

/**
 * Reset members of the command structure so it can be reused/filled again
 * 
 * @param cmd pointer to command struct to reset
 */
void reset_command(struct command *);

/**
 * Get the last command entered by the user
 * 
 * @return the user-entered command
 */
char *read_command(void);

/**
 * Parse a command line
 * 
 * Split the command line into tokens, count the arguments, detect any i/o redirects, 
 * expand the PID variable, and check for the background commands, filling 
 * a command struct with the information
 * 
 * @param cmd_str the full command line
 * @param cmd the command struct to store parsed data in
 */
void parse_command(char *, struct command *);

/**
 * Determine whether two strings are equal
 * 
 * This is just to make the syntax for checking string equality a little nicer
 * 
 * @param str1 the first string to compare
 * @param str2 the second string to compare
 * @return true if str1 == str2, false otherwise
 */
bool strmatch(const char *, const char *);

/**
 * Replace any instance of "$$" in token to the value of pid, storing the expanded version in result 
 * 
 * @param result a string to hold the current argument after "$$" has been replaced by the PID. Must
 *        be large enough to hold a string containing only '$' characters 
 *        (half the string length * the max number of digits in a PID)
 * @param token the original arg perform variable expansion on
 * @param the current PID, stored as a string
 */
void expand_pid(char *, char *, char *);

/**
 * Execute the command entered by the user
 * 
 * Execute built-in function if one exists, otherwise outsource execution, and
 * tell the main loop whether to continue or not. If the command is an empty line
 * or a comment, then continue without doing anything. If the command is "exit",
 * exit the main loop. Otherwise execute the appropriate built-in or outsource,
 * then continue.
 * 
 * @param  cmd the command struct holding information about the current command
 * @param  lastExit the exit value or terminating signal of the last process
 * @param  sa_SIGINT the SIGINT action struct to be passed down to run_non_builtin()
 * 
 * @return true if shell should continue running, false if the shell should stop running
 */
bool exec_cmd(struct command *, int *, struct sigaction, struct background *);

/**
 * Change the current directory
 * 
 * @param  dir is the path (relative or absolute) to the directory to change to
 * 
 * @return always true
 */
bool smallsh_cd(char *);

/**
 * Change the current directory
 * 
 * @param  dir is the path (relative or absolute) to the directory to change to
 * 
 * @return always true
 */
bool smallsh_status(int);

/**
 * Execute a non-built-in command
 * 
 * Create a new process and execute a command in the foreground or background.
 * If running in background and no i/o redirectoins are specified, redirect
 * input and output both to /dev/null. If not running in background, set the
 * SIGINT action to its default and handle any i/o redirects. 
 * 
 * @param cmd the command struct holding information about the command to execute
 * @param lastExit the exit value or terminating signal of the last process
 * @param sa_SIGINT the SIGINT action struct to be passed down to run_non_builtin()
 * @param bg the background struct holding information about the currently running background processes
 */
void run_non_builtin(struct command *, int *, struct sigaction, struct background *);

/**
 * Handle input and output file redirects
 * 
 * If direction is "in", redirect stdin to the specified file,
 * otherwise redirect stdout to the specified file
 * 
 * @param file the path to the file to redirect to
 * @param direction a string indicating whether the redirect is for input or output
 */
void handle_redirect(char *, const char *);

/**
 * Check on and clean up background processes
 * 
 * @param  bg the background struct holding information about the processes currently running in the background
 * @param  lastExit the exit value or terminating signal of the last process
 * 
 * @return a new background struct with information about the still-running background processess
 */
struct background run_bg_census(struct background, int);

#endif