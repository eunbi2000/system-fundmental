#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/user.h>

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
int kill_process(pid_t pid);
int check_id_list();
int continue_process(int d_id);
int stop_process(int d_id);
int wait_process(int d_id, char* state_change);
void put_new_into_list(pid_t pid, PSTATE pstate, char* name);
int release_process(int d_id);
int peek_process(int d_id, char* add, int val);
int poke_process(int d_id, char* add, char* val);
int bt_process(int d_id, int iter);


static int deetid = 0;
static int process_size = 0;
static int *pid_list;
static PSTATE *pstate_list;
static char **name_list;
static int *status_list;
static char *traced_list;
static int run_bool = 0;

void sigHandler(int sig, siginfo_t *info, void* context){
    if (sig == SIGINT) {
    	log_signal(SIGINT);
	    exit_deet();
	    fflush(stdout);
    }
    if (sig == SIGCHLD) {
    	// printf("si_code: %d sig_status: %d\n", info->si_code, info->si_status);
    	if (info->si_status == 99) {
    		//for error with execvp
    		// printf("!!!2!!!\n");
    		fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, info->si_pid, traced_list[deetid], "running", name_list[deetid]);
    		log_signal(SIGCHLD);
    		log_state_change(info->si_pid, PSTATE_RUNNING, PSTATE_DEAD, info->si_status);
    		fprintf(stdout, "%d\t%d\t%c\t%s\t0x100\t%s\n", deetid, info->si_pid, traced_list[deetid], "dead",name_list[deetid]);
    		pstate_list[deetid] = PSTATE_DEAD;
    		status_list[deetid] = 0x100;
    	}
    	else if (info->si_code == CLD_EXITED) {
    		//for cont
    	 	log_signal(SIGCHLD);
    		log_state_change(info->si_pid, pstate_list[deetid], PSTATE_DEAD, info->si_status);
			fprintf(stdout, "%d\t%d\t%c\t%s\t0x%x\t%s\n", deetid, info->si_pid, traced_list[deetid], "dead", info->si_status, name_list[deetid]);
			pstate_list[deetid] = PSTATE_DEAD;
			status_list[deetid] = info->si_status;
    	}
    	else if (info->si_code == CLD_TRAPPED || info->si_code == CLD_STOPPED) {
    		//for run
    		// printf("!!!2!!!\n");
    		// printf("si_code: %d si_status: %d, pid: %d\n", info->si_code, info->si_status, info->si_pid);
    		if (run_bool == 1) {
    			log_state_change(info->si_pid, PSTATE_NONE, PSTATE_RUNNING, info->si_status);
				fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, pid_list[deetid], traced_list[deetid], "running", name_list[deetid]);
    		}
    		log_signal(SIGCHLD);
    		log_state_change(info->si_pid, pstate_list[deetid], PSTATE_STOPPED, info->si_status);
    		fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, info->si_pid, traced_list[deetid], "stopped", name_list[deetid]);
    		pstate_list[deetid] = PSTATE_STOPPED;
    		run_bool = 0;
    	}
    	else if (info->si_code == CLD_KILLED) { //make sure to pstate is original
    		//for kill
    		// printf("!!!3!!!\n");
    		log_signal(SIGCHLD);
    		log_state_change(info->si_pid, PSTATE_KILLED, PSTATE_DEAD, info->si_status);
			fprintf(stdout, "%d\t%d\t%c\t%s\t0x%x\t%s\n", deetid, info->si_pid, traced_list[deetid], "dead", info->si_status, name_list[deetid]);
			status_list[deetid] =info->si_status;
			pstate_list[deetid] =PSTATE_DEAD;
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
    status_list = malloc(sizeof(int));
    name_list = malloc(sizeof(int));
    traced_list = malloc(sizeof(char));

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
            char *rest_command = get_rest(input);
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
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                    int i = atoi(rest_command);
                    if (stop_process(i) == -1) {
                    	goto error_case;
                    }
                    break;
                case 'c': //cont
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                    int did = atoi(rest_command);
                    if (continue_process(did) == -1) {
                		goto error_case;
                	}
                    break;
                case 'e': //release
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                    int j = atoi(rest_command);
                    if (release_process(j) == -1) {
                		goto error_case;
                	}
                    break;
                case 'w': //wait
                	if (arg_counter < 2 || arg_counter > 3) {
                		goto error_case;
                	}
                	// int status =0;
            		char *first_arg = get_first(rest_command);
                	int first = atoi (first_arg);
                	char *second_arg = get_rest(rest_command);
                	if (wait_process(first, second_arg) == -1) {
                		goto error_case;
                	}
                    break;
                case 'k': //kill
                	if (arg_counter != 2) {
                		goto error_case;
                	}
                    int id = atoi(rest_command);
                    if (kill_process(id) == -1) {
                		goto error_case;
                	}
                    break;
                case 'x': //peek
                	if (arg_counter < 3 || arg_counter > 4) {
                		goto error_case;
                	}
                	int x = atoi (get_first(rest_command));
                	char *g = get_rest(rest_command);
                	char *hex = get_first(g);
                	int val;
                	if (strcmp(get_rest(g), "") == 0) {
                		val = -1;;
                	}
                	else {
                		val = atoi(get_rest(g));
                	}
                	if (peek_process(x, hex, val) == -1) {
                		goto error_case;
                	}
                    break;
                case 'z': //poke
                	if (arg_counter != 4) {
                		goto error_case;
                	}
                	int dee_id = atoi (get_first(rest_command));
                	char *st = get_rest(rest_command);
                	char *add = get_first(st);
                	char *nya = get_rest(st);
                	if (poke_process(dee_id, add, nya) == -1) {
                		goto error_case;
                	}
                    break;
                case 'b': //bt
                	if (arg_counter < 2 || arg_counter > 3) {
                		goto error_case;
                	}
                	int nyaha = atoi(get_first(rest_command));
                	int iter = atoi(get_rest(rest_command));
                	if (bt_process(nyaha, iter) == -1) {
                		goto error_case;
                	}
                    break;
                default:
                error_case:
                    log_error(first_command);
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
	else if (strcmp(input, "wait") == 0) {
		return 'w';
	}
	else if (strcmp(input, "stop") == 0) {
		return 'p';
	}
	else if (strcmp(input, "release") == 0) {
		return 'e';
	}
	else if (strcmp(input, "peek") == 0) {
		return 'x';
	}
	else if (strcmp(input, "poke") == 0) {
		return 'z';
	}
	else if (strcmp(input, "bt") == 0) {
		return 'b';
	}
	else {
		return 'n';
	}
}

int peek_process(int d_id, char* add, int val) {
	if (d_id < 0 || d_id > process_size) {
		return -1;
	}
	int ctr = 0;
	long address = strtol(add, NULL, 16);
	while (ctr < val) {
		long returned_val = ptrace(PTRACE_PEEKDATA, pid_list[d_id], address+(ctr*sizeof(long)));
		if (returned_val == -1) {
			return -1;
		}
		fprintf(stdout,"%016lx\t%016lx\n", address+(ctr*sizeof(long)), returned_val);
		ctr++;
	}
	return 0;
}

int poke_process(int d_id, char* add, char* val) {
	if (d_id < 0 || d_id > process_size) {
		return -1;
	}
	long address = strtol(add, NULL, 16);
	long value = strtol(val, NULL, 16);
	if (address == 0) {
		return -1;
	}
	long returned_val = ptrace(PTRACE_POKEDATA, pid_list[d_id], address, value);
	if (returned_val == -1) {
		return -1;
	}
	return 0;
}

int bt_process(int d_id, int iter) {
	struct user_regs_struct regs;
	int i =0;
	if (iter == 0) {
		iter = 10;
	}
	long temp = ptrace(PTRACE_GETREGS, pid_list[d_id], NULL, &regs);
	if (temp == -1) {
		return -1;
	}
	long long returned_val;
	long long address;
	long long ptr = regs.rbp;
	while (i <= iter && ptr != 0x1) {
		address = ptrace(PTRACE_PEEKDATA, pid_list[d_id], ptr);
		returned_val = ptrace(PTRACE_PEEKDATA, pid_list[d_id], ptr+0x8);
		fprintf(stdout,"%016llx\t%016llx\n", ptr, returned_val);
		ptr = address;
		i++;
	}
	return 0;
}

void show_list() {
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] == PSTATE_NONE) {
			continue;
		}
		fprintf(stdout,"%d\t%d\t%c\t", i, pid_list[i], traced_list[i]);
		if (pstate_list[i] == PSTATE_DEAD) {
			fprintf(stdout, "%s\t0x%x", "dead", status_list[i]);
		}
		else if (pstate_list[i] == PSTATE_RUNNING) {
			fprintf(stdout, "%s\t", "running");
		}
		else if (pstate_list[i] == PSTATE_STOPPED) {
			fprintf(stdout, "%s\t", "stopped");
		}
		else if (pstate_list[i] == PSTATE_KILLED) {
			fprintf(stdout, "%s\t", "killed");
		}
		else if (pstate_list[i] == PSTATE_STOPPING) {
			fprintf(stdout, "%s\t", "stopping");
		}
		else if (pstate_list[i] == PSTATE_CONTINUING) {
			fprintf(stdout, "%s\t", "continuing");
		}
		fprintf(stdout, "\t%s\n", name_list[i]);
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
		status_list = (int *)realloc(status_list, (sizeof(int) * (deetid+1)));
		pstate_list = (PSTATE *)realloc(pstate_list, (sizeof(PSTATE) * (deetid+1)));
		name_list = (char **)realloc(name_list, (sizeof(char*) * (deetid+1)));
		traced_list = (char *)realloc(traced_list, (sizeof(char) * (deetid+1)));
	}
	name_list[deetid] = (char*)malloc(sizeof(char) * strlen(name)+1);
	pid_list[deetid] = pid;
	pstate_list[deetid] = pstate;
	traced_list[deetid] = 'T';
	strcpy(name_list[deetid], name);
	if (deetid == process_size) {
		process_size++;
	}
}

int check_id_list() {
	int status =0;
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] == PSTATE_DEAD) {
			log_state_change(pid_list[i], PSTATE_DEAD, PSTATE_NONE, status);
			pstate_list[i] = PSTATE_NONE;
		}
	}
	for (int j=0; j<process_size; j++) {
		if (pstate_list[j] == PSTATE_NONE) {
			return j;
		}
	}
	return process_size;
}

char *get_rest(char *input) {
	while (*input != 0 && *(input++) != ' ') {}
	return input;
}

void start_new(char *input) {
	run_bool = 1;
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
		if (execvp(rest[0], &rest[0]) == -1) {
			exit(99);
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

int continue_process(int d_id) {
	if (d_id < 0 || d_id > process_size) {
		return -1;
	}
	deetid = d_id;
	int status =0;
	if (traced_list[d_id] == 'U') {
		log_state_change(pid_list[d_id], pstate_list[deetid], PSTATE_CONTINUING, status);
		fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, pid_list[d_id], traced_list[d_id], "continuing", name_list[deetid]);
		pstate_list[deetid] = PSTATE_CONTINUING;
		kill(pid_list[d_id], SIGCONT);
	}
	else if (ptrace(PTRACE_CONT, pid_list[d_id], NULL, NULL) == -1) {
		fprintf(stderr, "%s\n","ptrace: No such process");
		return -1;
	}
	else {
		log_state_change(pid_list[d_id], pstate_list[deetid], PSTATE_RUNNING, status);
		fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, pid_list[d_id], traced_list[d_id], "running", name_list[deetid]);
		pstate_list[deetid] = PSTATE_RUNNING;
	}
	return 0;
}

int release_process(int d_id) {
	if (d_id <0 || d_id > process_size) {
		return -1;
	}
	if (traced_list[d_id] == 'U') {
		return -1;
	}
	deetid = d_id;
	int status = 0;
	if (ptrace(PTRACE_DETACH, pid_list[d_id], NULL, NULL) == -1) {
		fprintf(stderr, "%s\n","ptrace: No such process");
		return -1;
	}
	traced_list[deetid] = 'U';
	log_state_change(pid_list[d_id], pstate_list[deetid], PSTATE_RUNNING, status);
	fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, pid_list[d_id], traced_list[deetid], "running", name_list[deetid]);
	pstate_list[deetid] = PSTATE_RUNNING;

	kill(pid_list[d_id], SIGCONT);
	return 0;
}

int kill_process(int d_id) {
	if (d_id <0 || d_id > process_size) {
		return -1;
	}
	int status =0;
	sigset_t mask;
	deetid = d_id;
	pid_t pid = pid_list[d_id];
	if (pstate_list[deetid] == PSTATE_DEAD || pstate_list[deetid] == PSTATE_NONE) {
		return -1;
	}
	log_state_change(pid, pstate_list[deetid], PSTATE_KILLED, status);
	fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", deetid, pid, traced_list[deetid], "killed", name_list[deetid]);
	kill(pid, SIGKILL);
	sigfillset(&mask);
	sigdelset(&mask, SIGCHLD);
	sigsuspend(&mask);
	pstate_list[d_id] = PSTATE_DEAD;
	return 0;
}

void kill_all() {
	int status =0;
	sigset_t mask;
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] != PSTATE_DEAD && pstate_list[i] != PSTATE_NONE) {
			log_state_change(pid_list[i], pstate_list[i], PSTATE_KILLED, status);
			fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", i, pid_list[i], traced_list[i], "killed", name_list[i]);
		}
	}
	for (int i=0; i<process_size; i++) {
		if (pstate_list[i] != PSTATE_DEAD && pstate_list[i] != PSTATE_NONE) {
			deetid = i;
			kill(pid_list[i], SIGKILL);
			sigfillset(&mask);
			sigdelset(&mask, SIGCHLD);
			sigsuspend(&mask);
			pstate_list[i] = PSTATE_DEAD;
		}
	}
}

int wait_process(int d_id, char* state_change) {
	if (d_id <0 || d_id > process_size) return -1;
	// printf("deetid: %d %s\n", d_id, state_change);
	int status=0;
	deetid = d_id;
	if (strcmp(state_change, "") == 0 || strcmp(state_change, "dead") == 0) {
		if (pstate_list[d_id] != PSTATE_DEAD)
			while (pstate_list[d_id] != PSTATE_DEAD || waitpid(pid_list[d_id],&status, WUNTRACED) > 0);
	}
	else if (strcmp(state_change, "stopped") == 0) {
		if (pstate_list[d_id] != PSTATE_STOPPED)
			while (pstate_list[d_id] != PSTATE_STOPPED || waitpid(pid_list[d_id],&status, WUNTRACED) > 0);
	}
	else if (strcmp(state_change, "killed") == 0) {
		if (pstate_list[d_id] != PSTATE_KILLED)
			while (pstate_list[d_id] != PSTATE_KILLED || waitpid(pid_list[d_id],&status, WUNTRACED) > 0);
	}
	else if (strcmp(state_change, "running") == 0) {
		if (pstate_list[d_id] != PSTATE_RUNNING)
			while (pstate_list[d_id] != PSTATE_RUNNING || waitpid(pid_list[d_id],&status, WUNTRACED) > 0);
	}
	else if (strcmp(state_change, "stopping") == 0) {
		if (pstate_list[d_id] != PSTATE_RUNNING)
			while (pstate_list[d_id] != PSTATE_RUNNING || waitpid(pid_list[d_id],&status, WUNTRACED) > 0);
	}
	else {
		return -1;
	}
	return 0;
}

int stop_process(int d_id) { //if process already stopped, goto error
	// running to stopping to stopped
	deetid = d_id;
	if (pstate_list[d_id] == PSTATE_STOPPED
		|| pstate_list[d_id] == PSTATE_STOPPING
		|| pstate_list[d_id] == PSTATE_DEAD) {
		return -1;
	}
	int status=0;
	sigset_t mask;
	pid_t pid = pid_list[d_id];
	log_state_change(pid, pstate_list[d_id], PSTATE_STOPPING, status);
	fprintf(stdout, "%d\t%d\t%c\t%s\t\t%s\n", d_id, pid_list[d_id], traced_list[d_id], "stopping", name_list[d_id]);
	pstate_list[d_id] = PSTATE_STOPPING;
	kill(pid, SIGSTOP);
	sigfillset(&mask);
	sigdelset(&mask, SIGCLD);
	sigsuspend(&mask);
	pstate_list[d_id] = PSTATE_STOPPED;
	return 0;
}

void exit_deet() {
	kill_all();
	free(pid_list);
	free(pstate_list);
	for (int i=0; i<process_size; i++) {
		free(name_list[i]);
	}
	free(name_list);
	free(traced_list);
	free(status_list);
	log_shutdown();
	exit(0);
}