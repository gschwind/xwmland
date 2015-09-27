/*
 * Copyright © 2014 Intel Corporation
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

#ifndef XWAYLAND_H
#define XWAYLAND_H

#include <dix-config.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <wayland-client.h>

#include <X11/X.h>

#include <fb.h>
#include <input.h>
#include <dix.h>
#include <randrstr.h>
#include <exevents.h>

#include "wm.h"

struct xwl_screen {
	int lock_count;

    int width;
    int height;
    int depth;
    ScreenPtr screen;
    WindowPtr pointer_limbo_window;
    int expecting_event;

    Bool realizing;

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

    struct window_manager * wm;

};

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
    WindowPtr window;
    DamagePtr damage;
    struct xorg_list link_damage;
    struct xorg_list link_window;
    struct wl_callback *frame_callback;


	int properties_dirty;
	int pid;
	char *machine;
	char *class;
	char *name;
	struct xwl_window *transient_for;
	uint32_t protocols;
	xcb_atom_t type;
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
	struct wl_list link;

};

#define MODIFIER_META 0x01

struct xwl_touch {
    struct xwl_window *window;
    int32_t id;
    int x, y;
    struct xorg_list link_touch;
};

struct xwl_seat {
    DeviceIntPtr pointer;
    DeviceIntPtr keyboard;
    DeviceIntPtr touch;
    struct xwl_screen *xwl_screen;
    struct wl_seat *seat;
    struct wl_pointer *wl_pointer;
    struct wl_keyboard *wl_keyboard;
    struct wl_touch *wl_touch;
    struct wl_array keys;
    struct xwl_window *focus_window;
    uint32_t id;
    uint32_t pointer_enter_serial;
    struct xorg_list link;
    CursorPtr x_cursor;
    struct wl_surface *cursor;
    struct wl_callback *cursor_frame_cb;
    Bool cursor_needs_update;

    struct xorg_list touches;

    size_t keymap_size;
    char *keymap;
    struct wl_surface *keyboard_focus;
};

struct xwl_output {
    struct xorg_list link;
    struct wl_output *output;
    uint32_t server_output_id;
    struct xwl_screen *xwl_screen;
    RROutputPtr randr_output;
    RRCrtcPtr randr_crtc;
    int32_t x, y, width, height;
    Rotation rotation;
};

struct xwl_pixmap {
    struct wl_buffer *buffer;
    int fd;
    void *data;
    size_t size;
};

struct xwl_callable {
	struct xorg_list link;
	void (*callback)(struct xwl_callable * ths);
};

struct xwl_pixmap;

Bool xwl_screen_init_cursor(struct xwl_screen *xwl_screen);

struct xwl_screen *xwl_screen_get(ScreenPtr screen);

void xwl_seat_set_cursor(struct xwl_seat *xwl_seat);

void xwl_seat_destroy(struct xwl_seat *xwl_seat);

void xwl_seat_clear_touch(struct xwl_seat *xwl_seat, WindowPtr window);

Bool xwl_screen_init_output(struct xwl_screen *xwl_screen);

struct xwl_output *xwl_output_create(struct xwl_screen *xwl_screen,
                                     uint32_t id);

void xwl_output_destroy(struct xwl_output *xwl_output);

RRModePtr xwayland_cvt(int HDisplay, int VDisplay,
                       float VRefresh, Bool Reduced, Bool Interlaced);

void xwl_pixmap_set_private(PixmapPtr pixmap, struct xwl_pixmap *xwl_pixmap);
struct xwl_pixmap *xwl_pixmap_get(PixmapPtr pixmap);


Bool xwl_shm_create_screen_resources(ScreenPtr screen);
PixmapPtr xwl_shm_create_pixmap(ScreenPtr screen, int width, int height,
                                int depth, unsigned int hint);
Bool xwl_shm_destroy_pixmap(PixmapPtr pixmap);
struct wl_buffer *xwl_shm_pixmap_get_wl_buffer(PixmapPtr pixmap);


Bool xwl_glamor_init(struct xwl_screen *xwl_screen);

Bool xwl_screen_init_glamor(struct xwl_screen *xwl_screen,
                         uint32_t id, uint32_t version);
struct wl_buffer *xwl_glamor_pixmap_get_wl_buffer(PixmapPtr pixmap);

struct xwl_window *
xwl_screen_lock_window(struct xwl_screen *xwl_screen, uint32_t id);

void
xwl_screen_unlock_window(struct xwl_window * xwl_window);

#endif
