#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glob.h>
#include <errno.h>
#include <ctype.h>
#include "arraylist.h"
#define BUFLENGTH 32

#ifndef DEBUG
#define DEBUG 0
#endif

//Declaration of functions
void parse_line(arraylist_t *commands, char *line);
void pipe_commands(arraylist_t *cmd1, arraylist_t *cmd2, char *input_file, char *output_file, char *input_file_after, char *output_file_after);
void read_terminal(int fd, char *line, int *end);
void exit_command(arraylist_t *commands);
void cd_command(char *directory);
void pwd_command();
char *getPath(char *program);
void which_command(char *program);
void exec_cmd(arraylist_t *commands, char *input_file, char *output_file);
void interactive_mode();
void batch_mode();
//Global var. for prev. success for conditionals
int prevSuccess;

void wildcard(char *arg, arraylist_t *commands) {
    //glob
    glob_t wildcard_exp;
    if(glob(arg, GLOB_NOCHECK | GLOB_TILDE, NULL, &wildcard_exp) == 0) {
        for(int i = 0; i < wildcard_exp.gl_pathc; i++) {
            //expansion
            char *temp = malloc(strlen(wildcard_exp.gl_pathv[i]) + 1);
            if (temp == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            strcpy(temp, wildcard_exp.gl_pathv[i]);
            al_push(commands, temp); 
            free(temp);  //need to free tempword within cuz declared within if
        }
    }
    globfree(&wildcard_exp);
    return;
}

void parse_line(arraylist_t *commands, char *line) {
    if (strlen(line) == 0) {
        return; // Skip processing if the input line is empty
    }
    //declaration of arraylist of commands before & after pipe
    arraylist_t before_pipe;
    arraylist_t after_pipe; 
    al_init(&before_pipe, 10); 
    al_init(&after_pipe, 10);
    //input file before & after pipe
    char *input_file = NULL;
    char *input_file_after = NULL;
    //create arraylist of multiple output files before and after pipe
    arraylist_t output_files;
    arraylist_t output_files_after; 
    al_init(&output_files, 10);
    al_init(&output_files_after, 10);

    int pipe_found = 0; //boolean value to see if pipe was found
    char *token = strtok(line, " ");
    if (token == NULL){
        //nothing or weird like \t then free and get out
        al_destroy(&before_pipe);
        al_destroy(&after_pipe);
        al_destroy(&output_files);
        al_destroy(&output_files_after);
        return;
    }
    //Case to run then conditional
    if(strcmp(token, "then") == 0 && prevSuccess!=EXIT_SUCCESS){
        al_destroy(&before_pipe);
        al_destroy(&after_pipe);
        al_destroy(&output_files);
        al_destroy(&output_files_after);
        return;
    } 
    //Don't run then if prev failed
    else if (strcmp(token, "then") == 0 && prevSuccess==EXIT_SUCCESS){
        token = strtok(NULL, " ");
    } 
    //Don't run else if prev success
    else if (strcmp(token, "else") == 0 && prevSuccess!=EXIT_SUCCESS){
        token = strtok(NULL, " ");
    } 
    //Run if prev failed for Else
    else if (strcmp(token, "else") == 0 && prevSuccess==EXIT_SUCCESS){
        al_destroy(&before_pipe);
        al_destroy(&after_pipe);
        al_destroy(&output_files);
        al_destroy(&output_files_after);
        return;
    }
    //Parsing input
    while (token != NULL) {
        if (strchr(token, '|')) {
            pipe_found = 1; //pipe_found
            if (strcmp(token, "|") == 0) {
                break;
            }
            else {
                int len = strlen(token);
                int index = 0;
                
                for (int i = 0; i < len; i++) {
                    if(token[i] == '|') {
                        index = i;
                    }
                }

                if (index == 0) { //EDGE CASE: Pipe is first character in the entire command
                    char *temp = malloc(strlen(token));
                    strcpy(temp, token+1);
                    al_push(&after_pipe, temp);
                    free(temp);
                    break;
                }
                else if(index == (strlen(token) - 1)) { //Pipe is the last character in the command
                    char *temp = malloc(strlen(token));
                    strncpy(temp, token, strlen(token) - 1);
                    temp[strlen(token) - 1] = '\0';
                    al_push(&before_pipe, temp);
                    free(temp);
                    break;
                }
                else {
                    char *temp1 = malloc(index + 1);
                    char *temp2 = malloc(strlen(token) - index);
                    strncpy(temp1, token, index);
                    temp1[index] = '\0';
                    strcpy(temp2, token + index + 1);
                    al_push(&before_pipe, temp1);
                    al_push(&after_pipe, temp2);
                    free(temp1);
                    free(temp2);
                    break;
                }
            }
        }
        else if (strchr(token, '*')) {
            //call wildcard function
            wildcard(token, &before_pipe);
        }
        else if (strchr(token, '>') || strchr(token, '<')) {
            if(strcmp(token, "<") == 0) {
                // Input redirection symbol found
                token = strtok(NULL, " ");
                if (token != NULL) {
                    input_file = malloc(strlen(token) + 1);
                    strcpy(input_file, token);
                } else {
                    fprintf(stderr, "Missing filename after '<'\n");
                    return;
                }
            } else {
                int len = strlen(token);
                int found = 0;
                for(int i=0; i<len; i++) {
                    if(token[i] == '>') {
                        found = i;
                        break;
                    }
                }
                found = len;
                //Begining Case
                if(al_length(&before_pipe)==0) {
                    //Command is not found yet so start
                    int start = 0;
                    int end = 0;
                    for (int i = 0; i < len; i++) {
                        if(token[i] == '>' && start == 0) {
                            start = i;
                            continue;
                        }
                        if(start != 0) {
                            if(token[i] == '>') {
                                end = i;
                                break;
                            }
                        }
                    }
                    if(end==0)  end=len;

                    char *temp_cmd = malloc(start+1);
                    strncpy(temp_cmd, token, start+1);
                    temp_cmd[start] = '\0';
                    al_push(&before_pipe, temp_cmd);
                    free(temp_cmd);

                    if(end==len) {
                        //if there were 0 spaces
                        char *temp = malloc(end-start+1);
                        strncpy(temp, token+start+1, end-start);
                        temp[end-start-1] = '\0';
                        al_push(&output_files, temp);
                        free(temp);
                    } else {
                        al_destroy(&before_pipe);
                        al_destroy(&after_pipe);
                        al_destroy(&output_files);
                        al_destroy(&output_files_after);
                        return;
                    }
                        
                } else if(token[0]=='>' && len!=1) { //Output Redirection at start
                    //Take file output
                    int end = 0;
                    for(int i=1; i<len; i++) {
                        if(token[i] == '>') {
                            end = i-1;
                            break;
                        }
                    }
                    char *temp = malloc(end+1);
                    strcpy(temp, token+end);
                    temp[end-1] = '\0';
                    al_push(&output_files, temp);
                    free(temp);
                } else if(found != len) { //Redirection in the middle
                    int end = 0;
                    while(end != len) {
                        int start=0;
                        for (int i = 0; i < len; i++) {
                            if(token[i] == '>' && start == 0) {
                                start = i;
                                continue;
                            }
                            if(start != 0) {
                                if(token[i] == '>') {
                                    end = i;
                                    break;
                                }
                            }
                        }
                        if(end==0)  end=len;
                        char *temp = malloc(end-start+1);
                        strncpy(temp, token+start+1, end-start);
                        temp[end-start-1] = '\0';
                        al_push(&output_files, temp);
                        free(temp);
                        start = end;
                    }
                    
                } else {
                    token = strtok(NULL, " ");
                    char *temp = malloc(strlen(token) + 1);
                    strcpy(temp, token);
                    al_push(&output_files, temp);
                    free(temp);
                }
                
            }
        }
        else {
            //copy token into commands
            char *temp = malloc(strlen(token) + 1);
            strcpy(temp, token);
            if (temp)
            al_push(&before_pipe, temp);
            free(temp);
        }

        token = strtok(NULL, " ");
    }
    
    if (pipe_found) {
        //now to populate after_pipe
        token = strtok(NULL, " ");
        while (token != NULL) {
            if (strchr(token, '*')) {
                wildcard(token, &after_pipe);
            }
            else if (strchr(token, '>') || strchr(token, '<')) {
                //call redirection function
                if(strcmp(token, "<") == 0) {
                 // Input redirection symbol found
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        input_file = malloc(strlen(token) + 1);
                        strcpy(input_file_after, token);
                    } else {
                        fprintf(stderr, "Missing filename after '<'\n");
                        return;
                    }
                } else {
                    // Output redirection symbol found
                    token = strtok(NULL, " ");
                    char *temp = malloc(strlen(token) + 1);
                    strcpy(temp, token);
                    al_push(&output_files_after, temp);
                    free(temp);
                }
            }
            else {
                char *temp = malloc(strlen(token) + 1);
                strcpy(temp, token);
                if (temp)   al_push(&after_pipe, temp);
                free(temp);
            }
            token = strtok(NULL, " ");
        }
        //Redirection file check
        if(al_length(&output_files_after) > 1) {
            //if more than 1 output file after pipe
            if (al_length(&output_files) > 1) {
                for(int i=0; i<al_length(&output_files)-1; i++) { //open excess files
                    int file_descriptor = open(output_files.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (file_descriptor == -1) {
                        perror("Error creating file");
                    }
                    close(file_descriptor);
                }
                
                for(int i=0; i<al_length(&output_files_after)-1; i++) {
                    int file_descriptor = open(output_files_after.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (file_descriptor == -1) {
                        perror("Error creating file");
                    }
                close(file_descriptor);
                }
            pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after, output_files_after.data[al_length(&output_files_after)-1]);
            }
            else if (al_length(&output_files) == 1) { //only 1 file
                for(int i=0; i<al_length(&output_files_after)-1; i++) {
                    int file_descriptor = open(output_files_after.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (file_descriptor == -1) {
                        perror("Error creating file");
                    }
                close(file_descriptor);
                }
                
            pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after,  output_files_after.data[al_length(&output_files)-1]);

            } else { //no files
                for(int i=0; i<al_length(&output_files_after)-1; i++) {
                    int file_descriptor = open(output_files_after.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (file_descriptor == -1) {
                        perror("Error creating file");
                    }
                close(file_descriptor);
                }
                pipe_commands(&before_pipe, &after_pipe, input_file, NULL, input_file_after, output_files_after.data[al_length(&output_files_after)-1]);
            }
            
        } else if (al_length(&output_files) > 1) { //out files after isnt greater than 1
            if (al_length(&output_files_after) == 1) {
                for(int i=0; i<al_length(&output_files)-1; i++) {
                    int file_descriptor = open(output_files.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (file_descriptor == -1) {
                        perror("Error creating file");
                    }
                    close(file_descriptor);
                }
                pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after, output_files_after.data[al_length(&output_files_after)-1]);
            }
            else {
                for(int i=0; i<al_length(&output_files)-1; i++) {
                        int file_descriptor = open(output_files.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                        if (file_descriptor == -1) {
                            perror("Error creating file");
                        }
                        close(file_descriptor);
                }
                pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after, NULL);
            }
        }
        else if (al_length(&output_files) == 1 && al_length(&output_files_after) == 1) {
            pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after, output_files_after.data[al_length(&output_files)-1]);

        } else if (al_length(&output_files) == 1) {
            pipe_commands(&before_pipe, &after_pipe, input_file, output_files.data[al_length(&output_files)-1], input_file_after, NULL);

        } else if (al_length(&output_files_after) == 1) {
            pipe_commands(&before_pipe, &after_pipe, input_file, NULL, input_file_after, output_files_after.data[al_length(&output_files_after)-1]);

        }else {
            pipe_commands(&before_pipe, &after_pipe, input_file, NULL, input_file_after, NULL);
        }
    }
    else { //Same check for output files but when output after is NULL -> No output files before pipe
        if(al_length(&output_files) > 1) {
            for(int i=0; i<al_length(&output_files)-1; i++) {
                int file_descriptor = open(output_files.data[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (file_descriptor == -1) {
                    perror("Error creating file");
                }
                close(file_descriptor);
            }
            exec_cmd(&before_pipe, input_file, output_files.data[al_length(&output_files)-1]);
        } else if(al_length(&output_files) == 1) {
            for(int i=0; i<al_length(&before_pipe); i++) {
            }
            exec_cmd(&before_pipe, input_file, output_files.data[al_length(&output_files)-1]);
        } else {
            exec_cmd(&before_pipe, input_file, NULL);
        }
    }
    // Clean up memory
    if (input_file != NULL) {
        free(input_file);
    }
    
    al_destroy(&before_pipe);
    al_destroy(&after_pipe);
    al_destroy(&output_files);
    al_destroy(&output_files_after);
}

void pipe_commands(arraylist_t *cmd1, arraylist_t *cmd2, char *input_file, char *output_file, char *input_file_after, char *output_file_after) {
    //seperate command to run pipe commands
    int pipefd[2];
    pid_t pid1, pid2;

    // Create a pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid1 = fork();
    if (pid1 == 0) {
        // Child process for cmd1
        close(pipefd[0]); // Close read end of pipe
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to write end of pipe
        close(pipefd[1]); // Close write end of pipe

        // Handle input redirection for cmd1
        if (input_file != NULL) {
            int fd_in = open(input_file, O_RDONLY);
            if (fd_in == -1) {
                perror("Error opening input file");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_in, STDIN_FILENO) == -1) {
                perror("Error redirecting input");
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }

        if (output_file != NULL) {
            int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd_out == -1) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_out, STDOUT_FILENO) == -1) {
                perror("Error redirecting output");
                exit(EXIT_FAILURE);
            }
            close(fd_out);
        }

        // Execute cmd1
        if(strcmp(cmd1->data[0], "pwd") == 0){
            pwd_command();
        } else if(strcmp(cmd1->data[0], "which")==0) {
            which_command(cmd1->data[1]);
        } else if(strcmp(cmd1->data[0], "exit")==0) {
            exit_command(cmd1);
        } else {
            char *full_path = NULL;
            if(cmd1->data[0][0]=='/') {
                full_path = cmd1->data[0];
            } else {
                full_path = getPath(cmd1->data[0]);
            }
            if(full_path == NULL) {
                fprintf(stderr, "Command not found: %s\n", cmd1->data[0]);
                exit(EXIT_FAILURE);
            }
            al_push(cmd1, NULL); // NULL-terminate the command arraylist
            if (execv(full_path, cmd1->data) == -1) {
                perror("execv cmd1");
                free(full_path);
                exit(EXIT_FAILURE);
            }
            free(full_path);
        }
    }

    if(strcmp(cmd1->data[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    pid2 = fork();
    if (pid2 == 0) {
        // Child process for cmd2
        close(pipefd[1]); // Close write end of pipe
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to read end of pipe
        close(pipefd[0]); // Close read end of pipe

        // Handle output redirection for cmd2
        if (input_file_after != NULL) {
            int fd_in = open(input_file_after, O_RDONLY);
            if (fd_in == -1) {
                perror("Error opening input file");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_in, STDIN_FILENO) == -1) {
                perror("Error redirecting input");
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }

        if (output_file_after != NULL) {
            int fd_out = open(output_file_after, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd_out == -1) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_out, STDOUT_FILENO) == -1) {
                perror("Error redirecting output");
                exit(EXIT_FAILURE);
            }
            close(fd_out);
        }

        // Execute cmd2
        if(strcmp(cmd2->data[0], "pwd") == 0){
            pwd_command();
        } else if(strcmp(cmd2->data[0], "which")==0) {
            which_command(cmd2->data[1]);
        } else if(strcmp(cmd2->data[0], "exit")==0) {
            exit_command(cmd2);
        } else {
            char *full_path = NULL;
            if(cmd2->data[0][0]=='/') {
                full_path = cmd2->data[0];
            } else {
                full_path = getPath(cmd2->data[0]);
            }
            if(full_path == NULL) {
                fprintf(stderr, "Command not found: %s\n", cmd2->data[0]);
                exit(EXIT_FAILURE);
            }
            al_push(cmd2, NULL); // NULL-terminate the command arraylist
            if (execv(full_path, cmd2->data) == -1) {
                perror("execv cmd2");
                free(full_path);
                exit(EXIT_FAILURE);
            }
            free(full_path);
        }
    }

    if(strcmp(cmd2->data[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    // Close pipe in parent process
    close(pipefd[0]);
    close(pipefd[1]);
    // Wait for child processes to finish
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}


void read_terminal(int fd, char *line, int *end) {
    char ch;
    int bytes_read;
    int position = 0;

    while (1) {
        // Read a character
        bytes_read = read(fd, &ch, 1);

        // Check for read error or EOF
        if (bytes_read == -1) {
            perror("Error reading input");
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0 || ch == '\n') {
            // If EOF or newline is encountered, process the line and return
            line[position] = '\0';
            if(bytes_read == 0){
                *end = 1;
            }
            return;
        }

        // Store the character in the line 
        line[position++] = ch;
    }
}

void exit_command(arraylist_t *commands) {
    // Print any arguments received by the exit command
    for (unsigned i = 1; i < al_length(commands); i++) {
        printf("%s", commands->data[i]);
    }
    printf("\n");
    printf("mysh: exiting\n");
    al_destroy(commands);
    // Terminate the program
    exit(EXIT_SUCCESS);
}

void cd_command(char *directory) {
    if (directory == NULL) { //error if directory is none
        fprintf(stderr, "cd: missing directory\n");
        return;
    }

    if (chdir(directory) == -1) { //bad dir
        perror("cd");
    }
}

void pwd_command() {
    char cwd[4096]; //max size for line
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        fprintf(stdout, "%s\n", cwd);
        exit(EXIT_SUCCESS); // Exit with success
    } else {
        perror("getcwd() error");
        exit(EXIT_FAILURE); // Exit with failure
    }
}

char *getPath(char *program) {
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"}; //dir to traverse
    int num_directories = sizeof(directories) / sizeof(directories[0]); //number of dir

    char *full_path = NULL; //init to NULL

    for (int i = 0; i < num_directories; i++) {
        char *final = (char *)malloc(strlen(directories[i]) + strlen(program) + 2); //malloc length
        if (final == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
        sprintf(final, "%s/%s", directories[i], program); //print for it
        
        if (access(final, X_OK) == 0) { //use access to check
            if (full_path != NULL) {
                free(full_path);
            }
            return final;
        } else {
            free(final);
        }
    }

    return full_path; 
}

void which_command(char *program) {
    if (program == NULL) { //weird input that exit
        exit(EXIT_FAILURE);
    }

    if (strcmp(program, "cd") == 0 || strcmp(program, "pwd") == 0 || strcmp(program, "which") == 0 || strcmp(program, "exit") == 0) {
        exit(EXIT_FAILURE); //internal command -> exit fail
    }

    char *full_path = getPath(program);
    if (full_path != NULL) { //found path
        fprintf(stdout, "%s\n", full_path);
        free(full_path);
    } else {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}


void exec_cmd(arraylist_t *commands, char *input_file, char *output_file) {
    char *program = commands->data[0]; 
    if(strcmp(program, "exit")==0) { //if program is exit, then exit
        exit_command(commands);
    }
    
    if (strcmp(program, "cd") == 0) { //if cd then cd
        if (al_length(commands) != 2) {
            fprintf(stderr, "cd: too many arguements\n");
        } else {
            cd_command(commands->data[1]);
        }
    } else {
        pid_t pid = fork(); //start fork
        if (pid == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { //fork good
            if(strcmp(program, "pwd") == 0) { //pwd 
                int fd_out = STDOUT_FILENO;
                if (output_file != NULL) {
                    fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640); //output redirection
                    if (fd_out == -1) {
                        perror("Error opening output file");
                        exit(EXIT_FAILURE);
                    }
                }
                if (input_file != NULL) {
                    printf("No input redirection for pwd.");
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) { //dup2 to set 
                    perror("Error redirecting output");
                    exit(EXIT_FAILURE);
                }
                pwd_command(); //run pwd
                for(int i=1; i<al_length(commands); i++) { //remove any weird or excess arg into pwd
                    al_remove(commands, i);
                }
                if (output_file != NULL)    close(fd_out); //close
            } else if(strcmp(program, "which") == 0) {
                int fd_out = STDOUT_FILENO;
                if (output_file != NULL) {
                    fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640); //create the output file or access it
                    if (fd_out == -1) {
                        perror("Error opening output file");
                        exit(EXIT_FAILURE);
                    }
                }
                if (input_file != NULL) {
                    int fd_in = open(input_file, O_RDONLY); //read from input 
                    if (fd_in == -1) {
                        perror("Error opening input file");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(fd_in, STDIN_FILENO) == -1) { //dup2 to set input
                        perror("Error redirecting input");
                        exit(EXIT_FAILURE);
                    }
                    close(fd_in);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) { //dup2 to set output
                    perror("Error redirecting output");
                    exit(EXIT_FAILURE);
                }
                which_command(commands->data[1]); //run which
                if (output_file != NULL)    close(fd_out); //close
            }
            // Child process
            if (input_file != NULL) {
                int fd_in = open(input_file, O_RDONLY);
                if (fd_in == -1) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_in, STDIN_FILENO) == -1) {
                    perror("Error redirecting input");
                    exit(EXIT_FAILURE);
                }
                close(fd_in);
            }

            if (output_file != NULL) {
                int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (fd_out == -1) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) {
                    perror("Error redirecting output");
                    exit(EXIT_FAILURE);
                }
                close(fd_out);
            }

            char *full_path = NULL;
            if(program[0]=='/') {
                full_path = program; //if we recieve a /, then assume path to exe
            } else {
                full_path = getPath(program); //bareName so find path using getPath
            }
            
            if (full_path == NULL) { //fail case if still NULL
                fprintf(stderr, "Command not found: %s\n", program); 
                exit(EXIT_FAILURE);
            }

            al_push(commands, NULL); //remove the command once run

            if (execv(full_path, commands->data) == -1) {
                prevSuccess = 0; // if execv fails
                perror("Error executing command");
                free(full_path);
                exit(EXIT_FAILURE);
            }
            free(full_path);
        } else {
            wait(&prevSuccess); //wait for prev 
        }
    }
}

void interactive_mode() {
    //prints for welcome
    printf("Welcome to My Shell!\n");
    printf("Type 'exit' to quit.\n");

    char line[1024]; //line array

    int end = 0; //end but useless in interactive

    arraylist_t commands; //arraylist of commands
    al_init(&commands, 10);

    do {
        printf("mysh> ");

        fflush(stdout);

        read_terminal(STDIN_FILENO, line, &end);
        
        parse_line(&commands, line);
        
        al_destroy(&commands); //destroy commands
        al_init(&commands, 10); //init for next
         

    } while (1);

   
}

void batch_mode() {
    //to read from STDIN, will be set in main
    int end = 0; //used to know when we reach the end of input
    char line[1024];

    arraylist_t commands;
    al_init(&commands, 10);
    do {
        read_terminal(STDIN_FILENO, line, &end);
        parse_line(&commands, line);

        al_destroy(&commands);
        al_init(&commands, 10);

    } while (!end); //if end is 0, then keep going

    al_destroy(&commands); //destroy
}


int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    if (argc == 2) {
        //fd to read
        int file_descriptor = open(argv[1], O_RDONLY);
        if (file_descriptor < 0) {
            perror("Failed to open file");
            return EXIT_FAILURE;
        }
        
        if (dup2(file_descriptor, STDIN_FILENO) < 0) { //dup2 to change STDIN
            fprintf(stderr, "Failed to redirect stdin\n");
            close(file_descriptor);
            
        } else { //check if file is terminal or run batch
            if (isatty(STDIN_FILENO)) { 
                interactive_mode();
            } else {
                batch_mode();
            }
        }
    } else {
        if (!isatty(STDIN_FILENO)) { //input being piped here
            batch_mode(NULL); // Pass NULL to indicate input is piped
        } else {
            // No arguments and stdin is a terminal
            interactive_mode();
        }
    }

    return EXIT_SUCCESS;
}