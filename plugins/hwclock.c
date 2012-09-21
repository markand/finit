/* Save and restore RTC using hwclock
 *
 * Copyright (c) 2012  Joachim Nilsson <troglobit@gmail.com>
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

#include "finit.h"
#include "helpers.h"
#include "plugin.h"

static void save(void *UNUSED(arg))
{
	_d("Saving system clock to RTC ...");
	run_interactive("/sbin/hwclock --systohc --utc", "Saving system time (UTC) to RTC");
}

static void restore(void *UNUSED(arg))
{
	_d("Restoring system clock from RTC ...");
	run_interactive("/sbin/hwclock --hctosys --utc", "Restoring system clock (UTC) from RTC");
}

static plugin_t plugin = {
	.hook[HOOK_POST_SIGSETUP] = {
		.cb  = restore
	},
	.hook[HOOK_SHUTDOWN] = {
		.cb  = save
	}
};

static void init_plugin(void)
{
	plugin_register(&plugin);
}

static void exit_plugin(void)
{
	plugin_unregister(&plugin);
}

PLUGIN_INIT(init_plugin)
PLUGIN_EXIT(exit_plugin)

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
