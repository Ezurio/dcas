#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <libssh/server.h>
#include "debug.h"
#include "sdc_sdk.h"
#include "dcal_api.h"
#include "version.h"
#include "ssh_server.h"

void PrintVersion( void );
void PrintHelp( void );
void ExitClean( int ExitVal );

static char * runtime_name = "";
#define DEFAULT_KEYS_FOLDER "/etc/ssh"
#define DEFAULT_AUTH_FOLDER "~/.ssh"

static struct SSH_DATA ssh_data = {
	.alive = true,
	.reboot_on_exit = false,
	.port = 2222,
	.verbosity = 0,
	.ssh_disconnect = false,
	.method = METHOD_PUBKEY,
	.host_keys_folder = {0},
	.auth_keys_folder = {0},
	.username = {0},
	.password = {0},
};

void sigproc(int unused)
{
	DBGINFO("signal caught. Marking loop and threads for exit\n");
	ssh_data.alive = false;
	sem_post(&ssh_data.thread_list);
}

int main(int argc,char *argv[])
{
	PrintVersion();
	REPORT_ENTRY_DEBUG;

	int rc = 0;

	signal(SIGINT, sigproc);
	signal(SIGQUIT, sigproc);
	signal(SIGTERM, sigproc);

	// Define the options structure
	static struct option longopt[] = {
		{"host_keys", required_argument, NULL, 'k'},
		{"auth_keys", required_argument, NULL, 'a'},
		{"user", required_argument, NULL, 'u'},
		{"password", required_argument, NULL, 'P'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"port", required_argument, NULL, 'p'},
		{"ssh_disconnect", no_argument, NULL, 's'},
		{NULL, 0, NULL, 0}
	};

#ifdef DEBUG_BUILD
	// Printout the command-line for debug purposes.
#define CMDLINE_BUFF_MAX 128
	char cmdline[CMDLINE_BUFF_MAX];
	snprintf( cmdline, CMDLINE_BUFF_MAX, "Program command: %s", argv[0]);
	for( int i = 1; i < argc; i++ ) {
		strncat( cmdline, " ", CMDLINE_BUFF_MAX-1-strlen(cmdline));
		strncat( cmdline, argv[i], CMDLINE_BUFF_MAX-1-strlen(cmdline));
	}
	DBGDEBUG("%s\n", cmdline);
#endif

	strncpy(ssh_data.host_keys_folder, DEFAULT_KEYS_FOLDER, MAX_PATH-2);
	strncpy(ssh_data.auth_keys_folder, DEFAULT_AUTH_FOLDER, MAX_PATH-2);

	// Process command-line options
	runtime_name = argv[0]; // Save if needed later
	int c;
	int optidx=0;
	while ((c=getopt_long(argc,argv,"k:a:u:P:hvp:s",longopt,&optidx)) != -1) {
		switch(c) {
		case 'k':
			DBGDEBUG( "Setting host key directory:%s\n", optarg );
			strncpy(ssh_data.host_keys_folder, optarg, MAX_PATH-2);
			break;
		case 'a':
			DBGDEBUG( "Setting auth key directory:%s\n", optarg );
			strncpy(ssh_data.auth_keys_folder, optarg, MAX_PATH-2);
			break;
		case 'v':
			PrintVersion();
			ExitClean(0);
		case 'h':
			PrintVersion();
			PrintHelp();
			ExitClean(0);
			break;
		case 'p':
			ssh_data.port = atoi(optarg);
			DBGDEBUG("Setting port to %d\n", ssh_data.port);
			break;
		case 's':
			ssh_data.ssh_disconnect=true;
			DBGDEBUG("ssh_disconnection option enabled\n");
			break;
		case 'u':
			strncpy(ssh_data.username, optarg, MAX_USER-2);
			break;
		case 'P':
			strncpy(ssh_data.password, optarg, MAX_PASSWORD-2);
			ssh_data.method |= METHOD_PASSWORD;
			break;

		}
	}

	// if either username or password is set, both are required.
	if ((ssh_data.username[0] && !ssh_data.password[0])
		|| (!ssh_data.username[0] && ssh_data.password[0])) {
		printf("\n\n\t\tif password or user name is set, both are required\n");
		ExitClean(1);
	}

	sem_init(&ssh_data.thread_list, 0, MAX_THREADS);

	rc = run_sshserver( &ssh_data );
	DBGDEBUG("Got %d return from run_sshserver()\nDCAS Exiting\n", rc);

	sem_destroy(&ssh_data.thread_list);
	ExitClean( rc );
}

void PrintVersion( void )
{
	// Display the copyright message
	printf("%s version: %s-%s\n", runtime_name, SUMMIT_BUILD_NUMBER, DCAS_VERSION_STRING);
	printf("Device Control API Library version: %s\n", DCAL_VERSION_STR);
}

void PrintHelp( void )
{
	printf("\nDevice Control API Service\n\n"
	       "\tThis application is used to provide services for a\n"
	       "\tDevice Control API Layer (dcal) application on a remote\n"
	       "\thost to be able to configure the WB system.\n\n"
	       "\tUsage:\n"
	       "\t%s [-Dkvhp]\n\n"
	       "\t\t-k <path>\thost key directory path\n"
	       "\t\t-a <path>\tauthorized_keys directory path\n"
	       "\t\t-u <username>\tuser name (enables password authentication)\n"
	       "\t\t             \t -requires password\n"
	       "\t\t-P <Password>\tPassword (enables password authentication)\n"
	       "\t\t             \t -requires username\n"
	       "\t\t-p <port>\ttcp port to listen for connections\n"
	       "\t\t-s \t\tssh_disable - disables WiFi on last authorized ssh \n"
	       "\t\t   \t\t              session disconnect\n"
	       "\t\t-v\t\tversion\n"
	       "\t\t-h\t\thelp\n"
	       "\n\n", runtime_name);
}

void ExitClean( int ExitVal )
{
	exit( ExitVal );
}
