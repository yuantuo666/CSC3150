#include <linux/module.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/kmod.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");

extern struct filename *getname_kernel(const char *filename);
extern pid_t kernel_clone(struct kernel_clone_args *kargs);
extern long kernel_execve(const char *filename, const char *const *argv, const char *const *envp);
extern int kernel_wait(pid_t pid, int *stat);
extern void putname(struct filename *name);
extern void msleep(unsigned int msecs);

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

int my_exec(void)
{
	// • Print out the process id for both parent and child process. (5 points)
	pid_t pid = current->pid;
	printk("[program2] : The child process has pid = %d\n", pid);

	char *path = "/tmp/test";

	msleep(20);

	// • Within my_fork, fork a process to execute the test program. (10 points)
	/* execute a test program in child process */
	printk("[program2] : child process\n");

	struct filename *filename = getname_kernel(path);
	if (IS_ERR(filename))
	{
		printk(KERN_ERR "Failed to get filename\n");
		return PTR_ERR(filename);
	}

	const char *argv[] = {"test", NULL};
	const char *envp[] = {NULL};

	int return_code = kernel_execve(filename->name, argv, envp);

	putname(filename);

	if (return_code != 0)
		printk("Return code: %d\n", return_code);

	return return_code;
}

// implement fork function
int my_fork(void *argc)
{
	printk("[program2] : module_init kthread start\n");

	// set default sigaction for current process
	int i;
	struct k_sigaction *k_action = &current->sighand->action[0];
	for (i = 0; i < _NSIG; i++)
	{
		k_action->sa.sa_handler = SIG_DFL;
		k_action->sa.sa_flags = 0;
		k_action->sa.sa_restorer = NULL;
		sigemptyset(&k_action->sa.sa_mask);
		k_action++;
	}

	// • Within my_fork, fork a process to execute the test program. (10 points)
	/* fork a process using kernel_clone or kernel_thread */
	struct kernel_clone_args clone_args = {
		.flags = SIGCHLD,
		.pidfd = NULL,
		.child_tid = NULL,
		.parent_tid = NULL,
		.exit_signal = SIGCHLD,
		.stack = (unsigned long)&my_exec,
		.stack_size = 0,
		.tls = 0};

	int pid = kernel_clone(&clone_args);

	if (pid < 0)
	{
		printk("Error forking process\n");
		return -1;
	}
	else
	{
		// • The parent process will wait until child process terminates. (10 points)
		/* wait until child process terminates */

		msleep(10);

		// • Print out the process id for both parent and child process. (5 points)
		printk("[program2] : This is the parent process, pid = %d\n", current->pid);

		msleep(30);

		int status = 666;
		kernel_wait(pid, &status);

		// • Within this test program, it will raise signal. The signal could be caught
		//   and related message should be printed out in kernel log. (10 points)

		if ((status & 0x7f) == 0)
		{
			// normal termination
			int exit_code = (((status) & 0xff00) >> 8);
			char *status_str = map_status(exit_code);
			if (status_str == NULL)
				status_str = "UNKNOWN";
			printk("[program2] : get %s signal\n", status_str);
			printk("[program2] : child process terminated");
			printk("[program2] : The return signal is %d\n", exit_code);
		}

		if (status & 0x7f)
		{
			// terminated by signal
			int signal_number = status & 0x7f;
			char *status_str = map_status(signal_number);
			if (status_str == NULL)
				status_str = "UNKNOWN";
			printk("[program2] : get %s signal\n", status_str);

			if (status & 0x80)
			{
				printk("[program2] : Child produced a core dump");
			}

			printk("[program2] : child process terminated");
			printk("[program2] : The return signal is %d\n", signal_number);
		}

		if ((status & 0x7f) == 0x7f)
		{
			// stopped by signal
			int stop_signal = (status >> 8) & 0xff;
			char *status_str = map_status(stop_signal);
			if (status_str == NULL)
				status_str = "UNKNOWN";
			printk("[program2] : get %s signal\n", status_str);
			printk("[program2] : The return signal is %d\n", stop_signal);
		}
	}

	return 0;
}

static int __init program2_init(void)
{

	printk("[program2] : Module_init\n");

	/* write your code here */

	// • When program2.ko being initialized, create a kernel thread and run my_fork function. (10 points)
	/* create a kernel thread to run my_fork */
	struct task_struct *thread = kthread_create(&my_fork, NULL, "my_fork");
	printk("[program2] : module_init create kthread start\n");

	if (IS_ERR(thread))
	{
		printk("Error creating thread\n");
		return PTR_ERR(thread);
	}
	else
	{
		wake_up_process(thread);
	}

	return 0;
}

static void __exit program2_exit(void)
{
	printk("[program2] : Module_exit\n");
}

module_init(program2_init);
module_exit(program2_exit);
