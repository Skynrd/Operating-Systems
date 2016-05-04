/*
 * Name: Brian Leonard
 * ID #: 1000911183
 * Programming Assignment 2
 * Description: Shakespeare Word Search Service - this program searches the collected
 * 		works of Shakespeare for instances of a word, using a variable number of
 * 		worker processes, then compiles the results and prints how many instances
 * of that word were found.
 */


#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<sys/stat.h>

/*
 * Function: readline
 * Parameter(s): None
 * Returns: A char string representing the raw user input
 * Description: Gets the user input and stores it in a char string for other functions to process
 */

char *readline( void )
{
	char *input = NULL;
	ssize_t buffer = 0;

	// Getline handles the buffer for us
	getline( &input, &buffer, stdin);
	return input;
}

/*
 * Function: parseInput
 * Parameter(s): A char string representing the raw user input
 * Returns: An array of char strings representing the user command and any parameters they used
 * Description: Parses and tokenizes the user input string into an array that can be used later
 */

char **parseInput( char *input )
{
	int i = 0;
	char **args = malloc( 255 );
	char *arg;

	arg = strtok( input, " \r\t\n" );

	// Loop to tokenize the input and store in an array
	while ( arg != NULL )
	{
		args[i] = arg;
		i++;
		arg = strtok( NULL, " \r\t\n" );
	}
	args[i] = NULL;

	// Return the parsed array
	return args;
}

/*
 * Function: help
 * Parameter(s): None
 * Returns: None
 * Description: Help function, displays the commands the user can input
 */

void help()
{
	printf( "Shakespeare Word Search Service Command Help\n" );
	printf( "------------------------------------------\n" );
	printf( "help   - displays this message\n" );
	printf( "quit   - exits\n" );
	printf( "search [word] [workers] - searches the works of Shakespeare for [word] using\n" );
	printf( "                          [workers].  [workers] can be from 1 to 100\n>" );
	return;
}

/*
 * Function: split_and_srch
 * Parameter(s): Two char strings indicating the text to search for and the number of workers
 * Returns: None
 * Description: This function is the main worker process.  It takes the user input string and
 *      number of workers then opens the file to search.  A timer is then started.
 *      After mapping the file to memory, it spawns off the appropriate number of worker
 *      processes, each of which search a chunk of the file and report how many instances it
 *      found to the parent process via a pipe.  The parent process waits for all workers to
 *      report, then summarizes the results, stops the timer, and reports to the user both
 *      how many instances of the word were found, and also how long the search took.
 */

void split_and_srch( char* search_term, char* workers_string )
{
	// Convert the number of workers to an int
	int num_workers = atoi( workers_string );
	int time_elapsed, fd, offset_start, offset_finish, chunk, worker_num;
	int i = 0;
	int hits = 0;
	int status = 0;
	int result = 0;
	int result_sum = 0;

	// Determine the length of the search term
	size_t search_length = strlen( search_term );
	pid_t cpid, wpid;

	// Set up a struct to store information about the file
	struct stat sbuf;

	// And a struct for getting time information
	struct timeval start, end;
	char *buf, *data;

	// Attempt to open the file, and report if it errors out
	if (( fd = open( "shakespeare.txt", O_RDONLY, 0 )) < 0 )
	{
		perror( "open" );
		exit(1);
	}

	// Attempt to pull file information and report errors
	if ( stat( "shakespeare.txt", &sbuf ) < 0 )
	{
		perror( "stat" );
		exit(1);
	}

	// Start the search timer
	gettimeofday( &start, NULL );

	// Validate the number of workers requested
	if ( num_workers < 1 || num_workers > 100 )
	{
		printf( "Please enter a number of workers from 1 to 100\n");
		return;
	}

	// Set up the mmap for the worker processes to search
	data = mmap( (caddr_t)0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0 );

	// Determine the length of each worker's search space
	chunk = sbuf.st_size / num_workers;

	// Create our pipe array for communication between processes
	int comms[num_workers][2];
	do
	{
		// Determine offsets for this worker to search
		offset_start = i * chunk;
		offset_finish = ( i + 1 ) * chunk;

		// Set up the pipe and report errors
		if ( pipe( comms[i] ) == -1 )
		{
			perror( "pipe" );
			exit(1);
		}

		// Fork a child process with the current variable values and report errors
		cpid = fork();
		if ( cpid == -1 )
		{
			perror( "fork" );
			exit(1);
		}

		// Close the write end for this pipe in the parent process, then increment i
		if ( cpid != 0 )
		{
			close( comms[i][1] );
			i++;
		}
	}
	while ( i < num_workers && cpid != 0 );
	// Keep looping through creating worker processes until we do it num_workers times

	// Child process searches here
	if ( cpid == 0 )
	{
		// Copy our worker number into another variable so we can reuse the iterator
		worker_num = i;

		// Close the read end of this pipe
		close( comms[worker_num][0] );

		// Iterate through the address space as declared when this process was forked
		for ( i = offset_start; i < offset_finish; i++ )
		{
			// Compare the memory at this spot to the search term
			if ( memcmp( &data[i], search_term, search_length ) == 0 )
				hits++;
		}

		// After searching this worker's space, send the number of hits to the pipe
		write( comms[worker_num][1], &hits, sizeof(int));

		// Close the pipe so the parent process sees an EOF and knows to continue
		close( comms[worker_num][1] );
		exit( EXIT_SUCCESS );
	}

	// Wait for all worker processes to complete
	while ( (wpid = wait( &status )) > 0 );

	// After all workers complete, loop through them
	for ( i = 0; i < num_workers; i++ )
	{
		// Collect the results for each worker and sum to the result_sum variable
		while ( read ( comms[i][0], &result, sizeof(int) ) > 0 )
		{
			result_sum = result_sum + result;
		}
	}

	// End the timer
	gettimeofday( &end, NULL );

	// I'm not sure why this was required but the program didn't like me when I left it open
	close( comms[i][0] );

	// Calculate the time the search took
	time_elapsed = ((end.tv_sec*1000000+end.tv_usec) - (start.tv_sec*1000000+start.tv_usec));

	//
	printf("Found %d instances of %s in %d microseconds\n>", result_sum, search_term, time_elapsed);
	return;
}

/*
 * Function: main
 * Parameter(s): None
 * Returns: Exit value
 * Description: Main function that starts the program and calls functions as appropriate
 */

int main( void )
{
	int quit = 0;
	char *rawinput;
	char **parsedinput;

	// Prompt the user for input
	printf("Welcome to the Shakespeare word count service.\n");
	printf("Enter: search [word] [workers] to start your search.\n>");

	// Set a sentinel to determine whether to quit or not
	while ( quit == 0 )
	{
		// Get input from the user
		rawinput = readline();

		// Parse the input into a useable array
		parsedinput = parseInput( rawinput );

		// Test the input string for specific values like help and quit
		if ( parsedinput[0] == NULL )
			printf(">");
		else if ( strcmp( parsedinput[0], "help" ) == 0 )
			help();
		else if ( strcmp( parsedinput[0], "quit" ) == 0 )
			quit = 1;

		// If none are found, send the input to split_and_srch
		else if ( strcmp( parsedinput[0], "search" ) == 0 )
			split_and_srch( parsedinput[1], parsedinput[2] );

		// Catch any erroneous input and give another prompt
		else
			printf(">");
	}
	return 0;
}
