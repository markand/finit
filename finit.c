/* Improved fast init
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <dirent.h>
#include <linux/fs.h>

#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "service.h"
#include "signal.h"

int   debug    = 0;
int   verbose  = 1;
char *sdown    = NULL;
char *network  = NULL;
char *startx   = NULL;
char *username = NULL;
char *hostname = NULL;


static void parse_kernel_cmdline(void)
{
	FILE *fp;
	char line[LINE_SIZE];

	if ((fp = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, sizeof(line), fp);

		if (strstr(line, "finit_debug") || strstr(line, "--debug"))
			debug = 1;

		if (!debug && strstr(line, "quiet"))
			verbose = 0;

		fclose(fp);
	}
}

static char *build_cmd(char *cmd, char *line, int len)
{
	int l;
	char *c;

	/* Trim leading whitespace */
	while (*line && (*line == ' ' || *line == '\t'))
		line++;

	if (!cmd) {
		cmd = malloc (strlen(line) + 1);
		if (!cmd) {
			_e("No memory left for '%s'", line);
			return NULL;
		}
		*cmd = 0;
	}
	c = cmd + strlen(cmd);
	for (l = 0; *line && *line != '#' && *line != '\t' && l < len; l++)
		*c++ = *line++;
	*c = 0;

	_d("cmd = %s", cmd);
	return cmd;
}

static void parse_finit_conf(char *file)
{
	FILE *fp;
	char line[LINE_SIZE];
	char cmd[CMD_SIZE];

	/* Default username and hostname */
	username = strdup(DEFUSER);
	hostname = strdup(DEFHOST);

	if ((fp = fopen(file, "r")) != NULL) {
		char *x;

		_d("Parse %s ...", file);
		while (!feof(fp)) {
			if (!fgets(line, sizeof(line), fp))
				continue;
			chomp(line);

			_d("conf: %s", line);

			/* Skip comments. */
			if (MATCH_CMD(line, "#", x)) {
				continue;
			}
			/* Do this before mounting / read-write */
			if (MATCH_CMD(line, "check ", x)) {
				strcpy(cmd, "/sbin/fsck -C -a ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "Checking file system integrity on %s", x);
				continue;
			}
			if (MATCH_CMD(line, "user ", x)) {
				if (username) free(username);
				username = build_cmd(NULL, x, USERNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "host ", x)) {
				if (hostname) free(hostname);
				hostname = build_cmd(NULL, x, HOSTNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "shutdown ", x)) {
				if (sdown) free(sdown);
				sdown = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "module ", x)) {
				strcpy(cmd, "/sbin/modprobe ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "   Loading module %s", x);
				continue;
			}
			if (MATCH_CMD(line, "mknod ", x)) {
				strcpy(cmd, "/bin/mknod ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "   Creating device node %s", x);
				continue;
			}
			if (MATCH_CMD(line, "network ", x)) {
				if (network) free(network);
				network = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "startx ", x)) {
				if (startx) free(startx);
				startx = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "service ", x)) {
				if (service_register (x))
					_e("Failed, too many services to monitor.\n");
				continue;
			}
		}
		fclose(fp);
	}
}

int main(int UNUSED(args), char *argv[])
{
	/*
	 * Initial setup of signals, ignore all until we're up.
	 */
	sig_init();

	/*
	 * Mount base file system, kernel is assumed to run devtmpfs for /dev
	 */
	chdir("/");
	umask(0);
	mount("none", "/proc", "proc", 0, NULL);
	mount("none", "/proc/bus/usb", "usbfs", 0, NULL);
	mount("none", "/sys", "sysfs", 0, NULL);
	mkdir("/dev/pts", 0755);
	mkdir("/dev/shm", 0755);
	mount("none", "/dev/pts", "devpts", 0, "gid=5,mode=620");
	mount("none", "/dev/shm", "tmpfs", 0, NULL);
	umask(022);

	/*
	 * Populate /dev and prepare for runtime events from kernel.
	 */
#if defined(USE_UDEV)
	run_interactive("udevd --daemon", "Populating device tree");
#elif defined (MDEV)
	run_interactive(MDEV "-s", "Populating device tree");
#endif

	/*
	 * Parse kernel parameters
	 */
	parse_kernel_cmdline();

	cls();
	echo("finit " VERSION " (built " __DATE__ " " __TIME__ " by " WHOAMI ")");

	_d("Loading plugins ...");
	load_plugins(PLUGIN_PATH);

	/*
	 * Parse configuration file
	 */
	parse_finit_conf(FINIT_CONF);

	/*
	 * Mount filesystems
	 */
	_d("Mount filesystems in /etc/fstab ...");

#ifdef REMOUNT_ROOTFS_RW
	run("/bin/mount -n -o remount,rw /");
#endif
#ifdef SYSROOT
	run(SYSROOT, "/", NULL, MS_MOVE, NULL);
#endif
	umask(0);
	run("/bin/mount -na");
	run("/sbin/swapon -ea");
	umask(0022);

	/* Cleanup stale files, if any still linger on. */
	run_interactive("rm -rf /tmp/* /var/run/* /var/lock/*", "Cleanup temporary directories");

	/*
	 * Base FS up, enable standard SysV init signals
	 */
	sig_setup();

	/*
	 * Most user-level hooks run here, unless they are service hooks
	 */
	run_hooks(HOOK_POST_SIGSETUP);

	/*
	 * Network stuff
	 */

	/* Setup kernel specific settings, e.g. allow broadcast ping, etc. */
	run("/sbin/sysctl -e -p /etc/sysctl.conf >/dev/null");

	/* Set initial hostname. */
	set_hostname(hostname);

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);
	if (network)
		run_interactive(network, "Starting networking: %s", network);
	umask(022);

	/*
	 * Hooks that rely on loopback, or basic networking being up.
	 */
	run_hooks(HOOK_POST_NETWORK);

	/*
	 * Start service monitor framework
	 */
	_d("Starting services from %s", FINIT_CONF);
	service_startup();

	/* Run startup scripts in /etc/finit.d/, if any. */
	run_parts(FINIT_RCSD);

#ifdef LISTEN_INITCTL
	listen_initctl();
#endif

	if (!fork()) {
		/* child process */
		int i;
		char c;
		sigset_t nmask;
		struct sigaction sa;

		vhangup();

		close(2);
		close(1);
		close(0);

		if (open(CONSOLE, O_RDWR) != 0)
			exit(1);

		sigemptyset(&sa.sa_mask);
		sa.sa_handler = SIG_DFL;

		sigemptyset(&nmask);
		sigaddset(&nmask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask, NULL);

		for (i = 1; i < NSIG; i++)
			sigaction(i, &sa, NULL);

		dup2(0, STDIN_FILENO);
		dup2(0, STDOUT_FILENO);
		dup2(0, STDERR_FILENO);

		set_procname(argv, "console");

		/* ConsoleKit needs this */
		setenv("DISPLAY", ":0", 1);

		while (!fexist(SYNC_SHUTDOWN)) {
			char line[LINE_SIZE];

			if (fexist(SYNC_STOPPED)) {
				sleep(1);
				continue;
			}

			if (startx && !debug) {
				echo("Starting X ...");
				snprintf(line, sizeof(line), "su -c '%s' -l %s", startx, username);
				system(line);
			} else {
				static const char msg[] = "\nPlease press Enter to activate this console. ";

				i = write(STDERR_FILENO, msg, sizeof(msg) - 1);
				while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n')
				continue;

				if (fexist(SYNC_STOPPED))
					continue;

				run(GETTY);
			}
		}

		exit(0);
	}

	/*
	 * Hooks that should run at the very end
	 */
	run_hooks(HOOK_PRE_RUNLOOP);

	/*
	 * Enter main service monitor loop -- restarts services that dies.
	 */
	service_monitor();

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
