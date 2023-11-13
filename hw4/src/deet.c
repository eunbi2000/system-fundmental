#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "deet.h"

static int deetid = 0;

char *read_input(){
	char *prompt = "deet> ";
	char *input = '\0';
	size_t n = 0;
	fprintf(stdout, "%s", prompt);
  	if (getline(&input, &n, stdin) == -1) {
  		return read_input();
  	}
  	log_input(input);
  	return input;
}

int check_argc(char *input) {
	char *new_input = strdup(input);
	char *word = strtok(new_input, " ");
	int c = 0;
    while (word != NULL){
        c++;
        word = strtok(NULL, " ");
    }
    free(new_input);
    return c;
}

char *get_first(char *input) {
	char *new_input = strdup(input);
	char *word = strtok(new_input, " ");
	return word;
}

char compare_input(char *input) {
	if (strcmp(input, "quit") == 0) {
		return 'q';
	}
	else if (strcmp(input, "help") == 0) {
		return 'h';
	}
	else if (strcmp(input, "run") == 0) {
		return 'r';
	}
	else {
		return 'n';
	}
}

char **get_rest_command(char *input) {
	int i = 0;
	char **var= (char**)(malloc (sizeof(char) * strlen(input)+1));
	var[i] = strtok(input, " ");
	while(var[i] != NULL) {
		var[++i] = strtok(NULL, " ");
	}
	return var;
}

void start_new(char *input) {
	char **rest = get_rest_command(input); //includes first command
	int i=0;
	pid_t new_process = fork();
	int status = 0;
	if (new_process < 0) {
		fprintf(stderr, "Making new child process failed");
	}
	else if (new_process == 0) {
		dup2(STDOUT_FILENO, STDERR_FILENO);
		pid_t p = getpid();
		ptrace(PTRACE_TRACEME, p, NULL, NULL);
		log_state_change(p, PSTATE_NONE, PSTATE_RUNNING, status);
		fprintf(stdout, "%d\t%d\t%s\t%s\t", deetid, p, "T", "running");
		while(rest[i] != NULL) {
			fprintf(stdout, "%s ",rest[i]);
			i++;
		}
		fprintf(stdout, "\n");
		execvp(rest[1], rest);
	}
	else {
		waitpid(new_process, &status, WUNTRACED);
		kill(new_process, SIGSTOP);
		log_signal(SIGCHLD);
		log_state_change(new_process, PSTATE_RUNNING, PSTATE_STOPPED, status);
	}
	free(rest);
}
