# Minishell

A minimalist shell implementation written in C for a university assignment. This project mimics basic Bash behavior by implementing key shell functionalities, including built-in commands, process control, I/O redirection, pipes, and conditional/parallel execution.

## Features

- **Built-in Commands:**
  - Change Directory (`cd`)
  - Print Working Directory (`pwd`)
  - Exit the shell (`exit`/`quit`)
- **External Commands:**
  - Execution by creating a child process using `fork()` and `execvp()`
- **Environment Variables:**
  - Expansion of variables (e.g., using `$NAME`)
- **Command Operators:**
  - Sequential operator (`;`)
  - Parallel operator (`&`)
  - Pipe operator (`|`)
  - Conditional execution (`&&` and `||`)
- **I/O Redirection:**
  - Standard input redirection (`< filename`)
  - Standard output redirection (`> filename` and append mode `>> filename`)
  - Standard error redirection (`2> filename` and append mode `2>> filename`)
  - Combined redirection (`&> filename`)

## Building the Project

The Minishell code is located in the [`src`](src) directory. A Makefile is provided for building the executable and running tests.

To build the shell:

```bash
make -C src
```

This compiles the core files ([`main.c`](src/main.c), [`cmd.c`](src/cmd.c), [`utils.c`](src/utils.c)) into the `mini-shell` executable.

You can also run the automated test suite from the [`tests`](tests) directory:

```bash
cd tests
make check
```

## Interactive Mode Usage

Once built, running the shell will display a prompt (`> `) for entering commands. The shell accepts commands similar to a standard Bash shell. For example:

- **Changing Directory:**

```sh
> cd /usr/lib
> pwd
/usr/lib
```

- **Running an Application:**

```sh
> ./sum 2 4 1
7
```

- **Using Environment Variables:**

```sh
> NAME="John Doe"
> ./identify $NAME $LOCATION $AGE
```
(Undefined variables expand to an empty string.)

- **Command Chaining and Operators:**

```sh
> echo "Hello" ; echo "World" && false || echo "Alternate"
Hello
World
Alternate
```

- **I/O Redirection and Pipes:**

```sh
> echo "text" > file.txt
> cat < file.txt | grep "text"
text
```

## Implementation Details

- The shell’s parser is built using a custom parser in the `util/parser/` directory that tokenizes and builds a command tree (see [`parser.h`](util/parser/parser.h)).
- Each command is represented as a tree node in structure [`command_t`](src/cmd.h) and processed recursively in [`cmd.c`](src/cmd.c).
- Built-in commands such as `cd` and `exit` are handled differently from external commands.
- Process creation and I/O redirection use standard UNIX system calls such as `fork()`, `execvp()`, `open()`, `dup2()`, and `waitpid()`.
- Memory management is performed carefully with dynamic allocation in the parser and in utility functions ([`utils.c`](src/utils.c)).

## Memory Management

- Dynamic memory is allocated for string parts and parse structures.
- The function [`free_parse_memory()`](util/parser/parser.y) in [`parser.y`](util/parser/parser.y) (invoked from [`main.c`](src/main.c) after each command) ensures that all allocated memory is freed.

## Example Usage

```bash
$ ./mini-shell
> pwd
/home/student
> cd operating-systems/assignments/minishell
> pwd
/home/student/operating-systems/assignments/minishell
> NAME="Student"
> echo "Hello" && echo $NAME
Hello
Student
> false && echo "Won't run" || echo "Works instead"
Works instead
> quit
```

## Technical Notes

- The shell supports mixed operators with the following prioritization:
  1. Pipe (`|`)
  2. Conditional operators (`&&` and `||`)
  3. Parallel operator (`&`)
  4. Sequential operator (`;`)
- Redirection operators support both creation and append modes.
- Each external command is executed in a separate process, ensuring that the Minishell remains responsive.
- The automated checker and linter are integrated into the repository and can be run locally via the [`local.sh`](local.sh) script.

The project structure is as follows:

- [`src`](src) – Contains the core shell implementation.
- [`checker`](checker) – High-level script for checking and grading.
- [`tests`](tests) – Automated tests and grading scripts.
- [`util/parser`](util/parser) – Lexer and parser implementation for command processing.