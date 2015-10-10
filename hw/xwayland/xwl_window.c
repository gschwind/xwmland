/*
 * xwl_window.c
 *
 *  Created on: 7 oct. 2015
 *      Author: gschwind
 */

#include "xwl_window.h"

void xwl_window_update_set_flags(struct xwl_window *w, uint32_t flag) {
	w->state_flags |= flag;
}

void xwl_window_update_unset_flags(struct xwl_window *w, uint32_t flag) {
	w->state_flags &= ~flag;
}


