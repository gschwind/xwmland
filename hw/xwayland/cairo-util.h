/*
 * Copyright © 2008 Kristian Høgsberg
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

#ifndef _CAIRO_UTIL_H
#define _CAIRO_UTIL_H

#include <stdint.h>
#include <cairo.h>

#include <wayland-util.h>

#include "frame.h"

void
surface_flush_device(cairo_surface_t *surface);

void
render_shadow(cairo_t *cr, cairo_surface_t *surface,
	      int x, int y, int width, int height, int margin, int top_margin);

void
tile_source(cairo_t *cr, cairo_surface_t *surface,
	    int x, int y, int width, int height, int margin, int top_margin);

void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius);

cairo_surface_t *
load_cairo_surface(const char *filename);

struct theme {
	cairo_surface_t *active_frame;
	cairo_surface_t *inactive_frame;
	cairo_surface_t *shadow;
	int frame_radius;
	int margin;
	int width;
	int titlebar_height;
};

struct theme *
theme_create(void);
void
theme_destroy(struct theme *t);

enum {
	THEME_FRAME_ACTIVE = 1,
	THEME_FRAME_MAXIMIZED = 2,
	THEME_FRAME_NO_TITLE = 4
};

void
theme_set_background_source(struct theme *t, cairo_t *cr, uint32_t flags);
void
theme_render_frame(struct theme *t,
		   cairo_t *cr, int width, int height,
		   const char *title, struct wl_list *buttons,
		   uint32_t flags);



enum theme_location
theme_get_location(struct theme *t, int x, int y, int width, int height, int flags);

struct frame;


struct frame *
frame_create(struct theme *t, int32_t width, int32_t height, uint32_t buttons,
	     const char *title);

void
frame_destroy(struct frame *frame);

/* May set FRAME_STATUS_REPAINT */
int
frame_set_title(struct frame *frame, const char *title);

/* May set FRAME_STATUS_REPAINT */
void
frame_set_flag(struct frame *frame, enum frame_flag flag);

/* May set FRAME_STATUS_REPAINT */
void
frame_unset_flag(struct frame *frame, enum frame_flag flag);

void
frame_opaque_rect(struct frame *frame, int32_t *x, int32_t *y,
		  int32_t *width, int32_t *height);

int
frame_get_shadow_margin(struct frame *frame);

/* May set FRAME_STATUS_REPAINT */
enum theme_location
frame_pointer_enter(struct frame *frame, void *pointer, int x, int y);

/* May set FRAME_STATUS_REPAINT */
void
frame_pointer_leave(struct frame *frame, void *pointer);

/* Call to indicate that a button has been pressed/released.  The return
 * value for a button release will be the same as for the corresponding
 * press.  This allows you to more easily track grabs.  If you want the
 * actual location, simply keep the location from the last
 * frame_pointer_motion call.
 *
 * May set:
 *	FRAME_STATUS_MINIMIZE
 *	FRAME_STATUS_MAXIMIZE
 *	FRAME_STATUS_CLOSE
 *	FRAME_STATUS_MENU
 *	FRAME_STATUS_RESIZE
 *	FRAME_STATUS_MOVE
 */
enum theme_location
frame_pointer_button(struct frame *frame, void *pointer,
		     uint32_t button, enum frame_button_state state);

void
frame_touch_down(struct frame *frame, void *data, int32_t id, int x, int y);

void
frame_touch_up(struct frame *frame, void *data, int32_t id);

enum theme_location
frame_double_click(struct frame *frame, void *pointer,
		   uint32_t button, enum frame_button_state state);

void
frame_double_touch_down(struct frame *frame, void *data, int32_t id,
			int x, int y);

void
frame_double_touch_up(struct frame *frame, void *data, int32_t id);

void
frame_repaint(struct frame *frame, cairo_t *cr);


#endif
