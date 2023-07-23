//Client code

#include "myftp.h"


void myClient() {
    const char *ip = CUSTOM_IP;
	//Set up the address of the server
    struct addrinfo hints, *actualdata;
    memset(&hints, 0, sizeof(hints));

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    //Setup address of the server
    int err = getaddrinfo(ip, CUSTOM_PORT, &hints, &actualdata);
    if(err != 0) {
        printf("Error: %s\n", gai_strerror(err));
        fflush(stdout);
        exit(1);
    }

    int socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
    if(socketfd == -1) {
        printf("%s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }

    //Connect to the server
    int attemptConnect = connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen);
    if(attemptConnect != 0) {
        printf("%s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    printf("Succesfully connected to %s\n", ip);

  int readThis;
    //infinite loop to enter commands
    for(;;) {
        char cmd[1];
        char input[512];
        char serverMsg[512];
        memset(cmd, 0, sizeof(cmd));
        memset(input, 0, sizeof(input));
        memset(serverMsg, 0, sizeof(serverMsg));
       
    	//Header for commands
    	write(STDOUT_FILENO, "MFTP> ", sizeof("MFTP> "));

    	//Exit this read when detecting new line
    	while((readThis = read(STDOUT_FILENO, cmd, 1)) > 0) {
    		strncat(input, cmd, 1);
    		if(cmd[0] == '\n') { break; }
    	}
    	//If nothing was read
    	if(readThis < 0) {
    		printf("%s\n", strerror(errno));
    		exit(1);
    	}

    	char *cp = strdup(input);
    	char *parse = strtok(cp, " ");

    	// "exit" detected
    	if(strcmp(parse, "exit\n") == 0) {

    		write(socketfd, "Q\n", 2);

    		while(readThis = read(socketfd, cmd, 1) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
    		//If nothing was read
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}
    		//Server acknowledges exit
    		if(serverMsg[0] == 'A') {
    			close(socketfd);
    			free(actualdata);
                printf("Exiting\n");
    			exit(0);
    		}
    	}
    	// "ls" detected
    	else if(strcmp(parse, "ls\n") == 0) {
    		int status;

    		if(fork()) { wait(&status); }
    		else {
    			int fd[2];
    			int status_ls;

    			//Create pipe
    			if(pipe(fd) < 0) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

                //IMPORTANT: they are assigned after pipe for a reason
                int read = fd[0];
                int write = fd[1];

    			//"more -20" command
    			if(fork()) {
    				close(write);
    				close(0);
    				dup(read);
    				close(read);
    				wait(&status_ls);
    				execlp("more", "more", "-d", "-20", (char*)NULL);
    			}
    			//"ls" command
    			else {
    				close(read);
    				close(1);
    				dup(write);
    				close(write);
    				execlp("ls", "ls", "-l", "-a", (char*)NULL);
    			}
    		}
    	}
    	// "rls" detected
    	else if(strcmp(parse, "rls\n") == 0) {
    		write(socketfd, "D\n", 2);

    		// Load message
    		while((readThis = read(socketfd, cmd, 1)) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}

    		//Server acknowledges message
    		if(serverMsg[0] = 'A') {
    			//Set up address
    			struct addrinfo hints2, *actualdata;
    			memset(&hints2, 0, sizeof(hints2));

    			hints2.ai_socktype = SOCK_STREAM;
    			hints2.ai_family = AF_INET;
    			char *port = strtok(serverMsg, "A\n");

    			int err2 = getaddrinfo(ip, port, &hints2, &actualdata);
    			if(err2 != 0) {
    				printf("Error %s\n", gai_strerror(err));
    				fflush(stdout);
    			}

    			int socketfd2 = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
    			if(socketfd2 == -1) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			int connectfd2 = connect(socketfd2, actualdata->ai_addr, actualdata->ai_addrlen);
    			if(connectfd2 != 0) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			//Send L to server
    			write(socketfd, "L\n", 2);
    			memset(serverMsg, 0, sizeof(serverMsg));
    			readThis = read(socketfd, serverMsg, 512);

    			//Nothing was read
    			if(readThis < 0) {
    				printf("%s\n", strerror(errno));
    				exit(1);
    			}
    			int status;

    			//Server acknowledges
    			if(serverMsg[0] == 'A') {
    				if(fork()) {
    					wait(&status);
    					close(socketfd2);
    					free(actualdata);
    				}
    				else {
    					close(0);
    					dup(socketfd2);
    					close(socketfd2);
    					execlp("more", "more", "-d", "-20", (char*)NULL);
    				}
    			}
                else if(serverMsg[0] = 'E') {
                    printf("Error, command rls unsuccesful; %s\n", serverMsg);
                }
    		}
    		//Server sends error "E"
    		else if(serverMsg[0] == 'E') {
    			printf("Error, command rls unsuccesful; %s\n", serverMsg);
    		}
    	}
    	// "cd" detected
    	else if(strcmp(parse, "cd") == 0) {
    		struct stat path;

    		parse = strtok(NULL, " \n");
    		lstat(parse, &path);

    		//Check if it's a directory and readable
    		if(S_ISDIR(path.st_mode) && access(parse, R_OK) == 0) {
    				chdir(parse);
    		}
    		else {
    			printf("%s\n", strerror(errno));
    		}
    	}
    	// "rcd" detected
    	else if(strcmp(parse, "rcd") == 0) {
    		parse = strtok(NULL, " ");

    		//Null terminate 
    		parse[strlen(parse)] = '\0';

    		//Formate it like the server wants
    		write(socketfd, "C", 1);
    		write(socketfd, parse, strlen(parse));

    		//Read message
    		while((readThis = read(socketfd, cmd, 1)) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}
    		if(serverMsg[0] == 'E') {
    			printf("Error, command rcd unsuccesful; %s\n",serverMsg);
    		}
    	}
    	//"get" detected
    	else if(strcmp(parse, "get") == 0) {

    		//Send "D" for data connection
    		write(socketfd, "D\n", 2);

    		//Read in command
    		while((readThis = read(socketfd, cmd, 1)) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
    		//Nothing read
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}
    		parse = strtok(NULL, " ");

    		//Null terminate
    		parse[strlen(parse)] = '\0';

    		if(serverMsg[0] == 'A') {
    			//Setup address stuff again
    			struct addrinfo hints2, *actualdata;
    			memset(&hints2, 0, sizeof(hints2));

    			hints2.ai_socktype = SOCK_STREAM;
    			hints2.ai_socktype = AF_INET;

    			char *port = strtok(serverMsg, "A\n");
    			int err2 = getaddrinfo(ip, port, &hints2, &actualdata);
    			if(err2 != 0) {
    				printf("Error %s\n", gai_strerror(err));
    				fflush(stdout);
    			}

    			int socketfd2 = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
    			if(socketfd2 == -1) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			int connectfd2 = connect(socketfd2, actualdata->ai_addr, actualdata->ai_addrlen);
    			if(connectfd2 != 0) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			//Formate like the server wants
    			write(socketfd, "G", 1);
    			write(socketfd, parse, strlen(parse));

    			//read message
                
    			readThis = read(socketfd, serverMsg, 512);
    			if(readThis < 0) {
    				printf("%s\n", strerror(errno));
    				exit(1);
    			}
                
    			if(serverMsg[0] == 'A') {
                    printf("A2\n");
    				//Get file name
    				char *sep = strtok(parse, "/");
    				char *name;

    				while(sep != NULL) {
    					name = strdup(sep);
    					sep = strtok(NULL, "/");
    				}

    				//Null terminate name
    				name[strlen(name)-1] = '\0';

    				//Make the file
    				char buf[512];
    				int fd = open(name, O_CREAT | O_EXCL | O_RDWR, 0700);
    				if(fd == -1) {
    					printf("%s\n", strerror(errno));
    					fflush(stdout);
    				}
    				else {
    					//Read command
    					while((readThis = read(socketfd2, buf, 512)) > 0) {
    						write(fd, buf, readThis);
    					}
    					//Nothing was read
    					if(readThis < 0) {
    						printf("%s\n", strerror(errno));
    						unlink(name);
    					}
    					close(fd);
    				}

    			}
    			else if(serverMsg[0] == 'E') {
    				printf("Error, command get unsuccesful; %s\n",serverMsg);
    			}
    			close(socketfd2);
    			free(actualdata);

    		}
    		//Server sends error
    		else if(serverMsg[0] == 'E') {
    			printf("D error: %s\n", serverMsg);
    		}
    	}
    	//"show" detected
    	else if(strcmp(parse, "show") == 0) {
    		write(socketfd, "D\n", 2);

    		while((readThis = read(socketfd, cmd, 1)) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}
    		parse = strtok(NULL, " ");

    		//Null terminate
    		parse[strlen(parse)] = '\0';

    		//Server acknowledged
    		if(serverMsg[0] == 'A') {

    			//Set up address stuff again
    			struct addrinfo hints2, *actualdata;
    			memset(&hints2, 0, sizeof(hints2));

    			hints2.ai_socktype = SOCK_STREAM;
    			hints2.ai_family = AF_INET;
    			char *port = strtok(serverMsg, "A\n");

    			int err2 = getaddrinfo(ip, port, &hints2, &actualdata);
    			if(err2 != 0) {
    				printf("Error %s\n", gai_strerror(err));
    				fflush(stdout);
    			}

    			int socketfd2 = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
    			if(socketfd2 == -1) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			int connectfd2 = connect(socketfd2, actualdata->ai_addr, actualdata->ai_addrlen);
    			if(connectfd2 != 0) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			//Formate it like the server wants
    			write(socketfd, "G", 1);
    			write(socketfd, parse, strlen(parse));

    			//Nothing was read
    			readThis = read(socketfd, serverMsg, 512);
    			if(readThis < 0) {
    				printf("%s\n", strerror(errno));
    				exit(1);
    			}
    			//Server acknowledged
    			int status;
    			if(serverMsg[0] == 'A') {
    				if(fork()) {
    					wait(&status);
    					close(socketfd2);
    					free(actualdata);
    				}
    				//20 line system again
    				else { 
    					close(0);
    					dup(socketfd2);
    					close(socketfd2);
    					execlp("more", "more", "-20", (char*)NULL);
    				}
    			}
    		}
    		//Server sent an error
    		else if(serverMsg[0] == 'E') {
    			printf("Error, command show unsuccesful; %s\n", serverMsg);
    		}
    	}
    	else if(strcmp(parse, "put") == 0) {
            
    		parse = strtok(NULL, " ");
    		parse[strlen(parse)] = '\0';

    		struct stat stat;
    		lstat(parse, &stat);

    		//Check if it's regular and readable
    		if(!S_ISREG(stat.st_mode) && access(parse, R_OK) == 0) {
    			printf("%s\n", strerror(errno));
                continue;
    		}

    		//Get server response
    		write(socketfd, "D\n", strlen("D\n"));
    		while((readThis = read(socketfd, cmd, 1)) > 0) {
    			strncat(serverMsg, cmd, 1);
    			if(cmd[0] == '\n') { break; }
    		}
            
    		//Nothing was read
    		if(readThis < 0) {
    			printf("%s\n", strerror(errno));
    			exit(1);
    		}

    		//Server acknowledges
    		if(serverMsg[0] == 'A') {

    			//Setup address stuff again
    			struct addrinfo hints2, *actualdata;
    			memset(&hints2, 0, sizeof(hints2));

    			hints2.ai_socktype = SOCK_STREAM;
    			hints2.ai_socktype = AF_INET;

    			char *port = strtok(serverMsg, "A\n");

    			int err2 = getaddrinfo(ip, port, &hints2, &actualdata);
    			if(err2 != 0) {
    				printf("Error %s\n", gai_strerror(err));
    				fflush(stdout);
    			}

    			int socketfd2 = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
    			if(socketfd2 == -1) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			int connectfd2 = connect(socketfd2, actualdata->ai_addr, actualdata->ai_addrlen);
    			if(connectfd2 != 0) {
    				printf("%s\n", strerror(errno));
    				fflush(stdout);
    			}

    			//Formate it like the server wants
    			write(socketfd, "P", 1);
    			write(socketfd, parse, strlen(parse));

    			readThis = read(socketfd, serverMsg, 512);
    			if(readThis < 0) {
    				printf("%s\n", strerror(errno));
                    exit(1);
    			}

    			//server acknowledges again
    			if(serverMsg[0] == 'A') {
    				char *sep = strtok(parse, "/");
    				char *name;

    				while(sep != NULL) {
    					name = strdup(sep);
    					sep = strtok(NULL, "/");
    				}
    				//Null terminate
    				name[strlen(name)-1] = '\0';

    				char buf[512];
    				int fd = open(name, O_RDONLY);

    				//Write stuff to server
    				while((readThis = read(fd, buf, 512)) > 0) {
    					write(socketfd2, buf, readThis);
    				}
    				//Nothing was read
    				if(readThis < 0) {
    					printf("%s\n", strerror(errno));
    					exit(1);
    				}
    				close(fd);
    			}
    			else if(serverMsg[0] == 'E') {
    				write(STDOUT_FILENO, serverMsg, 512);
    			}
    			close(socketfd2);
    			free(actualdata);
    		}
    		else if(serverMsg[0] == 'E') {
    			printf("Error, command put unsuccesful; %s\n", serverMsg);
    		}
    	}

        //Invalid command
    	else {
    		printf("Invalid command: %s\n", parse);
    	}
    	free(cp);
    }
}



int main(int argc, char const *argv[]) {
	//Client arguments
    int n = 1;
    while(n < argc && argc > 1) {
    	if(n == 1) {
    		CUSTOM_PORT = argv[n]; //Get custom port
    	}
    	else if(n == 2) {
    		CUSTOM_IP = argv[n]; //Get custom IP
    	}
        n++;
    }

    printf("Using port: %s\n", CUSTOM_PORT);
    printf("Using IP: %s\n", IP_NUMBER);
    


	myClient();
	return 0;
}