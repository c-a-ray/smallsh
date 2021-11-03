#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

bool cont = true;

void handle_SIGTSTP(int signo)
{
    // cont = false;
    char *msg = "You pressed Ctrl-Z!\n\n";
    write(STDOUT_FILENO, msg, 21);
}

int main(void)
{
    signal(SIGTSTP, handle_SIGTSTP);

    while (cont)
    {
        printf("hey\n");
        pause();
    }

    return 0;
}