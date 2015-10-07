/*
 * Copyright © 2015 Benoit Gschwind <gschwind@gnu-log.net>
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


#ifndef HW_XWAYLAND_XWL_EVENT_H_
#define HW_XWAYLAND_XWL_EVENT_H_

#include <dix-config.h>
#include "window.h"

#include <pthread.h>
#include "list.h"
/**
 * This implement an event internal event queue
 **/

#define XWL_EVENT_MAP              1
#define XWL_EVENT_UNMAP            2
#define XWL_EVENT_FAKE_UNMAP       3
#define XWL_EVENT_PROPERTY_CHANGE  4

#define DEFAULT_POOL_SIZE 1024


struct xwl_event {
	int type;
	struct xorg_list link;
	union {
		struct {
			Window window;
		} map;
		struct {
			struct xwl_window *window;
		} unmap;
		struct {
			Window window;
		} fake_unmap;
		struct {
			Window window;
		} property_change;
	} data;
};


struct xwl_event_queue {
	pthread_mutex_t lock;
	struct xorg_list queue;
};

struct xwl_event * xwl_event_new(void);
void xwl_event_free(struct xwl_event * e);

struct xwl_event_queue * xwl_event_queue_new(void);
int xwl_event_queue_init(struct xwl_event_queue * eq);
void xwl_event_queue_append(struct xwl_event_queue * eq, struct xwl_event * e);
struct xwl_event * xwl_event_queue_pop(struct xwl_event_queue * eq);


#endif /* HW_XWAYLAND_XWL_EVENT_H_ */
