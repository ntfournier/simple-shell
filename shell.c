/*******************************************************************************
 * Shell.c
 *
 * Authors:	Vincent Fournier
 * 		Jean-Philippe R. Parent
 * 		Nadir Hamroun
 *
 * Descriptions: Simple command line interpreter supporting background tasks.
 * 	Instructions
 *		<cmd>		execute the command in GNU/Linux
 *		<cmd> &		same as <cmd> but send the task in background.
 *		btasks		list tasks in background
 *		cd		change directory
 *		exit		quit the shell
 *
 * Changes:	Refer to cvs.
 ******************************************************************************/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKGROUND_PROCESS 10

/* The prompt for the command line */
static const char *PROMPT = "$>";

/* The delimeter to split input from user */
static const char *DELIMITER = " ";

/* Ask the user a command */
void getCommand(int *p_cmdc, char **p_cmd);

/* Run the specified command */
void runCommand(char **p_cmd, bool p_isBackground);

/* Run cd command */
void runBuiltinCd(char *p_path);

/* Print some statistics */
int printChildrenStatistics(struct timeval *startTime, struct timeval *endTime);

/**
 * Background process
 */

/* All the ids of the background process currently running */
pid_t bProcessIds[BACKGROUND_PROCESS] = { 0 };
char *bProcessNames[BACKGROUND_PROCESS];

/* Add a background process to the pool */
void addBackgroundProcess(pid_t p_pid, char* p_cmdName);

/* List all the running background process */
void listBackgroundProcess();

/* Update the list of background process to keep only the running ones */
int updateBackgroundProcess();

int main(int argc, char **argv) {
	if (optind != argc) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}

	for (;;) {
		int cmdc;
		char* cmd[30] = { NULL };
		getCommand(&cmdc, cmd);

		if (cmd[0] == NULL){
			continue;
		}

		/* Handle user input accordingly */
	        if (strcmp(cmd[0], "exit") == 0) {
			listBackgroundProcess();

			int bCount = updateBackgroundProcess();
			if (bCount != 0) {
				printf("There's still %d background(s) process(es) running\n", bCount);
			} else {
				return 0;
			}

		} else if (strcmp(cmd[0], "btasks") == 0 || strcmp(cmd[0], "ap") == 0) {
			listBackgroundProcess();

		} else if (strcmp(cmd[0], "cd") == 0) {
			runBuiltinCd(cmd[1]);
		} else if (strcmp(cmd[cmdc - 1], "&") == 0) {
			cmd[cmdc - 1] = '\0'; // Remove the ampersand from cmd.
			runCommand(cmd, true);

		} else {
			runCommand(cmd, false);
		}
	}

	return 0;
}

/**
 * getCommand
 *
 * Show user a prompt and wait for an input.
 * The user input will then be transform to the same format as argc and argv.
 *
 * param p_cmdc		length of the command.
 * param p_cmd		an array of char representing the user command.
 */
void getCommand(int *p_cmdc, char **p_cmd) {
	printf("%s", PROMPT); // Print the prompt.

	// Read the command from standard input.
	char *buffer = NULL;
	size_t len = 0;
	ssize_t chars = getline(&buffer, &len, stdin);

	if (buffer[0] == '\n') { // return if no command was entered.
		return;
	}

	if (buffer[chars - 1] == '\n') { // Remove the trailing newline.
		buffer[chars - 1] = '\0';
	}

	// Create an array from the buffer.
	char *token;
	token = strtok(buffer, DELIMITER);
	int i = 0;
	while (token != NULL) {
		p_cmd[i] = token;
		token = strtok(NULL, DELIMITER);
		++i;
	}
	p_cmd[i] = NULL; // finish the array with NULL.
	*p_cmdc = i;
}

/**
 * runCommand
 *
 * Run the specified GNU/Linux command in a new thread.
 *
 * param p_cmd		the command to execute.
 * param p_isBackground	if we wait for end of execution of the command.
 */
void runCommand(char **p_cmd, bool p_isBackground) {
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Couldn't fork the program, please retry.");
	} else if (pid == 0) {
		struct timeval startTime;
		gettimeofday(&startTime, NULL);
		pid_t childPid = fork();
		if (childPid == -1) {
			fprintf(stderr, "Couldn't fork the child process");
			exit(EXIT_FAILURE);
		}

		if (childPid == 0) {
			int err = execvp(p_cmd[0], &p_cmd[0]);

			// If execvp failed we need to tell the user and kill the children.
			fprintf(stderr, "Error no: %d during execution of command, did you type correctly.\n", err);
			exit(err);
		} else {
			wait(NULL);
			struct timeval endTime;
			gettimeofday(&endTime, NULL);

			printChildrenStatistics(&startTime, &endTime);
			exit(0);
		}
	} else {
		if (p_isBackground) {
			// TODO: Create a string from the receive p_cmd.
			addBackgroundProcess(pid, p_cmd[0]);
		} else {
			wait(NULL);
		}
	}
}

/**
 * runBuiltinCd
 *
 * Run the change directory builtin.
 *
 * param p_path		the path to access.
 */
void runBuiltinCd(char *p_path) {
	if (p_path == NULL) {
		fprintf(stderr, "Please specify a directory parameter when using cd\n");
	} else {
		chdir(p_path);
		int err = errno; // Get the number of the last error.

		if (err != 0) {
			char *errorMsg;
			if (err == ENOENT) {
				errorMsg = "A component of the path does not name an existing directory";
			} else if (err == EACCES) {
				errorMsg = "Search permission are denied for any component of the pathname.";
			} else if (err == ENOTDIR) {
				errorMsg = "A component of the path is not a directory.";
			} else {
				errorMsg = "Unhandled error.";
			}
			fprintf(stderr, "Error running builtin \"cd %s\", %s\n", p_path, errorMsg);
		}
	}
}

/**
 * printChildrenStatistics
 *
 * Print some statistics from getrusage.
 */
int printChildrenStatistics(struct timeval *startTime, struct timeval *endTime) {
	struct rusage usage;
	getrusage(RUSAGE_CHILDREN, &usage);

	printf("\n----------------------------------------\n");
	printf("Statistics\n");
	printf("----------------------------------------\n");

	long wallClock = (endTime->tv_sec - startTime->tv_sec) * 1000000
		+ (endTime->tv_usec - startTime->tv_usec);
	printf("\tWall-clock time: %ld ms\n", wallClock);

	long cpuTime = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000
		+ usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
	printf("\tCPU time used (user and Kernel): %ld ms\n", cpuTime);

	printf("\tVoluntary context switches: %ld\n", usage.ru_nvcsw);
	printf("\tInvoluntary context switches: %ld\n", usage.ru_nivcsw);
	printf("\tPage faults: %ld\n", usage.ru_majflt);
	printf("\tPage faults satisfied by cache read: %ld\n", usage.ru_minflt);
}

/**
 * addBackgroundProcess
 *
 * Add a new background process to the array of background process.
 *
 * param p_pid		the pid of the process to add.
 * param p_cmdName	the name of the process.
 */
void addBackgroundProcess(pid_t p_pid, char* p_cmdName) {
	updateBackgroundProcess();
	for (int i = 0; i < BACKGROUND_PROCESS; ++i) {
		if (bProcessIds[i] == 0) {
			bProcessIds[i] = p_pid;
			bProcessNames[i] = p_cmdName;

			printf("\t\t[%d] %d\n\n", i, (int) p_pid);
			return;
		}
	}
}

/**
 * listBackgroundProcess
 *
 * Print the currently running background process.
 */
void listBackgroundProcess() {
	updateBackgroundProcess();
	for (int i = 0; i < BACKGROUND_PROCESS; ++i) {
		if (bProcessIds[i] != 0) {
			printf("\t\t[%d] %d\t%s\n", i, (int) bProcessIds[i], bProcessNames[i]);
		}
	}
}

/**
 * updateBackgroundProcess
 *
 * Check for all running process if they are still running.
 *
 * return		the number of still running background process.
 */
int updateBackgroundProcess() {
	int count = 0;

	for (int i = 0; i < BACKGROUND_PROCESS; ++i) {
		if (bProcessIds[i] != 0) {
			int status;
			pid_t pid = waitpid(bProcessIds[i], &status, WNOHANG);

			if (status == 0) {
				bProcessIds[i] = 0;
			} else {
				++count;
			}
		}
	}

	return count;
}
