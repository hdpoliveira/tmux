/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <getopt.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Swap one window with another.
 */

int	cmd_swap_window_parse(void **, int, char **, char **);
void	cmd_swap_window_exec(void *, struct cmd_ctx *);
void	cmd_swap_window_send(void *, struct buffer *);
void	cmd_swap_window_recv(void **, struct buffer *);
void	cmd_swap_window_free(void *);

struct cmd_swap_window_data {
	int	 dstidx;
	int	 srcidx;
	char	*srcname;
	int	 flag_detached;
};

const struct cmd_entry cmd_swap_window_entry = {
	"swap-window", "swapw", "[-i index] name index",
	CMD_NOCLIENT,
	cmd_swap_window_parse,
	cmd_swap_window_exec, 
	cmd_swap_window_send,
	cmd_swap_window_recv,
	cmd_swap_window_free
};

int
cmd_swap_window_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_swap_window_data	*data;
	const char			*errstr;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->flag_detached = 0;
	data->dstidx = -1;
	data->srcidx = -1;
	data->srcname = NULL;

	while ((opt = getopt(argc, argv, "di:")) != EOF) {
		switch (opt) {
		case 'i':
			data->dstidx = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "index %s", errstr);
				goto error;
			}
			break;
		case 'd':
			data->flag_detached = 1;
			break;
		default:
			goto usage;
		}
	}	
	argc -= optind;
	argv += optind;
	if (argc != 2)
		goto usage;

	data->srcname = xstrdup(argv[0]);
	data->srcidx = strtonum(argv[1], 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		xasprintf(cause, "index %s", errstr);
		goto error;
	}

	return (0);

usage:
	usage(cause, "%s %s",
	    cmd_swap_window_entry.name, cmd_swap_window_entry.usage);

error:
	cmd_swap_window_free(data);
	return (-1);
}

void
cmd_swap_window_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_swap_window_data	*data = ptr;
	struct session			*dst = ctx->session, *src;
	struct winlink			*srcwl, *dstwl;
	struct window			*w;

	if (data == NULL)
		return;
	
	if ((src = session_find(data->srcname)) == NULL) {
		ctx->error(ctx, "session not found: %s", data->srcname);
		return;
	}

	if (data->srcidx < 0)
		data->srcidx = -1;
	if (data->srcidx == -1)
		srcwl = src->curw;
	else {
		srcwl = winlink_find_by_index(&src->windows, data->srcidx);
		if (srcwl == NULL) {
			ctx->error(ctx, "no window %d", data->srcidx);
			return;
		}
	}

	if (data->dstidx < 0)
		data->dstidx = -1;
	if (data->dstidx == -1)
		dstwl = dst->curw;
	else {
		dstwl = winlink_find_by_index(&dst->windows, data->dstidx);
		if (dstwl == NULL) {
			ctx->error(ctx, "no window %d", data->dstidx);
			return;
		}
	}

	w = dstwl->window;
	dstwl->window = srcwl->window;
	srcwl->window = w;

	if (!data->flag_detached) {
		session_select(dst, dstwl->idx);
		if (src != dst)
			session_select(src, srcwl->idx);
	}
	server_redraw_session(src);
	if (src != dst)
		server_redraw_session(dst);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_swap_window_send(void *ptr, struct buffer *b)
{
	struct cmd_swap_window_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->srcname);
}

void
cmd_swap_window_recv(void **ptr, struct buffer *b)
{
	struct cmd_swap_window_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->srcname = cmd_recv_string(b);
}

void
cmd_swap_window_free(void *ptr)
{
	struct cmd_swap_window_data	*data = ptr;

	if (data->srcname != NULL)
		xfree(data->srcname);
	xfree(data);
}