/*
 * Name: Brian Leonard
 * ID #: 1000911183
 * Programming Assignment 1
 * Description: Mav Shell, a user shell.  Developed in C
 */


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>


/*
 * Function: readline
 * Parameter: none
 * Returns: An unprocessed char string.
 * Description: Gets the user input from a stdin line
 * and returns it for processing.
 */

char *readline( void )
{
  char *input = NULL;
  ssize_t buffer = 0;
  getline(&input, &buffer, stdin); // Getline handles the buffer for us
  return input;
}

/*
 * Function: parse_command
 * Parameter: input - A pointer to a char string
 * that is the unprocessed user input string.
 * Returns: A parsed array of char strings
 * Description: Tokenizes the user input and splits
 * it so that execvp can recognize distinct flags
 */

char **parse_command( char *input )
{
  int i = 0;
  char **args = malloc( 255 );
  char *arg;

  arg = strtok( input, " \r\t\n" );
  while ( arg != NULL ) // Tokenize the arguments one by one
  {
    args[i] = arg;
    i++;
    arg = strtok( NULL, " \r\t\n" );
  }
  args[i] = NULL; // Terminate our array in NULL so execvp knows when to stop
  return args;
}

/*
 * Function: exec_command
 * Parameter: args - An array of arguments to pass
 * to execvp.
 * Returns: An integer value indicating whether
 * to continue processing commands or to quit the shell.
 * Description: Calls execvp and executes commands
 * in a child process as input by the user.
 * Special cases for changing directory and quitting
 * the shell.
 */

int exec_command( char **args )
{
  pid_t pid;
  int quit = 0;
  int status;

  // Catch an empty input line first
  if ( args[0] == NULL )
    return quit;

  // Special case for changing directory without forking a child process
  else if ( strcmp( args[0], "cd" ) == 0 )
  {
    chdir( args[1] );
  }

  // Special case for quitting the shell
  else if ( strcmp( args[0], "quit") == 0 || strcmp ( args [0], "exit" ) == 0 )
  {
    quit = 1;
  }
  else
  {
    // fork a child process to execute commands while the shell is still running
    pid = fork();
    if ( pid == 0 )
    {
      // While in the child process, execute the parsed command
      if ( execvp( args[0], args ) == -1 )
      {
        // Catch command not found errors and print to screen
        printf( "%s: command not found\n", args[0]);
      }
      // If we're still here after execution, exit the child process
      exit( EXIT_FAILURE );
    }
    else // Wait here for the child process to exit
     pid = wait( pid, &status );
  }
  // After running, return an int to determine whether to quit the shell
  return quit;
}

int main( void )
{
  int quit = 0;
  char *cmd;
  char **a
  signal(SIGINT, SIG_IGN ); // Catch ctrl+c and ignore
  signal(SIGTSTP, SIG_IGN ); // Catch ctrl+z and ignore

  while( quit == 0 ) // Main program loop
  {
    printf( "msh> "); // Print prompt
    cmd = readline(); // Get user input
    args = parse_command( cmd ); // Parse input
    quit = exec_command( args ); // Execute commands
  }

  return 0 ;
}
