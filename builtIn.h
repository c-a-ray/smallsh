#ifndef BUILT_IN
#define BUILT_IN

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

char *built_in_commands[] = {"cd", "exit"};

int get_num_built_in_commands()
{
    return sizeof(built_in_commands) / sizeof(char *);
}

int smallsh_cd(char **args)
{
    if (args[1] == NULL)
        fprintf(stderr, "smallsh: missing argument\n");
    else if (chdir(args[1]) != 0)
        perror("smallsh");
    return 1;
}

int smallsh_exit(char **args)
{
    return 0;
}

#endif