#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>

#include "debug.h"
#include "version.h"

void PrintVersion( void );
void PrintHelp( void );
void ExitClean( int ExitVal );

static char * runtime_name = "";

int main(int argc,char *argv[])
{
	REPORT_ENTRY_DEBUG;

	// Define the options structure
	static struct option longopt[] =
	{
		{"daemon", no_argument, NULL, 'D'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

// 	bool DaemonMode = false;

#ifdef DEBUG_BUILD
	// Printout the command-line for debug purposes.
	#define CMDLINE_BUFF_MAX 128
	char cmdline[CMDLINE_BUFF_MAX];
	snprintf( cmdline, CMDLINE_BUFF_MAX, "Program command: %s", argv[0]);
	for( int i = 1; i < argc; i++ )
	{
		strncat( cmdline, " ", CMDLINE_BUFF_MAX-1-strlen(cmdline));
		strncat( cmdline, argv[i], CMDLINE_BUFF_MAX-1-strlen(cmdline));
	}
	DBGDEBUG("%s\n", cmdline);
#endif

	// Process command-line options
	runtime_name = argv[0]; // Save if needed later
	int c;
	int optidx=0;
	while ((c=getopt_long(argc,argv,"p:cvh",longopt,&optidx)) != -1)
	{
		switch(c)
		{
			case 'D':
				DBGDEBUG("Daemon mode enabled\n");
// 				DaemonMode = true;
				break;

			case 'v':
				PrintVersion();
				ExitClean(0);
			case 'h':
				PrintVersion();
				PrintHelp();
				ExitClean(0);
				break;
		}
	}

	printf("Hello world\n");
	ExitClean(0);
}

void PrintVersion( void )
{
	// Display the copyright message
	printf("%s version: %s\n", runtime_name, _LRD_VERSION_STRING);
}

void PrintHelp( void )
{
}

void ExitClean( int ExitVal )
{
	exit( ExitVal );
}
