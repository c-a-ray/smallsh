#ifndef SMALLSH
#define SMALLSH

#define TOKEN_BUFFER_SIZE 64
#define TOKEN_DELIMITER " \n\a\r\t"

void smallsh(void);
char *read_command(void);
char **parse_command(char *);
int execute_command(char **);
int execute_non_builtIn_command(char **);

#endif