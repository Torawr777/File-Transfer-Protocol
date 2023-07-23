//Server code

#include "myftp.h"



//Server response 'E', error resposne
int error(int fd, const char* response) {
	if(debug_flag == 1) { printf("Child %d: Sending error response\n", getpid()); }

	printf("Child %d: Error: %s\n", getpid(), response);
	write(fd, "E\n", 2);
	write(fd, response, strlen(response));
	return 1;
}

// Server response 'A', acknowledgement
int acknowledgement(int fd) {
	if(debug_flag == 1) { printf("Child %d: Sending positive acknowledgement\n", getpid()); }

	write(fd, "A\n", 2);
	return 0;
}

// Terminate client program after telling server's child process to do same
int exit_cmd(int fd) {
	printf("Child %d: Quitting\n", getpid());
	acknowledgement(fd);
	close(fd);

	if(debug_flag == 1) { printf("Child %d: exiting normally\n", getpid()); }
	exit(0);
}

//Test if path exists and if file is regular and readable
int test_path(char *path) {
	struct stat area, *s = &area;

	if(stat(path, s) == 0) {
		if((s->st_mode &S_IRUSR) && S_ISREG(s->st_mode)) {
			return 1;
		}
	}
	return -1;
}

//Test if we directory exists and we have permission
int test_dir(int fd, char *path) {
	struct stat buffer, *s = &buffer;

	//Check if directory exists and can be accessed
	if(access(path, F_OK) != 0) {
		error(fd, "No such file or directory\n");
		return -1;
	}

	//Check if we have permission
	if((lstat(path, s) == 0) && (S_ISDIR(s->st_mode) == 1) ) {
		if((s->st_mode &S_IRUSR) && (s->st_mode &S_IXUSR)) {
			return 1;
		}
		else { 
			error(fd, "No permissions for this file or directory\n");
			return -1;
		}
	}
	return 1;
}

//Function to change directories
int change_dir(int fd, char* path) {

	//Null terminate the path
	path++;
	path[strlen(path)-1] = '\0';

	if(test_dir(fd, path) == -1) {
		printf("Could not change to directory\n");
		return -1;
	}

	if(chdir(path) == -1) {
		printf("%s\n", strerror(errno));
		error(fd, "Could not change to directory\n");
		return -1;
	}
	else {
		printf("Child %d: Successfully change to directory %s\n", getpid(), path);
		acknowledgement(fd);
	}
	path--;
	return 1;
}

// retrieve <pathname> and store it locally in client’s current working directory
int get_cmd(int fd, int fd2, char *path) {
	int file;
	path++;

	//Check if path was entered
	if(strlen(path) < 1) {
		error(fd, "Invalid path\n");
		return -1;
	}
	// Null terminate the path
	path[strlen(path)-1] = '\0';

	//Check if file is accessible, readable, and regular
	if((access(path, F_OK) == 0) && (access(path, R_OK) == 0) && (test_path(path) == 1) ) {
		acknowledgement(fd);
		file = open(path, O_RDONLY);
	}
	else {
		error(fd, "File isn't accesible, readable, or regular\n");
		return -1;
	}

	if(lseek(file, 0, SEEK_SET) == -1) { printf("%s\n", strerror(errno)); }
	if(debug_flag == 1) { printf("Child %d: Sending %s to client\n", getpid(), path); }

	int readThis;
	char buf[512];
	while((readThis = read(file, buf, sizeof(path))) > 0) {
		write(fd2, buf, readThis);
	}
	close(file);
	
}

// Execute "ls -l", list directory
int list_dir(int fd) {
	int pid;

	if((pid = fork()) == 0) {
		dup2(fd, STDOUT_FILENO);
		close(fd);
		execlp("ls", "ls", "-l", (char*)NULL);
		printf("%s\n", strerror(errno));
		exit(1);
	}
	else if(pid == -1) {
		printf("%s\n", strerror(errno));
		exit(1);
	}
	else {
		waitpid(pid, NULL, 0);
		printf("Child %d: ls success\n", getpid());
	}
	return 1;
}


int put_cmd(int fd, int fd2, char *path) {
	path++;
	//Check if path was entered
	if(strlen(path) < 1) {
		error(fd, "Invalid path\n");
		return -1;
	}

	//Get the name
	char *token = strtok(path, "/");
	char *name;
	while(token != NULL) {
	
		name = strdup(token);
		token = strtok(NULL, "/");
	}
	//Null terminate name
	name[strlen(name)-1] = '\0';

	// Check if file exists
	if(access(name, F_OK) == 0) {
		error(fd, "File alreaday exists\n");
		return -1;
	}

	acknowledgement(fd);
	if(debug_flag == 1) { printf("Child %d: put acknowledged\n", getpid()); }

	int file = open(name, O_CREAT | O_EXCL | O_RDWR); 
	if(lseek(file, 0, SEEK_SET) == -1) { printf("%s\n", strerror(errno)); }

	int readThis;
	int writeThis;
	char buf[512];
	while((readThis = read(fd2, buf, sizeof(path))) != -1) {
		writeThis = write(file, path, readThis);

		//Unlink and close in case writing fails
		if(writeThis == -1) {
			printf("%s\n", strerror(errno));
			unlink(name);
			close(file);
			return -1;		
		}

		//Unlink and close in case reading fails
		if(readThis == -1) {
			printf("%s\n", strerror(errno));
			unlink(name);
			close(file);
			return -1;
		}
	}

	printf("Child %d: Put command succesfulon %s\n", getpid(), name);
	close(file);
	return 1;
}

int remote(int fd) {
	// Start by making a socket (SOCK_STREAM is TCP)
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == 0) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: socket created with descriptor %d\n", listenfd); }

    // Clear the socket and make it available if the server is killed
    int setSocket = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(setSocket) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }

    //Bind the socket to a port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = 0; //Using Port 0
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        perror("bind");
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: socket bound to port %s\n", CUSTOM_PORT); }

    //Get address of fd
    int length = sizeof(struct sockaddr_in);
    if(getsockname(listenfd, (struct sockaddr *) &servAddr, &length) == -1) {
    	error(fd, "Cannot bind\n");
    	perror("bind");
    	fflush(stderr);
    	exit(1);
    }

    //Set up a connection queue 1 level deep
    int setListen = listen(listenfd, 1);
    if(setListen == -1) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: listening with connection queue of 1\n"); }

    //ASCII coded decimal int 
    char portNum[20];
    memset(portNum, 0, sizeof(portNum));
    sprintf(portNum, "A%d\n", ntohs(servAddr.sin_port));
    write(fd, portNum, strlen(portNum));
    if(debug_flag == 1) { printf("Child %d: Acknowledgement sent\n", getpid()); }

    //Used for accepting connections
    struct sockaddr_in clientAddr;

    //Used for getting text host name
    char hostName[NI_MAXHOST];
    int hostEntry;

    //Accept connections
    int connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);

    if(connectfd < 0) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }
    else {
    	if(debug_flag == 1) {  printf("Parent: accepted client connection with descriptor %d\n", connectfd); }
        
        //Get text host name
        hostEntry = getnameinfo((struct sockaddr*)&clientAddr, sizeof(clientAddr),
                                hostName, sizeof(hostName), NULL, 0, NI_NUMERICSERV);
        if(hostEntry != 0) {
            printf("Error: %s\n", gai_strerror(hostEntry));
            fflush(stderr);
        }
        
        close(listenfd);
        return connectfd;
    }
}

// Execute commands according to user input
int execute(int fd) {
	int more = -20;
	while(fd, F_GETFD) {

		int readThis;
		char buffer[1];
		char cmd[PATH_MAX+1];
		memset(cmd, 0, sizeof(cmd));

		//Read the command (stop at newline)
		while((readThis = read(fd, buffer, 1) > 0)) {
			strncat(cmd, buffer, readThis);
			if(buffer[0] == '\n') { break; }
		}

		if(readThis == 0) {
			close(fd);
			printf("Child %d: Connection closed, exiting\n", getpid());
			exit(1);
		}

		//If a command was read
		if(readThis > 0) {
			if(debug_flag == 1) { printf("Entering read stage\n"); }
			fflush(stdout);
			//"D" Establish data connection
			if(strcmp(cmd, "D\n") == 0) {
				if(debug_flag == 1) { printf("D received\n"); }
				if(more > 0) { 
					error(fd, "Socket already exists\n"); 
				}
				else {
					more = remote(fd);
				}
			}
			//"C<pathname>" Change directory
			else if(cmd[0] == 'C') {
				change_dir(fd, cmd);
			}
			//"L" List directory
			else if(strcmp(cmd, "L\n") == 0) {
				if(debug_flag == 1) { printf("L received\n"); }
				if(more < 0) {
					error(fd, "No connection\n");
				}
				else {
					acknowledgement(fd);
					list_dir(more);
					close(more);
					more = -20;
				}
			}
			//"G<pathname>" Get a file
			else if(cmd[0] == 'G') {
				if(debug_flag == 1) { printf("G received\n"); }
				if(more < 0) {
					error(fd, "No connection\n");
				}
				else {
					get_cmd(fd, more, cmd);
					close(more);
					more = -20;
				}
			}
			//"P<pathname>" Put a file
			else if(cmd[0] == 'P') {
				if(debug_flag == 1) { printf("P received"); }
				if(more < 0) {
					error(fd, "No connection\n");
				}
				else {
					put_cmd(fd, more, cmd);
					close(more);
					more = -20;
				}
			}
			//"Q" Quit
			else if(strcmp(cmd, "Q\n") == 0) {
				if(debug_flag == 1) { printf("Q received\n"); }
				exit_cmd(fd);
			}
			else {
				printf("Command '%s' is unkown - ignored\n", cmd);
			}
		}
	}
	return 1;
}


void myServer() {

	// Start by making a socket (SOCK_STREAM is TCP)
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: socket created with descriptor %d\n", listenfd); }

    // Clear the socket and make it available if the server is killed
    int setSocket = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(setSocket == -1) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }

    //Bind the socket to a port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(CUSTOM_PORT));
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        perror("bind");
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: socket bound to port %s\n", CUSTOM_PORT); }

    //Set up a connection queue 4 levels deep
    int setListen = listen(listenfd, 4);
    if(setListen == -1) {
        printf("%s\n", strerror(errno));
        fflush(stderr);
        exit(1);
    }
    if(debug_flag == 1) { printf("Parent: listening with connection queue of 4\n"); }

    //Used for accepting connections
    int pid;
    int connectfd;
    int length = sizeof(struct sockaddr_in);
    struct sockaddr_in clientAddr;

    //Used for getting text host name
    char hostName[NI_MAXHOST];
    int hostEntry;

    //Infinite loop (server runs forever until told so)
    for(;;) {
        //Accept connections
        connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);

        if(connectfd < 0) {
            printf("%s\n", strerror(errno));
            fflush(stderr);
            exit(1);
        }
        else {
        	if(debug_flag == 1) {  printf("Parent: accepted client connection with descriptor %d\n", connectfd); }
            
            //Get text host name
            hostEntry = getnameinfo((struct sockaddr*)&clientAddr, sizeof(clientAddr),
                                    hostName, sizeof(hostName), NULL, 0, NI_NUMERICSERV);
            if(hostEntry != 0) {
                printf("Error: %s\n", gai_strerror(hostEntry));
                fflush(stderr);
            }
            printf("Parent: Connection accepted from %s\n", hostName);

            //If connection accepted, create child process
            if((pid = fork()) == 0) {
            	execute(connectfd);
            }
            else if(pid == -1) {
            	printf("%s\n", strerror(errno));
            }
            //Close off parent and wait for child
            else {
                int status;
                close(connectfd);
                waitpid(-1, &status, WNOHANG);
                if(debug_flag == 1) { printf("Parent: spawned child %d\n", pid); }
            }
        }
    }
}


int main(int argc, char const *argv[]){

	//Server arguments
	int n = 1;
	while(n < argc && argc > 1) {
		if(strcmp(argv[n], "-d") == 0) {
			debug_flag = 1; //Enable debug messages
		}
		if(strcmp(argv[n], "-p") == 0) {
			CUSTOM_PORT = argv[n+1]; //Get custom port
		}
		n++;
	}

	if(debug_flag == 1) {
		printf("Debug output enabled.\n");
		printf("Using port: %s\n", CUSTOM_PORT);
	}

	//Initiate server
	myServer();

    return 0;
}
