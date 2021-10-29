#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "builtIn.h"

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