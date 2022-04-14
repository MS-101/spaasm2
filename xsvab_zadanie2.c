#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <string.h>

char *shell_input() {
    char *input = NULL;
    size_t input_size = 0;
    if (getline(&input, &input_size, stdin) == -1) {
        return NULL;
    }

    return input;
}

#define default_arg_count 32
char **shell_args(char * input) {
    int arg_count = default_arg_count;
    char **args = malloc(arg_count * sizeof(char *));
    char *arg;

    int arg_index = 0;
    arg = strtok(input, " \n");
    while (arg != NULL) {
        args[arg_index] = arg;
        arg_index++;

        if (arg_index >= arg_count) {
            arg_count += default_arg_count;
            args = realloc(args, arg_count / sizeof(char *));
        }

        arg = strtok(NULL, " \n");
    }

    args[arg_index] = NULL;

    return args;
}

int shell_run(char **args) {
    int pid, wpid, status;

    pid = fork();

    if (pid < 0) {
        // error
        return 1;
    } else if (pid == 0) {
        // child
        int exec_status = execvp(args[0], args);

        if (exec_status == -1) {
            return 1;
        }
    } else {
        // parent
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int shell_execute(char **args) {
    // implement built-in functions here

    return shell_run(args);
}

void shell_loop() {
    char *input;
    char **args;
    int status;

    time_t rawtime = time(0);
    struct tm *timeInfo;
    char timeBuffer[10];
    char hostname[HOST_NAME_MAX + 1];
    char username[LOGIN_NAME_MAX + 1];

    while(status) {
        timeInfo = localtime( &rawtime );
        strftime(timeBuffer, sizeof timeBuffer,"%I:%M", timeInfo);
        gethostname(hostname, HOST_NAME_MAX + 1);
        getlogin_r(username, LOGIN_NAME_MAX + 1);

        printf("%s %s@%s# ", timeBuffer, username, hostname);

        input = shell_input();
        args = shell_args(input);
        status = shell_execute(args);

        free(input);
        free(args);
    }
}

int main() {
    shell_loop();

    return 0;
}