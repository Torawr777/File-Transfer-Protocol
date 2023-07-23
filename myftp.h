//header file with declarations common to both programs
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <dirent.h>
#include <ctype.h>

#define PORT_NUMBER 49999
#define PORT_NUMBER_STRING "49999"
#define IP_NUMBER "10.0.2.15"

int debug_flag;
const char *CUSTOM_PORT = PORT_NUMBER_STRING;
const char *CUSTOM_IP = IP_NUMBER;
