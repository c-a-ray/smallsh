#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtIn.h"
#include "helpers.h"
#include "smallsh.h"

char *built_in_commands[] = {"cd", "exit", "#"};
int (*built_in_func[])(char **) = {&smallsh_cd, &smallsh_exit, &smallsh_comment};

void smallsh(void)
{
    char *command;
    char **command_args;
    int status;

    do
    {
        printf(": ");
        command = read_command();
        command_args = parse_command(command);
        status = execute_command(command_args);

        free(command);
        free(command_args);
    } while (status == 1);
}

char *read_command(void)
{
    size_t buffer_size = 0;
    char *line = NULL;

    if (getline(&line, &buffer_size, stdin) == -1)
    {
        if (feof(stdin))
            exit(EXIT_SUCCESS);
        else
            exit(EXIT_FAILURE);
    }

    return line;
}

char **parse_command(char *command)
{
    int buffer_size = TOKEN_BUFFER_SIZE;
    char **tokens = malloc(buffer_size * sizeof(char *));

    if (!tokens)
        error_and_exit("memory allocation error");

    char *token = strtok(command, TOKEN_DELIMITER);
    int token_index = 0;

    while (token != NULL)
    {
        tokens[token_index] = token;
        token_index++;

        if (token_index >= buffer_size)
        {
            buffer_size += TOKEN_BUFFER_SIZE;
            tokens = realloc(tokens, buffer_size * sizeof(char *));
            if (!tokens)
                error_and_exit("memory allocation error");
        }

        token = strtok(NULL, TOKEN_DELIMITER);
    }

    tokens[token_index] = NULL;
    return tokens;
}

int execute_command(char **command_args)
{
    if (command_args[0] == NULL)
        return 1;

    expand_pid_variable(command_args);

    for (int i = 0; i < sizeof(built_in_commands) / sizeof(char *); i++)
        if (strcmp(command_args[0], built_in_commands[i]) == 0)
            return (*built_in_func[i])(command_args);

    return execute_non_builtIn_command(command_args);
}

void expand_pid_variable(char **command_args)
{
    int i = 0;
    while (command_args[i] != NULL)
    {
        if (strcmp(command_args[i], "$$") == 0)
        {
            char *pid = malloc(sizeof(char) * 5);
            sprintf(pid, "%d", getpid());
            command_args[i] = pid;
        }
        i++;
    }
}

int execute_non_builtIn_command(char **args)
{
    pid_t pid, wpid;
    int status;
    pid = fork();

    if (pid < 0)
        perror("smallsh");
    else if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
            perror("smallsh");
        exit(EXIT_FAILURE);
    }
    else
        do
        {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    return 1;
}