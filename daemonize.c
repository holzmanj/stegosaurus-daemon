/*
 * daemonize.c
 * This example daemonizes a process, writes a few log messages,
 * sleeps 20 seconds and terminates afterwards.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

/* Both time values are treated as seconds */
const int CHECK_INTERVAL = 60;
const int FILE_LIFESPAN  = 86400;	// 24 hours

// converts relative path to absolute path
int rel_to_abs (char* rel, char* abs_buf, int buf_len)
{
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	int rel_len = strlen(rel);

	/* converts if dir is relative, otherwise returns the original string */
	if (rel[0] != '/') {
		int cwd_len = strlen(cwd);

		if (buf_len < rel_len + cwd_len + 2) {
			perror("Buffer length is too short for absolute path.");
			return EXIT_FAILURE;
		}
		strcpy(abs_buf, cwd);
		abs_buf[cwd_len] = '/';
		strcpy(abs_buf + cwd_len + 1, rel);
		abs_buf[buf_len + cwd_len + 2] = '\0';
		printf("cwd: %s\ndir: %s\nabs: %s\n", cwd, rel, abs_buf);
	} else {
		if (rel_len >= buf_len) {
			perror("Buffer length is too short for absolute path.");
			return EXIT_FAILURE;
		}
		strcpy(abs_buf, rel);
		abs_buf[rel_len] = '\0';
	}

	return EXIT_SUCCESS;
}

void remove_old_files()
{
	struct stat st;
    size_t i;
	DIR *dir;
	struct dirent *ent;
	char cwd[PATH_MAX], fpath[PATH_MAX], log_msg[300];
	time_t current_time;

	getcwd(cwd, sizeof(cwd));
	if ((dir = opendir(cwd)) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_type == DT_REG) {
				if ((stat(ent->d_name, &st)) == 0) {
					if (current_time - st.st_ctime >= FILE_LIFESPAN) {
						sprintf(fpath, "%s/%s", cwd, ent->d_name);
						
						sprintf(log_msg, "deleting file: %s", fpath);
						syslog(LOG_NOTICE, log_msg);

						// remove file
						remove(fpath);
					}
				} else {
					sprintf(log_msg, "Problem reading file: %s\terrno: %d\n", ent->d_name, errno);
					syslog(LOG_ERR, log_msg);
				}
			}
		}
	} else {
		sprintf(log_msg, "Error while opening directory: %s\n", cwd);
		syslog(LOG_ERR, log_msg);
	}
}

static void filewatch_daemon(char* directory)
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */

    if(chdir(directory) != 0) {
		perror("Error while accessing directory.");
		exit(EXIT_FAILURE);
	}

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
        close (x);
    }

    /* Open the log file */
	char proc_name[strlen(directory) + 18];
	sprintf(proc_name, "filewatch_daemon:%s", directory);
    openlog(proc_name, LOG_PID, LOG_DAEMON);
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		puts("Not enough arguments.  Execute like this:\n\t./filewatch_daemon <directory_to_watch>\n");
		return EXIT_FAILURE;
	}
	
	char watch_dir[PATH_MAX];
	if(rel_to_abs(argv[1], watch_dir, PATH_MAX) != 0) {
		perror("Something went wrong converting relative path to absolute path.");
		return EXIT_FAILURE;
	}

	filewatch_daemon(watch_dir);
	char startup_msg[300];
	sprintf(startup_msg, "filewatch_daemon started. watching: %s", watch_dir);
	syslog(LOG_NOTICE, startup_msg);

    while (1)
    {
		remove_old_files();
        sleep(CHECK_INTERVAL);
    }

    syslog(LOG_NOTICE, "filewatch_daemon shutting down.");
    closelog();

    return EXIT_SUCCESS;
}
