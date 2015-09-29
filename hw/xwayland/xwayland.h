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
#include <fb.h>
#include <input.h>
#include <dix.h>
#include <randrstr.h>
#include <exevents.h>

#include "xwl_screen.h"
#include "xwl_window.h"


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
    int last_pointer_x;
    int last_pointer_y;

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
