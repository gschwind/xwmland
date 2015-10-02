/*
 * xwl_screen.h
 *
 *  Created on: 29 sept. 2015
 *      Author: gschwind
 */

#ifndef HW_XWAYLAND_XWL_SCREEN_H_
#define HW_XWAYLAND_XWL_SCREEN_H_

#include <dix-config.h>

#include "misc.h"
#include "list.h"
#include "xwm/hash.h"
#include "screenint.h"
#include "scrnintstr.h"
#include "window.h"

struct xwl_screen {
	int lock_count;

    int width;
    int height;
    int depth;
    ScreenPtr screen;
    WindowPtr pointer_limbo_window;
    int expecting_event;

    int realizing;

    int wm_fd;
    int listen_fds[5];
    int listen_fd_count;
    int rootless;
    int glamor;

    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr CloseScreen;
    CreateWindowProcPtr CreateWindow;
    DestroyWindowProcPtr DestroyWindow;
    RealizeWindowProcPtr RealizeWindow;
    UnrealizeWindowProcPtr UnrealizeWindow;

    ClipNotifyProcPtr ClipNotify;
    WindowExposuresProcPtr WindowExposures;
    ClearToBackgroundProcPtr ClearToBackground;

    struct xorg_list output_list;
    struct xorg_list seat_list;
    struct xorg_list damage_window_list;

    pthread_mutex_t window_hash_lock;
    struct hash_table * window_hash;

    int wayland_fd;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_registry *input_registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_shell *shell;

    uint32_t serial;

#define XWL_FORMAT_ARGB8888 (1 << 0)
#define XWL_FORMAT_XRGB8888 (1 << 1)
#define XWL_FORMAT_RGB565   (1 << 2)

    int prepare_read;

    char *device_name;
    int drm_fd;
    int fd_render_node;
    struct wl_drm *drm;
    uint32_t formats;
    uint32_t capabilities;
    void *egl_display, *egl_context;
    struct gbm_device *gbm;
    struct glamor_context *glamor_ctx;

    pthread_mutex_t callback_queue_lock;
    struct xorg_list callback_queue;

    struct theme * theme;
    struct window_manager * wm;

    struct xwl_window * net_active_window;

	VisualPtr visual;
	Colormap colormap_id;
	ColormapPtr colormap;

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
		Atom		 net_active_window;
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
		Atom		 net_close_window;
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

#endif /* HW_XWAYLAND_XWL_SCREEN_H_ */
