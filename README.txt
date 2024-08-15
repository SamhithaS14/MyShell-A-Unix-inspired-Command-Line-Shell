Names:  Krithish Ayyappan       
        Samhitha Sangaraju              

Code Design and Commands:
        Process Overview:
            The code begins by determining if the given input file is either a file or a terminal. Based on that, 
            it will run batch_mode() for file inupt or interactive_mode() for termina input. We read line by line and
            pass the line to our parse_line() command. After we parse the input to extract our necessities, we run our our
            commands based on what the user desires. If it is a pipe command, it will run pipe_command(), else it will run
            exec_cmd() to run it. 

        batch_mode():
            This command is called in main() and used if the program recieves a file that is not a terminal. We Initialize an 
            arraylist of commands to use at the begining to store our commands. We also destroy this line-by-line to keep consistency.
            This will read from the file line by line and parse the commands in the line extracted. Then, we will 
            execute the command with the commands we parsed. This is all done in a do-while loop until we 
            reached the end of the file.

        interactive_mode():
            This command is called in main() and used if the program either recieves no file or if we recieved
            a file path and it is a path to a terminal. This command reads the input line using read() from the terminal,
            then it will pass it to parse_line(). If the command is piped "|", pipe_commands will be called to execute,
            else exec_cmd() will be called to run the command. This is also done in a do-while loop until "exit" is called.

        read_terminal():
            This command is used to read from a file or the terminal by passing a file descriptor, an address to a char array,
            and an address to an integer. We take the file descriptor to know where we are reading from and the address of
            the char array to know where we are updating our line. The address to the integer is utilized in batch_mode
            to know when we reached the end of a file to stop reading and terminate.

        parse_line():
            This command is used to parse input given a char* array with the line. It takes into account for wildcard tokens, 
            pipe characters, and redirection characters. Regular commands are read and stored in our arraylist of commands.
            Pipe commands are read till the pipe and stored, then after the pipe they are stored in a different arraylist. 
            The before and after commands are then passed into pipe_commands(). File redirection is taken care of when we 
            encounter ">" or "<". We extract the input and/or output file/s. If the user has multiple outputs, we use open()
            to create these files and only pass the last output file to exec_cmd or pipe_commands().

            **NOTE**: Our format for code needs to be properly spaced: such as ls > a.txt > b.txt. Everything needs to be spaced.

        exec_cmd():
            This command takes into the arraylist of commands, an input file and an output file. We first check if the given
            command is an exit call, if it is we call our exit_command(). Then, we check if program is cd to run our cd_command().
            We now fork() the process and first check for internal commands such as pwd and which. If it is not an internal command,
            it will check if given progrma is already a path to an executable or a bare name. If it starts with a '/', we assume its
            a path and use this as our exectuable, else we find the path for the exctuable using getPath(). If there are any input 
            or outpute files given, they are not NULL, then we use dup2 to open the file and set it to our input/output in the 
            process. 

        pipe_commands():
            Works the same as exec_cmd(). However, we set 2 forks to use, one for before the pipe to run first and one after. We use
            dup2 to do input and output files and do the same for after the pipe command as well.

        Internal Commands:
            pwd_command(): prints the current working directory

            which_command(): prints the path of the executable of the program using getPath(). If not found or given the name 
                            to a internal command, it will EXIT_FAILURE and print nothing.

            exit_command(): will free all the commands in our arraylist and then print everything recieved after.
            
        Helper Commands:
            getPath(): Using access(), we will search for the path of the exectuable in the 3 directories {usr/bin/local, /usr/bin, /bin}.
                        If nothing found, then NULL.

            wildcard(): Takes in the arguement and the list of commands. This will take into the account of (*) and expand it using glob.
