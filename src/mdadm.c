/* Handle MD arrays
 *
 * Copyright (c) 2016-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <glob.h>
#include <stdio.h>
#include <string.h>

#include "helpers.h"


static glob_t *get_arrays(void)
{
	static glob_t arrays;

	glob("/sys/block/md*", 0, NULL, &arrays);
	if (arrays.gl_pathc > 0)
		return &arrays;

	return NULL;
}

/*
 * If system has an MD raid, we must tell it to stop before continuing
 * with the shutdown.  Some controller cards, in particular the Intel(R)
 * Matrix Storage Manager, must be properly notified.
 */
void mdadm_wait(void)
{
	size_t i;
	glob_t *gl;
	char cmd[160];

	/* Setup kernel specific settings, e.g. allow broadcast ping, etc. */
	gl = get_arrays();
	if (gl) {
		for (i = 0; i < gl->gl_pathc; i++) {
			char *array;

			array = basename(gl->gl_pathv[i]);
			snprintf(cmd, sizeof(cmd), "mdadm --wait-clean /dev/%s >/dev/null", array);
			run_interactive(cmd, "Marking MD array %s as clean", array);
		}
		globfree(gl);
	}
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
