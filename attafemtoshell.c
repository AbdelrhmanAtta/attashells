#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROMPT " atta femtoshell > "

#define STATUS_SUCCESS  0
#define STATUS_EXIT     1
#define STATUS_INVALID  2
#define STATUS_EMPTY    3

static void
cmd_echo(char *input)
{
    char *text = input + 4;
    while (' ' == *text)
        ++text;
    printf("%s\n", text);
    fflush(stdout);
}

static int
input_handler(char *command)
{
    char *newline = strchr(command, '\n');
    if (NULL != newline)
        *newline = '\0';

    while (' ' == *command)
        ++command;

    if ('\0' == *command)
        return STATUS_EMPTY;

    if (0 == strncmp("exit", command, 4) && (('\0' == command[4]) || ' ' == command[4]))
    {
        printf("Good Bye\n");
        fflush(stdout);
        return STATUS_EXIT;
    }
    else if (0 == strncmp("echo", command, 4) && (('\0' == command[4]) || ' ' == command[4]))
    {
        cmd_echo(command);
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID;
}

int
femtoshell_main(int argc, char **argv)
{
    char *buffer = NULL;
    size_t capacity = 0;
    ssize_t len;
    int last_status = EXIT_SUCCESS;

    (void)argc;
    (void)argv;

    for (;;)
    {
        printf("%s", PROMPT);
        fflush(stdout);

        len = getline(&buffer, &capacity, stdin);

        if (len < 0)
            break;

        int status = input_handler(buffer);

        if (STATUS_EXIT == status)
        {
            break;
        }
        else if (STATUS_INVALID == status)
        {
            printf("Invalid command\n");
            fflush(stdout);
            last_status = EXIT_FAILURE;
        }
        else if (STATUS_SUCCESS == status)
        {
            last_status = EXIT_SUCCESS;
        }
    }

    free(buffer);

    return last_status;
}
