#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#define PROMPT " atta microshell > \n"

typedef struct
{
    char *name;
    char *value;
    int exported;
} var_t;

static var_t *vars = NULL;
static int var_count = 0;
static int var_capacity = 0;

static var_t *
find_var(const char *name)
{
    int i = 0;
    while (i < var_count)
    {
        if (0 == strcmp(name, vars[i].name))
            return &vars[i];
        ++i;
    }
    return NULL;
}

static char *
my_strdup(const char *s)
{
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (NULL != d)
    {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}

static void
set_var(const char *name, const char *value, int exported)
{
    var_t *v = find_var(name);
    if (NULL != v)
    {
        free(v->value);
        v->value = my_strdup(value);
        if (exported)
            v->exported = 1;
    }
    else
    {
        if (var_count >= var_capacity)
        {
            var_capacity = (0 == var_capacity) ? 8 : var_capacity * 2;
            vars = (var_t *)realloc(vars, (size_t)var_capacity * sizeof(var_t));
        }
        vars[var_count].name = my_strdup(name);
        vars[var_count].value = my_strdup(value);
        vars[var_count].exported = exported;
        ++var_count;
    }

    if (exported || (NULL != v && v->exported))
        setenv(name, value, 1);
}

static void
free_vars(void)
{
    int i = 0;
    while (i < var_count)
    {
        free(vars[i].name);
        free(vars[i].value);
        ++i;
    }
    free(vars);
}

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

static char *
my_strndup(const char *s, size_t n)
{
    char *d = (char *)malloc(n + 1);
    if (NULL != d)
    {
        memcpy(d, s, n);
        d[n] = '\0';
    }
    return d;
}

static char *
substitute_one(const char *arg)
{
    char *result = (char *)malloc(1);
    if (NULL == result)
        return NULL;
    result[0] = '\0';

    while ('\0' != *arg)
    {
        if ('$' == *arg)
        {
            ++arg;
            const char *name_start = arg;
            while ('\0' != *arg && (isalnum((unsigned char)*arg) || '_' == *arg))
                ++arg;
            size_t name_len = (size_t)(arg - name_start);

            if (0 < name_len)
            {
                char *name = my_strndup(name_start, name_len);
                var_t *v = find_var(name);
                free(name);

                if (NULL != v)
                {
                    size_t old_len = strlen(result);
                    size_t val_len = strlen(v->value);
                    char *tmp = (char *)realloc(result, old_len + val_len + 1);
                    if (NULL == tmp)
                    {
                        free(result);
                        return NULL;
                    }
                    result = tmp;
                    memcpy(result + old_len, v->value, val_len);
                    result[old_len + val_len] = '\0';
                }
            }
            else
            {
                size_t old_len = strlen(result);
                char *tmp = (char *)realloc(result, old_len + 2);
                if (NULL == tmp)
                {
                    free(result);
                    return NULL;
                }
                result = tmp;
                result[old_len] = '$';
                result[old_len + 1] = '\0';
            }
        }
        else
        {
            size_t old_len = strlen(result);
            char *tmp = (char *)realloc(result, old_len + 2);
            if (NULL == tmp)
            {
                free(result);
                return NULL;
            }
            result = tmp;
            result[old_len] = *arg;
            result[old_len + 1] = '\0';
            ++arg;
        }
    }

    return result;
}

static char **
substitute_vars(char **args, int *argc)
{
    int count = *argc;
    char **new_args = (char **)malloc(((size_t)count + 1) * sizeof(char *));
    if (NULL == new_args)
        return NULL;

    int i = 0;
    while (i < count)
    {
        new_args[i] = substitute_one(args[i]);
        if (NULL == new_args[i])
        {
            int j = 0;
            while (j < i)
            {
                free(new_args[j]);
                ++j;
            }
            free(new_args);
            return NULL;
        }
        ++i;
    }
    new_args[count] = NULL;

    return new_args;
}

static void
close_redirects(int fd_in, int fd_out, int fd_err)
{
    if (0 <= fd_in)
        close(fd_in);
    if (0 <= fd_out)
        close(fd_out);
    if (0 <= fd_err)
        close(fd_err);
}

static int
parse_redirects(char **args, int *argc, int *fd_in, int *fd_out, int *fd_err,
               int *saved_stdout, int *saved_stderr)
{
    int i = 0;
    int j = 0;
    int s_out = -1;
    int s_err = -1;

    *fd_in = -1;
    *fd_out = -1;
    *fd_err = -1;
    *saved_stdout = -1;
    *saved_stderr = -1;

    while (i < *argc)
    {
        char *token = args[i];
        int is_in  = (0 == strcmp("<", token));
        int is_out = (0 == strcmp(">", token));
        int is_err = (0 == strcmp("2>", token));

        if ((is_in || is_out || is_err) && i + 1 < *argc)
        {
            char *filename = args[i + 1];

            if (is_in)
            {
                if (0 <= *fd_in) { if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(*fd_in, *fd_out, *fd_err); return -1; }
                *fd_in = open(filename, O_RDONLY);
                if (0 > *fd_in) { fprintf(stderr, "cannot access %s: %s\n", filename, strerror(errno)); if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(-1, *fd_out, *fd_err); return -1; }
            }
            else if (is_out)
            {
                if (0 <= *fd_out) { if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(*fd_in, *fd_out, *fd_err); return -1; }
                if (0 > s_out) s_out = dup(1);
                *fd_out = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (0 > *fd_out) { perror(filename); if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(*fd_in, -1, *fd_err); return -1; }
                dup2(*fd_out, 1);
            }
            else
            {
                if (0 <= *fd_err) { if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(*fd_in, *fd_out, *fd_err); return -1; }
                if (0 > s_err) s_err = dup(2);
                *fd_err = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (0 > *fd_err) { perror(filename); if (0 <= s_out) { dup2(s_out, 1); close(s_out); } if (0 <= s_err) { dup2(s_err, 2); close(s_err); } close_redirects(*fd_in, *fd_out, -1); return -1; }
                dup2(*fd_err, 2);
            }

            ++i;
        }
        else
        {
            args[j] = token;
            ++j;
        }

        ++i;
    }

    *saved_stdout = s_out;
    *saved_stderr = s_err;
    args[j] = NULL;
    *argc = j;
    return 0;
}

static void
apply_redirects(int fd_in, int fd_out, int fd_err)
{
    if (0 <= fd_in) { dup2(fd_in, 0); close(fd_in); }
    if (0 <= fd_out) close(fd_out);
    if (0 <= fd_err) close(fd_err);
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
exec_external(char **args, int fd_in, int fd_out, int fd_err)
{
    pid_t pid = fork();

    if (0 > pid)
    {
        perror("fork");
        close_redirects(fd_in, fd_out, fd_err);
        return EXIT_FAILURE;
    }

    if (0 == pid)
    {
        apply_redirects(fd_in, fd_out, fd_err);
        execvp(args[0], args);
        fprintf(stderr, "%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    }

    close_redirects(fd_in, fd_out, fd_err);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return EXIT_FAILURE;
}

int
microshell_main(int argc, char **argv)
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

        {
            int has_eq_token = 0;
            int i = 0;
            while (i < arg_count)
            {
                if (0 == strcmp("=", args[i]))
                {
                    has_eq_token = 1;
                    break;
                }
                ++i;
            }
            if (has_eq_token)
            {
                printf("Invalid command\n");
                fflush(stdout);
                last_status = EXIT_FAILURE;
                free_args(args);
                continue;
            }
        }

        if (NULL != strchr(args[0], '='))
        {
            if (1 < arg_count)
            {
                printf("Invalid command\n");
                fflush(stdout);
                last_status = EXIT_FAILURE;
            }
            else
            {
                char *eq = strchr(args[0], '=');
                if (eq == args[0])
                {
                    printf("Invalid command\n");
                    fflush(stdout);
                    last_status = EXIT_FAILURE;
                }
                else
                {
                    *eq = '\0';
                    set_var(args[0], eq + 1, 0);
                    last_status = EXIT_SUCCESS;
                }
            }
            free_args(args);
            continue;
        }

        {
            char **sub_args = substitute_vars(args, &arg_count);
            free_args(args);
            args = sub_args;

            if (NULL == args || 0 == arg_count || '\0' == args[0][0])
            {
                free_args(args);
                continue;
            }
        }

        {
            int fd_in = -1;
            int fd_out = -1;
            int fd_err = -1;
            int saved_out = -1;
            int saved_err = -1;

            if (0 > parse_redirects(args, &arg_count, &fd_in, &fd_out, &fd_err, &saved_out, &saved_err))
            {
                close_redirects(fd_in, fd_out, fd_err);
                free_args(args);
                last_status = EXIT_FAILURE;
                continue;
            }

            if (0 == arg_count)
            {
                if (0 <= saved_out) { dup2(saved_out, 1); close(saved_out); }
                if (0 <= saved_err) { dup2(saved_err, 2); close(saved_err); }
                close_redirects(fd_in, fd_out, fd_err);
                free_args(args);
                continue;
            }

            if (0 == strcmp("exit", args[0]))
            {
                if (0 <= saved_out) { dup2(saved_out, 1); close(saved_out); }
                if (0 <= saved_err) { dup2(saved_err, 2); close(saved_err); }
                close_redirects(fd_in, fd_out, fd_err);
                if (NULL != args[1])
                    last_status = atoi(args[1]);
                free_args(args);
                free_vars();
                free(buffer);
                printf("Good Bye\n");
                fflush(stdout);
                return last_status;
            }
            else if (0 == strcmp("export", args[0]))
            {
                if (NULL != args[1])
                {
                    var_t *v = find_var(args[1]);
                    if (NULL != v)
                    {
                        v->exported = 1;
                        setenv(v->name, v->value, 1);
                        last_status = EXIT_SUCCESS;
                    }
                    else
                    {
                        last_status = EXIT_FAILURE;
                    }
                }
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
                last_status = exec_external(args, fd_in, fd_out, fd_err);
            }

            if (0 <= saved_out) { dup2(saved_out, 1); close(saved_out); }
            if (0 <= saved_err) { dup2(saved_err, 2); close(saved_err); }
            close_redirects(fd_in, fd_out, fd_err);
            free_args(args);
        }
    }

    free_vars();
    free(buffer);
    return last_status;
}
