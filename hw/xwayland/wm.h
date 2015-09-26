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

#include "cairo-util.h"

/**
 * Compile-time computation of number of items in a hardcoded array.
 *
 * @param a the array being measured.
 * @return the number of items hardcoded into the array.
 */
#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

struct window_manager {
	ScreenPtr screen;
	uint32_t lock_count;
	pthread_t proc;
	Bool running;

	char * display;
	int fd_display;
	xcb_connection_t *conn;
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


#endif /* HW_XWAYLAND_WM_H_ */
