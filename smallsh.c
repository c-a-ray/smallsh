#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "smallsh.h"

bool allow_bg = true;

int main()
{
    struct sigaction sa_SIGINT = setup_sigactions();

    struct command cmd;
    cmd.args = malloc(sizeof(char *) * MAX_ARGS);

    struct background bg;
    int lastExit = 0;
    bool cont = true;

    do
    {
        printf(": ");
        fflush(stdout);

        reset_command(&cmd);

        parse_command(read_command(), &cmd);
        cont = exec_cmd(&cmd, &lastExit, sa_SIGINT, &bg);
        bg = run_bg_census(bg, lastExit);

        fflush(stdout);
        free(cmd.args);
        free(cmd.inFile);
        free(cmd.outFile);
    } while (cont);
}

struct sigaction setup_sigactions()
{
    struct sigaction sa_SIGTSTP = {0};
    sa_SIGTSTP.sa_handler = handle_SIGTSTP;
    sigfillset(&sa_SIGTSTP.sa_mask);
    sa_SIGTSTP.sa_flags = 0;
    sigaction(SIGTSTP, &sa_SIGTSTP, NULL);

    struct sigaction sa_SIGINT = {0};
    sa_SIGINT.sa_handler = SIG_IGN;
    sigfillset(&sa_SIGINT.sa_mask);
    sa_SIGINT.sa_flags = 0;
    sigaction(SIGINT, &sa_SIGINT, NULL);

    return sa_SIGINT;
}

void handle_SIGTSTP(int signo)
{
    if (allow_bg)
    {
        allow_bg = false;
        const char *msg = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    else
    {
        allow_bg = true;
        const char *msg = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    fflush(stdout);
}

void reset_command(struct command *cmd)
{
    memset((*cmd).args, 0, MAX_ARGS);
    (*cmd).run_in_bg = false;
    (*cmd).nargs = 0;
    (*cmd).inFile = NULL;
    (*cmd).outFile = NULL;
}

char *read_command(void)
{
    size_t buffer_size = 0;
    char *line = NULL;
    getline(&line, &buffer_size, stdin);
    return line;
}

void parse_command(char *cmd_str, struct command *cmd)
{
    char **tokens = malloc(sizeof(char *) * MAX_INPUT_LEN);
    char *token = strtok(cmd_str, TOKEN_DELIMITER);

    while (token != NULL)
    {
        if (strmatch(token, "<"))
        {
            token = strtok(NULL, TOKEN_DELIMITER);
            (*cmd).inFile = malloc(sizeof(char) * MAX_FILENAME_LEN);
            strcpy((*cmd).inFile, token);
        }
        else if (strmatch(token, ">"))
        {
            token = strtok(NULL, TOKEN_DELIMITER);
            (*cmd).outFile = malloc(sizeof(char) * MAX_FILENAME_LEN);
            strcpy((*cmd).outFile, token);
        }
        else
        {
            if (strmatch(token, "$$"))
            {
                char *pid_str = malloc(sizeof(char) * MAX_PID_LEN);
                sprintf(pid_str, "%d", getpid());
                tokens[(*cmd).nargs] = pid_str;
            }
            else
                tokens[(*cmd).nargs] = token;

            (*cmd).nargs++;
        }

        token = strtok(NULL, TOKEN_DELIMITER);
    }

    if ((*cmd).nargs > 1 && strmatch(tokens[(*cmd).nargs - 1], "&"))
    {
        if (allow_bg)
            (*cmd).run_in_bg = true;

        (*cmd).nargs--;
        tokens[(*cmd).nargs] = NULL;
    }

    (*cmd).args = tokens;
}

bool strmatch(const char *str1, const char *str2)
{
    if (strcmp(str1, str2) == 0)
        return true;
    return false;
}

bool exec_cmd(struct command *cmd, int *lastExit, struct sigaction sa_SIGINT, struct background *bg)
{
    char *firstArg = (*cmd).args[0];
    char *secondArg = (*cmd).args[1];

    if (firstArg == NULL || (strncmp(firstArg, "#", 1)) == 0)
        return true;
    else if (strmatch(firstArg, "exit"))
        return false;
    else if (strmatch(firstArg, "cd"))
        return smallsh_cd(secondArg);
    else if (strmatch(firstArg, "status"))
        return smallsh_status(*lastExit);

    run_non_builtin(cmd, lastExit, sa_SIGINT, bg);
    return true;
}

bool smallsh_cd(char *dir)
{
    if (dir == NULL)
    {
        const char *homeDir = getenv("HOME");
        if (chdir(homeDir) == -1)
            printf("directory %s not found\n", homeDir);
        else
            printf("changing to directory %s\n", homeDir);
    }
    else if (chdir(dir) == -1)
        printf("directory %s not found\n", dir);

    return true;
}

bool smallsh_status(int lastExit)
{
    if (WIFEXITED(lastExit))
        printf("exit value %d\n", WEXITSTATUS(lastExit));
    else
        printf("terminated by signal %d\n", WTERMSIG(lastExit));
    return true;
}

void run_non_builtin(struct command *cmd, int *lastExit, struct sigaction sa_SIGINT, struct background *bg)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "out of resources\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if ((*cmd).run_in_bg)
        {
            if ((*cmd).inFile == NULL)
                handle_redirect("/dev/null", "in");
            if ((*cmd).outFile == NULL)
                handle_redirect("/dev/null", "out");
        }
        else
        {
            sa_SIGINT.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sa_SIGINT, NULL);

            if (((*cmd).inFile != NULL))
                handle_redirect((*cmd).inFile, "in");
            if (((*cmd).outFile != NULL))
                handle_redirect((*cmd).outFile, "out");
        }

        if (execvp((*cmd).args[0], (*cmd).args))
        {
            fprintf(stderr, "%s: no such file or directory\n", (*cmd).args[0]);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if (!((*cmd).run_in_bg))
        {
            do
            {
                waitpid(pid, lastExit, 0);
            } while (!WIFEXITED(*lastExit) && !WIFSIGNALED(*lastExit));

            if (WIFSIGNALED(*lastExit))
                printf("terminated by signal %d\n", WTERMSIG(*lastExit));
        }
        else
        {
            printf("background pid: %d\n", pid);
            (*bg).pids[(*bg).size++] = pid;
        }
    }
}

void handle_redirect(char *file, const char *direction)
{
    int fd, dup_res;

    if (strmatch(direction, "in"))
    {
        fd = open(file, O_RDONLY);
        if (fd == -1)
        {
            fprintf(stderr, "cannot open file %s for input\n", file);
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(fd, 0);
        if (dup_res == -1)
        {
            fprintf(stderr, "cannot redirect input to file %s\n", file);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
    else
    {
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            fprintf(stderr, "cannot open file %s for output\n", file);
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(fd, 1);
        if (dup_res == -1)
        {
            fprintf(stderr, "cannot redirect output to file %s\n", file);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
}

struct background run_bg_census(struct background bg, int lastExit)
{
    pid_t pid;
    struct background still_running = {};
    for (int i = 0; i < bg.size; i++)
    {
        pid = waitpid(bg.pids[i], &lastExit, WNOHANG);
        if (pid > 0)
        {
            printf("background pid %d is done: ", pid);
            smallsh_status(lastExit);
        }
        else if (pid == 0)
            still_running.pids[still_running.size++] = bg.pids[i];
    }
    return still_running;
}