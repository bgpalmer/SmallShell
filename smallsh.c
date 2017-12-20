/*********************
** Brian Palmer
** CS344
** Project3
** smallsh.c
**********************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

/* Background Process Counter */
int pid_c = 0;

/* Exit Status Var */
char exit_status[256];

/* Source Flag */
int source_flag = 0;
char *source_file = NULL;

/* Target Flag */
int target_flag = 0;
char *target_file = NULL;

/* Foreground Only Flag */
int foreground_only = 0;

/* Foreground Toggle Catcher */
void catchSIGTSTP( int signo ) {
	if (foreground_only != 0) {
		char * message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 31);
		foreground_only = 0;
	} else {
		char *message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		foreground_only = 1;
	}
	sleep(5);
}

int main( void ) {

	char user_input[ 2048 ];
	pid_t pid_list[ 512 ] = { 0 };
	char *token;
	memset(exit_status, '\0', sizeof(exit_status));

	/* holds exit status of subshells or terminating signals */
	while( 1 ) {

		/* Default Sig Interrupt Setup */
		struct sigaction SIGINT_action = { 0 };
		SIGINT_action.sa_handler = SIG_IGN;
		sigfillset( &SIGINT_action.sa_mask );
		SIGINT_action.sa_flags = 0;

		sigaction( SIGINT, &SIGINT_action, NULL );

		/* Default Sig Stop Setup */

		struct sigaction SIGTSTP_action = { 0 };
		SIGTSTP_action.sa_handler = catchSIGTSTP;
		sigfillset( &SIGTSTP_action.sa_mask );
		SIGTSTP_action.sa_flags = 0;

		sigaction( SIGTSTP, &SIGTSTP_action, NULL );

		char *args[ 512 ] = { NULL };
		memset(user_input, '\0', sizeof( user_input ));
		printf(": "); fflush(stdout);
		fgets(user_input, sizeof(user_input), stdin);
		
		int argi = 0;
		if( (token = strtok(user_input, "\n ")) != NULL) {
			args[ argi++ ] = token;
			while( (token = strtok(NULL, "\n ")) != NULL ) {
				if (strcmp(token, "<") == 0) {
					source_flag = 1;
					source_file = strtok(NULL, "\n ");
				}
				else if (strcmp(token, ">") == 0) {
					target_flag = 1;
					target_file = strtok(NULL, "\n ");
				} 
				else {
					args[ argi++ ] = token;
				}
			}
		}

		char *tword = NULL;
		char modWord[ 256 ];
		memset(modWord, '\0', sizeof(modWord));

		/* Maybe Expand $$ */
		int k;
		for (k = 0; k < argi; k++) {
			if(( tword = strstr(args[ k ], "$$") ) != NULL ) {
				char *front = NULL;
				front = strtok(args[ k ], "$");
				if (front == NULL) {
					sprintf(modWord, "%d%s", getpid(), tword + 2);
				} else {
					sprintf(modWord, "%s%d%s", front, getpid(), tword + 2);
				}
				args[ k ] = modWord;
			}
		}


		/* No Command was entered; Do nothing */
		if ( args[ 0 ] == NULL ) { }

		/* A Comment was entered; Do nothing */
		else if( user_input[ 0 ] == '#') { }

		/* User entered the exit command */
		else if( strcmp(args[ 0 ], "exit") == 0) {
			if (strcmp( args[ argi - 1], "&") == 0) {
				args[ argi - 1 ] = NULL;
			}
			int i;
			for (i = 0; i < pid_c; i++) {
				kill(pid_list[i], SIGTERM);
			}
			exit(0);
		}

		/* User entered the cd command */
		else if( strcmp( args[ 0 ], "cd") == 0) {
			if (strcmp( args[ argi - 1], "&") == 0) {
				args[ argi - 1 ] = NULL;
			}
			char directory[ 256 ];
			memset(directory, '\0', sizeof(directory));

			if ( args[ 1 ] != NULL && args[ 1 ][ 0 ] == '/' ) {
				strcat(directory, args[ 1 ]);
				chdir(directory);
			}
			else {
				/* Use chdir() in built in command */
				strcpy(directory, getenv("HOME"));
				if (args[1] != NULL) {
					strcat(directory,"/");
					strcat(directory, args[ 1 ]);
				}
				chdir(directory);
			}

		}

		/* User entered the status command */
		else if( strcmp( args[ 0 ], "status") == 0) {
			/* command goes here */
			if (strcmp( args[ argi - 1], "&") == 0) {
				args[ argi - 1 ] = NULL;
			}
			printf("%s\n", exit_status); fflush(stdout);
		}

		/* Try calling command from PATH */

		else {
			pid_t spawn_pid = -5;

			/* Background Job */

			if (strcmp( args[ argi - 1 ], "&") == 0 && ! foreground_only) {
				spawn_pid = fork();
				if (spawn_pid < 0) {
					perror("smallsh: in main()");
					exit(1);
				}

				else if (spawn_pid == 0) {

					/* If the source & target files are undefined, set to /dev/null */

					if (source_file == NULL) {
						source_file = "/dev/null";
					}
					if (target_file == NULL) {
						target_file = "/dev/null";
					}
					int sourceFd = open(source_file, O_RDONLY);
					int targetFd = open(target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

					dup2(targetFd, 1);
					dup2(sourceFd, 0);
					args[ argi - 1 ] = NULL;
					if (execvp(args[0], args ) != 0) {
						fprintf(stderr, "%s: no such file or directory\n", args[ 0 ]); fflush(stderr);
						exit(1);
					}		
				}
				else {
					printf("background pid is %d\n", spawn_pid); fflush(stdout);
					pid_list[ pid_c++ ] = spawn_pid;
				}
			}

			/* Foreground Job */

			else {

				/* Child Signal Setup */


				struct sigaction SIGINT_action2 = { 0 };
				SIGINT_action2.sa_handler = SIG_DFL;
				sigfillset(&SIGINT_action2.sa_mask);
				SIGINT_action2.sa_flags = 0;

				int check_exit_status;
				spawn_pid = fork();
				
				if (spawn_pid == -1 ) {
						perror("smallsh: in main()");
						exit(1);
				} else if (spawn_pid == 0) {

					/* ^C Sig Action call */
					sigaction( SIGINT, &SIGINT_action2, NULL);

					int 	targetFd = -5,
							sourceFd = -5;
					
					if (source_flag) {
						sourceFd = open( source_file, O_RDONLY );
						if (sourceFd < 0) {
							fprintf(stderr, "cannot open %s for input\n", source_file); fflush(stderr);
							exit(1);
						}
						else {
							dup2( sourceFd, 0 );
						}
					}
					if (target_flag) {
						targetFd = open( target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
						if (targetFd < 0) {
							fprintf(stderr, "cannot open %s for output\n", args[ 0 ]); fflush(stderr);
							exit(1);
						}
						else {
							dup2( targetFd, 1 );
						}
					}

					/* If in foreground only mode, then remove the ampersand if it exists */
					if ( foreground_only && strcmp( args[ argi - 1 ], "&" ) == 0) {
						args[ argi - 1 ] = NULL;
					}
					if ( execvp( args[ 0 ], args ) < 0) {
						fprintf(stderr, "%s: no such file or directory\n", args[ 0 ]); fflush(stderr);
						exit(1);
					}
				}
				else {
					while( waitpid(spawn_pid, &check_exit_status, 0) < 0 );

					if ( check_exit_status != 0 )
						sprintf(exit_status, "exit value %d", 1);
					else
						sprintf(exit_status, "exit value %d", 0);
					if (WIFEXITED( check_exit_status) == 0) {
						sprintf(exit_status, "terminated by signal %d", WTERMSIG(check_exit_status));
						printf("%s\n", exit_status);
					}

				}
				
			}
		}

		/* Check for completed background jobs and clean up finished / terminated jobs.
		 * Up-date status with exit values and signal numbers */

		int i;
		int check_exit_status = 0;
		for (i = 0; i < pid_c; i++ ) {
			if( waitpid( pid_list[ i ], &check_exit_status, WNOHANG ) != 0 ) {
				if ( WIFEXITED( check_exit_status ) && WTERMSIG( check_exit_status ) == 0) {
					if ( WEXITSTATUS(check_exit_status) != 0 ) {
						sprintf(exit_status, "exit value %d", 1);
						//exit_status = 1;
						printf("background pid %d is done: %s\n", pid_list[ i ], exit_status); fflush(stdout);
					}
					else {
						sprintf(exit_status, "exit value %d", 0);
						// exit_status = 0;
						printf("background pid %d is done: %s\n", pid_list[ i ], exit_status); fflush(stdout);
					}
					// printf("background pid %d is done: exit value %d\n", pid_list[ i ], exit_status); fflush(stdout);
				} else {
					sprintf(exit_status, "terminated by signal %d", WTERMSIG(check_exit_status));
					printf("background pid %d is done: %s\n", pid_list[ i ], exit_status); fflush(stdout);
				}
				int j;
				for (j = i; j < pid_c - 1; j++)
					pid_list[ j ] = pid_list[ j + 1 ];
				pid_c--;
			}
		}

		/* Reset file controllers */

		source_file = NULL;
		target_file = NULL;
		source_flag = 0;
		target_flag = 0;
	}
	return 0;
}