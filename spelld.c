#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "levenshtein.c"
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

extern int errno;

#define QLEN 20

// add a verbose switch
bool Verbose = false;

// Struct added to carry values into *spelld
typedef struct args{
	int COUNT; 
	char DICT[22];
	int PORTNUM;
} args;

struct args pass = {0, "", 0};

int daemonize()
{
   pid_t pid;

   if ( (pid = fork()) < 0 ) {   // fork error
      return -1;
   } else if ( pid != 0 ) {      // parent should exit
      exit(0);
   }

   // child continues from here on

   // become a session leader
   setsid();

   // change working directory to / 
   chdir("/");

   // clear out file creation masks
   umask(0);

   return 0;
}

// CLI function
void spell(int COUNT, const char *DICT){

	// Max word size in list
	char dictstring[30];

	// create file pointer
	FILE *dictionary;

	// Max input length
	char user[30];

	// Hold the strings within distance [12 words, 30 length]
	// x counter for alike position
	char alike[50][30] = {{'\n'}};
	int x = 0;

	// bool flag if word was found
	bool found = false;

	while(1){

		// Open the dictionary list
		dictionary = fopen(DICT,"r");

		// count lines scanned
		int length = 0;

		// get input
		printf("Enter one word: ");
		fgets(user,30,stdin);

		// Exit program on <quit>
		if(strcmp(user,"<quit>\n") == 0){
			break;
		}

		// convert string to lowercase
		for(int i = 0; user[i]; i++){
			user[i] = tolower(user[i]);
		}

		// while not at the end of the file
		while(! feof(dictionary) ){

			// get the current line 
			fgets(dictstring,30,dictionary);

			// lowercase dictionary
			for(int i = 0; dictstring[i]; i++){
				dictstring[i] = tolower(dictstring[i]);
			}

			// compare the input and current file line
			if(strcmp(user,dictstring) == 0){
				printf("\nFound match!\nUser: %sFile: %s",user,dictstring);
				printf("Location: %d\n",length);
				found = true;
				break;
			}

			// Get distance and add to alike array
			int distance = levenshtein_distance(user,dictstring);
			if(distance > 0 && distance < 3){
				if(x < COUNT){
					strcpy(alike[x], dictstring);
					x++;
				}
			}

			length++;

			// if EoF print no match
			if(feof(dictionary)){
				printf("No match found\nUser: %s",user);
				break;
			}
		}

		// bool check for word found
		if(!found){

			// create a holding array
			char hold[50][30] = {{'\n'}};
			int y = 0;
			int printed = 0;
			printf("Did you mean?\n");

			// loop through the alike array
			for(int i = 0; i < COUNT; i++){

				int distance = levenshtein_distance(user, alike[i]);

				// print distance 1 words, hold distance 2 words
				if(distance == 1){
					printf("\t%s",alike[i]);
					printed++;
				}else if(distance == 2){
					strcpy(hold[y],alike[i]);
					y++;
				}
			}

			// if there are distance 2 words, print them now
			if(y != 0){
				y = 0;
				while(printed < COUNT){
					printf("\t%s",hold[y]);
					printed++;
					y++;
				}
			}else{
				while(printed < COUNT){
					printf("\n");
					printed++;
				}
			}
		} 

		// verbose switch to display unsorted list
		if(Verbose){
			if(!found){
				printf("Unsorted List\n");
				for(int i = 0; i < COUNT;i++){
					printf("\t%s",alike[i]);
				}
			}

			printf("Count: %d\nDict: %s\n",COUNT, DICT);
		}

		printf("<END>\n\n");

		// Reset 
		found = false;
		x = 0;
		for(int i = 0; i < COUNT; i++){
			char newl[1] = {'\n'};
			strcpy(alike[i],newl);
		}

	}
	// close file
	fclose(dictionary);
}

// Help menu
void help(){
	printf("Simple spell check\n");
	printf("USE:\n\t.\\spell <options>\n");
	printf("Options:\n\t--dictionary, -d\tSupply wordlist\n");
	printf("\t--count, -c\t\tSupply maxmimim suggestions, no more than 50\n");
	printf("\t--version, -v\t\tsee version number\n");
	printf("\t--verbose, -b\t\tinclude verbose output\n");
	printf("\t--daemon, -s\t\tstart as daemon\n");
	printf("\t--port, -p\t\tspecify port number [default 2345]\n");
	printf("\t--help, -h\t\tdisplay this menu\n");
}

// Threaded function for accepting connections and providing spell check service
void *spelld(void *p){
	int n;

	// assign conn to p so the function doesn't need to be altered 
	int conn = *(int*)p;

	// Max word size in list
	char dictstring[30];

	// create file pointer
	FILE *dictionary;

	// Hold the strings within distance [12 words, 30 length]
	// x counter for alike position
	char alike[50][30] = {{'\n'}};
	int x = 0;

	// bool flag if word was found
	bool found = false;

	// pull struct values
	int COUNT = pass.COUNT;
	char *DICT = pass.DICT;

	while(1){
		// begin operations

		// Open the dictionary list
		dictionary = fopen(DICT,"r");

		// count lines scanned
		int length = 0;

		// get input
		char buf[80] ={""};
		write(conn,"Enter one word: ",17);
		n = read(conn, buf, sizeof(buf));
		if(n == 0){
			// shutdown on ctrl+c input
			shutdown(conn,SHUT_RDWR);
			close(conn);
			pthread_exit(NULL);
		}

		// compare to quit
		if(strncmp(buf,"<quit>\n",7) == 0){
			close(conn);
			break;
		}

		for(int i = 0; buf[i]; i++ ){
			buf[i] = tolower(buf[i]);
		}

		while(! feof(dictionary) ){
			// get the current line 
			fgets(dictstring,30,dictionary);

			// lowercase dictionary
			for(int i = 0; dictstring[i]; i++){
				dictstring[i] = tolower(dictstring[i]);
			}

			// compare the input and current file line
			if(strncmp(buf,dictstring,n) == 0){
				write(conn,"\nFound match!\n",15);
				write(conn,"User: ", 7);
				write(conn, buf, n);
				write(conn, "Dict: ",7);
				write(conn,dictstring,strlen(dictstring));
				write(conn,"\n",2);
				found = true;
				break;
			}

			// write(conn,"flag5",6);
			// Get distance and add to alike array
			int distance = levenshtein_distance(buf,dictstring);
			if(distance > 0 && distance < 3){
				if(x < COUNT){
					strcpy(alike[x], dictstring);
					x++;
				}
			}

			length++;

			// if EoF print no match
			if(feof(dictionary)){
				fclose(dictionary);
				write(conn,"No match found\nUser: ",22);
				write(conn,buf,n);
				break;
			}
		}

		if(!found){
			// create a holding array
			char hold[50][30] = {{'\n'}};
			int y = 0;
			int printed = 0;
			write(conn,"Did you mean?\n",15);

			// loop through the alike array
			for(int i = 0; i < COUNT; i++){
				int distance = levenshtein_distance(buf, alike[i]);

				// print distance 1 words, hold distance 2 words
				if(distance == 1){
					write(conn,"\t",2);
					write(conn,alike[i],strlen(alike[i]));
					printed++;
				}else if(distance == 2){
					strcpy(hold[y],alike[i]);
					y++;
				}
			}

			// if there are distance 2 words, print them now
			if(y != 0){
				y = 0;
				while(printed < COUNT){
					write(conn,"\t",2);
					write(conn,hold[y],strlen(hold[y]));
					printed++;
					y++;
				}
			}else{
				// print newlines to fill the space 
				while(printed < COUNT){
					write(conn,"\n",2);
					printed++;
				}
			}
		} 

		write(conn,"<END>\n\n",8);
		found = false;
		x = 0;
		for(int i = 0; i < COUNT; i++){
			char newl[1] = {'\n'};
			strcpy(alike[i],newl);
		}

		// end operations
	}

	// close file
	fclose(dictionary);

	// done processing request
	shutdown(conn,SHUT_RDWR);
	close(conn);
	pthread_exit(NULL);
}

// create socket and accept connections
void start_daemon(int COUNT, const char *DICT,int PORTNUM){
	// daemonize function
	daemonize();
	int sock, conn;
	int val;
	struct sockaddr_in sin ;
	struct sockaddr_in remote_addr ;
	socklen_t addr_len ;
	addr_len = sizeof ( remote_addr ) ;
	// create socket
	sock = socket ( AF_INET, SOCK_STREAM, 0 ) ;
	if ( sock < 0 ) {
	   fprintf ( stderr, "Can't create socket: %s\n", strerror(errno) ) ;
	   exit(1) ;
	}
	memset ( &sin, 0, sizeof(sin) ) ;
	sin.sin_family = AF_INET ;
	sin.sin_addr.s_addr = INADDR_ANY ;
	sin.sin_port = htons ( (short) PORTNUM ) ;
	// try to bind to port_num
	val = bind ( sock, (struct sockaddr *)&sin, sizeof(sin) ) ;
	if ( val < 0 ) {
	   fprintf ( stderr, "Can't bind socket: %s\n", strerror(errno) ) ;
	   exit(1) ;
	}
	// we have to listen for TCP sockets 
	val = listen ( sock, QLEN ) ;
	if ( val < 0 ) {
	   fprintf ( stderr, "Can't listen on socket: %s\n", strerror(errno) ) ;
	   exit(1) ;
	}
	for ( ; ; ) {
	   conn = accept ( sock, (struct sockaddr *)&remote_addr, &addr_len ) ;
	   if (conn == -1) { // accept error
	      if (errno == EINTR) { // caused by an interrupt
	         continue; // try again
	      } else {
	         fprintf(stderr, "%s", strerror(errno));
	      }
	   }
	   // args pass = {COUNT, DICT,PORTNUM};
	   pass.COUNT = COUNT; 
	   strcpy(pass.DICT, DICT);
	   pass.PORTNUM = PORTNUM;
	   // create new thread for connection and detach it
	   pthread_t thread;
	   pthread_create(&thread, NULL, spelld, (void*) &conn);
	   pthread_detach(thread);
	}
}

// Main function/getopts
int main(int argc, char **argv){
	int c;
	int count = 12;
	char dict[22] = "/usr/share/dict/words";
	int PORTNUM = 2345;
	while(1){
		static struct option long_options[]={
			{"dictionary", required_argument, 0, 'd'},
			{"count", required_argument, 0, 'c'},
			{"version", no_argument, 0, 'v'},
			{"help", no_argument, 0, 'h'},
			{"verbose", no_argument, 0, 'b'},
			{"port", required_argument, 0, 'p'},
			{"daemon", no_argument, 0, 's'},
			{0,0,0,0}
		};
		int option_index = 0;
		c = getopt_long (argc, argv, "d:c:p:vhbs", long_options, &option_index);
		if(c == -1){
			spell(count, dict);
			break;
		}
		switch(c){
			case 'd':
				strcpy(dict, optarg);
				break;
			case 'c':
				count = atoi(optarg);
				if(count > 50){
					printf("Maximum count is 50\n");
					exit(0);
				}
				break;
			case 'v':
				printf("Version 1.0\n");
				exit(0);
			case 'h':
				help();
				exit(0);
			case 'b':
				Verbose = true;
				break;
			case 'p':
				PORTNUM = atoi(optarg);
			case 's':
				start_daemon(count, dict, PORTNUM);
			case '?':
				break;
			default:
				abort();
		}
	}
	return 0;
}