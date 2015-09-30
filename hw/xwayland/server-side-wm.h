/*
 * server-side-wm.h
 *
 *  Created on: 29 sept. 2015
 *      Author: gschwind
 */

#ifndef HW_XWAYLAND_SERVER_SIDE_WM_H_
#define HW_XWAYLAND_SERVER_SIDE_WM_H_

/**
 * This is the server side WM API
 *
 * If you want include header here, you are wrong. put them into
 * server-side-wm.c or client-side-wm.c as needed.
 *
 * This file is used by client-side-wm.c to make server request
 *
 **/

#include <stdint.h>

typedef int Bool;
#define True 1
#define False 0

struct xwl_screen;
struct xwl_window;
struct window_manager;
enum theme_location;

enum cursor_type {
	XWM_CURSOR_TOP,
	XWM_CURSOR_BOTTOM,
	XWM_CURSOR_LEFT,
	XWM_CURSOR_RIGHT,
	XWM_CURSOR_TOP_LEFT,
	XWM_CURSOR_TOP_RIGHT,
	XWM_CURSOR_BOTTOM_LEFT,
	XWM_CURSOR_BOTTOM_RIGHT,
	XWM_CURSOR_LEFT_PTR,
};

void register_wm_callback(struct window_manager * wm);
void window_manager_get_resources(struct xwl_screen *wm);
void window_manager_get_visual_and_colormap(struct xwl_screen *wm);
void window_manager_window_set_wm_state(struct xwl_window *window, int32_t state);
void window_manager_window_set_net_wm_state(struct xwl_window *window);
void window_manager_window_set_virtual_desktop(struct xwl_window *window, int desktop);
void window_manager_window_read_properties(struct xwl_window *window);
void send_wm_delete_window(struct xwl_window * xwl_window);
void xwl_window_draw_decoration(struct xwl_window *xwl_window);
int get_cursor_for_location(enum theme_location location);

#endif /* HW_XWAYLAND_SERVER_SIDE_WM_H_ */
