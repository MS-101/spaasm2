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

    int pipes[2];

    pipe(pipes);
    int read_pipe = pipes[0];
    int write_pipe = pipes[1];

    pid = fork();
    if (pid < 0) {
        // error
        return 0;
    } else if (pid == 0) {
        // child
        close(read_pipe);
        dup2(write_pipe, 1);
        close(write_pipe);

        //execl("/bin/ls", "ls", "-l", (char *)NULL);
        int exec_status = execvp(args[0], args);

        if (exec_status == -1) {
            return 1;
        }

        return 0;
    } else {
        // parent
        close(write_pipe);

        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        return read_pipe;
    }
}

int shell_execute(char **args) {
    // implement built-in functions here

    return shell_run(args);
}

char *shell_prompt() {
    char *prompt = malloc(20 * sizeof(char));

    time_t rawtime = time(0);
    struct tm *timeInfo;
    char timeBuffer[10];
    char hostname[HOST_NAME_MAX + 1];
    char username[LOGIN_NAME_MAX + 1];

    timeInfo = localtime( &rawtime );
    strftime(timeBuffer, sizeof timeBuffer,"%I:%M", timeInfo);
    gethostname(hostname, HOST_NAME_MAX + 1);
    getlogin_r(username, LOGIN_NAME_MAX + 1);

    strcat(prompt, timeBuffer);
    strcat(prompt, " ");
    strcat(prompt, username);
    strcat(prompt, "@");
    strcat(prompt, hostname);
    strcat(prompt, "#");

    return prompt;
}

void print_data(int data) {
    char output_buffer[100];

    while (read(data, output_buffer, sizeof(output_buffer)) > 0) {
        printf("%s", output_buffer);
    }
}

void shell_loop() {
    char *prompt;
    char *input;
    char **args;
    int data;

    while(1 == 1) {
        prompt = shell_prompt();

        printf("%s ", prompt);

        input = shell_input();
        args = shell_args(input);
        data = shell_execute(args);

        print_data(data);

        free(prompt);
        free(input);
        free(args);
    }
}

void run_client(char *ip_address, int port) {
    int client_socket;
    char client_message[100];
    char server_response[100];
    int data;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip_address);

    connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    printf("Client connected to server!\n");

    while (1 == 1) {
        recv(client_socket, &server_response, sizeof(server_response), 0);
        printf("%s ", server_response); // prompt

        char *input = shell_input();
        strcpy(client_message, input);
        send(client_socket, client_message, sizeof(client_message), 0);

        recv(client_socket, &data, sizeof(data), 0);
        printf("%d\n", data);
        print_data(data);

        free(input);
    }

    close(client_socket);
}

void run_server(int port) {
    int server_socket;
    char server_message[100];
    char client_response[100];

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
    printf("Server accepted client connection.\n");

    while (1 == 1) {
        char *prompt = shell_prompt();
        strcpy(server_message, prompt);
        send(client_socket, server_message, sizeof(server_message), 0);
        printf("Server sent shell prompt to client.\n");

        recv(client_socket, &client_response, sizeof(client_response), 0);
        char **args = shell_args(client_response);
        int data = shell_execute(args);

        printf("%d\n", data);
        print_data(data);
        send(client_socket, &data, sizeof(data), 0);
        printf("Server sent output to client.\n");

        free(prompt);
        free(args);
    }

    close(server_socket);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        shell_loop();
    } else {
        if (strcmp(argv[1], "-c") == 0) {
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

                if (strcmp(argv[i], "-i") == 0) {
                    if (i + 1 >= argc) {
                        printf("IP ADDRESS IS MISSING!");
                        return 0;
                    }

                    strcpy(ip_address, argv[i+1]);
                }
            }

            run_client(ip_address, port);
        } else if (strcmp(argv[1], "-s") == 0) {
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