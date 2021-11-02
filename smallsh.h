#ifndef SMALLSH
#define SMALLSH

#define MAX_INPUT_LEN 2048
#define MAX_FILENAME_LEN 256
#define MAX_ARGS 512
#define BG_CAP 1000
#define MAX_PID_LEN 8
#define TOKEN_DELIMITER " \r\a\n\t"

struct command
{
    char **args;
    int nargs;
    char *inFile;
    char *outFile;
    bool run_in_bg;
};

struct background
{
    pid_t pids[BG_CAP];
    int size;
};

struct sigaction setup_sigactions();
void handle_SIGTSTP(int);
void reset_command(struct command *);
char *read_command(void);
void parse_command(char *, struct command *);
bool strmatch(const char *, const char *);
bool exec_cmd(struct command *, int *, struct sigaction, struct background *);
bool smallsh_cd(char *);
bool smallsh_status(int);
void run_non_builtin(struct command *, int *, struct sigaction, struct background *);
void handle_redirect(char *, const char *);
struct background run_bg_census(struct background, int);

#endif