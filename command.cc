
/*
 * CS354: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "command.h"

SimpleCommand::SimpleCommand()
{
	// Creat available space for 5 arguments
	_numberOfAvailableArguments = 5;
	_numberOfArguments = 0;
	_arguments = (char **) malloc( _numberOfAvailableArguments * sizeof( char * ) );
}

void
SimpleCommand::insertArgument( char * argument )
{
	if ( _numberOfAvailableArguments == _numberOfArguments  + 1 ) {
		// Double the available space
		_numberOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numberOfAvailableArguments * sizeof( char * ) );
	}
	
	_arguments[ _numberOfArguments ] = argument;

	// Add NULL argument at the end
	_arguments[ _numberOfArguments + 1] = NULL;
	
	_numberOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numberOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numberOfSimpleCommands * sizeof( SimpleCommand * ) );

	_numberOfSimpleCommands = 0;
	_numberOfPipes = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append= 0;
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
	if ( _numberOfAvailableSimpleCommands == _numberOfSimpleCommands ) {
		_numberOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numberOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numberOfSimpleCommands ] = simpleCommand;
	_numberOfSimpleCommands++;
	_numberOfPipes++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numberOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		free ( _simpleCommands[ i ] );
	}

	if ( _outFile ) {
		free( _outFile );
	}

	if ( _inputFile ) {
		free( _inputFile );
	}

	if ( _errFile ) {
		free( _errFile );
	}

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append= 0;
	_numberOfPipes = 0;
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
		printf("\n");
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background   Append\n" );
	printf( "  ------------ ------------ ------------ ------------ ---------\n" );
	printf( "  %-12s %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inputFile?_inputFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO", _append?"YES":"NO");
	printf( "\n\n" );
	
}

void
Command::execute()
{      
	// Don't do anything if there are no simple commands
	if ( _numberOfSimpleCommands == 0 ) {
		prompt();
		return;
	}

	// Print contents of Command data structure
	print();

	// Add execution here
	// For every simple command fork a new process
	// Setup i/o redirection
	// and call exec
	int defaultin = dup( 0 );
	int defaultout = dup( 1 );
	int defaulterr = dup( 2 );
	int infd, outfd;
	int fdpipe[_numberOfPipes][2];
	// Redirection part
	for(int i=0 ; i<_numberOfSimpleCommands ; i++){
		printf("Iteration %d\n",i);
		if ( pipe(fdpipe[i]) == -1) {
			perror( "cat_grep: pipe");
			exit( 2 );
		}
		if (i == 0)
		{ 
		// Input Redirection
			if (_inputFile){
				infd = open( _inputFile, 0666 );
				if ( infd < 0 ) { // read from a file
					perror( "cat_grep: creat outfile" );
					exit( 2 );
				}
				dup2(infd, 0);
				close(infd);
			}
			else{
				dup2(defaultin, 0);
				close(defaultin);
			}
		// Output Redirection
			if(_numberOfSimpleCommands > 1){ // If there is more than one command then there must be a pipe
				dup2(fdpipe[i][1],1);
				close(fdpipe[i][1]);
			}
		}


		else if ( i == _numberOfSimpleCommands-1){
			// input redirection
				if(_numberOfSimpleCommands > 1){
				dup2(fdpipe[i-1][0], 0);
				close(fdpipe[i-1][0]);
				}
			// Output Redirection
				if(_append){ // append to a file
				outfd = open(_outFile, O_WRONLY | O_CREAT | O_APPEND, 0777);
				if (outfd < 0) {
					perror("cat_grep: open outfile");
					exit(2);
				}
				printf("I am here");
				// Redirect output to the created outfile instead off printing to stdout 
				dup2(outfd, 1);
				close(outfd);
				}

				else if(_outFile){  // rewrite on a file
					int outfd = creat( _outFile, 0666 );
					if ( outfd < 0 ) {
						perror( "cat_grep: creat outfile" );
						exit( 2 );
					}
					// Redirect output to the created utfile instead off printing to stdout 
					dup2( outfd, 1 );
					close( outfd );
				}
				else{ // default system output
					dup2(defaultout, 1);
					close(defaultout);
				}

		}
		else{ // Middle Command
			dup2(fdpipe[i-1][0],0);
			close(fdpipe[i-1][0]);
			dup2(fdpipe[i][1],1);
			close(fdpipe[i][1]);
		}
		// Forking

		pid_t pid = fork();
		if (pid < 0){
			perror("Fork Failed");
			exit(2);
		}
		else if (pid == 0)
		{
			execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);

		}
		else{
			dup2(defaultin,0);
			dup2(defaultout,1);
			if(!_background){
				waitpid(pid, 0, 0);
			}
		}
	}
	// Clear to prepare for next command
	clear();
	
	// Print new prompt
	prompt();
}

// Shell implementation
void
Command::prompt()
{
	printf("myshell>");
	fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);
void sig_handler(int signal)
{
   //system("stty -echoctl");
}
int 
main()
{
        
        signal(SIGINT, SIG_IGN);
        //system("stty -werase");
        //clear();
	Command::_currentCommand.prompt();	
	yyparse();
       // system("stty echoctl");
	return 0;
}

