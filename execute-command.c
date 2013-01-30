// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
* DFS for traversing tree constructed with tokens in read-command.c.
* The pipes will be a size 2 integer array of file descriptors for piping.
* If not used, they will be set to NULL.
* pipe_direction is 0 for out and 1 for in.
*/
void DFStree(command_t node, int input, int output, int closef) {
	int fildes[2];
	int status;
	int exit_status;

	// Switch on enum command_type
	switch(node->type) {

	case AND_COMMAND:	// A && B
		// node->left
		DFStree(node->u.command[0], input, output, closef);
		if(node->u.command[0]->status == EXIT_SUCCESS) {
			// node->right
			DFStree(node->u.command[1], input, output, closef);
			//take the status of the rhs
			node->status = node->u.command[1]->status;
		} else {
			node->status = EXIT_FAILURE;
		}
		return;

	case OR_COMMAND:	// A || B
		DFStree(node->u.command[0], input, output, closef);
		if(node->u.command[0] != EXIT_SUCCESS) {
			DFStree(node->u.command[1], input, output, closef);
			node->status = node->u.command[1]->status;
		} else {
			node->status = EXIT_SUCCESS;
		}
		return;

	case PIPE_COMMAND:	// A | B

		// pipe returns 2 file descriptors after opening a pipe
		// between them. One is conventionally used for input
		// while the other is for output.
		status = pipe(fildes);
		if (status == -1) {
			/* an error occured */
			fprintf(stderr, "error: status == %d\n", status);
			return;
		}
		
		//run lhs of pipe
		DFStree(node->u.command[0], input, fildes[1], fildes[0]);
		
		//close the write port of the pipe
		close(fildes[1]);
		
		//run rhs of pipe
		DFStree(node->u.command[1], fildes[0], output, fildes[1]);
		
		//close read port of the pipe
		close(fildes[0]);
		
		//node takes the status of the rhs
		node->status = node->u.command[1]->status;
		return;

	case SUBSHELL_COMMAND:	// ( A )
		// This basically acts as a root of another tree.
		// Parallelization can occur within the subshell, but
		// I doubt we can parallelize this with other commands
		// in the parent tree.
		DFStree(node->u.subshell_command, input, output, closef);
		node->status = node->u.subshell_command->status;
		return;

	case SIMPLE_COMMAND:	// a simple command
		
		// Fork a new process
		switch (fork()) {
		case -1: /* an error occured */
			fprintf(stderr, "bad fork\n");
			return;
		case 0:
			//close our file descriptor from other side of pipe
			if(closef >= 0)
				close(closef);
		
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
			if(dup2(input, 0) < 0) {
			//if(dup2(0, input) < 0) {
				fprintf(stderr, "bad dup2(input, 0)\n");
				return;
			}

			// redirect stdout to our output file descriptor using dup2
			if(dup2(output, 1) < 0) {
			//if(dup2(1, output) < 0) {
				fprintf(stderr, "bad dup2(output, 1)\n");
				return;
			}

			execvp(node->u.word[0], node->u.word);
			
			//we should never get here
			printf("execvp failed on %s\n", node->u.word[0]);
			return;

		default: // Parent process block
			// Subject to change later with parallelization
			wait(&exit_status);
			node->status = exit_status;
			return;
		}
	default: //SEQUENCE_COMMAND
		fprintf(stderr, "got sequence command\n");
		return;
	}
}
   
   
int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, bool time_travel)
{
  DFStree(c, STDIN_FILENO, STDOUT_FILENO, -1);
}
