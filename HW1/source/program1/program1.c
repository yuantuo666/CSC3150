#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

void signal_handler()
{
	printf("Parent process receives SIGCHLD signal\n");
}

char *map_status(int status)
{
	// from man 7 signal
	// Signal        x86/ARM     Alpha/   MIPS   PARISC   Notes
	//                most others   SPARC
	//    ─────────────────────────────────────────────────────────────────
	//    SIGHUP           1           1       1       1
	//    SIGINT           2           2       2       2
	//    SIGQUIT          3           3       3       3
	//    SIGILL           4           4       4       4
	//    SIGTRAP          5           5       5       5
	//    SIGABRT          6           6       6       6
	//    SIGIOT           6           6       6       6
	//    SIGBUS           7          10      10      10
	//    SIGEMT           -           7       7      -
	//    SIGFPE           8           8       8       8
	//    SIGKILL          9           9       9       9
	//    SIGUSR1         10          30      16      16
	//    SIGSEGV         11          11      11      11
	//    SIGUSR2         12          31      17      17
	//    SIGPIPE         13          13      13      13
	//    SIGALRM         14          14      14      14
	//    SIGTERM         15          15      15      15
	//    SIGSTKFLT       16          -       -        7

	//    SIGCHLD         17          20      18      18
	//    SIGCLD           -          -       18      -
	//    SIGCONT         18          19      25      26
	//    SIGSTOP         19          17      23      24
	//    SIGTSTP         20          18      24      25
	//    SIGTTIN         21          21      26      27
	//    SIGTTOU         22          22      27      28
	//    SIGURG          23          16      21      29
	//    SIGXCPU         24          24      30      12
	//    SIGXFSZ         25          25      31      30
	//    SIGVTALRM       26          26      28      20
	//    SIGPROF         27          27      29      21
	//    SIGWINCH        28          28      20      23
	//    SIGIO           29          23      22      22
	//    SIGPOLL                                            Same as SIGIO
	//    SIGPWR          30         29/-     19      19
	//    SIGINFO          -         29/-     -       -
	//    SIGLOST          -         -/29     -       -
	//    SIGSYS          31          12      12      31
	//    SIGUNUSED       31          -       -       31

	char *status_str;
	switch (status)
	{
	case SIGHUP:
		status_str = "SIGHUP";
		break;
	case SIGINT:
		status_str = "SIGINT";
		break;
	case SIGQUIT:
		status_str = "SIGQUIT";
		break;
	case SIGILL:
		status_str = "SIGILL";
		break;
	case SIGTRAP:
		status_str = "SIGTRAP";
		break;
	case SIGABRT:
		status_str = "SIGABRT";
		break;
	case SIGBUS:
		status_str = "SIGBUS";
		break;
	case SIGFPE:
		status_str = "SIGFPE";
		break;
	case SIGKILL:
		status_str = "SIGKILL";
		break;
	case SIGUSR1:
		status_str = "SIGUSR1";
		break;
	case SIGSEGV:
		status_str = "SIGSEGV";
		break;
	case SIGUSR2:
		status_str = "SIGUSR2";
		break;
	case SIGPIPE:
		status_str = "SIGPIPE";
		break;
	case SIGALRM:
		status_str = "SIGALRM";
		break;
	case SIGTERM:
		status_str = "SIGTERM";
		break;
	case SIGSTKFLT:
		status_str = "SIGSTKFLT";
		break;
	case SIGCHLD:
		status_str = "SIGCHLD";
		break;
	case SIGCONT:
		status_str = "SIGCONT";
		break;
	case SIGSTOP:
		status_str = "SIGSTOP";
		break;
	case SIGTSTP:
		status_str = "SIGTSTP";
		break;
	case SIGTTIN:
		status_str = "SIGTTIN";
		break;
	case SIGTTOU:
		status_str = "SIGTTOU";
		break;
	case SIGURG:
		status_str = "SIGURG";
		break;
	case SIGXCPU:
		status_str = "SIGXCPU";
		break;
	case SIGXFSZ:
		status_str = "SIGXFSZ";
		break;
	case SIGVTALRM:
		status_str = "SIGVTALRM";
		break;
	case SIGPROF:
		status_str = "SIGPROF";
		break;
	case SIGWINCH:
		status_str = "SIGWINCH";
		break;
	case SIGIO:
		status_str = "SIGIO";
		break;
	case SIGPWR:
		status_str = "SIGPWR";
		break;
	case SIGSYS:
		status_str = "SIGSYS";
		break;
	default:
		status_str = "UNKNOWN";
		break;
	}

	return status_str;
}

int main(int argc, char *argv[])
{
	signal(SIGCHLD, signal_handler);

	/* fork a child process */
	printf("Process start to fork\n");
	int pid = fork();

	/* execute test program */
	if (pid == 0)
	{
		/* child process */
		printf("I'm the Child Process, my pid = %d\n", getpid());
		printf("Child process start to execute test program: \n");
		execl(argv[1], argv[1], NULL);
	}
	else
	{
		printf("I'm the Parent Process, my pid = %d\n", getpid());
	}

	/* wait for child process terminates */
	int status;
	while (1)
	{
		pid_t result = waitpid(pid, &status, WUNTRACED | WCONTINUED); // save the termination status of child process

		if (result == -1)
		{
			printf("waitpid error\n");
			exit(EXIT_FAILURE);
		}

		/* check child process'  termination status */

		if (WIFEXITED(status))
		{
			printf("Normal termination with EXIT STATUS = %d\n", WEXITSTATUS(status));
			break;
		}

		if (WIFSIGNALED(status))
		{
			char *status_str = map_status(WTERMSIG(status));
			if (status_str == NULL)
				status_str = "UNKNOWN";
			printf("child process get %s signal\n", status_str);
			break;
		}

		if (WIFSTOPPED(status))
		{
			char *status_str = map_status(WSTOPSIG(status));
			if (status_str == NULL)
				status_str = "UNKNOWN";
			printf("child process get %s signal\n", status_str);
			break;
		}
	}
}
