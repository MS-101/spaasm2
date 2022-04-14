#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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
    int status = 1;

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

void run_client(char *ip_address, int port) {
    int client_socket;
    char server_response[100];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip_address);

    connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    printf("Client connected to server!\n");

    recv(client_socket, &server_response, sizeof(server_response), 0);
    printf("Client received message from server: %s\n", server_response);

    close(client_socket);
}

void run_server(int port) {
    int server_socket;
    char server_message[100] = "You have reached the server!";

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    listen(server_socket, 1);
    printf("Server listening for clients...\n");

    int client_socket;
    client_socket = accept(server_socket, NULL, NULL);

    send(client_socket, server_message, sizeof(server_message), 0);
    printf("Server responded to client...\n");

    close(server_socket);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        shell_loop();
    } else {
        if (strcmp(argv[1], "-c") == 0) {
            printf("client detected\n");

            int port = 10666;
            char ip_address[32] = "127.0.0.1";
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-p") == 0) {
                    if (i + 1 >= argc) {
                        printf("PORT NUMBER IS MISSING!");
                        return 0;
                    }

                    port = atoi(argv[i+1]);
                    if (port == 0) {
                        printf("PORT NUMBER IS INCORRECT!");
                        return 0;
                    }

                    i++;
                }
            }

            run_client(ip_address, port);
        } else if (strcmp(argv[1], "-s") == 0) {
            printf("server detected\n");

            int port = 10666;
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-p") == 0) {
                    if (i + 1 >= argc) {
                        printf("PORT NUMBER IS MISSING!");
                        return 0;
                    }

                    port = atoi(argv[i+1]);
                    if (port == 0) {
                        printf("PORT NUMBER IS INCORRECT!");
                        return 0;
                    }

                    i++;
                }
            }

            run_server(port);
        }
    }

    return 0;
}