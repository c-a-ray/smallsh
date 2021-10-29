#ifndef SMALLSH
#define SMALLSH

void smallsh(void);
char *read_command(void);
char **parse_command(char *);
int execute_command(char **);
int execute_non_builtIn_command(char **);

#endif