//
//  DUMBclient.c
//  DUMBclient
//
//
//

#include "DUMBcommons.h"
#include <readline/history.h>
#include <readline/readline.h>
#define DUMB_MAX_SERV_VER_ALLOWED 1

struct commandinfo {
	bool isputmg;
	bool hasarg;
	char command[6];
	char argprompt[200];
	char errorresponse[200];
	char successresponse[200];
	bool mboxnamerr;
	char boxname[27];
	char incmd[20];
};

void error(char *msg)
{
	perror(msg);
	exit(1);
}

void printhelp()
{
	printf("quit\t\tquit the client.\n");
	// ...
	// TODO: put in rest of help commands
	printf("help\t\tprint this message.\n");
}
/**
 *
 *    		quit		(which causes: E.1 GDBYE)
            create		(which causes: E.2 CREAT)
            delete		(which causes: E.6 DELBX)
            open		(which causes: E.3 OPNBX)
            close		(which causes: E.7 CLSBX)
            next		(which causes: E.4 NXTMG)
            put			(which causes: E.5 PUTMG)
            On 'help' the client should list the commands above.
*/

// convert msg to command or return false
bool commandLibrary(char *msg, struct commandinfo *details)
{
	bool result = true;
	// initialize details:
	details->hasarg = false;
	details->mboxnamerr = false;
	details->isputmg = false;

	if (strcmp(msg, "quit") == 0) {

		strcpy(details->command, "GDBYE");

	} else if (strcmp(msg, "create") == 0) {

		strcpy(details->command, "CREAT");
		strcpy(details->argprompt, "Okay, what should the name be for the new message box?\n");
		// strcpy(details->errorresponse, "Message box '%s' does not exist.\n");
		strcpy(details->successresponse, "Message box '%s' has been created.\n");
		details->hasarg = true;

	} else if (strcmp(msg, "delete") == 0) {

		strcpy(details->command, "DELBX");
		details->hasarg = true;
		strcpy(details->argprompt, "Okay, which message box would you like to delete?\n");
		strcpy(details->successresponse, "Message box '%s' has been deleted.\n");

	} else if (strcmp(msg, "open") == 0) {

		strcpy(details->command, "OPNBX");
		details->hasarg = true;
		strcpy(details->argprompt, "Okay, which message box would you like to open?\n");
		strcpy(details->successresponse, "Message box '%s' is now open.\n");

	} else if (strcmp(msg, "close") == 0) {

		strcpy(details->command, "CLSBX");
		details->hasarg = true;
		strcpy(details->argprompt, "Okay, which message box would you like to close?\n");
		strcpy(details->successresponse, "Message box '%s' is now closed.\n");

	} else if (strcmp(msg, "next") == 0) {

		strcpy(details->command, "NXTMG");

	} else if (strcmp(msg, "put") == 0) {

		strcpy(details->command, "PUTMG");
		details->hasarg = true;
		details->isputmg = true;
		strcpy(details->argprompt, "Okay, please enter the message you wish to add to the box.\n");

	} else if (strcmp(msg, "help") == 0) {

		printhelp();

	} else {
		result = false;
	}

	return result;
}

/**
 * get variable length string, only needed for when using put
 * returns NULL if fail
 * returns ptr else
 **/

char *getvarleniput(char *buffer)
{
	buffer = (char *)realloc(buffer, 1 * sizeof(char));

	if (buffer != NULL) {
		int c = EOF;
		size_t i = 0;
		// accept user input until hit enter or end of file
		while ((c = getchar()) != '\n' && c != EOF) {
			buffer[i] = (char)c;
			buffer = realloc(buffer, ++i);
		}
		buffer[i] = '\0';

		return buffer;
	}
	return NULL;
}

int main(int argc, const char *argv[])
{
	int sockfd, portno, opt;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int connectionAttempts = 0;
	int connection = -1;
	char *buffer = calloc(MAX_BUFFER_SIZE, sizeof(char));

	if (argc != 3) {
		error("ERROR: DUMBclient requires a hostname and port, but either one or both were not "
		      "provided.\n");
	}

	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("ERROR: a problem occured when trying to open socket.");
	}

	server = gethostbyname(argv[1]);
	if (server == NULL) {
		error("ERROR: no such host.\n");
	}

	char hello[7] = "HELLO";

	while (connectionAttempts < 3 && connection != 0) {
		memset((char *)&serv_addr, '\0', sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;

		memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
		serv_addr.sin_port = htons(portno);

		connection = connect(sockfd, &serv_addr, sizeof(serv_addr));

		bool ready = false;

		if (connection == 0) {
			// send hello to server
			if (write(sockfd, hello, 7) == 0) {
				memset(buffer, '\0', MAX_BUFFER_SIZE);
				if (read(sockfd, buffer, MAX_BUFFER_SIZE) == 0) {
					unsigned version;
					sscanf(buffer, "HELLO DUMBv%d ready!", &version);
					if (version > 0 && version <= DUMB_MAX_SERV_VER_ALLOWED) {
						ready = true;
						break;
					}
				}
			}
		}

		if (!ready) {
			connectionAttempts++;
		}
	}

	if (connection != 0) {
		error("ERROR: a problem occured when trying to connect.");
	}

	// loop until "quit" is entered
	while (strcmp(buffer, "quit") != 0) {
		struct commandinfo details;
		memset(buffer, '\0', MAX_BUFFER_SIZE);
		while (true) {
			// prompt user for message
			printf("> ");
			// get user input

			scanf("%s", details.incmd);
			if (commandLibrary(details.incmd, &details) == true) {
				// success
				break;
			} else {
				printf("That is not a command, for a command list enter 'help'.\n");
			}
		}

		if (details.hasarg == true) {
			// tell the user they need to enter an argument
			printf(details.argprompt);
			while (true) {
				printf("%s> ", details.incmd);

				// free buffer because readline is going to reassign it to a new malloc()
				free(buffer);
				// get user input:
				buffer = readline(NULL);
				// printf("entered message: %s\n", buffer);

				if (!details.isputmg)
				// all arguments must be between 5 and 25 characaters except PUTMG
				{
					if (strlen(buffer) < 5 || strlen(buffer) > 25) {
						strcpy(details.errorresponse,
						       "Error. A valid mailbox must be between 5 and 25 characters.\n");
						details.mboxnamerr = true;
					} else {
						// success
						// store boxname for later
						strcpy(details.boxname, buffer);
						break;
					}
				} else {
					size_t msglen = strlen(buffer);
					size_t intreplen = ceil(log10(msglen)) + 1;
					char *tempmsg = (char *)calloc(msglen, sizeof(char));
					buffer = realloc(buffer, intreplen * sizeof(char));
					// we need the length of the character representation of arginputlen
					snprintf(tempmsg, intreplen, "%d", strlen(buffer));
					// COMND!NLEN!MSG
					buffer = (char *)realloc(buffer, (5 + 1 + intreplen + 1 + msglen) * sizeof(char));

					// PUTMG has no length constraints
					// construct message
					// detail.command!strlen(buffer)!buffer
					sprintf(buffer, "%s!%d!%s", details.command, msglen, tempmsg);
					free(tempmsg);
					break;
				}
			}

			// send command we just constructed

			long long sizeremains = strlen(buffer);
			size_t sofar = 0;
			if (sizeremains < MAX_BUFFER_SIZE) {
				write(sockfd, buffer, sizeremains);
			} else {
				do {
					write(sockfd, buffer + sofar, MAX_BUFFER_SIZE);
					sofar += MAX_BUFFER_SIZE;
					sizeremains -= MAX_BUFFER_SIZE;
				} while (sizeremains >= MAX_BUFFER_SIZE);
			}
			// read the response

			buffer = (char *)realloc(buffer, MAX_BUFFER_SIZE * sizeof(char));
			memset(buffer, '\0', MAX_BUFFER_SIZE);

			size_t bytesread = 0;
			unsigned timesread = 0;
			sofar = 0;

			do {
				bytesread = read(sockfd, buffer + sofar, MAX_BUFFER_SIZE);
				sofar += MAX_BUFFER_SIZE;
				buffer = realloc(buffer, (++timesread) * MAX_BUFFER_SIZE);
			} while (bytesread == MAX_BUFFER_SIZE);

			if (strlen(buffer) > 3) {

				// putmg
			} else if (strncmp(buffer, "OK!", 3) == 0) {

				printf("Success! ");
				printf(details.successresponse, details.boxname);
				continue;

			} else if (strcmp(buffer, "ER:NOOPN") == 0) {

				printf("Error. No mailbox is currently open, please try again.\n");

			} else if (strcmp(buffer, "ER:NEXST") == 0) {

				printf("Error. The specified mailbox does not exit.\n");

			} else if (strcmp(buffer, "ER:	OPEND") == 0) {

				printf("Error. Specified m	ailbox is open, please try again.\n");

			} else if (strcmp(buffer, "ER:	NOTMT") == 0) {

				printf(
				"Error. Specified mailbox isn't empty and can not be deleted, please try again.\n");

			} else if (details.mboxnamerr == true) {

				printf(details.errorresponse, details.boxname);
				continue;

			} else if (strcasecmp(buffer, "ER:WHAT?") == 0) {

				printf("That is not a command, for a command list enter 'help'.\n");
				continue;
			}
		}
		// reset buffer size
		buffer = realloc(buffer, MAX_BUFFER_SIZE);
	}

	free(buffer);

	return 0;
}
