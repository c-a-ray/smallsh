#include <stdio.h>
#include <stdlib.h>

#include "helpers.h"

void error_and_exit(char *msg)
{
    fprintf(stderr, "smallsh: %s\n", msg);
    exit(EXIT_FAILURE);
}