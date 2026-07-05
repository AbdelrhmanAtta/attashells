# attashells

A progressive series of Unix shells written in C — from a two-command toy to a fully-featured shell with variables, I/O redirection, and environment management.

## Shells

| Shell | File | Features |
|-------|------|----------|
| **Femto Shell** | `attafemtoshell.c` | `echo`, `exit`, invalid command detection |
| **Pico Shell** | `attapicoshell.c` | + `pwd`, `cd`, external command execution (`fork`/`execvp`) |
| **Nano Shell** | `attananoshell.c` | + shell variables, `export`, `$variable` substitution |
| **Micro Shell** | `attamicroshell.c` | + I/O redirection (`<`, `>`, `2>`), multiple redirects per command |

## Compilation

All shells compile with zero warnings under `-Wall -Wextra`:

```sh
# Femto Shell
gcc -Wall -Wextra -o attafemtoshell attafemtoshell.c

# Pico Shell
gcc -Wall -Wextra -o attapicoshell attapicoshell.c

# Nano Shell
gcc -Wall -Wextra -o attananoshell attananoshell.c

# Micro Shell
gcc -Wall -Wextra -o attamicroshell attamicroshell.c
```

## Example Output

### Femto Shell

```
$ ./attafemtoshell
💻 atta@femtoshell➡ echo Hello my shell
Hello my shell
💻 atta@femtoshell➡ ls
Invalid command
💻 atta@femtoshell➡ exit
Good Bye
```

### Pico Shell

```
$ ./attapicoshell
 atta picoshell > echo Hello World
Hello World
 atta picoshell > cd /tmp
 atta picoshell > pwd
/tmp
 atta picoshell > ls -la
total 1234 ...
 atta picoshell > exit
Good Bye
```

### Nano Shell

```
$ ./attananoshell
 atta nanoshell > x=5
 atta nanoshell > echo $x
5
 atta nanoshell > folder=home
 atta nanoshell > ls /$folder
# lists /home directory
 atta nanoshell > x = 5
Invalid command
 atta nanoshell > export x
 atta nanoshell > exit
Good Bye
```

### Micro Shell

```
$ ./attamicroshell
 atta microshell > x=5
 atta microshell > echo Hello > /tmp/out.txt
 atta microshell > cat /tmp/out.txt
Hello
 atta microshell > wc -c < /tmp/out.txt
6
 atta microshell > ls /root 2> /dev/null
 atta microshell > exit
Good Bye
```

## Design

- **Dynamic argument parsing** — flexible array allocation, zero memory leaks
- **External commands** — `fork`/`execvp` with PATH search and `waitpid` exit-status tracking
- **Variable storage** — `realloc`-grown array with `setenv` for exported vars
- **Input redirection** — `open` in parent (catches errors before forking), `dup2` in child or parent for immediate streaming
- **Error messages** — `cannot access` for input (`<`), raw `perror` for output (`>`, `2>`)
- **Exit status** — tracks last command's return code; `exit N` supported

## Coding style

- Yoda conditions (`NULL == ptr`)
- Consistent function signatures
- C89-compatible with `-Wall -Wextra -pedantic`
