/*
 * Name: Brian Leonard
 * ID #: 1000911183
 * Programming Assignment 4
 * Description: Mav File System, a filesystem demonstration.
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

// Defining global constants allow us to easily modify the structure of the filesystem later
#define NUM_BLOCKS 1250
#define BLOCK_SIZE 4096
#define NUM_FILES 128
#define NAME_LENGTH 255

// The actual file system data structure
unsigned char fileData[NUM_BLOCKS][BLOCK_SIZE];

// A parallel array that indicates whether a given block is available to use
int arrayStatus[NUM_BLOCKS];

// Defining the file information struct that will store our file directory info
typedef struct
{
	int valid;
	char name[NAME_LENGTH];
	int index[32];
	int size;
	struct tm *timeStamp;
} fileInfoStruct;

// Declare an array of these structs, one element per possible file
fileInfoStruct fileInfo[NUM_FILES];


/*
 * Function: readline
 * Parameter: none
 * Returns: An unprocessed char string
 * Description: Gets the user input from a stdin line
 *              and returns it for processing
 */

char *readline( void )
{
	char *input = NULL;
	ssize_t buffer = 0;

	// Getline handles the buffer for us
	getline(&input, &buffer, stdin);
	return input;
}


/*
 * Function: parse_command
 * Parameter: input - A pointer to a char string that is
 *              the unprocessed user input string
 * Returns: A parsed array of char strings
 * Description: Tokenizes the user input and splits it so
 *              we can pass specific parts to relevant functions
 */

char **parse_command( char *input )
{
	int i = 0;
	char **args = malloc( 255 );
	char *arg;

	arg = strtok( input, " \r\t\n" );

	// Tokenize the arguments one by one
	while ( arg != NULL )
	{
		args[i] = arg;
		i++;
		arg = strtok( NULL, " \r\t\n" );
	}

	// Terminate the array in NULL so the getFile function knows how many parameters are in use
	args[i] = NULL;
	return args;
}


/*
 * Function: displayFree
 * Parameter: display - An integer that dictates whether to display
 *                      the free space or just return it
 * Returns: An integer that equals the amount of free space in the file system
 * Description: Searches through the arrayStatus array and stores the free space
 *              in a variable, then outputs the free space if requested and
 *              returns the value
 */

int displayFree( int display )
{
	int i;
	int freeSpace = 0;

	// Search through the arrayStatus array and check to see if the block is in use
	for( i = 0; i < NUM_BLOCKS; i++ )
	{
		// If the block is in use, bump the free space up one block's worth
		if( arrayStatus[i] == 0 )
			freeSpace += BLOCK_SIZE;
	}

	// If the call requested a display, output the free space
	if( display == 1 )
		printf( "%d bytes free.\n", freeSpace );

	// Return the amount free whether a display was requested or not
	return freeSpace;
}

/*
 * Function: putFile
 * Parameter: filename - A char string that is the file name to load
 *                      into the filesystem
 * Returns: none
 * Description: Determines whether a file is valid to load, determines where to
 *              put it in the filesystem, and loads it
 */

void putFile( char **filename )
{
	struct stat buf;
	int status, copySize, offset, blockIndex, bytes, freeSpace;
	int i = 0;
	int fileNum = 0;
	int sentinel = 0;
	status = stat( filename, &buf );
	time_t rawTime;

	// Check free space but don't output the result
	freeSpace = displayFree( 0 );

	// If the file information loaded correctly, execute this block
	if( status != -1 )
	{

		// Check to see if the file system has enough space to store the requested file
		if( buf.st_size > freeSpace )
		{
			printf( "Error: Insufficient free space to store this file.\n" );
			return;
		}

		// Loop through the file directory array and see if this file is already loaded
		for( i = 0; i < NUM_FILES; i++ )
		{
			if( strcmp( fileInfo[i].name, filename ) == 0 )
			{
				printf( "file already exists, select another file\n" );
				return;
			}
		}

		// Find a directory entry to use for this file
		for( i = 0; i < NUM_FILES && sentinel == 0; i++ )
		{
			if( fileInfo[i].valid == 0 )
			{
				// This entry is free, set the directory information and
				// break the sentinel to stop looking for free entries
				sentinel = 1;
				fileNum = i;
				fileInfo[fileNum].valid = 1;
				strcpy( fileInfo[fileNum].name, filename );
				time ( &rawTime );
				fileInfo[fileNum].timeStamp = localtime ( &rawTime );
				fileInfo[fileNum].size = buf.st_size;
			}
		}

		// If it looped through the whole directory and couldn't find an available
		// entry to use
		if( sentinel == 0 )
		{
			printf( "insufficient file directory space\n" );
			return;
		}

		// Now we have the file directory entry set, this section actually copies the file
		FILE *ifp = fopen ( filename, "r" );
		copySize = buf.st_size;
		offset = 0;
		blockIndex = 0;

		// While there is still data left to copy
		while( copySize > 0 && blockIndex < NUM_BLOCKS )
		{
			// Seek the correct distance into the input file
			fseek( ifp, offset, SEEK_SET );

			// If the next file index is undefined
			if( fileInfo[fileNum].index[blockIndex] == -1 )
			{
				i = 0;
				// Search for the next free block in our file system
				while( fileInfo[fileNum].index[blockIndex] == -1 && i < NUM_BLOCKS )
				{
					if( arrayStatus[i] == 0 )
						fileInfo[fileNum].index[blockIndex] = i;
					i++;
				}
			}

			// Copy BLOCK_SIZE amount of data into the correct block
			bytes = fread( fileData[fileInfo[fileNum].index[blockIndex]], BLOCK_SIZE, 1, ifp );

			// Set the block status to "in use"
			arrayStatus[fileInfo[fileNum].index[blockIndex]] = 1;

			// Check for file read errors
			if( bytes == 0 && !feof( ifp ) )
			{
				printf( "An error occured reading from the input file.\n" );
				return -1;
			}

			// Then iterate our variables and do the same thing for the next block
			clearerr( ifp );
			copySize -= BLOCK_SIZE;
			offset += BLOCK_SIZE;
			blockIndex ++;
		}

		// This shouldn't be necessary, if displayFree returns the correct data, but
		// I left it in as a failsafe.  This is leftover code from a previous version
		if( copySize > 0 )
		{
			printf( "Error: insufficient filesystem space to store this file.\n" );
			delFile( filename );
		}
		// After we're done copying, close the input file handle
		fclose( ifp );
	}

	// If opening the file failed (status == -1)
	else
	{
		printf( "Unable to open file: %s\n", filename );
		perror( "Opening the input file returned: " );
		return -1;
	}
	return;
}


/*
 * Function: getFile
 * Parameters: Two char arrays, one for the file to retrieve from the file system
 *              and the other for an optional new name for the output file
 * Returns: none
 * Description: Copies a file from our file system back onto disk.  The output
 *              name is the same as the original file name by default, but is
 *              customizable using a second parameter
 */

void getFile( char **filename, char **newFilename )
{
	struct stat buf;
	int status, offset, numBytes;
	int blockIndex = 0;
	int valid = 0;
	int i = 0;
	int fileNum = 0;

	// Check to see if there is a new filename, if not then reuse the original name
	if( newFilename == NULL )
		newFilename = filename;

	// Find the directory entry for the file specified
	for( i = 0; i < NUM_FILES; i++ )
	{
		if( strcmp( fileInfo[i].name, filename ) == 0 )
		{
			fileNum = i;
			valid = 1;
		}
	}

	// If the file isn't in the directory, return an error
	if( valid == 0 )
	{
		printf( "get error: File not found\n" );
		return;
	}

	// Set our variables to prepare for coyping back to disk
	int copySize = fileInfo[fileNum].size;
	FILE *ofp;
	ofp = fopen( newFilename, "w" );
	if( ofp == NULL )
	{
		printf( "Could not open output file: %s\n", newFilename );
		return;
	}

	// After the output file opens successfully, tell the user how much we are copying
	printf( "Writing %d bytes to %s\n", fileInfo[fileNum].size, newFilename );

	// While data is left to copy, do this loop
	while( copySize > 0 )
	{
		// Check to see if we're copying a full block or just part of one
		if( copySize < BLOCK_SIZE )
			numBytes = copySize;
		else
			numBytes = BLOCK_SIZE;

		// Write the block to our output file
		fwrite( fileData[fileInfo[fileNum].index[blockIndex]], numBytes, 1, ofp );

		// Iterate our variables
		copySize -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
		blockIndex ++;

		// Update our file offset and repeat
		fseek( ofp, offset, SEEK_SET );
	}

	// After the file is done copying, close the file handle
	fclose( ofp );
	return;
}


/*
 * Function: delFile
 * Parameter: A char string that is the filename of the file to delete
 * Returns: none
 * Description: Removes a file from the filesystem.
 */

void delFile( char **filename )
{
	int fileNum = -1;
	int valid = 0;
	int i;

	// Find the file directory entry of the file to delete
	for( i = 0; i < NUM_FILES; i++ )
	{
		if( strcmp( fileInfo[i].name, filename ) == 0 && fileInfo[i].valid == 1 )
		{
			fileNum = i;
			valid = 1;
		}
	}

	// If the file does not have a directory entry, return an error
	if( valid == 0 )
	{
		printf( "del error: File not found\n" );
		return;
	}

	// Loop through the indices in the directory entry
	for( i = 0; fileInfo[fileNum].index[i] != -1 && i < 32; i++ )
	{

		// Set the block to "not in use"
		arrayStatus[fileInfo[fileNum].index[i]] = 0;

		// Reset the directory index to "none"
		fileInfo[fileNum].index[i] = -1;
	}

	// Blank the directory information
	fileInfo[fileNum].name[0] = '\0';
	fileInfo[fileNum].valid = 0;

	return;
}


/*
 * Function: listFiles
 * Parameter: none
 * Returns: none
 * Description: Lists the files currently in the file directory
 */

void listFiles( void )
{
	int i;
	char buffer [20];

	// Loop through each directory entry
	for( i = 0; i < NUM_FILES; i++ )
	{

		// If this entry is in use, display the file information
		if( fileInfo[i].valid == 1 )
		{
			strftime( buffer, 20, "%h %d %H:%M", fileInfo[i].timeStamp );
			printf( "%d\t%s %s\n", fileInfo[i].size, buffer, fileInfo[i].name );
		}
	}
	return;
}


/*
 * Function: main
 * Parameter: none
 * Returns: An int; 0 for success, 1 if there was an error
 * Description: The main program loop
 */

int main( void )
{
	int quit = 0;
	int i, j;
	char *rawInput;
	char **parsedInput;

	// Initialize our block status array to unused (0)
	for ( i = 0; i < NUM_BLOCKS; i++ )
	{
		arrayStatus[i] = 0;
	}

	// Initialize our file directory to null/unused
	for ( i = 0; i < NUM_FILES; i++ )
	{
		fileInfo[i].valid = 0;
		fileInfo[i].name[0] = '\0';
		for ( j = 0; j < 32; j++ )
		{
			fileInfo[i].index[j] = -1;
		}
	}

	// Main program loop
	while( quit == 0 )
	{
		// Print a prompt
		printf( "mfs> ");

		// Get input from user
		rawInput = readline();

		// Parse the input into a useable array
		parsedInput = parse_command( rawInput );

		// Find out what to do with the input array
		if ( parsedInput[0] == NULL )
			printf( "");

		else if ( strcmp( parsedInput[0], "put" ) == 0 )
			putFile( parsedInput[1] );

		else if ( strcmp( parsedInput[0], "get" ) == 0 )
			getFile( parsedInput[1], parsedInput[2] );

		else if ( strcmp( parsedInput[0], "del" ) == 0 )
			delFile( parsedInput[1] );

		else if ( strcmp( parsedInput[0], "list" ) == 0 )
			listFiles();

		else if ( strcmp( parsedInput[0], "df" ) == 0 )
			displayFree( 1 );

		else if ( strcmp( parsedInput[0], "quit" ) == 0 )
			quit = 1;
		else
			printf( "Unrecognized input, please try again\n" );
	}

	return 0 ;
}