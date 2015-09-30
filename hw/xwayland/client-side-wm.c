/*
 * Copyright © 2011 Intel Corporation
 * Copyright © 2015 Benoit Gschwind <gschwind@gnu-log.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 *
 * This part of the code is about the WM helper. The helper listen
 * events from the server.
 *
 * If you include server header here and/or use server call YOU are wrong!
 * Use the server side to implement a wrapper.
 *
 **/

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "client-side-wm.h"
#include "server-side-wm.h"

#include "cairo-util.h"

#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/composite.h>
#include <cairo/cairo.h>

struct window_manager {
	pthread_mutex_t init_conn;
	pthread_t init_conn_thread;
	int fd[2];
	xcb_connection_t *conn;
	xcb_window_t identity_window;
	xcb_xfixes_region_t region;
};

pthread_mutex_t * window_manager_init_conn(struct window_manager * wm) {
	return &wm->init_conn;
}

pthread_t * window_manager_init_conn_thread(struct window_manager * wm) {
	return &wm->init_conn_thread;
}

int window_manager_client_fd(struct window_manager * wm) {
	return wm->fd[1];
}


//struct window_manager {
//	ScreenPtr screen;
//	uint32_t lock_count;
//	Bool running;
//
//	pthread_t init_conn_thread;
//	pthread_mutex_t init_conn;
//	int fd[2];
//
//	char * display;
//	int fd_display;
//	xcb_connection_t *conn;
//	XID identity_window;
//	const xcb_query_extension_reply_t *xfixes;
//	//struct wl_event_source *source;
//	//xcb_screen_t *screen;
//	struct hash_table *window_hash;
//	//struct weston_xserver *server;
//	xcb_window_t wm_window;
//	struct weston_wm_window *focus_window;
//	struct theme *theme;
//	xcb_cursor_t *cursors;
//	int last_cursor;
//	//xcb_render_pictforminfo_t format_rgb, format_rgba;
//
////	struct wl_listener create_surface_listener;
////	struct wl_listener activate_listener;
////	struct wl_listener transform_listener;
////	struct wl_listener kill_listener;
////	struct wl_list unpaired_window_list;
//
//	Window selection_window;
//	Window selection_owner;
//	int incr;
//	int data_source_fd;
//	struct wl_event_source *property_source;
//	xcb_get_property_reply_t *property_reply;
//	int property_start;
////	struct wl_array source_data;
//	xcb_selection_request_event_t selection_request;
//	Atom selection_target;
////	TimeStamp selection_timestamp;
//	int selection_property_set;
//	int flush_property_on_delete;
////	struct wl_listener selection_listener;
//
//	Window dnd_window;
//	Window dnd_owner;
//
//	/* temporary region */
//	xcb_xfixes_region_t region;
//
//
//};

//static void
//window_manager_window_send_damage(struct xwl_window *window) {
//	int width, height;
//	xcb_rectangle_t rec;
//
//	width = frame_width(window->frame);
//	height = frame_width(window->frame);
//
//	rec.x = 0;
//	rec.y = 0;
//	rec.width = width;
//	rec.height = height;
//	xcb_xfixes_create_region(window->xwl_screen->wm->conn, window->xwl_screen->wm->region, 1, &rec);
//	xcb_damage_add(window->xwl_screen->wm->conn, window->frame_window->drawable.id, window->xwl_screen->wm->region);
//	xcb_xfixes_destroy_region(window->xwl_screen->wm->conn, window->xwl_screen->wm->region);
//	xcb_flush(window->xwl_screen->wm->conn);
//	LogWrite(0, "send damage (%d) (%d,%d,%d,%d)\n", window->frame_window->drawable.id, rec.x, rec.y, rec.width, rec.height);
//
//}

int window_manager_identity_window(struct window_manager * wm) {
	return wm->identity_window;
}

static void
window_manager_handle_map_notify(struct window_manager *wm, xcb_generic_event_t *event)
{
	xcb_map_notify_event_t * map = (xcb_map_notify_event_t *) event;
	uint32_t values;
	printf("XCB_MAP_NOTIFY (window %d)\n", map->window);
	values = XCB_EVENT_MASK_PROPERTY_CHANGE|XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_change_window_attributes(wm->conn, map->window, XCB_CW_EVENT_MASK, &values);

}

/**
 * Return a pointer to the frame painted
 */
char *
window_manager_window_draw_frame(struct frame * frame) {
	int width = frame_width(frame);
	int height = frame_height(frame);
	char * data = malloc(sizeof(uint32_t)*width*height);
	cairo_surface_t * sbuff = cairo_image_surface_create_for_data((uint8_t*)data, CAIRO_FORMAT_ARGB32, width, height, width*4);
	cairo_t * cr = cairo_create(sbuff);
//	cairo_save(cr);
//	cairo_set_source_rgb(cr, 0, 0, 0);
//	cairo_paint(cr);
//	cairo_restore(cr);
	frame_repaint(frame, cr);
	cairo_destroy(cr);
	cairo_surface_flush(sbuff);
	cairo_surface_destroy(sbuff);
	return data;
}

static void
window_manager_handle_configure_notify(struct window_manager *wm, xcb_generic_event_t *event) {
	struct xcb_configure_notify_event_t * configure_notify = (struct xcb_configure_notify_event_t*)event;
	struct xwl_window * window;
	printf("XCB_CONFIGURE_NOTIFY (window %d) %d,%d @ %dx%d\n",
	       configure_notify->window,
	       configure_notify->x, configure_notify->y,
	       configure_notify->width, configure_notify->height);

//	window = hash_table_lookup(xwl_screen_get(wm->screen)->window_hash, configure_notify->window);
//	if(!window)
//		return;
//
//	if(window->window->drawable.id != configure_notify->event)
//		return;

}


static void
window_manager_handle_event(struct window_manager *wm)
{
	xcb_generic_event_t *event;
	while ((event = xcb_poll_for_event(wm->conn)) != 0) {
//		if (weston_wm_handle_selection_event(wm, event)) {
//			free(event);
//			count++;
//			continue;
//		}
//
//		if (weston_wm_handle_dnd_event(wm, event)) {
//			free(event);
//			count++;
//			continue;
//		}
//
		switch (event->response_type&(~0x80)) {
//		case XCB_BUTTON_PRESS:
//		case XCB_BUTTON_RELEASE:
//			weston_wm_handle_button(wm, event);
//			break;
//		case XCB_ENTER_NOTIFY:
//			window_manager_handle_enter(wm, event);
//			break;
//		case XCB_LEAVE_NOTIFY:
//			window_manager_handle_leave(wm, event);
//			break;
//		case XCB_MOTION_NOTIFY:
//			weston_wm_handle_motion(wm, event);
//			break;
//		case XCB_CREATE_NOTIFY:
//			weston_wm_handle_create_notify(wm, event);
//			break;
//		case XCB_MAP_REQUEST:
//			weston_wm_handle_map_request(wm, event);
//			break;
		case XCB_MAP_NOTIFY:
			window_manager_handle_map_notify(wm, event);
			break;
//		case XCB_UNMAP_NOTIFY:
//			weston_wm_handle_unmap_notify(wm, event);
//			break;
//		case XCB_REPARENT_NOTIFY:
//			weston_wm_handle_reparent_notify(wm, event);
//			break;
//		case XCB_CONFIGURE_REQUEST:
//			weston_wm_handle_configure_request(wm, event);
//			break;
		case XCB_CONFIGURE_NOTIFY:
			window_manager_handle_configure_notify(wm, event);
			break;
//		case XCB_DESTROY_NOTIFY:
//			weston_wm_handle_destroy_notify(wm, event);
//			break;
//		case XCB_MAPPING_NOTIFY:
//			LogWrite(0, "XCB_MAPPING_NOTIFY\n");
//			break;
//		case XCB_PROPERTY_NOTIFY:
//			weston_wm_handle_property_notify(wm, event);
//			break;
//		case XCB_CLIENT_MESSAGE:
//			weston_wm_handle_client_message(wm, event);
//			break;
//		case XCB_FOCUS_IN:
//			weston_wm_handle_focus_in(wm, event);
//			break;
		case 0:
			printf("Error ocurrent\n");
			break;
		}

		xcb_flush(wm->conn);
		free(event);
	}

}

void window_manager_wakeup_handler(void *data, int err, void *read_mask)
{
	struct window_manager *wm = data;

    if (err < 0)
        return;

    window_manager_handle_event(wm);

}

static xcb_atom_t
get_atom(struct window_manager *wm, char const * name) {
	xcb_intern_atom_cookie_t ck = xcb_intern_atom(wm->conn, 0, strlen(name), name);
	xcb_generic_error_t * err;
	xcb_intern_atom_reply_t * reply = xcb_intern_atom_reply(wm->conn, ck, &err);
	if(reply) {
		xcb_atom_t ret = reply->atom;
		free(reply);
		return ret;
	} else {
		xcb_discard_reply(wm->conn, ck.sequence);
	}
	return XCB_NONE;
}

static
void * window_manager_init_conn_func(void * data) {
	struct window_manager *wm = data;
	uint32_t values = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_screen_iterator_t s;
	xcb_screen_t * screen;
	int err;
	char const * name = "page";
	char wm_sn[] = "WM_S0";
	char net_wm_cm[] = "_NET_WM_CM_S0";
	int pid = getpid();
	xcb_atom_t wm_sn_atom;
	xcb_atom_t cm_sn_atom;
	uint32_t attrs[2];

	wm->conn = xcb_connect_to_fd(wm->fd[0], NULL);
	if((err = xcb_connection_has_error(wm->conn)) > 0) {
		printf("Error %d\n", err);
	}
	s = xcb_setup_roots_iterator(xcb_get_setup(wm->conn));
	screen = s.data;

	wm->region = xcb_generate_id(wm->conn);

	wm_sn_atom = get_atom(wm, wm_sn);
	cm_sn_atom = get_atom(wm, net_wm_cm);

	/* OVERRIDE_REDIRECT */
	attrs[0] = 1;
	attrs[1] = XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_PROPERTY_CHANGE;
	uint32_t attrs_mask = XCB_CW_OVERRIDE_REDIRECT|XCB_CW_EVENT_MASK;

	/* Warning: This window must be focusable, thus it MUST be an INPUT_OUTPUT window */
	xcb_window_t identity_window = xcb_generate_id(wm->conn);
	wm->identity_window = identity_window;
	xcb_create_window(wm->conn, XCB_COPY_FROM_PARENT, identity_window,
			screen->root, -100, -100, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
			XCB_COPY_FROM_PARENT, attrs_mask, attrs);

	xcb_change_property(wm->conn, XCB_PROP_MODE_REPLACE, identity_window, get_atom(wm, "_NET_WM_NAME"), get_atom(wm, "UTF8_STRING"), 8, strlen(name), name);
	xcb_change_property(wm->conn, XCB_PROP_MODE_REPLACE, identity_window, get_atom(wm, "_NET_SUPPORTING_WM_CHECK"), get_atom(wm, "WINDOW"), 32, 1, &identity_window);
	xcb_change_property(wm->conn, XCB_PROP_MODE_REPLACE, identity_window, get_atom(wm, "_NET_WM_PID"), get_atom(wm, "CARDINAL"), 32, 1, &pid);
	xcb_map_window(wm->conn, identity_window);

	/**
	 * we request to become the owner then we check if we successfully
	 * become the owner.
	 **/
	xcb_set_selection_owner(wm->conn, identity_window, wm_sn_atom, XCB_CURRENT_TIME);
	xcb_set_selection_owner(wm->conn, identity_window, cm_sn_atom, XCB_CURRENT_TIME);

	xcb_change_window_attributes(wm->conn, screen->root, XCB_CW_EVENT_MASK, &values);
	xcb_flush(wm->conn);
	pthread_mutex_unlock(&wm->init_conn);
	return NULL;
}


struct window_manager *
window_manager_create()
{
	struct window_manager *wm;

	wm = (struct window_manager *)calloc(1, sizeof *wm);
	if (wm == NULL)
		return NULL;

	if(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm->fd) < 0) {
		printf("Cannot pair socket\n");
	}

	pthread_mutex_init(&wm->init_conn, NULL);
	pthread_mutex_lock(&wm->init_conn);
	pthread_create(&wm->init_conn_thread, NULL, window_manager_init_conn_func, wm);
	pthread_detach(wm->init_conn_thread);

	register_wm_callback(wm);

	return wm;
}


