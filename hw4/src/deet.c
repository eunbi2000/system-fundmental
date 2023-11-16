#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>

#include "deet.h"
#include "debug.h"

char *read_input(int hide);
char compare_input(char *input);
char *get_first(char *input);
void start_new(char *input);
int check_argc(char *input);
void exit_deet();
void sigint_func();
void show_list();
char *get_rest(char *input);
void kill_process(pid_t pid);
int check_id_list();
void continue_process(int d_id);

static int deetid = 0;
static int process_size = 0;
static int *pid_list;
static PSTATE *pstate_list;
static char **name_list;

static void sigHandler(int sig, siginfo_t *info, void* context){
    if (sig == SIGINT) {
    	log_signal(SIGINT);
	    exit_deet();
	    fflush(stdout);
    }
    if (sig == SIGCHLD) {
    	// printf("si_code: %d si_status: %d\n", info->si_code, info->si_status);
    	if (info->si_code == CLD_EXITED) {
    	 	log_signal(SIGCHLD);
    		log_state_change(info->si_pid, PSTATE_RUNNING, PSTATE_DEAD, info->si_status);
			fprintf(stdout, "%d\t%d\t%s\t%s\t0x%x\t%s\n", deetid, info->si_pid, "T", "dead", info->si_status, name_list[deetid]);
			pstate_list[deetid] = PSTATE_DEAD;
    	}
    	else if (info->si_status == 0) {
    		// printf("!!!1!!!\n");
    		fprintf(stdout, "%d\t%d\t%s\t%s\t\t%s\n", deetid, info->si_pid, "T", "running", name_list[deetid]);
    		log_signal(SIGCHLD);
    		pstate_list[deetid] = PSTATE_DEAD;
    		log_state_change(info->si_pid, PSTATE_RUNNING, PSTATE_DEAD, info->si_status);
    		fprintf(stdout, "%d\t%d\t%s\t%s\t0x100\t%s\n", deetid, info->si_pid, "T", "dead", name_list[deetid]);
    	}
    	else if (info->si_code == CLD_TRAPPED) {
    		//for run
    		// printf("!!!2!!!\n");
    		fprintf(stdout, "%d\t%d\t%s\t%s\t\t%s\n", deetid, info->si_pid, "T", "running", name_list[deetid]);
    		log_signal(SIGCHLD);
    		log_state_change(info->si_pid, pstate_list[deetid], PSTATE_STOPPED, info->si_status);
    		fprintf(stdout, "%d\t%d\t%s\t%s\t\t%s\n", deetid, info->si_pid, "T", "stopped", name_list[deetid]);
    		pstate_list[deetid] = PSTATE_STOPPED;
    	}
    	else if (info->si_code == CLD_KILLED) { //make sure to pstate is original
    		//for kill
    		// printf("!!!3!!!\n");
    		log_state_change(info->si_pid, pstate_list[deetid], PSTATE_KILLED, info->si_status);
			fprintf(stdout, "%d\t%d\t%s\t%s\t\t%s\n", deetid, info->si_pid, "T", "killed", name_list[deetid]);
    		log_signal(SIGCHLD);
    		log_state_change(info->si_pid, PSTATE_KILLED, PSTATE_DEAD, info->si_status);
			fprintf(stdout, "%d\t%d\t%s\t%s\t0x%x\t%s\n", deetid, info->si_pid, "T", "dead", info->si_status, name_list[deetid]);
			fflush(stdout);
    	}
    }
}

int init(int hide) {
	log_startup();
    log_prompt();
    struct sigaction act = {0};
    act.sa_sigaction = sigHandler;
    act.sa_flags = SA_SIGINFO;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);
    pid_list = malloc(sizeof(int));
    pstate_list = malloc(sizeof(int));
    name_list = malloc(sizeof(int));

    int quit = 1;
    while (quit != 0) {
        char *input = read_input(hide);
        if (input == NULL) {
        	if (errno == EINTR) {
        		clearerr(stdin);
        		continue;
        	}
        	else {
        		return 0;
        	}
        }
        if (strcmp(input,"\n") == 0) {
            log_prompt();
        }
        else {
            input = strtok(input, "\n");
            char *first_command = get_first(input);
            int arg_counter = check_argc(input);

            switch(compare_input(first_command)){
                case 'h': //help
                    printf("Available commands:\nhelp -- Print this help message\nquit (<=0 args) -- Quit the program\n");
                    printf("show (<=1 args) -- Show process info\nrun (>=1 args) -- Start a process\nstop (1 args) -- Stop a running process\n");
                    printf("cont (1 args) -- Continue a stopped process\nrelease (1 args) -- Stop tracing a process, allowing it to continue normally\n");
                    printf("wait (1-2 args) -- Wait for a process to enter a specified state\nkill (1 args) -- Forcibly terminate a process\n");
                    printf("peek (2-3 args) -- Read from the address space of a traced process\npoke (3 args) -- Write to the address space of a traced process\n");
                    printf("bt (1 args) -- Show a stack trace for a traced process\n");
                    break;
                case 'q': //quit
                    if (arg_counter == 1) {
                        exit_deet();
                        quit = 0;
                    }
                    else {
                        goto error_case;
                    }
                    break;
                case 's': //show
                	if (arg_counter == 1) {
                		show_list();
                	}
                	else {
                		goto error_case;
                	}
                    break;
                case 'r': //run
                    if (arg_counter <= 1) {
                        goto error_case;
                    }
                    start_new(input);
                    break;
                case 'p': //stop
                    break;
                case 'c': //cont
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                	char *str = get_rest(input);
                    int did = atoi(str);
                    continue_process(did);
                    break;
                case 'e': //release
                    break;
                case 'w': //wait
                    break;
                case 'k': //kill
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                	char *string = get_rest(input);
                    int id = atoi(string);
                    kill_process(id);
                    break;
                case 'x': //peek
                    break;
                case 'z': //poke
                    break;
                case 'b': //bt
                    break;
                default:
                error_case:
                    log_error(input);
                    printf("?\n");
                    break;
            }
            free(first_command);
            free(input);
            log_prompt();
            fflush(stdout);
        }
    }
    return 0;
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
	else if (strcmp(input, "show") == 0) {
		return 's';
	}
	else if (strcmp(input, "kill") == 0) {
		return 'k';
	}
	else if (strcmp(input, "cont") == 0) {
		return 'c';
	}
	else {
		return 'n';
	}
}

void show_list() {
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] != PSTATE_DEAD) {
			fprintf(stdout,"%d\t%d\t%d\t\t%s\n", i, pid_list[i], pstate_list[i], name_list[i]);
		}
	}
}

char *read_input(int hide){
	char *input = '\0';
	size_t n = 0;
	if (hide == 0){
		fprintf(stdout, "deet> ");
	}
  	if (getline(&input, &n, stdin) == -1) {
  		return NULL;
  	}
  	log_input(input);
  	fflush(stdout);
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

char **make_string_arr(char *input, int argc) {
	int i = 0;
	char **var= (char**)(malloc (sizeof(char*) * (argc+1)));
	char *token = strtok(input, " ");
	while(token != NULL) {
		var[i] = strdup(token);
		token = strtok(NULL, " \0");
		i++;
	}
	var[i] = NULL;
	return var;
}

void put_new_into_list(pid_t pid, PSTATE pstate, char* name) {
	deetid = check_id_list();
	if (deetid == process_size) {
		pid_list = (int *)realloc(pid_list, (sizeof(int) * (deetid+1)));
		pstate_list = (PSTATE *)realloc(pstate_list, (sizeof(PSTATE) * (deetid+1)));
		name_list = (char **)realloc(name_list, (sizeof(char*) * (deetid+1)));
	}
	name_list[deetid] = (char*)malloc(sizeof(char) * strlen(name)+1);
	pid_list[deetid] = pid;
	pstate_list[deetid] = pstate;
	strcpy(name_list[deetid], name);
	if (deetid == process_size) {
		process_size++;
	}
}

int check_id_list() {
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] == PSTATE_DEAD) {
			return i;
		}
	}
	return process_size;
}

char *get_rest(char *input) {
	while (*input != 0 && *(input++) != ' ') {}
	return input;
}

void start_new(char *input) {
	char *string = get_rest(input);
	char *name = strdup(string);
	int argc = check_argc(string);
	char **rest = make_string_arr(string, argc); //includes first command
	pid_t new_process = fork();
	int status = 0;
	if (new_process < 0) {
		fprintf(stderr, "Making new child process failed");
	}
	else if (new_process == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		pid_t p = getpid();
		ptrace(PTRACE_TRACEME, p, NULL, NULL);
		log_state_change(p, PSTATE_NONE, PSTATE_RUNNING, status);
		if (execvp(rest[0], &rest[0]) ==-1) {
			exit(0);
		}
	}
	else {
		put_new_into_list(new_process, PSTATE_RUNNING, name);
		waitpid(new_process, &status, WUNTRACED);
	}
	int j =0;
	while(j < argc) {
		free(rest[j]);
		j++;
	}
	free(rest);
	free(name);
}

void continue_process(int d_id) {
	deetid = d_id;
	int status =0;
	log_state_change(pid_list[d_id], pstate_list[deetid], PSTATE_RUNNING, status);
	fprintf(stdout, "%d\t%d\t%s\t%s\t\t%s\n", deetid, pid_list[d_id], "T", "running", name_list[deetid]);
	pstate_list[deetid] = PSTATE_RUNNING;

	ptrace(PTRACE_CONT, pid_list[d_id], 1, 0);
}

void kill_process(int d_id) {
	sigset_t mask;
	deetid = d_id;
	pid_t pid = pid_list[d_id];
	kill(pid, SIGKILL);
	sigfillset(&mask);
	sigdelset(&mask, SIGCHLD);
	sigsuspend(&mask);
	pstate_list[d_id] = PSTATE_DEAD;
}

void exit_deet() {
	for (int i=0; i<process_size;i++) {
		if (pstate_list[i] != PSTATE_DEAD)
			kill_process(i);
	}
	free(pid_list);
	free(pstate_list);
	for (int i=0; i<deetid; i++) {
		free(name_list[i]);
	}
	free(name_list);
	log_shutdown();
	exit(0);
}