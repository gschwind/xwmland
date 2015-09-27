/*
 * wm.h
 *
 *  Created on: 26 sept. 2015
 *      Author: gschwind
 */

#ifndef HW_XWAYLAND_WM_H_
#define HW_XWAYLAND_WM_H_

#include <X11/X.h>
#include <X11/Xatom.h>
#define GC XlibGC
#include <X11/Xlib.h>
#include <X11/Xcursor/Xcursor.h>
#undef GC

#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/composite.h>
#include <cairo/cairo-xcb.h>

#include <dix-config.h>
#include <dix.h>

#include "xwl_window.h"
#include "cairo-util.h"
#include "xwayland.h"

/**
 * Compile-time computation of number of items in a hardcoded array.
 *
 * @param a the array being measured.
 * @return the number of items hardcoded into the array.
 */
#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif



#define USPosition	(1L << 0)
#define USSize		(1L << 1)
#define PPosition	(1L << 2)
#define PSize		(1L << 3)
#define PMinSize	(1L << 4)
#define PMaxSize	(1L << 5)
#define PResizeInc	(1L << 6)
#define PAspect		(1L << 7)
#define PBaseSize	(1L << 8)
#define PWinGravity	(1L << 9)


#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_DECOR_EVERYTHING \
	(MWM_DECOR_BORDER | MWM_DECOR_RESIZEH | MWM_DECOR_TITLE | \
	 MWM_DECOR_MENU | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW      (1L<<0)

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */


struct window_manager {
	ScreenPtr screen;
	uint32_t lock_count;
	Bool running;

	pthread_t init_conn_thread;
	pthread_mutex_t init_conn;
	int fd[2];

	char * display;
	int fd_display;
	xcb_connection_t *conn;
	XID identity_window;
	const xcb_query_extension_reply_t *xfixes;
	//struct wl_event_source *source;
	//xcb_screen_t *screen;
	struct hash_table *window_hash;
	//struct weston_xserver *server;
	xcb_window_t wm_window;
	struct weston_wm_window *focus_window;
	struct theme *theme;
	xcb_cursor_t *cursors;
	int last_cursor;
	//xcb_render_pictforminfo_t format_rgb, format_rgba;
	VisualPtr visual;
	Colormap colormap_id;
	ColormapPtr colormap;
//	struct wl_listener create_surface_listener;
//	struct wl_listener activate_listener;
//	struct wl_listener transform_listener;
//	struct wl_listener kill_listener;
//	struct wl_list unpaired_window_list;

	Window selection_window;
	Window selection_owner;
	int incr;
	int data_source_fd;
	struct wl_event_source *property_source;
	xcb_get_property_reply_t *property_reply;
	int property_start;
//	struct wl_array source_data;
	xcb_selection_request_event_t selection_request;
	Atom selection_target;
//	TimeStamp selection_timestamp;
	int selection_property_set;
	int flush_property_on_delete;
//	struct wl_listener selection_listener;

	Window dnd_window;
	Window dnd_owner;

	/* temporary region */
	xcb_xfixes_region_t region;

	struct {
		Atom		 wm_protocols;
		Atom		 wm_normal_hints;
		Atom		 wm_take_focus;
		Atom		 wm_delete_window;
		Atom		 wm_state;
		Atom		 wm_s0;
		Atom		 wm_client_machine;
		Atom		 net_wm_cm_s0;
		Atom		 net_wm_name;
		Atom		 net_wm_pid;
		Atom		 net_wm_icon;
		Atom		 net_wm_state;
		Atom		 net_wm_state_maximized_vert;
		Atom		 net_wm_state_maximized_horz;
		Atom		 net_wm_state_fullscreen;
		Atom		 net_wm_user_time;
		Atom		 net_wm_icon_name;
		Atom		 net_wm_desktop;
		Atom		 net_wm_window_type;
		Atom		 net_wm_window_type_desktop;
		Atom		 net_wm_window_type_dock;
		Atom		 net_wm_window_type_toolbar;
		Atom		 net_wm_window_type_menu;
		Atom		 net_wm_window_type_utility;
		Atom		 net_wm_window_type_splash;
		Atom		 net_wm_window_type_dialog;
		Atom		 net_wm_window_type_dropdown;
		Atom		 net_wm_window_type_popup;
		Atom		 net_wm_window_type_tooltip;
		Atom		 net_wm_window_type_notification;
		Atom		 net_wm_window_type_combo;
		Atom		 net_wm_window_type_dnd;
		Atom		 net_wm_window_type_normal;
		Atom		 net_wm_moveresize;
		Atom		 net_supporting_wm_check;
		Atom		 net_supported;
		Atom		 motif_wm_hints;
		Atom		 clipboard;
		Atom		 clipboard_manager;
		Atom		 targets;
		Atom		 utf8_string;
		Atom		 wl_selection;
		Atom		 incr;
		Atom		 timestamp;
		Atom		 multiple;
		Atom		 compound_text;
		Atom		 text;
		Atom		 string;
		Atom		 text_plain_utf8;
		Atom		 text_plain;
		Atom		 xdnd_selection;
		Atom		 xdnd_aware;
		Atom		 xdnd_enter;
		Atom		 xdnd_leave;
		Atom		 xdnd_drop;
		Atom		 xdnd_status;
		Atom		 xdnd_finished;
		Atom		 xdnd_type_list;
		Atom		 xdnd_action_copy;
		Atom		 wl_surface_id;
	} atom;
};

struct window_manager *
window_manager_create(ScreenPtr screen);

void
window_manager_window_read_properties(struct xwl_window *window);

#define ICCCM_WITHDRAWN_STATE	0
#define ICCCM_NORMAL_STATE	1
#define ICCCM_ICONIC_STATE	3

void
window_manager_window_set_wm_state(struct xwl_window *window, int32_t state);

void
window_manager_window_set_net_wm_state(struct xwl_window *window);

void
window_manager_window_set_virtual_desktop(struct xwl_window *window,
				     int desktop);

void
window_manager_get_visual_and_colormap(struct window_manager *wm);

void
window_manager_window_draw_decoration(struct xwl_window *xwl_window, PixmapPtr pixmap);

#endif /* HW_XWAYLAND_WM_H_ */
