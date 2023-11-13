#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "deet.h"
#include "helper.h"

int main(int argc, char *argv[]) {
    // TO BE IMPLEMENTED
    // Remember: Do not put any functions other than main() in this file.
    log_startup();
    log_prompt();
    int quit = 1;
    while (quit != 0) {
        char *input = read_input();
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
                        quit = 0;
                    }
                    else {
                        goto error_case;
                    }
                    break;
                case 's': //show
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
                    break;
                case 'e': //release
                    break;
                case 'w': //wait
                    break;
                case 'k': //kill
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

    return EXIT_SUCCESS;
}
