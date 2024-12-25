// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	if (dir && dir->next_word == NULL)
		return chdir(dir->string);

	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	return SHELL_EXIT;
}

char *concatenate_parts(word_t *s)
{
	word_t *has_next_part = s->next_part;

	while (has_next_part) {
		if (has_next_part->expand) {
			char *expanded_env = getenv(has_next_part->string);

			if (expanded_env)
				strcat(s->string, getenv(has_next_part->string));
		} else {
			strcat(s->string, has_next_part->string);
		}
		has_next_part = has_next_part->next_part;
	}
	return s->string;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	DIE(s == NULL || s->up == NULL, "Invalid command format");

	if (strcmp(s->verb->string, "cd") == 0) {
		if (s->out) {
			int fout = open(s->out->string, O_CREAT, 0744);

			close(fout);
		}
		if (s->err) {
			int ferr = open(s->out->string, O_CREAT, 0744);

			close(ferr);
		}
		return shell_cd(s->params);
	}

	if (strcmp(s->verb->string, "exit") == 0 || strcmp(s->verb->string, "quit") == 0)
		return shell_exit();

	int size = 0;
	char **args = get_argv(s, &size);

	if (s->verb->next_part) {
		char *name = s->verb->string;
		char *value = strchr(args[0], '=') + 1;

		return setenv(name, value, 1);
	}

	int status;

	pid_t pid = fork();

	DIE(pid == -1, "fork() error");

	if (pid == 0) {
		int fin, fout, ferr;

		if (s->in) {
			fin = open(concatenate_parts(s->in), O_RDONLY, 0744);
			dup2(fin, STDIN_FILENO);
			close(fin);
		}
		if (s->out) {
			fout = open(concatenate_parts(s->out), O_CREAT | O_WRONLY | s->out->io_flag, 0744);
			dup2(fout, STDOUT_FILENO);
			close(fout);
		}
		if (s->err) {
			if (s->out && strcmp(s->err->string, s->out->string) == 0) {
				dup2(STDOUT_FILENO, STDERR_FILENO);
			} else {
				ferr = open(concatenate_parts(s->err), O_CREAT | O_WRONLY | s->err->io_flag, 0744);
				dup2(ferr, STDERR_FILENO);
				close(ferr);
			}
		}
		execvp(args[0], args);

		printf("Execution failed for '%s'\n", s->verb->string);
		exit(EXIT_FAILURE);
	}

	pid_t wait_ret = waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return 0;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int status;

	pid_t pid1 = fork();

	DIE(pid1 == -1, "fork() error");

	if (pid1 == 0)
		exit(parse_command(cmd1, level, father));

	pid_t pid2 = fork();

	DIE(pid2 == -1, "fork() error");

	if (pid2 == 0)
		exit(parse_command(cmd2, level, father));


	pid_t wait_ret1 = waitpid(pid1, &status, 0);
	pid_t wait_ret2 = waitpid(pid2, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return 0;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int pipe_fd[2];
	int ret = pipe(pipe_fd);

	DIE(ret == -1, "pipe() error");

	int status;

	pid_t pid1 = fork();

	DIE(pid1 == -1, "fork() error");

	if (pid1 == 0) {
		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[0]);
		close(pipe_fd[1]);

		exit(parse_command(cmd1, level, father));
	}

	pid_t pid2 = fork();

	DIE(pid2 == -1, "fork() error");

	if (pid2 == 0) {
		dup2(pipe_fd[0], STDIN_FILENO);
		close(pipe_fd[0]);
		close(pipe_fd[1]);

		exit(parse_command(cmd2, level, father));
	}

	close(pipe_fd[0]);
	close(pipe_fd[1]);

	pid_t wait_ret1 = waitpid(pid1, &status, 0);
	pid_t wait_ret2 = waitpid(pid2, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return 0;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	DIE(c == NULL, "No command found");

	if (c->op == OP_NONE)
		return parse_simple(c->scmd, level, father);

	int ret1, ret2;

	switch (c->op) {
	case OP_SEQUENTIAL:
		ret1 = parse_command(c->cmd1, level, father);
		ret2 = parse_command(c->cmd2, level, father);
		break;

	case OP_PARALLEL:
		return run_in_parallel(c->cmd1, c->cmd2, level + 1, father);

	case OP_CONDITIONAL_NZERO:
		ret1 = parse_command(c->cmd1, level + 1, father);

		if (ret1 != 0)
			ret2 = parse_command(c->cmd2, level + 1, father);

		return ret2;

	case OP_CONDITIONAL_ZERO:
		ret1 = parse_command(c->cmd1, level + 1, father);

		if (ret1 == 0)
			ret2 = parse_command(c->cmd2, level + 1, father);

		return ret2;

	case OP_PIPE:
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, father);

	default:
		return SHELL_EXIT;
	}

	return 0;
}
