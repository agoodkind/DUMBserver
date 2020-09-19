//
//  DUMBcommons.h
//
//

#ifndef DUMBcommons_h
#define DUMBcommons_h

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include <netdb.h>

#define MAX_BUFFER_SIZE 320
// 320 is enough for 40 characters
// 16kbit
#define h_addr h_addr_list[0] /* for backward compatibility */

#endif /* DUMBcommons_h */
