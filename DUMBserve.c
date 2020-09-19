//
//  DUMBserve.c
//  DUMBserve
//

#include "DUMBcommons.h"
#define DUMB_SERV_VER_V1 1
// unimplemented feature:
#undef SUPPORTS_MULTI_BOX

typedef struct pthread_arg_t {
	int sockfd;
	struct sockaddr_in cliaddr;
	pthread_t id;
} pthread_arg_t;

typedef struct _message {
	char *text;
	struct _message *next;
	struct _message *prev;
} message;

typedef struct _messageList {
	message *head;
	message *tail;

} messageList;

typedef struct _mailBox {
	char *name;
	messageList *messages;
	struct _mailBox *next;
	struct _mailBox *prev;
	pthread_mutex_t mutex;
} mailBox;

typedef struct _mailBoxes {
	mailBox *head; // beginning of mailbox list
	mailBox *tail; // end of mailbox list

} mailBoxes;

struct optargs {
	bool boxexists;
	bool validname;
	bool messagelistempty;
	mailBox *box;
};

mailBoxes mailBoxList;

// deleteBox <-- delete from LL and free()
void deleteBox(mailBox *mailBoxToBeDeleted)
{
	if (strcmp(mailBoxList.head->name, mailBoxToBeDeleted->name) == 0) {
		mailBox *temp = mailBoxList.head;
		mailBoxList.head = mailBoxList.head->next;
		mailBoxList.head->prev = NULL;
		free(temp);
	} else {
		mailBox *current = mailBoxList.head->next;
		mailBox *prev = mailBoxList.head;
		while (current != NULL) {
			if (strcmp(current->name, mailBoxToBeDeleted->name) == 0) {
				prev->next = mailBoxToBeDeleted->next;
				prev->next->prev = prev;
				free(mailBoxToBeDeleted->name);
				free(mailBoxToBeDeleted->messages);
				pthread_mutex_unlock(&mailBoxToBeDeleted->mutex);
				pthread_mutex_destroy(&mailBoxToBeDeleted->mutex);
				free(mailBoxToBeDeleted);
				break;
			}
			prev = current;
			current = current->next;
		}
	}
}

void addBox(char *boxName)
{
	mailBox *newBox = (mailBox *)malloc(sizeof(mailBox));
	newBox->next = NULL;
	newBox->prev = mailBoxList.tail;
	newBox->name = (char *)calloc(strlen(boxName) + 1, sizeof(char));
	strncpy(newBox->name, boxName, strlen(boxName));
	newBox->name[strlen(boxName)] = '\0';
	newBox->messages = (messageList *)calloc(1, sizeof(messageList));
	pthread_mutex_init(&newBox->mutex, NULL);

	// empty list
	if (mailBoxList.head == NULL) {
		mailBoxList.head = newBox;
		mailBoxList.tail = mailBoxList.head;
		return;
	} else {
		// at least one item in list
		mailBoxList.tail->next = newBox;
		mailBoxList.tail = newBox;
	}
}

mailBox *searchForBox(char *boxName)
{
	// iterative LL search
	// check if tail is desired box at end
	mailBox *current = mailBoxList.head;
	if (current == NULL) {
		return NULL;
	} else {
		while (current != NULL) {
			if (strcmp(boxName, current->name) == 0) {
				return current;
			} else {
				current = current->next;
			}
		}
	}

	// print errnotfound

	return NULL;
}

void addMessage(char *msgtext, mailBox *box)
{
	message *newmsg = (message *)malloc(sizeof(message));
	newmsg->text = (char *)calloc(strlen(msgtext) + 1, sizeof(char));
	strncpy(newmsg->text, msgtext, strlen(msgtext));
	newmsg->text[strlen(msgtext)] = '\0';

	newmsg->next = NULL;
	newmsg->prev = box->messages->tail;

	// empty list
	if (box->messages->head == NULL) {
		box->messages->head = newmsg;
		box->messages->tail = box->messages->head;
		return;
	} else {
		// at least one item in list
		box->messages->tail->next = newmsg;
		box->messages->tail = newmsg;
	}
}

void deleteMessage(message *msg, mailBox *box)
{
	if (msg == box->messages->head) {
		// at head
		box->messages->head = NULL;
		box->messages->tail = NULL;

	} else if (msg == box->messages->tail) {
		// at tail
		box->messages->tail = box->messages->tail->prev;
		box->messages->tail->prev->next = NULL;
	} else {
		// somewhere in middle
		message *current = box->messages->head;

		while (current != msg) {
			current = current->next;
		}

		current->prev->next = current->next;
		current->next->prev = current->prev;
	}

	free(msg->text);
	free(msg);
}

bool validateBoxName(char *name)
{

	if (*(name - 1) != '!' || isalpha(name[0]) == false || strlen(name) < 5 || strlen(name) > 25) {
		// print error invalid name

		return false;
	}

	return true;
}

void updateboxargs(struct optargs *curboxargs, char *mailBoxName)
{
	curboxargs->validname = false;
	curboxargs->boxexists = false;
	curboxargs->messagelistempty = NULL;
	curboxargs->box = NULL;

	if (validateBoxName(mailBoxName)) {
		curboxargs->validname = true;
		if ((curboxargs->box = searchForBox(mailBoxName)) != NULL) {
			curboxargs->boxexists = true;
			if (curboxargs->box->messages->head == NULL) {
				curboxargs->messagelistempty = true;
			} else {
				curboxargs->messagelistempty = false;
			}
		}
	}
}

void error(char *msg)
{
	perror(msg);
	exit(1);
}

/**
 * singleclientthread contains all the control & program logic for a single connection
 */
void clientconnectionhandler(void *args)
{
	pthread_arg_t *threadargs = (pthread_arg_t *)args;
	int sockfd = threadargs->sockfd;

	// char array to store data going to and coming from the socket
	// The server reads characters from the socket connection into this buffer.
	char *buffer = (char *)calloc(MAX_BUFFER_SIZE, sizeof(char));

	// send hello command to client
	sprintf(buffer, "HELLO DUMBv%d ready!\n", DUMB_SERV_VER_V1);

	if (write(sockfd, buffer, MAX_BUFFER_SIZE) < 0) {
		error("ERROR: a problem occured when trying to write to socket");
	}

	// TODO: add time & ip
	printf("time date ip connected\n");

	mailBox *currentMailBox = NULL;
	message *currentMessage = NULL;
	char cmd[6];
	char err[9];

	while (true) {

		memset(buffer, '\0', MAX_BUFFER_SIZE);

		size_t bytesread = 0;
		unsigned timesread = 0;
		size_t sofar = 0;

		do {
			bytesread = read(sockfd, buffer + sofar, MAX_BUFFER_SIZE);
			sofar += MAX_BUFFER_SIZE;
			buffer = realloc(buffer, (++timesread) * MAX_BUFFER_SIZE);
		} while (bytesread == MAX_BUFFER_SIZE);

		strncpy(cmd, buffer, 5);
		cmd[5] = '\0';
		// TODO: add time & ip
		printf("time date ip %s\n", cmd);

		if (strncmp(cmd, "GDBYE", 5) == 0) {
			if (currentMailBox != NULL) {
				pthread_mutex_unlock(&currentMailBox->mutex);
			}
			break;
		}

		char *inputMailBoxName = &buffer[6];
		struct optargs tempboxargs;

		updateboxargs(&tempboxargs, inputMailBoxName);

		// validate input

		if (strcmp(buffer, "NXTMG") == 0) {
			// logic for getting the next message in the mailbox

			if (currentMailBox == NULL) {
				strcpy(buffer, "ER:NOOPN");
			} else if (currentMessage == currentMailBox->messages->tail) {
				strcpy(buffer, "ER:EMPTY");
			} else {
				size_t msgsize = strlen(currentMessage->text);
				size_t nrps = ceil(log10(msgsize)) + 1;
				buffer = (char *)realloc(buffer, nrps * sizeof(char));
				// we need the length of the character representation of arginputlen
				snprintf(buffer, nrps, "%lu", strlen(buffer));
				// COMND!NLEN!MSG
				size_t totalmsgsize = 5 + 1 + nrps + 1 + msgsize;
				buffer = (char *)realloc(buffer, totalmsgsize * sizeof(char));

				currentMessage = currentMessage->next;
				sprintf(buffer, "OK!%zu", msgsize);
			}

		} else if (strncmp(buffer, "CREAT", 5) == 0) {
			// logic for creating a new mailbox

			if (tempboxargs.boxexists) {
				strcpy(buffer, "ER:EXIST");
			} else if (!tempboxargs.validname) {
				strcpy(buffer, "ER:WHAT?");
			} else {
				addBox(inputMailBoxName);
				strcpy(buffer, "OK!");
			}

		} else if (strncmp(buffer, "DELBX", 5) == 0) {
			// logic for deleting a box

			if (!tempboxargs.boxexists) {
				strcpy(buffer, "ER:NEXST");
			} else if (pthread_mutex_trylock(&tempboxargs.box->mutex) == EBUSY) {
				strcpy(buffer, "ER:OPEND");
			} else if (!tempboxargs.messagelistempty) {
				strcpy(buffer, "ER:NOTMT");
			} else if (!tempboxargs.validname) {
				strcpy(buffer, "ER:WHAT?");
			} else {
				deleteBox(tempboxargs.box);
				strcpy(buffer, "OK!");
			}

		} else if (strncmp(buffer, "OPNBX", 5) == 0) {
			// logic for opening  a box
			if (!tempboxargs.boxexists) {
				strcpy(buffer, "ER:NEXST");
			} else if (pthread_mutex_trylock(&tempboxargs.box->mutex) == EBUSY) {
				strcpy(buffer, "ER:OPEND");
			} else if (!tempboxargs.validname) {
				strcpy(buffer, "ER:WHAT?");
			}
#if defined SUPPORTS_MULTI_BOX
			else {
				// not implemented
			}
#else
			else if (currentMailBox != NULL) {
				strcpy(buffer, "ER:NTIMP");
				// not implemented
			} else {
				currentMailBox = tempboxargs.box;
				strcpy(buffer, "OK!");
			}
#endif
		}

#if defined SUPPORTS_MULTI_BOX
		else if (strncmp(buffer, "NXTBX", 5) == 0) {
			// not implemented
		}
#endif
		else if (strncmp(buffer, "CLSBX", 5) == 0) {
			// logic for closing a box

#if defined SUPPORTS_MULTI_BOX
			// not implemented
#else
			if (currentMailBox == NULL || !tempboxargs.boxexists ||
			    strcmp(currentMailBox->name, inputMailBoxName) != 0) {
				strcpy(buffer, "ER:NOOPN");
			} else {
				strcpy(buffer, "OK!");
				pthread_mutex_unlock(&currentMailBox->mutex);
				currentMailBox = NULL;
			}

#endif
		} else if (strncmp(buffer, "PUTMG", 5) == 0) {

			size_t msgsize;

			int nummatched = sscanf(buffer, "PUTMG!%zu!%[^\n]", &msgsize, buffer);

			if (nummatched != 2 || strlen(buffer) != msgsize) {
				strcpy(buffer, "ER:WHAT?");
			} else {
				addMessage(buffer, currentMailBox);
				sprintf(buffer, "OK!%zu", msgsize);
			}

			// logic for putting a message
			// make message node inside messageList
		} else {
			strcpy(buffer, "ER:WHAT?");
		}

		long long sizeremains = strlen(buffer);
		sofar = 0;
		if (sizeremains < MAX_BUFFER_SIZE) {
			write(sockfd, buffer, sizeremains);
		} else {
			do {
				write(sockfd, buffer + sofar, strlen(buffer));
				sofar += MAX_BUFFER_SIZE;
				sizeremains -= MAX_BUFFER_SIZE;
			} while (sizeremains >= MAX_BUFFER_SIZE);
		}

		if (buffer[0] == 'E') {
			strncpy(err, buffer, 8);
			err[8] = '\0';
			// TODO: add time & ip
			fprintf(stderr, "time date ip %s\n", err);
		}

		// use strlen(buffer)

		// reset buffer size
		// buffer = realloc(buffer, MAX_BUFFER_SIZE*sizeof(char));
		free(buffer);
		buffer = (char *)calloc(MAX_BUFFER_SIZE, sizeof(char));
	}

	free(buffer);
	close(sockfd);
	// TODO: add time & ip
	printf("time date ip disconnected\n");

	pthread_exit(NULL);
}

int main(int argc, const char *argv[])
{

	if (argc != 2) {
		error("ERROR: DUMBserve requires a port number to start up the DUMB server on.\n");
	}

	// sockfd and newsockfd are file descriptors, i.e. array subscripts into the file descriptor table
	// . These two variables store the values returned by the socket system call and the accept system call.
	// portno stores the port number on which the server accepts connections.
	// clilen stores the size of the address of the client. This is needed for the accept system call.
	int sockfd, newsockfd, portno;
	unsigned int clilen;

	/*
	 A sockaddr_in is a structure containing an internet address. This structure is defined in
	 <netinet/in.h>. Here is the definition: struct sockaddr_in { short   sin_family; u_short
	 sin_port; struct  in_addr sin_addr; char    sin_zero[8];
	 };
	 An in_addr structure, defined in the same header file, contains only one field, a unsigned
	 long called s_addr. The variable serv_addr will contain the address of the server, and
	 cli_addr will contain the address of the client which connects to the server.
	 */
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("ERROR: a problem occured when trying to open the socket.\n");
	}

	// initialize serv_addr to zeros.
	memset((char *)&serv_addr, '\0', sizeof(serv_addr));

	// convert this from a string of digits to an integer.
	portno = atoi(argv[1]);

	// This should always be set to the symbolic constant AF_INET.
	serv_addr.sin_family = AF_INET;

	// convert the port number in host byte order to a port number in network byte order.
	serv_addr.sin_port = htons(portno);

	// This field contains the IP address of the host. For server code, this will always be the IP
	// address of the machine on which the server is running, and there is a symbolic constant INADDR_ANY which gets this address.
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}

	// this socket could be already in use on this machine.
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		error("ERROR: a problem occured when trying to bind to an address");
	}

	// listen on the socket for connections.
	if (listen(sockfd, 5) == 0) {
		printf("Listening on %s:%d\n", inet_ntoa(serv_addr.sin_addr), portno);
	} else {
		error("ERROR: a problem occured when trying to listen for new connections.\n");
	}

	clilen = sizeof(cli_addr);

	// block until a client connects to the server.
	while ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen))) {

		pthread_arg_t *threadargs = malloc(sizeof(threadargs));
		pthread_attr_t threadattr;
		pthread_attr_init(&threadattr);

		threadargs->sockfd = newsockfd;
		threadargs->cliaddr = cli_addr;

		// create the thread detached
		if (pthread_attr_setdetachstate(&threadattr, PTHREAD_CREATE_DETACHED) != 0) {
			error(
			"ERROR: a problem occured when trying to create a thread for a new connection.\n");
		}

		if (pthread_create(&threadargs->id, &threadattr, (void *)clientconnectionhandler, threadargs) != 0) {
			perror(
			"ERROR: a problem occured when trying to create a thread for a new connection.\n");
			free(threadargs);
			continue;
		}
	}

	if (newsockfd != 0) {
		error("ERROR: a problem occured when trying to accept a new connection.\n");
	}

	return 0;
}
