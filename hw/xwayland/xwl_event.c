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

#include "xwl_event.h"


struct xwl_event_pool {
	struct xorg_list link;
	struct xwl_event pool[1000];
};

static pthread_mutex_t pool_look = PTHREAD_MUTEX_INITIALIZER;
static struct xorg_list pool_event = {0, 0};
static struct xorg_list free_event = {0, 0};


struct xwl_event * xwl_event_new(void) {

	pthread_mutex_lock(&pool_look);

	if(!pool_event.next) {
		xorg_list_init(&pool_event);
		xorg_list_init(&free_event);
	}

	if(xorg_list_is_empty(&free_event)) {
		struct xwl_event_pool * p;
		int i;
		p = malloc(sizeof *p);
		if(!p) {
			pthread_mutex_unlock(&pool_look);
			return NULL;
		}
		for(i = 0; i < 1000; ++i) {
			xorg_list_init(&p->pool[i].link);
			xorg_list_append(&p->pool[i].link, &free_event);
		}
		xorg_list_append(&p->link, &pool_event);
	}

	if(!xorg_list_is_empty(&free_event)) {
		struct xwl_event * e =
				xorg_list_first_entry(&free_event, struct xwl_event, link);
		xorg_list_del(&e->link);
		pthread_mutex_unlock(&pool_look);
		return e;
	} else {
		pthread_mutex_unlock(&pool_look);
		return NULL;
	}

}

void xwl_event_free(struct xwl_event * e) {
	pthread_mutex_lock(&pool_look);
	xorg_list_add(&e->link, &free_event);
	pthread_mutex_unlock(&pool_look);
}

struct xwl_event_queue * xwl_event_queue_new(void) {
	struct xwl_event_queue * eq = NULL;
	eq = malloc(sizeof *eq);
	if(!eq) {
		return NULL;
	}

	if(xwl_event_queue_init(eq) != 0) {
		free(eq);
		return NULL;
	}

	return eq;

}

int xwl_event_queue_init(struct xwl_event_queue * eq) {
	struct xwl_event * pool = NULL;

	pool = malloc(sizeof(struct xwl_event)*DEFAULT_POOL_SIZE);
	if(!pool) {
		return -1;
	}

	pthread_mutex_init(&eq->lock, NULL);
	xorg_list_init(&eq->queue);

	return 0;
}

void xwl_event_queue_append(struct xwl_event_queue * eq, struct xwl_event * e) {
	pthread_mutex_lock(&eq->lock);
	xorg_list_append(&e->link, &eq->queue);
	pthread_mutex_unlock(&eq->lock);
}

struct xwl_event * xwl_event_queue_pop(struct xwl_event_queue * eq) {
	struct xwl_event * e = NULL;
	pthread_mutex_lock(&eq->lock);
	if(!xorg_list_is_empty(&eq->queue)) {
		e = xorg_list_first_entry(&eq->queue, struct xwl_event, link);
		xorg_list_del(&e->link);
	}
	pthread_mutex_unlock(&eq->lock);
	return e;
}

