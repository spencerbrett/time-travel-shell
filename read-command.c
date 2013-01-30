// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "alloc.h"


char* subString(char* string, int start, int end) {
	size_t size = end - start + 1;
	char* substr = (char*) checked_malloc(size + 1);
	memcpy((void*) substr, string + start, size);
	substr[size] = '\0';
	return substr;
}

char* getTail(char* string, int start) {
	char c = string[start];
	int end = start;
	while(c != '\0') {
		end++;
		c = string[end];
	}
	return subString(string, start, end);
}



typedef struct stack_node {
	command_t data;
	struct stack_node* next;
}* stack_node_t;

command_t pop(stack_node_t* head) {
	if(head == NULL || *head == NULL)
		return NULL;
	command_t data = (*head)->data;
	stack_node_t temp = (*head);
	*head = (*head)->next;
	free(temp);
	return data;
}

void push(stack_node_t* head, command_t command) {
	stack_node_t newNode = (stack_node_t) checked_malloc(sizeof(struct stack_node));
	newNode->data = command;
	newNode->next = *head;
	*head = newNode;
	return;
}

typedef struct simple_command {
	char** command;
	char* input;
	char* output;
}* simple_command_t;

typedef enum { simple_command, and, or, pipes, semicolon, open_paren, close_paren } token_type_t;

/* these will form a linked list of tokens, in the order they are written to input
 * Example input: a > b || c
 * Ouput: list of three tokens, "a > b", "||", and "c".
 * note: newlines might be able to be replaced with semicolons to make things easier
 * requirements: 
 *   "ands", "ors", "pipes", and "semicolons" must have a word before and after (no newlines, no other tokens except parentheses)
 *   parentheses must be formed correctly
 *   comments start with # and shouldn't be preceded by an "ordinary token", whatever that means  
 *   we can remove comments */
typedef struct token {
	token_type_t type;
	simple_command_t command; //only set this for simple_command type, null otherwise
	struct token* next;
}* token_t;

typedef struct token_list {
	token_t head;
	token_t tail;
} * token_list_t;


token_list_t new_list(void) {
	token_list_t tl = checked_malloc(sizeof(struct token_list));
	tl->head = tl->tail = NULL;
	return tl;
}

token_list_t add_to_list(token_list_t tl, token_t tokenIn) {
	token_t newToken = (token_t) checked_malloc(sizeof(struct token));
	newToken->type = tokenIn->type;
	newToken->command = tokenIn->command;
	newToken->next = NULL;

	if(tl == NULL){
		printf("List not initialized\n");
      		free(newToken);
    		return tl;
	}
	else if( tl ->head == NULL && tl->tail == NULL )
	{
		tl->head = tl->tail = newToken;
		return tl;
	}
	else if( tl->head == NULL || tl->tail == NULL )
  	{
  		fprintf(stderr, "There is something seriously wrong with your assignment of head/tail to the list\n");
 		free(newToken);
  		return NULL;
  	}
	else 
	{
		tl->tail->next = newToken;
		tl->tail = newToken;
	}
	
	return tl;
}

token_list_t remove_from_list( token_list_t tl) {
	token_t h = NULL;
	token_t p = NULL;

	if( tl == NULL ){
		printf("List is empty\n");
		return tl;	
	}
	else if( tl ->head == NULL && tl->tail == NULL ) {
		printf("List is empty\n");
		return tl;
	}
	else if( tl->head == NULL || tl->tail == NULL ) {
  		fprintf(stderr, "There is something seriously wrong with your assignment of head/tail to the list\n");
  		return NULL;
	}
	
	h = tl->head;
	p = h->next;
	free(h);
	tl->head = p;
	if( NULL == tl->head )  
		tl->tail = tl->head;

	return tl;
}

token_list_t free_list( token_list_t tl ) {
	while(tl->head) {
		remove_from_list(tl);
	}
	return tl;
}

char* getLine(int (*get_next_byte) (void *), void* get_next_byte_argument) {
	
	char* buffer = (char*) checked_malloc(25*sizeof(char));
	size_t bufferLimit = 25;
	unsigned int i = 0;

	int byte = (*get_next_byte) (get_next_byte_argument);
	char c;
	
	if(byte < 0)
		return NULL;
		
	while(1) {
		
		if(byte < 0 || (c = (char) byte) == '\n' ) {
			buffer[i] = '\0';
			return buffer;
		}
		if(i + 1 >= bufferLimit) {
			bufferLimit *= 2;
			buffer = (char*) checked_realloc((void*) buffer, bufferLimit);
		}
		buffer[i] = c;
		i++;
		byte = (*get_next_byte) (get_next_byte_argument);
	}
}

//	returns true if the given character is in the approved word character set, otherwise false
bool is_word_char( char c ) {
	return (isalnum(c) || c == '!' || c == '%' || c == '+' || c == ',' || c == '-' ||
	c == '.' || c =='/' || c == ':' || c == '@' || c == '^' || c == '_');
}

bool check_for_space( const char * str) {
	while(*str != '\0') {
		if(isspace((int)*str))
			return true;
		str++;
	}
	return false;
}

/* takes strings of the form a, a<b, a>c, a<b>c. Returns a simple_command_t object,
 * with the proper settings (remove whitespace before and after all strings, newlines
 * are not allowed directly after < or >, cant have a>c<b).  */
simple_command_t parseSimpleCommand (char* simpleCommandString) {
	const char* lp;
	int start = 0;
	int running_alpha = 0;
	bool found_word = false;
	simple_command_t simp_command = (simple_command_t) checked_malloc(sizeof(struct simple_command));
	lp = simpleCommandString;

	/* check for mismatched ordering of I/O */
	if(strchr(simpleCommandString, '<') != NULL && strchr(simpleCommandString, '>') != NULL 
		&& (strchr(simpleCommandString, '<') > strchr(simpleCommandString, '>')))
		return NULL;

	/* Skip leading whitespace characters (use ctype.h)*/
	while(isspace((int)*lp)) {
		lp++;
		start++;
	}

	if(!*lp)
		return NULL;

	/* if first non-whitespace character is not part of the word set it is invalid input. return null. */
	if(!is_word_char(*lp)) 	
		return NULL;

	/* found a word for the command string */
	found_word = true;
	
	int wordLimit = 5;
	char** wordList = (char**) checked_malloc(wordLimit*sizeof(char*)+1);
	int nWords = 0;
	int running_start = start;
	bool inWord = false;
	/* Found the command string */
	while(*lp != '\0' && !strchr("<>", *lp)){
		if(is_word_char(*lp)) {
			if(!inWord){
				inWord = true;
				running_start = lp - simpleCommandString;
			}
			running_alpha = lp - simpleCommandString;
		} else if(isspace(*lp) && inWord) {
			inWord = false;
			if(nWords == wordLimit) {
				wordLimit = 2*wordLimit;
				wordList = (char**) checked_realloc((void*) wordList, wordLimit+1);
			}
			wordList[nWords] = subString(simpleCommandString, running_start, running_alpha);
			nWords++;
		}
		lp++;
	}
	/*if(nWords == 0) {
		return NULL;
	}*/
	if(inWord) {
		if(nWords == wordLimit) {
			wordList = (char**) checked_realloc((void*) wordList, 2*wordLimit+1);
		}
		wordList[nWords] = subString(simpleCommandString, running_start, running_alpha);
		nWords++;
	}
	wordList[nWords] = '\0';
	
	
	/* update simple command with command string */
	simp_command->command = wordList;	
	//simp_command->command = subString(simpleCommandString, start, running_alpha);
	simp_command->input = simp_command->output = NULL;

	/* check if done */
	if(!*lp && found_word)
		return simp_command;

	/* reset found word for the input and/or output */
	found_word = false;

	/* it must be either < or > if not done*/
	
	/* look for input */
	if(strchr("<", *lp)) {
		lp++;
		start = lp - simpleCommandString;
		/* Skip leading whitespace characters */
		while(isspace((int)*lp)) {
			lp++;
			start++;
		}
		if(!*lp)
			return NULL;
		/* if first non-whitespace character is not part of the word set it is invalid input. return null. */
		if(!is_word_char(*lp)) 	
			return NULL;
		
		found_word = true;

		/* Found the input string */
		while(*lp != '\0' && !strchr(">", *lp)){
			//lp++;
			//start++;
			if(is_word_char(*lp)) {
				running_alpha = lp - simpleCommandString;
			}
			lp++;
		}
		
		/* update simple command with input string */
		simp_command->input = subString(simpleCommandString,start, running_alpha);

		/* check if done */
		// Check if there are any invalid characters in file name
		if(simp_command->input != NULL && check_for_space(simp_command->input))
			return NULL;
		else if(!*lp  && found_word)
			return simp_command;
	}

	/* reset found word flag */
	found_word = false;

	/* look for output */
	if(strchr(">", *lp)) {
		lp++;
		start = lp - simpleCommandString;
		/* Skip leading whitespace characters */
		while(isspace((int)*lp)) {
			lp++;
			start++;
		}
		if(!*lp)
			return NULL;
		/* if first non-whitespace character is not part of the word set it is invalid input. return null. */
		if(!is_word_char(*lp)) 	
			return NULL;
		
		found_word = true;

		/* Found the output string */
		while(*lp != '\0' && !strchr(">", *lp)){
			//lp++;
			//start++;
			if(is_word_char(*lp)) {
				running_alpha = lp - simpleCommandString;
			}
			lp++;
		}
		
		/* update simple command with input string */
		simp_command->output = subString(simpleCommandString,start, running_alpha);

		/* check if done */
		// Check if there are any invalid characters in file name
		if(simp_command->output != NULL && check_for_space(simp_command->output))
			return NULL;
		else if(!*lp  && found_word)
			return simp_command;
	}	

	return NULL;
}




token_t createSimpleCommandToken(char* string, int start, int end) {
	char* substr = subString(string, start, end);
	simple_command_t sc = parseSimpleCommand(substr);
	if(sc == NULL) {
		return NULL;
	}
	token_t t = (token_t) checked_malloc(sizeof(struct token));
	t->type = simple_command;
	t->command = sc;
	t->next = NULL;
	return t;
}


command_t makeTree(token_t head) {

		//create two command stacks, one for operators, one for operands
	stack_node_t operandTop = NULL;
	stack_node_t operatorTop = NULL;
	
	if(head == NULL) {
		return NULL;
	}

	//for each item in the expression:
	while (head)
	{
		//  if the item is a simple command
		if(head->type == simple_command) {
			//    create a command object
			command_t operand = (command_t) checked_malloc(sizeof(struct command));
			//    set its type to SIMPLE_COMMAND
			operand->type = SIMPLE_COMMAND;
			//    set "word" to the command string
			operand->u.word = head->command->command;
			//    set input and output
			operand->input = head->command->input;
			operand->output = head->command->output;
			//    push it on the operand stack
			push(&operandTop, operand);
			//    move to next token_t	
			//head = head->next;
		} else if(head->type == close_paren) {
			//too many close_parens
			return NULL;
		}
		//  if the item is an open parenthesis
		else if(head->type == open_paren){
			//    create a token list, which goes from the open parenthesis to the corresponding closed parenthesis (non-inclusive)
			//	have a running parentheses count to figure out corresponding closed paren
			int paren_count = 1;
			//	start the subshell list at the first token after the open_paren
			//token_list_t tl = new_list();
			//	advance list pointer past open_paren
			token_t endList = head;
			head = head->next;
			token_t startList = head;
			while(head != NULL) {
				if(head->type == open_paren)
					paren_count++;
				if(head->type == close_paren) {
					paren_count--;
					if(paren_count == 0) {
						endList->next = NULL;
						break;
					}
				}
				//add_to_list(tl, head);
				
				head = head->next;
				endList = endList->next;
			}
			
			if(paren_count != 0) {
				//not enough close_parens
				return NULL;
			}
			
			//    call makeTree recursively with the token list as an argumen
			//    create a command object
			//    set its type to SUBSHELL_COMMAND
			//    set subshell_command to point to the result of makeTree
			command_t subShell = makeTree(startList);
			
			if(subShell != NULL) {
				command_t operand = (command_t) checked_malloc(sizeof(struct command));
				operand->type = SUBSHELL_COMMAND;
				operand->u.subshell_command = subShell;
				push(&operandTop, operand);
			}
			endList->next = head;
			
			//    set status
////////////////////////	/*FLAG*/
			//operand->status = -1;
			//    push it on the operand stack
////////////////////////	/*FLAG*/
			
			//free_list(tl);
		}
		//  if the item i is an operator (&&, ||, |, ;) and the operator stack is empty or the operator on the top of the stack is higher priority
		else if((head->type == and || head->type == or || head->type == pipes || head->type == semicolon) && (operatorTop!=NULL && (operatorTop->data->type == PIPE_COMMAND || ((operatorTop->data->type == AND_COMMAND || operatorTop->data->type == OR_COMMAND) && head->type != pipes)) /*((operatorTop->data->type == PIPE_COMMAND && head->type != pipe) || (operatorTop->data->type != SEQUENCE_COMMAND && head->type == semicolon))*/)) {
			//    pop an operator from the operator stack
			command_t operand = pop(&operatorTop);
			//    pop two operands from the operand stack
			//    set the operator's command[0] <- second operand, command[1] <- first operand
			operand->u.command[1] = pop(&operandTop);
			operand->u.command[0] = pop(&operandTop);
			if(operand->u.command[1] == NULL || operand->u.command[0] == NULL) {
				return NULL;
			}
			//    push the result on the operand stack
			push(&operandTop, operand);
			//    push i on the operator stack
			command_t operator = (command_t) checked_malloc(sizeof(struct command));
			if(head->type == and)
				operator->type = AND_COMMAND;
			if(head->type == or)
				operator->type = OR_COMMAND;
			if(head->type == pipes)
				operator->type = PIPE_COMMAND;
			if(head->type == semicolon)
				operator->type = SEQUENCE_COMMAND;
			push(&operatorTop, operator);
		}
		//  else: (the item is an operator, and the operator stack is not empty, and the opeator on the top of the stack is lower priority)
		else {
			//    create a command object
			//    set its type to the appropriate value
			//    push it on the operator stack
			command_t operator = (command_t) checked_malloc(sizeof(struct command));
			if(head->type == and)
				operator->type = AND_COMMAND;
			if(head->type == or)
				operator->type = OR_COMMAND;
			if(head->type == pipes)
				operator->type = PIPE_COMMAND;
			if(head->type == semicolon)
				operator->type = SEQUENCE_COMMAND;
			push(&operatorTop, operator);
			//while the operator stack is not empty
		}
		head = head->next;
	}
	while(operatorTop) {
		//  pop an operator from the operator stack
		command_t operator = pop(&operatorTop);			
		//  pop two operands from the oprand stack
		//  set the operator's command[0] <- second operand, command[1] <- first operand
		operator->u.command[1] = pop(&operandTop);
		operator->u.command[0] = pop(&operandTop);
		if(operator->u.command[1] == NULL || operator->u.command[0] == NULL) {
			return NULL;
		}
		//  push the result on the operand stack
		push(&operandTop, operator);	
		
		
	}
	//if we ever run out of items in the operand stack, then we dont have a valid input

	//if we have more than one item in the operand stack at the end, then we dont have a valid input
			//return the single item in the operand stack (the head of the tree)
	command_t tree_head = pop(&operandTop);
	if(operandTop == NULL)
		return tree_head;
	else
		return NULL;
}


struct command_stream {
	command_t data;
	struct command_stream* next;
	struct command_stream* prev;
};

bool isEmpty(command_stream_t head) {
	if(head->next == head)
		return true;
	return false;
}

command_stream_t createCommandStream(void) {
	command_stream_t headDummy = (command_stream_t) checked_malloc(sizeof(struct command_stream));
	headDummy->data = NULL;
	headDummy->next = headDummy;
	headDummy->prev = headDummy;
	return headDummy;
}

void commandStreamEnqueue(command_stream_t queueHead, command_t data) {
	command_stream_t newNode = (command_stream_t) checked_malloc(sizeof(struct command_stream));
	newNode->data = data;
	newNode->next = queueHead;
	newNode->prev = queueHead->prev;
	queueHead->prev->next = newNode;
	queueHead->prev = newNode;
}

command_t commandStreamDequeue(command_stream_t queueHead) {
	command_stream_t next = queueHead->next;
	queueHead->next->next->prev = queueHead;
	queueHead->next = queueHead->next->next;
	command_t data = next->data;
	free(next);
	return data;
}


command_stream_t make_command_stream (int (*get_next_byte) (void *), void *get_next_byte_argument) {
	token_t headDummy = (token_t) checked_malloc(sizeof(struct token));
	headDummy->next = NULL;
	token_t tail = headDummy;

	command_stream_t commandStream = createCommandStream();
	
	char* line = getLine(*get_next_byte, get_next_byte_argument);
	int i = 0;
	int startCommand = 0;
	int lineNo = 1;
	
	bool inWord = false;
	int parenDepth = 0;
	char c;
	while(line != NULL) {
		while(1) {
			c = line[i];
		
			if(isalnum(c) || c == '!' || c == '%' || c == '+' || c == ',' || c == '-' ||
				c == '.' || c == '/' || c == ':' || c == '@' || c == '^' || c == '_' || c == '<' || c == '>') {
				inWord = true;
				i++;
			} else if(isspace(c)) {
				i++;
			} else if(c == '|') {
				if(!inWord) {
					//there has been no simple command, so fail					
					fprintf(stderr, "%d: No command given\n", lineNo);
					exit(1);
				}
			
				token_t sct = createSimpleCommandToken(line, startCommand, i-1);
				if(sct == NULL) { 
					fprintf(stderr, "%d: Invalid command\n", lineNo);
					exit(1);
				}
				
				token_t orPipeToken = (token_t) checked_malloc(sizeof(struct token));
				orPipeToken->command = NULL;
				orPipeToken->next = NULL;
				sct->next = orPipeToken;
				
				if(line[i+1] == '|') {
					orPipeToken->type = or;
					i += 2;
				} else {
					orPipeToken->type = pipes;
					i++;
				}
				tail->next = sct;	
				tail = orPipeToken;
				startCommand = i;
			} else if(c == '&') {
				if(!inWord) {
					fprintf(stderr, "%d: No command given\n", lineNo);
					exit(1);
				}
				if(line[i+1] != '&') {
					fprintf(stderr, "%d: invalid character '&'\n", lineNo);
					exit(1);
				}
			
				token_t sct = createSimpleCommandToken(line, startCommand, i-1);
				if(sct == NULL) {
					fprintf(stderr, "%d: Not a valid command\n", lineNo);
					exit(1);
				} else {
					token_t andToken = (token_t) checked_malloc(sizeof(struct token));
					andToken->type = and;
					andToken->command = NULL;
					andToken->next = NULL;
					sct->next = andToken;
					tail->next = sct;
					tail = andToken;
					i += 2;
					startCommand = i;
				}
			} else if(c == '(' || c == ')') {
				token_t pToken = (token_t) checked_malloc(sizeof(struct token));
				if(c == '(') {
					parenDepth++;
					pToken->type = open_paren;
				} else {
					parenDepth--;
					if(parenDepth < 0) {
						fprintf(stderr, "%d: Incorrect parentheses\n", lineNo);
						exit(1);
					}
					pToken->type = close_paren;
				}
				pToken->command = NULL;
				pToken->next = NULL;
			
				if(inWord) {
					token_t sct = createSimpleCommandToken(line, startCommand, i-1);
					if(sct == NULL) {
						//fprintf(stderr, "%d: Invalid command\n", lineNo);
						//exit(1);
						tail->next = pToken;
					} else {
						sct->next = pToken;
						tail->next = sct;
					}
					tail = pToken;
				} else {
					tail->next = pToken;
					tail = pToken;
				}
				i++;
				startCommand = i;
			} else if(c == ';') {
				//if not in parens: send everything to makeTree, nextLine is everything after ';' in this line, same lineno
				//if in parens, then create a sequence token, continue with algo
				if(parenDepth > 0) {
					token_t sequenceToken = (token_t) checked_malloc(sizeof(struct token));
					sequenceToken->type = semicolon;
					sequenceToken->command = NULL;
					sequenceToken->next = NULL;
					if(inWord) {
						token_t sct = createSimpleCommandToken(line, startCommand, i-1);
						if(sct == NULL) {
							fprintf(stderr, "%d: Invalid command\n", lineNo);
							exit(1);
						} else {
							sct->next = sequenceToken;
							tail->next = sct;
						}
					} else {
						tail->next = sequenceToken;
					}
					i++;
					startCommand = i;
					inWord = false;
					tail = sequenceToken;
				} else {
					if(inWord) {
						token_t sct = createSimpleCommandToken(line, startCommand, i-1);
						if(sct == NULL) {
							fprintf(stderr, "%d: Invalid statement\n", lineNo);
							exit(1);
						} else {
							tail->next = sct;
							tail = sct;
						}
					}
					command_t commandHead = makeTree(headDummy->next);
					if(commandHead != NULL) {
						commandStreamEnqueue(commandStream, commandHead);
						token_t curr = headDummy->next;
						while(curr != NULL) {
							if(curr->type == simple_command && curr->command != NULL) {
								//free(curr->command);
							}
							token_t tempToken = curr;
							curr = curr->next;
							free(tempToken);
						}
						headDummy->next = NULL;
						tail = headDummy;
						i++;
						char* tempLine = getTail(line, i);
						free(line);
						line = tempLine;
						i = 0;
						startCommand = 0;
						inWord = false;
					} else {
						// must have a valid statement if we get a ';'
						fprintf(stderr, "%d: Invalid command\n", lineNo);
						exit(1);						
					}
				}
			} else if(c == '#' || c == '\0') {
				if(inWord) {
					token_t sct = createSimpleCommandToken(line, startCommand, i-1);
					if(sct != NULL) {
						tail->next = sct;
						tail = sct;
						inWord = false;
					}
				}
				command_t commandHead = makeTree(headDummy->next);
				if(commandHead != NULL) {
					commandStreamEnqueue(commandStream, commandHead);
					token_t curr = headDummy->next;
					while(curr != NULL) {
						if(curr->type == simple_command && curr->command != NULL) {
							//free(curr->command);	
						}
						token_t tempToken = curr;
						curr = curr->next;
						free(tempToken);
					}
					headDummy->next = NULL;
					tail = headDummy;
				}
				free(line);
				i = 0;
				startCommand = 0;
				break;
			} else {
				fprintf(stderr, "%d: Invalid character\n", lineNo);
				exit(1);
			} 
		}
		line = getLine(*get_next_byte, get_next_byte_argument);
		lineNo++;
	}
	if(isEmpty(commandStream)) {
		fprintf(stderr, "%d: Invalid structure\n", lineNo);
		exit(1);
	}
	return commandStream;
}

command_t read_command_stream (command_stream_t stream) {
	return commandStreamDequeue(stream);
}
