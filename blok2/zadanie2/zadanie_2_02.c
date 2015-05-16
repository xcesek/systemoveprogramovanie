#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "barier.h"

typedef enum {
	UNSET = 0,
	SET = 1
} OPTION;

typedef struct {
	OPTION h;
	OPTION n;
	char * nproc;
	int n_proc;
} ARGS;

int quit = 1;
 
void printHelpAndExit(FILE* stream, int exitCode){
	fprintf(stream, "Usage: parametre [-h] [-n | --n_proc <N>] \n");
	fprintf(stream, "Zadanie 2-01\n");
	fprintf(stream, "Prepinace:\n");
	fprintf(stream, " -h, --help   vypise help\n");
	fprintf(stream, " -n, --n_proc pocet procesov\n");
	exit(exitCode);
}

void parseArgs(int argc, char * argv[], ARGS * args) {
	int opt;

	args->h = UNSET;
	args->n = UNSET;
	args->nproc = NULL;
	args->n_proc = 4;
	
	static struct option long_options[] = {
                   						  {"help", 0, NULL, 'h'},
                   						  {"n_proc", 0, NULL, 'n'},
                   						  {0, 0, 0, 0}
               							  };
	int option_index = 0;

	do {
		opt = getopt_long(argc, argv, ":hn:", long_options, &option_index);
		switch (opt) {
		case 'h':
			args->h = SET;
			printHelpAndExit(stderr, EXIT_SUCCESS);
			break;
		case 'n':
			args->n = SET;
			args->nproc = optarg;
			break;
		case '?': 	
			fprintf(stderr,"Neznama volba -%c\n", optopt);
			printHelpAndExit(stderr, EXIT_FAILURE);
		case ':': 	
			fprintf(stderr, "Nebol zadany argument prepinaca '-%c'\n", optopt);
			printHelpAndExit(stderr, EXIT_FAILURE);
		default:
			break;
		}
		
	} while(opt != -1);

	while(optind < argc ) {
		printf("Debug:    non-option ARGV-element: %s\n", argv[optind++]);
	}
}

void validateArgs(ARGS * args) {
	if(args->n == SET){
		if(sscanf(args->nproc, "%d", &args->n_proc) <= 0 ) {
			fprintf(stderr, "Argument prepinaca -n nie je cislo!\n");
			printHelpAndExit(stderr, EXIT_FAILURE);
		}
		if(args->n_proc < 4){
			fprintf(stderr, "Argument prepinaca -n musi byt vacsi ako 4\n");
			printHelpAndExit(stderr, EXIT_FAILURE);
		}	
	}
}

void shuffle(int *array, size_t n){
    if (n > 1) {
        size_t i;
	for (i = 0; i < n - 1; i++) {
	  size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
	  int t = array[j];
	  array[j] = array[i];
	  array[i] = t;
	}
    }
}

void ChildSignalHandler(){
	quit = 0;
}

void doWork(){
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &ChildSignalHandler;
	CHECK(sigaction(SIGUSR1, &sa, NULL) ==  0);

	while(quit){
		fprintf(stderr, "pred barierou: %u\n", getpid());
		Barier1();
		fprintf(stderr, "za barierou: %u\n", getpid());
		Barier2();
		sleep(1);
	}
	fprintf(stderr, "Killed %u\n", getpid());
}

void createProc(int number, pid_t* pid_array){
	int i = 0;
	pid_t pid;

	for(i = 0; i < number; ++i){
		switch(pid = fork()){
			case 0:
      			doWork();
				exit(EXIT_SUCCESS);
    		case -1:
      			perror("fork");
      			exit(EXIT_FAILURE);
    		default:
				pid_array[i] = pid;
      			break;
		}
	}
}

int main(int argc, char * argv[]){
 	ARGS args;
	pid_t *pid;
	int i = 0;

	parseArgs(argc, argv, &args);
	validateArgs(&args);

	init(args.n_proc);

	pid = (pid_t*)malloc(sizeof(pid_t) * args.n_proc); 	
	createProc(args.n_proc, pid);

	//KILLOVANIE deti
	sigset_t set;
	int sig_num;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	CHECK(sigprocmask(/*SIG_BLOCK*/ SIG_SETMASK, &set, NULL) == 0);
	CHECK(sigwait(&set, &sig_num) == 0);

	for(i = 0; i < args.n_proc; i++){
		CHECK(kill(pid[i], SIGUSR1) == 0 );
	}

	for(i = 0; i < args.n_proc; i++){
		CHECK(wait(NULL) != -1);	
	}

	teardown();
	free(pid);
	printf("detske procesy ukoncene\n");
	return EXIT_SUCCESS;
}
