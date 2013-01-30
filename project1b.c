/*
 * DFS for traversing tree constructed with tokens in read-command.c.
 * The pipes will be a size 2 integer array of file descriptors for piping.
 * If not used, they will be set to NULL.
 * pipe_direction is 0 for out and 1 for in.
*/
int DFStree(command_t node, int input, int output) {
	// Switch on enum command_type
	switch(node->type) {

	case AND_COMMAND:	// A && B
		// node->left
		if(DFStree(node->u.command[0], input, output) == 0) {
			// node->right
			return DFStree(node->u.command[1], input, output);
		} else {
			return -1;
		}
		break;

	case OR_COMMAND:	// A || B
		if(DFStree(node->u.command[0], input, output) == -1) {
			return DFStree(node->u.command[1], input, output);
		} else {
			return 0;
		}
		break;

	case PIPE_COMMAND:	// A | B
		int fildes[2];
		int status;

		// pipe returns 2 file descriptors after opening a pipe
		// between them. One is conventionally used for input
		// while the other is for output.
		status = pipe(fildes);
		if (status == -1) {
			/* an error occured */
			return -1;
		}
		// Parent writes to pipe;
		// Check for error in writing process
		if(DFStree(node->u.command[0], input, fildes[0])
			return -1;

		// Child reads from pipe
		return DFStree(node->u.command[1], fildes[1], output);
		break;

	case SUBSHELL_COMMAND:	// ( A )
		// This basically acts as a root of another tree.
		// Parallelization can occur within the subshell, but
		// I doubt we can parallelize this with other commands
		// in the parent tree.
		return DFStree(node->u.subshell_command, input, output);
		break;

	case SIMPLE_COMMAND:	// a simple command
		int exit_status;
		// Fork a new process
		switch (fork()) {
		case -1: /* an error occured */
			return -1;
		case 0: // Check if piping is occuring
                // The current process is going to write to the pipe
                // Check if the command has any input file, if it does open it and redirect to it from stdin
                if(node->input != NULL) {
                    // open system call uses fcnt1.h library and returns a file descriptor
                    // second argument is a list of flags. I've used the read only flag.
                    input = open(node->input, O_RDONLY);
                }

			    // Check if the command has any output file, if it does, open it and redirect to it from stdout
			    if(node->output != NULL) {
			        // using write only flag
                    output = open(node->output, O_WRONLY);
			    }

			    // redirect stdin to our input file descriptor using dup2 which is a system call that aliases one fd to another.
                // dup2 returns negative if an error occurs.
                if(dup2(input, 0) < 0)
                    return -1;

                // redirect stdout to our output file descriptor using dup2
                if(dup2(output, 1) < 0)
                    return -1;

				execvp(node->u.word[0], node->u.word);
				node->status = EXIT_SUCCESS;
				exit(node->status);

		default: // Parent process block
			// Subject to change later with parallelization
			wait(&exit_status);
			return exit_status;
		}
	}
}
