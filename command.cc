
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
#include <time.h>
#include <glob.h>
SimpleCommand::SimpleCommand()
{
	// Creat available space for 5 arguments
	_numberOfAvailableArguments = 5;
	_numberOfArguments = 0;
	_arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
	if (_numberOfAvailableArguments == _numberOfArguments + 1)
	{
		// Double the available space
		_numberOfAvailableArguments *= 2;
		_arguments = (char **)realloc(_arguments,
									  _numberOfAvailableArguments * sizeof(char *));
	}

	_arguments[_numberOfArguments] = argument;

	// Add NULL argument at the end
	_arguments[_numberOfArguments + 1] = NULL;

	_numberOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numberOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc(_numberOfSimpleCommands * sizeof(SimpleCommand *));

	_numberOfSimpleCommands = 0;
	_numberOfPipes = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
	if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
	{
		_numberOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
													_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
	}

	_simpleCommands[_numberOfSimpleCommands] = simpleCommand;
	_numberOfSimpleCommands++;
	_numberOfPipes++;
}

void Command::clear()
{
	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
		{
			free(_simpleCommands[i]->_arguments[j]);
		}

		free(_simpleCommands[i]->_arguments);
		free(_simpleCommands[i]);
	}

	if (_outFile)
	{
		free(_outFile);
	}

	if (_inputFile)
	{
		free(_inputFile);
	}

	if (_errFile)
	{
		free(_errFile);
	}

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append = 0;
	_numberOfPipes = 0;
}

void Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");

	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		printf("  %-3d ", i);
		for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
		{
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
		}
		printf("\n");
	}

	printf("\n\n");
	printf("  Output       Input        Error        Background   Append\n");
	printf("  ------------ ------------ ------------ ------------ ---------\n");
	printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
		   _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
		   _background ? "YES" : "NO", _append ? "YES" : "NO");
	printf("\n\n");
}
void Command::newline()
{
	prompt();
}
// global variable
int global_pid;

void log_file(int dumm)
{
	FILE *fp;
	time_t now;
	int pid = global_pid;
	time(&now);
	fp = fopen("log_file.txt", "a");
	if (fp == NULL)
	{
		perror("Error opening the log file");
		printf("Error opening the log file");
		return;
	}

	struct tm *local = localtime(&now);
	int hour, min, sec;
	hour = local->tm_hour;
	min = local->tm_min;
	sec = local->tm_sec;
	if (hour < 12)
	{
		fprintf(fp, "Child %d Terminated %02d:%02d:%02d am\n", pid, hour, min, sec);
	}
	else
	{
		fprintf(fp, "Child %d Terminated %02d:%02d:%02d pm\n", pid, hour, min, sec);
	}

	fclose(fp);
}

void Command::execute()
{
	// Don't do anything if there are no simple commands
	if (_numberOfSimpleCommands == 0)
	{
		prompt();
		return;
	}

	// Print contents of Command data structure
	print();

	// Add execution here
	// For every simple command fork a new process
	// Setup i/o redirection
	// and call exec
	int defaultin = dup(0);
	int defaultout = dup(1);
	int defaulterr = dup(2);
	int infd, outfd;
	int fdpipe[_numberOfPipes][2];
	// Redirection part
	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		// handling wildcard
		if (strcmp(_simpleCommands[i]->_arguments[0], "echo") == 0)
		{
			glob_t results;

        // Perform wildcard expansion for the given argument
        int status = glob(_simpleCommands[i]->_arguments[1], GLOB_NOCHECK, NULL, &results);

        if (status == 0) {
            // Iterate through the matched filenames
            for (size_t j = 0; j < results.gl_pathc; ++j) {
                printf("%s\n", results.gl_pathv[j]);
            }

            // Free the memory allocated by glob
            globfree(&results);
        }  else {
            perror("Error during wildcard expansion");
        }
			printf("\n");
			clear();
			prompt();
			return;
		}
		// handling cd [dir]
		if (strcmp(_simpleCommands[i]->_arguments[0], "cd") == 0)
		{
			if (_simpleCommands[i]->_arguments[1] == NULL)
			{
				chdir(getenv("HOME"));
			}
			else
			{
				chdir(_simpleCommands[i]->_arguments[1]);
			}
			clear();
			prompt();
			return;
		}
		// handling other commands with wildcard
		printf("Iteration %d\n", i);
		if (pipe(fdpipe[i]) == -1)
		{
			perror("cat_grep: pipe");
			exit(2);
		}
		if (i == 0)
		{
			// Input Redirection
			if (_inputFile)
			{
				infd = open(_inputFile, 0666);
				if (infd < 0)
				{ // read from a file
					perror("cat_grep: creat outfile");
					exit(2);
				}
				dup2(infd, 0);
				close(infd);
			}
			else
			{
				dup2(defaultin, 0);
				// close(defaultin);
			}
			// Output Redirection
			if (_numberOfSimpleCommands > 1)
			{ // If there is more than one command then there must be a pipe
				dup2(fdpipe[i][1], 1);
				close(fdpipe[i][1]);
			}
		}

		if (i == _numberOfSimpleCommands - 1)
		{
			// input redirection
			if (_numberOfSimpleCommands > 1)
			{
				dup2(fdpipe[i - 1][0], 0);
				close(fdpipe[i - 1][0]);
			}
			// Output Redirection
			if (_append)
			{ // append to a file
				outfd = open(_outFile, O_WRONLY | O_CREAT | O_APPEND, 0777);
				if (outfd < 0)
				{
					perror("cat_grep: open outfile");
					exit(2);
				}
				printf("I am here");
				// Redirect output to the created outfile instead off printing to stdout
				dup2(outfd, 1);
				close(outfd);
			}

			else if (_outFile)
			{ // rewrite on a file
				int outfd = creat(_outFile, 0666);
				if (outfd < 0)
				{
					perror("cat_grep: creat outfile");
					exit(2);
				}
				// Redirect output to the created utfile instead off printing to stdout
				dup2(outfd, 1);
				close(outfd);
			}
			else
			{ // default system output
				dup2(defaultout, 1);
				// close(defaultout);
			}
		}
		
		else
		{ // Middle Command
			dup2(fdpipe[i - 1][0], 0);
			close(fdpipe[i - 1][0]);
			dup2(fdpipe[i][1], 1);
			close(fdpipe[i][1]);
		}
		// Forking

		pid_t pid = fork();
		if (pid < 0)
		{
			perror("Fork Failed");
			exit(2);
		}
		else if (pid == 0)
		{
			execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
		}
		else
		{
			dup2(defaultin, 0);
			dup2(defaultout, 1);

			global_pid=pid;
			signal(SIGCHLD, log_file);
			if (!_background)
			{
				waitpid(pid, 0, 0);
			}
		}
	}

	close(defaultin);
	close(defaultout);
	// Clear to prepare for next command
	clear();

	// Print new prompt
	prompt();
}

// Shell implementation
void Command::prompt()
{
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL)
	{
		printf("myshell %s> ", cwd);
	}
	else
	{
		perror("getcwd() error");
	}
	// printf("myshell>");
	// fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int yyparse(void);
void sig_handler(int signal)
{
	system("stty -echoctl");
	// system("stty -werase");
}
int main()
{

	signal(SIGINT, sig_handler);
	// system("stty -werase");
	// clear();
	Command::_currentCommand.prompt();
	yyparse();
	system("stty echoctl");
	return 0;
}
