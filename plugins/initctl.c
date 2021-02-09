/* Optional /dev/initctl FIFO monitor, for receiving commands from telinit
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"		/* Generated by configure script */

#include <errno.h>
#include <fcntl.h>		/* O_RDONLY et al */
#include <signal.h>
#include <string.h>
#include <unistd.h>		/* read() */
#include <sys/stat.h>
#include <lite/lite.h>

#include "finit.h"
#include "conf.h"
#include "helpers.h"
#include "plugin.h"
#include "sig.h"
#include "service.h"

static void parse(void *arg, int fd, int events);
static void setup(void *arg);

static plugin_t plugin = {
	.name = __FILE__,
	.io = {
		.cb    = parse,
		.flags = PLUGIN_IO_READ,
	},
	.hook[HOOK_BASEFS_UP] = {
		.cb    = setup
	},
	.depends = { "bootmisc", },
};

static void fifo_open(void)
{
	plugin.io.fd = open(FINIT_FIFO, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (-1 == plugin.io.fd) {
		_e("Failed opening %s FIFO, error %d: %s", FINIT_FIFO, errno, strerror(errno));
		return;
	}
}

/*
 * We only handle INIT_HALT for differentiating between halt/poweroff
 */
static void set_env(char *data)
{
	const char *var = "INIT_HALT";

	if (strncmp(data, var, strlen(var)))
		return;
	data += strlen(var);

	/*
	 * No value means unset env, in this case cancel shutdown.
	 */
	if (data[0] == 0) {
		halt = SHUT_DEFAULT;
		/* XXX: Cancel timer here, when we get support for timed shutdown */
		return;
	}

	if (string_match(data, "=POWERDOWN"))
		halt = SHUT_OFF;
	if (string_match(data, "=HALT"))
		halt = SHUT_HALT;

	/* Now we wait for runlevel change, or cancelled shutdown */
}

/*
 * Standard reboot/shutdown utilities talk to init using /run/initctl.
 * We should check if the fifo was recreated and reopen it.
 *
 * For SysV compatibility the default is to halt the system when issuing
 * `init 0`, unless INIT_HALT=POWERDOWN, as performed by the SysV utils.
 */
static void parse(void *arg, int fd, int events)
{
	int lvl;
	struct init_request rq;

	while (1) {
		ssize_t len = read(fd, &rq, sizeof(rq));

		if (len <= 0) {
			if (-1 == len) {
				if (EINTR == errno)
					continue;

				if (EAGAIN == errno)
					break;

				_e("Failed reading initctl request, error %d: %s", errno, strerror(errno));
			}

			_d("Nothing to do, bailing out.");
			break;
		}

		if (rq.magic != INIT_MAGIC || len != sizeof(rq)) {
			_e("Invalid initctl request.");
			break;
		}

		switch (rq.cmd) {
		case INIT_CMD_RUNLVL:
			switch (rq.runlevel) {
			case 's':
			case 'S':
				rq.runlevel = '1'; /* Single user mode */
				/* fallthrough */

			case '0'...'9':
				_d("Setting new runlevel %c", rq.runlevel);
				lvl = rq.runlevel - '0';
				if (lvl == 6)
					halt = SHUT_REBOOT;
				service_runlevel(lvl);
				break;

			default:
				_d("Unsupported runlevel: %d", rq.runlevel);
				break;
			}
			break;

		case INIT_CMD_RELOAD:
			service_reload_dynamic();
			break;

		case INIT_CMD_SETENV:
			set_env(rq.data);
			break;

		default:
			_d("Unsupported cmd: %d", rq.cmd);
			break;
		}
	}

	close(fd);
	fifo_open();
}

/* Must run after the base FS is up, needs /run, or /var/run */
static void setup(void *arg)
{
	_d("Setting up %s", FINIT_FIFO);
	makefifo(FINIT_FIFO, 0600);

	fifo_open();
	plugin_io_init(&plugin);
}

PLUGIN_INIT(plugin_init)
{
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
