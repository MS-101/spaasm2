#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEFAULT_DATA_STRUCT_SIZE 100 
#define DEFAULT_COMMANDS_STRUCT_SIZE 32
#define DEFAULT_COMMAND_SIZE 100
#define DEFAULT_PIPE_STRUCT_SIZE 32
#define DEFAULT_PIPE_SIZE 100
#define DEFAULT_INPUT_SIZE 100

struct command_struct {
    int commands_size;
    int commands_count;
    char **commands;
};

struct pipe_struct {
    int commands_count;

    int *pipe_size;
    int *pipe_count;

    char ***commands;
};

struct final_input_struct {
    int commands_count;
    int *pipe_count;
    char ***commands;
    char ***stdin_redirection;
    char ***stdout_redirection;
};

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

FILE *shell_execute(struct final_input_struct my_final_input_struct, int command_number, int pipe_number, FILE *input_fd, FILE *output_fd) {
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
        char **cmd_args = shell_args(my_final_input_struct.commands[command_number][pipe_number]);

        close(read_pipe);

        if (output_fd != NULL) {
            dup2(fileno(output_fd), 1);
        } else {
            dup2(write_pipe, 1);
        }
        close(write_pipe);

        if (input_fd != NULL) {
            dup2(fileno(input_fd), 0);
            close(fileno(input_fd));
        }

        int exec_status = execvp(cmd_args[0], cmd_args);
        if (exec_status == -1) {
            return NULL;
        }

        return NULL;
    } else {
        // parent

        close(write_pipe);

        wait(NULL);

        if (input_fd != NULL) {
            fclose(input_fd);
        }
        
        if (output_fd != NULL) {
            fclose(output_fd);
        }

       return fdopen(read_pipe, "r");
    }
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

struct data_struct {
    int buffers_size;
    int buffers_count;
    char (*buffers)[100];
};

void print_data(struct data_struct my_data_struct) {
    for (int i = 0; i < my_data_struct.buffers_count; i++) {
        printf("%s", my_data_struct.buffers[i]);
    }

    printf("\n");
}

struct data_struct process_data(FILE *data_file) {
    struct data_struct my_data_struct;

    my_data_struct.buffers_size = DEFAULT_DATA_STRUCT_SIZE;
    my_data_struct.buffers_count = 0;
    my_data_struct.buffers = malloc(DEFAULT_DATA_STRUCT_SIZE * sizeof(*my_data_struct.buffers));

    char output_buffer[100];
    while (fgets(output_buffer, sizeof(output_buffer), data_file)) {
        if (my_data_struct.buffers_count >= my_data_struct.buffers_size) {
            my_data_struct.buffers_size += DEFAULT_DATA_STRUCT_SIZE;
            my_data_struct.buffers = realloc(my_data_struct.buffers, my_data_struct.buffers_size * sizeof(*my_data_struct.buffers));
        }

        strcpy(my_data_struct.buffers[my_data_struct.buffers_count], output_buffer);
        my_data_struct.buffers_count++;
    }

    return my_data_struct;
}

void remove_comment(char *input) {
    bool found_backslash = false;

    for (int i = 0; input[i]; i++) {
        if (!found_backslash && (input[i] == '\\')) {
            found_backslash = true;
        } else {
            found_backslash = false;
        }

        if (!found_backslash && input[i] == '#') {
            input = realloc(input, (i+1) * sizeof(char));
            input[i] = '\0';
        }
    }
}

struct final_input_struct process_redirections(struct pipe_struct my_pipe_struct) {
    struct final_input_struct my_final_input_struct;

    my_final_input_struct.commands_count = my_pipe_struct.commands_count;
    my_final_input_struct.commands = malloc(my_final_input_struct.commands_count * sizeof(char **));
    my_final_input_struct.stdin_redirection = malloc(my_final_input_struct.commands_count * sizeof(char **));
    my_final_input_struct.stdout_redirection = malloc(my_final_input_struct.commands_count * sizeof(char **));

    my_final_input_struct.pipe_count = malloc(my_final_input_struct.commands_count * sizeof(int));
    for (int i = 0; i < my_final_input_struct.commands_count; i++) {
        my_final_input_struct.pipe_count[i] = my_pipe_struct.pipe_count[i];
        my_final_input_struct.commands[i] = malloc(my_final_input_struct.pipe_count[i] * sizeof(char *));
        my_final_input_struct.stdin_redirection[i] = malloc(my_final_input_struct.pipe_count[i] * sizeof(char *));
        my_final_input_struct.stdout_redirection[i] = malloc(my_final_input_struct.pipe_count[i] * sizeof(char *));

        for (int j = 0; j < my_final_input_struct.pipe_count[i]; j++) {
            int new_cmd_index = 0;
            int new_cmd_size = DEFAULT_COMMAND_SIZE;
            char *new_cmd = malloc(new_cmd_size * sizeof(char));

            bool found_backslash = false;
            bool found_input = false;
            bool found_output = false;
            bool found_first_char = false;

            char *new_input = NULL;
            char *new_output = NULL;

            int input_index = 0;
            int output_index = 0;

            for (int k = 0; my_pipe_struct.commands[i][j][k]; k++) {
                char read_char = my_pipe_struct.commands[i][j][k];

                if (!found_backslash && (read_char == '<')) {
                    found_input = true;
                    continue;
                }

                if (!found_backslash && (read_char == '>')) {
                    found_output = true;
                    continue;
                }

                if (found_input) {
                    if (!found_first_char && (read_char != ' ')) {
                        found_first_char = true;
                        new_input = malloc(100 * sizeof(char));
                        new_input[input_index] = read_char;
                        input_index++;

                        continue;
                    }

                    if (found_first_char && (read_char != ' ' && read_char != '\n')) {
                        new_input[input_index] = read_char;
                        input_index++;

                        continue;
                    }

                    if (found_first_char && (read_char == ' ' || read_char == '\n')) {
                        new_input[input_index] = '\0';

                        found_first_char = false;
                        found_input = false;
                        input_index = 0;

                        continue;
                    }
                }

                if (found_output) {
                    if (!found_first_char && (read_char != ' ')) {
                        found_first_char = true;
                        new_output = malloc(100 * sizeof(char));
                        new_output[output_index] = read_char;
                        output_index++;

                        continue;
                    }

                    if (found_first_char && (read_char != ' ' && read_char != '\n')) {
                        new_output[output_index] = read_char;
                        output_index++;

                        continue;
                    }

                    if (found_first_char && (read_char == ' ' || read_char == '\n')) {
                        new_output[output_index] = '\0';

                        found_first_char = false;
                        found_output = false;
                        output_index = 0;

                        continue;
                    }
                }

                if (new_cmd_index+1 >= new_cmd_size) {
                    new_cmd_size += DEFAULT_COMMAND_SIZE;
                    new_cmd = realloc(new_cmd, new_cmd_index * sizeof(char));
                }

                new_cmd[new_cmd_index] = read_char;
                new_cmd_index++;

                if (!found_backslash && (my_pipe_struct.commands[i][j][k] == '\\')) {
                    found_backslash = true;
                } else {
                    found_backslash = false;
                }
            }

            new_cmd[new_cmd_index] = '\0';
            my_final_input_struct.commands[i][j] = new_cmd;

            if (found_input) {
                new_input[input_index] = '\0';

                found_first_char = false;
                found_input = false;
                output_index = 0;
            }

            if (found_input) {
                new_output[output_index] = '\0';

                found_first_char = false;
                found_output = false;
                output_index = 0;
            }

            my_final_input_struct.stdin_redirection[i][j] = new_input;
            my_final_input_struct.stdout_redirection[i][j] = new_output;
        }
    }

    return my_final_input_struct;
}

struct pipe_struct split_pipes(struct command_struct my_command_struct) {
    struct pipe_struct my_pipe_struct;

    my_pipe_struct.commands_count = my_command_struct.commands_count;
    my_pipe_struct.commands = malloc(my_pipe_struct.commands_count * sizeof(char**));
    my_pipe_struct.pipe_size = malloc(my_pipe_struct.commands_count * sizeof(int));
    my_pipe_struct.pipe_count = malloc(my_pipe_struct.commands_count * sizeof(int));

    for (int i = 0; i < my_command_struct.commands_count; i++) {
        my_pipe_struct.pipe_count[i] = 0;
        my_pipe_struct.pipe_size[i] = DEFAULT_PIPE_STRUCT_SIZE;
        my_pipe_struct.commands[i] = malloc(my_command_struct.commands_size * sizeof(char *));

        int new_pipe_index = 0;
        int new_pipe_size = DEFAULT_PIPE_SIZE;
        char *new_pipe = malloc(new_pipe_size * sizeof(char *));

        bool found_backslash = false;

        for (int j = 0; my_command_struct.commands[i][j]; j++) {
            if (!found_backslash && (my_command_struct.commands[i][j] == '|')) {
                new_pipe[new_pipe_index] = '\0';

                my_pipe_struct.commands[i][my_pipe_struct.pipe_count[i]] = new_pipe;
                my_pipe_struct.pipe_count[i]++;

                if (my_pipe_struct.pipe_count[i] >= my_pipe_struct.pipe_size[i]) {
                    my_pipe_struct.pipe_size[i] += DEFAULT_PIPE_SIZE;
                    my_pipe_struct.commands[i] = realloc(my_pipe_struct.commands[i], my_pipe_struct.pipe_size[i] * sizeof(char *));
                }

                new_pipe_size = DEFAULT_PIPE_SIZE;
                new_pipe = malloc(new_pipe_size * sizeof(char));
                new_pipe_index = 0;

                continue;
            }

            if (new_pipe_index+1 >= new_pipe_size) {
                new_pipe_size += DEFAULT_PIPE_SIZE;
                new_pipe = realloc(new_pipe, new_pipe_size * sizeof(char));
            }

            new_pipe[new_pipe_index] = my_command_struct.commands[i][j];
            new_pipe_index++;

            if (!found_backslash && (my_command_struct.commands[i][j] == '\\')) {
                found_backslash = true;
            } else {
                found_backslash = false;
            }
        }

        if (new_pipe_index > 0) {
            new_pipe[new_pipe_index] = '\0';
            my_pipe_struct.commands[i][my_pipe_struct.pipe_count[i]] = new_pipe;
            my_pipe_struct.pipe_count[i]++;
        }
    }

    return my_pipe_struct;
}

struct command_struct split_commands(char *input) {
    struct command_struct my_command_struct;

    my_command_struct.commands_count = 0;
    my_command_struct.commands_size = DEFAULT_COMMANDS_STRUCT_SIZE;
    my_command_struct.commands = malloc(my_command_struct.commands_size * sizeof(char *));

    bool found_backslash = false;
    int new_cmd_index = 0;
    int new_cmd_size = DEFAULT_COMMAND_SIZE;
    char *new_cmd = malloc(new_cmd_size * sizeof(char));
    for (int i = 0; input[i]; i++) {
        if (!found_backslash && (input[i] == ';')) {
            new_cmd[new_cmd_index] = '\0';

            my_command_struct.commands[my_command_struct.commands_count] = new_cmd;
            my_command_struct.commands_count++;

            if (my_command_struct.commands_count >= my_command_struct.commands_size) {
                my_command_struct.commands_size += DEFAULT_COMMANDS_STRUCT_SIZE;
                my_command_struct.commands = realloc(my_command_struct.commands, my_command_struct.commands_size);
            }

            new_cmd_size = DEFAULT_COMMAND_SIZE;
            new_cmd = malloc(new_cmd_size * sizeof(char));
            new_cmd_index = 0;

            continue;
        }

        if (new_cmd_index+1 >= new_cmd_size) {
            new_cmd_size += DEFAULT_COMMAND_SIZE;
            new_cmd = realloc(new_cmd, new_cmd_size * sizeof(char));
        }

        new_cmd[new_cmd_index] = input[i];
        new_cmd_index++;

        if (!found_backslash && (input[i] == '\\')) {
            found_backslash = true;
        } else {
            found_backslash = false;
        }
    }

    if (new_cmd_index > 0) {
        new_cmd[new_cmd_index] = '\0';
        my_command_struct.commands[my_command_struct.commands_count] = new_cmd;
        my_command_struct.commands_count++;
    }

    return my_command_struct;
}

struct final_input_struct format_input(char *input) {
    remove_comment(input);
    struct command_struct my_command_struct = split_commands(input);
    struct pipe_struct my_pipe_struct = split_pipes(my_command_struct);
    struct final_input_struct my_final_input_struct = process_redirections(my_pipe_struct);

   return my_final_input_struct;
}

void shell_loop() {
    char *prompt;
    char *input;
    FILE *data_file = NULL;

    while(1 == 1) {
        prompt = shell_prompt();

        printf("%s ", prompt);

        input = shell_input();

        struct final_input_struct my_final_input_struct = format_input(input);

        //args = shell_args(input);
        for (int i = 0; i < my_final_input_struct.commands_count; i++) {
            for (int j = 0; j < my_final_input_struct.pipe_count[i]; j++) {
                FILE *stdin_redirection = NULL;
                if (my_final_input_struct.stdin_redirection[i][j] != NULL) {
                    stdin_redirection = fopen(my_final_input_struct.stdin_redirection[i][j], "r");
                }

                FILE *stdout_redirection = NULL;
                if (my_final_input_struct.stdout_redirection[i][j] != NULL) {
                    stdout_redirection = fopen(my_final_input_struct.stdout_redirection[i][j], "w");  
                }

                if (stdin_redirection != NULL) {
                    data_file = shell_execute(my_final_input_struct, i, j, stdin_redirection, stdout_redirection);
                } else {
                    data_file = shell_execute(my_final_input_struct, i, j, data_file, stdout_redirection);
                }
            }

            struct data_struct my_data_struct = process_data(data_file);
            print_data(my_data_struct);
            free(my_data_struct.buffers);
        }

        free(prompt);
        free(input);
    }
}

void run_client(char *ip_address, int port) {
    int client_socket;
    char client_message[100];
    char server_response[100];

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

        int buffer_count;
        recv(client_socket, &buffer_count, sizeof(buffer_count), 0);
        for (int i = 0; i < buffer_count; i++) {
            recv(client_socket, &server_response, sizeof(server_response), 0);
            printf("%s ", server_response);
        }

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
        FILE *data_file = NULL; //shell_execute(args)

        struct data_struct my_data_struct = process_data(data_file);

        printf("Server is sending output to client...\n");

        int buffer_count = my_data_struct.buffers_count;

        send(client_socket, &buffer_count, sizeof(buffer_count), 0);
        for (int i = 0; i < buffer_count; i++) {
            strcpy(server_message, my_data_struct.buffers[i]);
            send(client_socket, &server_message, sizeof(server_message), 0);
        }

        printf("Server finished sending output to client.\n");

        free(prompt);
        free(args);
    }

    close(server_socket);
}

int main(int argc, char *argv[]) {
    /*
    char *arg_list[] = {"grep", "\"test\"", "test1", NULL};
    for (int i = 0; arg_list[i] != NULL; i++) {
        printf("arg_list[%d] = %s\n", i, arg_list[i]);
    }
    execvp(arg_list[0], arg_list);
    */

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