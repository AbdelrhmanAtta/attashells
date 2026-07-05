#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define PROMPT " atta picoshell > \n"

static void
free_args_partial(char **args, int count)
{
    if (NULL == args)
        return;
    int i = 0;
    while (i < count)
    {
        free(args[i]);
        ++i;
    }
    free(args);
}

static void
free_args(char **args)
{
    if (NULL == args)
        return;
    int i = 0;
    while (NULL != args[i])
    {
        free(args[i]);
        ++i;
    }
    free(args);
}

static char **
parse_line(char *line, int *argc)
{
    int capacity = 8;
    int count = 0;
    char **args = (char **)malloc((size_t)capacity * sizeof(char *));

    if (NULL == args)
        return NULL;

    while (' ' == *line)
        ++line;

    while ('\0' != *line)
    {
        if (count >= capacity - 1)
        {
            capacity *= 2;
            char **tmp = (char **)realloc(args, (size_t)capacity * sizeof(char *));
            if (NULL == tmp)
            {
                free_args_partial(args, count);
                return NULL;
            }
            args = tmp;
        }

        char *end = line;
        while ('\0' != *end && ' ' != *end)
            ++end;

        size_t tok_len = (size_t)(end - line);
        args[count] = (char *)malloc(tok_len + 1);
        if (NULL == args[count])
        {
            free_args_partial(args, count);
            return NULL;
        }
        memcpy(args[count], line, tok_len);
        args[count][tok_len] = '\0';
        ++count;

        line = end;
        while (' ' == *line)
            ++line;
    }

    args[count] = NULL;
    *argc = count;
    return args;
}

static int
cmd_echo(char **args)
{
    int i = 1;
    while (NULL != args[i])
    {
        if (1 != i)
            printf(" ");
        printf("%s", args[i]);
        ++i;
    }
    printf("\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}

static int
cmd_pwd(void)
{
    char *cwd = getcwd(NULL, 0);
    if (NULL != cwd)
    {
        printf("%s\n", cwd);
        fflush(stdout);
        free(cwd);
        return EXIT_SUCCESS;
    }
    else
    {
        perror("pwd");
        return EXIT_FAILURE;
    }
}

static int
cmd_cd(char **args)
{
    const char *path = args[1];

    if (NULL == path || 0 == strcmp("~", path))
    {
        path = getenv("HOME");
        if (NULL == path)
            path = "/";
    }

    if (0 > chdir(path))
    {
        fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
exec_external(char **args)
{
    pid_t pid = fork();

    if (0 > pid)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (0 == pid)
    {
        execvp(args[0], args);
        fprintf(stderr, "%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return EXIT_FAILURE;
}

int
picoshell_main(int argc, char **argv)
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

        if (0 > len)
        {
            break;
        }

        if (0 < len && '\n' == buffer[len - 1])
            buffer[len - 1] = '\0';

        int arg_count = 0;
        char **args = parse_line(buffer, &arg_count);

        if (NULL == args || 0 == arg_count)
        {
            free_args(args);
            continue;
        }

        if (0 == strcmp("exit", args[0]))
        {
            free_args(args);
            printf("Good Bye\n");
            fflush(stdout);
            break;
        }
        else if (0 == strcmp("echo", args[0]))
        {
            last_status = cmd_echo(args);
        }
        else if (0 == strcmp("pwd", args[0]))
        {
            last_status = cmd_pwd();
        }
        else if (0 == strcmp("cd", args[0]))
        {
            last_status = cmd_cd(args);
        }
        else
        {
            last_status = exec_external(args);
        }

        free_args(args);
    }

    free(buffer);
    return last_status;
}
