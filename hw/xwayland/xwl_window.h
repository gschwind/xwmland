/*
 * xwl_window.h
 *
 *  Created on: 27 sept. 2015
 *      Author: gschwind
 */

#ifndef HW_XWAYLAND_XWL_WINDOW_H_
#define HW_XWAYLAND_XWL_WINDOW_H_

#include "list.h"
#include "window.h"
#include "damage.h"

struct xwl_screen;

struct wm_size_hints {
	uint32_t flags;
	int32_t x, y;
	int32_t width, height;	/* should set so old wm's don't mess up */
	int32_t min_width, min_height;
	int32_t max_width, max_height;
	int32_t width_inc, height_inc;
	struct {
		int32_t x;
		int32_t y;
	} min_aspect, max_aspect;
	int32_t base_width, base_height;
	int32_t win_gravity;
};

struct motif_wm_hints {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	int32_t input_mode;
	uint32_t status;
};

struct xwl_window {
	pthread_mutex_t lock;
    struct xwl_screen *xwl_screen;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct frame * frame;
    WindowPtr frame_window;

    struct xorg_list list_childdren;
    struct xorg_list link_sibling;

    WindowPtr client_window;
    DamagePtr damage;
    struct xorg_list link_damage;
    struct xorg_list link_window;
    struct xorg_list link_cleanup;
    struct wl_callback *frame_callback;

    int has_32b_visual;
    int layout_is_dirty;
	int properties_dirty;
	int pid;
	char *machine;
	char *class_;
	char *name;
	struct xwl_window *transient_for;
	struct xwl_window *below;
	uint32_t protocols;
	Atom type;
	int width, height;
	int x, y;
	int saved_width, saved_height;
	int decorate;
	int override_redirect;
	int fullscreen;
	int has_alpha;
	int delete_window;
	int maximized_vert;
	int maximized_horz;
	struct wm_size_hints size_hints;
	struct motif_wm_hints motif_hints;

};



#endif /* HW_XWAYLAND_XWL_WINDOW_H_ */
