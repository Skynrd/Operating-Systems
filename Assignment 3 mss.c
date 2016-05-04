/*
 * Name: Brian Leonard
 * ID #: 1000911183
 * Programming Assignment 3
 * Description: Shakespeare Word Search Service - this program searches the collected
 *      works of Shakespeare for instances of a word, using a variable number of
 *      worker threads, then compiles the results in a global variable (locked
 *      against race conditions) and prints how many instances of that word were found.
 *      It can also replace a string with another string and reset the file to the
 *      original configuration
 */


#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<pthread.h>

// Global variables!  I know there's a way to pass file descriptors and structures
// between functions but this works fine for our purposes
int fd;
struct stat sbuf;
char * data;
int result;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct thread
{
	pthread_t thread_id;
	char * search;
	char * replace;
	int length;
	int start;
	int finish;
};


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
	printf( "                          [workers].  [workers] can be from 1 to 100\n" );
	printf( "replace [word 1] [word 2] [workers] - search the works of Shakespeare for\n" );
	printf( "                          [word 1] using [workers] and replaces each\n" );
	printf( "                          instance with [word 2].  [workers] can be from\n" );
	printf( "                          1 to 100.\n" );
	printf( "reset  - will reset the memory mapped file back to its original state.\n>" );
	return;
}


/* Function: reset
 * Parameter(s): none
 * Returns: None
 * Description: Resets the mapped file to its original configuration
 */

void reset()
{
	int fd2;
	struct stat sbuf2;
	char * data2;

	// Open the file and the backup
	if (( fd = open( "shakespeare.txt", O_RDWR, 0 )) < 0 )
	{
		perror( "open" );
		exit(1);
	}
	if (( fd2 = open( "shakespeare_backup.txt", O_RDONLY, 0 )) < 0 )
	{
		perror( "open backup" );
		exit(1);
	}
	if ( stat( "shakespeare.txt", &sbuf ) < 0 )
	{
		perror( "stat" );
		exit(1);
	}
	if ( stat( "shakespeare_backup.txt", &sbuf2 ) < 0 )
	{
		perror( "stat backup" );
		exit(1);
	}

	// Map both text files to memory
	data = mmap( (caddr_t)0, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );
	data2 = mmap( (caddr_t)0, sbuf2.st_size, PROT_READ, MAP_SHARED, fd2, 0 );

	// Copy the backup over the modified file
	memcpy( data, data2, sbuf2.st_size );

	// Close the files to clean things up
	close ( fd );
	close ( fd2 );
	printf(">");
	return;
}


/* Function: search_and_replace
 * Parameter(s): A struct with our thread-specific information.
 * Returns: None
 * Description: This function is called once for each thread and performs the main work
 *      of searching and/or replacing text in the file.  The counter is protected by a
 *      mutex to prevent race conditions.
 */

void search_and_replace( struct thread *worker )
{
	// Local counter variable to track before we update the global var at the end
	int hit = 0;
	int i;

	// Check to see if we're just searching
	if ( worker->replace == NULL )
	{
		// Loop through this thread's memory space
		for ( i = worker->start; i < worker->finish; i++ )
		{
			// Check for our search term
			if ( memcmp( &data[i], worker->search, worker->length ) == 0 )
				hit++;
		}

		// Lock the global results var before we update
		pthread_mutex_lock ( &mutex );
		result = result + hit;
		// And unlock after we're done
		pthread_mutex_unlock ( &mutex );
	}
	// If there is a replace term, we're replacing instead of searching
	else
	{
		// Add spaces to the replacement so we have equal lengths
		for ( ; strlen( worker->replace ) < worker->length; )
		{
			strcat( worker->replace, " ");
		}
		// Then iterate through the memory space for this thread
		for ( i = worker->start; i < worker->finish; i++ )
		{
			// Check for hits, and if we have one then replace it
			if ( memcmp( &data[i], worker->search, worker->length ) == 0 )
				memcpy( &data[i], worker->replace, worker->length );
		}
	}
	return;
}


/*
 * Function: split
 * Parameter(s): Three char strings indicating the text to search for or replace and the
 *      number of workers
 * Returns: None
 * Description: This function is the main worker process.  It takes the user input string and
 *      number of workers then opens the file to search.  A timer is then started.
 *      After mapping the file to memory, it spawns off the appropriate number of worker
 *      processes, each of which search a chunk of the file and report how many instances it
 *      found to the parent process via a pipe.  The parent process waits for all workers to
 *      report, then summarizes the results, stops the timer, and reports to the user both
 *      how many instances of the word were found, and also how long the search took.
 */

void split( char* search_term, char* replace_term, char* workers_string )
{
	// Convert the number of workers to an int
	int num_workers = atoi( workers_string );
	int time_elapsed, chunk, worker_num, i;
	result = 0;
	int j;

	// Declare an array of thread structs to pass the information to our threads
	struct thread worker[num_workers];

	// Create a struct for getting time information
	struct timeval start, end;
	char *buf;

	// Open the file to search
	if (( fd = open( "shakespeare.txt", O_RDWR, 0 )) < 0 )
	{
		perror( "open" );
		exit(1);
	}
	if ( stat( "shakespeare.txt", &sbuf ) < 0 )
	{
		perror( "stat" );
		exit(1);
	}

	// And map it to memory
	data = mmap( (caddr_t)0, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );

	// Start the search timer
	gettimeofday( &start, NULL );

	// Validate the number of workers requested
	if ( num_workers < 1 || num_workers > 100 )
	{
		printf( "Please enter a number of workers from 1 to 100\n");
		return;
	}

	// Determine the length of each worker's search space
	chunk = sbuf.st_size / num_workers;

	// Split of the appropriate number of threads
	for ( i = 0; i < num_workers; i++ )
	{
		// Determine offsets for this worker to search
		worker[i].start = i * chunk;
		worker[i].finish = ( i + 1 ) * chunk;

		// Store our search term, replace term, and length
		worker[i].search = search_term;
		worker[i].length = strlen( search_term );
		worker[i].replace = replace_term;

		// Spawn a separate thread and have it perform the search_and_replace function
		pthread_create ( &worker[i].thread_id, NULL, search_and_replace, &worker[i] );
	};

	// Wait for all threads to finish
	for ( j = 0; j < num_workers; j++ )
	{
		pthread_join ( worker[j].thread_id, NULL );
	}

	// End the timer
	gettimeofday( &end, NULL );

	// Calculate the time the search took
	time_elapsed = ((end.tv_sec*1000000+end.tv_usec) - (start.tv_sec*1000000+start.tv_usec));

	// Print output if appropriate
	if ( replace_term == NULL )
		printf("Found %d instances of %s in %d microseconds\n>", result, search_term, time_elapsed);
	else
		printf(">");

	// Close the file to clean up
	close( fd );
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
		else if ( strcmp( parsedinput[0], "reset" ) == 0 )
			reset();
		// Check for searching, send the input to split_and_srch
		else if ( strcmp( parsedinput[0], "search" ) == 0 )
			split( parsedinput[1], NULL, parsedinput[2] );

		// Check for replacing, and send to split_and_replace
		else if ( strcmp( parsedinput[0], "replace" ) == 0 )
			split( parsedinput[1], parsedinput[2], parsedinput[3] );

		// Catch any erroneous input and give another prompt
		else
			printf(">");
	}
	return 0;
}



