#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define FIFO_NAME "fifo-sp01"

char file_path[PATH_MAX + 1];
int  size_ = 0;
sig_atomic_t child_status = 0;

void handler(int signal_number){
  //int signal;
  //wait(&signal);
   wait((int*)&child_status);
}

int child(){
	int fifo;
        fprintf(stderr, "Zadajte cestu k priecinku:");
        scanf("%100s", (char*)&file_path);
	//FIFO
	umask(0);
	if(	mkfifo(FIFO_NAME, 0660) == -1 && errno != EEXIST){
		perror("mkfifo");
		return EXIT_FAILURE;
	}

	if((fifo = open(FIFO_NAME, O_WRONLY)) == -1 ){
		perror("open");
		exit(EXIT_FAILURE);
	}
	if(unlink(FIFO_NAME) != 0){
		perror("unlink");
		exit(EXIT_FAILURE);
	} 

	write(fifo, &file_path, sizeof(file_path));

	if(close(fifo) != 0) {	
		perror("close");
		exit(EXIT_FAILURE);
	}
	return 1;
}

void printHelpAndExit(FILE* stream, int exitCode){
	fprintf(stream, "Usage: parametre [-h] [-s | --size] [<dir>]\n");
	fprintf(stream, "Program vypise pocet regularnych suborov v adresarovej strukture. Ak je definovany prepinac, zobrazi sa aj velkost reuglarnych suborov\n");
	exit(exitCode);
}

void getPath(){
	char buffer[50];
	fprintf(stderr, "Zadajte cestu k novemu priecinku:");
	scanf("%49s", (char*)&buffer);
	strncpy(file_path, buffer, sizeof(file_path));
}

void getPathFromChild(){
	struct sigaction sa;
	int fifo;
	pid_t pid;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &handler;
	sigaction(SIGCHLD, &sa, NULL);

	pid = fork();
  	switch(pid){
    		case 0:
      			child(file_path);
			exit(EXIT_SUCCESS);
    		case -1:
      			perror("fork");
      			exit(EXIT_FAILURE);
    		default:
			while(child_status){}
      			break;
  	}
	//FIFO
	umask(0); 
	if(	mkfifo(FIFO_NAME, 0660) == -1 &&  errno != EEXIST){
		perror("mkfifo");
		return EXIT_FAILURE;
	}

	if((fifo = open(FIFO_NAME, O_RDONLY)) == -1) {
                perror("open");
                exit(EXIT_FAILURE);
        }

	if(read(fifo, &file_path, sizeof(file_path)) != sizeof(file_path)) {
		fprintf(stderr, "Error occured while reading from FIFO");
		exit(EXIT_FAILURE);	
	}
        if(close(fifo) != 0) {
                perror("close");
                exit(EXIT_FAILURE);
        }

}

void parseArgs(int argc, char * argv[]) {
	int opt;

	static struct option long_options[] = {    		  {"help", 0, NULL, 'h'},
                   						  {"size", 0, NULL, 's'},
                   						  {0, 0, 0, 0}
               							  };
	int option_index = 0;

	do {
		opt = getopt_long(argc, argv, "hs", long_options, &option_index);

		switch (opt) {
		case 'h':
			printHelpAndExit(stderr, EXIT_SUCCESS);	
			break;
		case 's':
			size_ = 1;
			break;
		case '?': 	
			fprintf(stderr,"Neznama volba -%c\n", optopt);
			printHelpAndExit(stderr, EXIT_FAILURE);
		default:
			break;
		}
		
	} while(opt != -1);

	if(optind < argc ) {
		strncpy(file_path, argv[argc - 1], sizeof(file_path));
	} else {
		getPathFromChild();
	} 
}

void readDir(char* dir){
	DIR* directory;
	struct dirent* entry;
	int count = 0;
	size_t length;
	int size = 0;
	length = strlen(dir);
	if (dir[length - 1] != '/') {
    		dir[length] = '/';
    		dir[length + 1] = '\0';
    		++length;
  	}
	if((directory = opendir(dir)) == NULL){
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	struct stat st;
	char file[PATH_MAX + 1];
	strcpy(file, dir);

	while(( entry = readdir(directory)) != NULL) {
		if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
    			strncpy(file + length, entry->d_name, sizeof(file) - length);
    			lstat (file, &st);
			if(S_ISDIR (st.st_mode)){
				pid_t pid = fork(); //musi byt aby sme mohli rekurzivne volat(nemoze byt otvorenzch tolko dir jednym porcesom
				if(!pid){
					readDir(file);
					exit(EXIT_SUCCESS);
				} else {
					wait(NULL);
				}
			}
			if(S_ISREG (st.st_mode)){
				count++;
				if(size_)
					size += entry->d_reclen; 
			}
		}
	}
	fprintf(stderr, "# of reg files in %s is %i\n", dir, count);
	if(size_)
		fprintf(stderr,"and size is %i\n", size);
	if(closedir(directory) != 0 ) {
		perror("closedir");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char * argv[]){

	parseArgs(argc, argv);
	while(1){
		readDir(file_path);
		getPath();
	}
	return EXIT_SUCCESS;
}
