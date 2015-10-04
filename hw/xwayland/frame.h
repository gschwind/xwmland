/*
 * frame.h
 *
 *  Created on: 29 sept. 2015
 *      Author: gschwind
 */

#ifndef HW_XWAYLAND_FRAME_H_
#define HW_XWAYLAND_FRAME_H_

#include <stdint.h>

struct frame;
struct theme;

enum theme_location {
	THEME_LOCATION_INTERIOR = 0,
	THEME_LOCATION_RESIZING_TOP = 1,
	THEME_LOCATION_RESIZING_BOTTOM = 2,
	THEME_LOCATION_RESIZING_LEFT = 4,
	THEME_LOCATION_RESIZING_TOP_LEFT = 5,
	THEME_LOCATION_RESIZING_BOTTOM_LEFT = 6,
	THEME_LOCATION_RESIZING_RIGHT = 8,
	THEME_LOCATION_RESIZING_TOP_RIGHT = 9,
	THEME_LOCATION_RESIZING_BOTTOM_RIGHT = 10,
	THEME_LOCATION_RESIZING_MASK = 15,
	THEME_LOCATION_EXTERIOR = 16,
	THEME_LOCATION_TITLEBAR = 17,
	THEME_LOCATION_CLIENT_AREA = 18,
};

enum frame_status {
	FRAME_STATUS_NONE = 0,
	FRAME_STATUS_REPAINT = 0x1,
	FRAME_STATUS_MINIMIZE = 0x2,
	FRAME_STATUS_MAXIMIZE = 0x4,
	FRAME_STATUS_CLOSE = 0x8,
	FRAME_STATUS_MENU = 0x10,
	FRAME_STATUS_RESIZE = 0x20,
	FRAME_STATUS_MOVE = 0x40,
	FRAME_STATUS_ALL = 0x7f
};

enum frame_flag {
	FRAME_FLAG_ACTIVE = 0x1,
	FRAME_FLAG_MAXIMIZED = 0x2
};

enum {
	FRAME_BUTTON_NONE = 0,
	FRAME_BUTTON_CLOSE = 0x1,
	FRAME_BUTTON_MAXIMIZE = 0x2,
	FRAME_BUTTON_MINIMIZE = 0x4,
	FRAME_BUTTON_ALL = 0x7
};

enum frame_button_state {
	FRAME_BUTTON_RELEASED = 0,
	FRAME_BUTTON_PRESSED = 1
};

struct theme *
theme_create(void);

struct frame * frame_create(struct theme *t, int32_t width, int32_t height, uint32_t buttons, const char *title);
void frame_resize(struct frame *frame, int32_t width, int32_t height);
void frame_resize_inside(struct frame *frame, int32_t width, int32_t height);
int32_t frame_width(struct frame *frame);
int32_t frame_height(struct frame *frame);

void frame_interior(struct frame *frame, int32_t *x, int32_t *y, int32_t *width, int32_t *height);
void frame_input_rect(struct frame *frame, int32_t *x, int32_t *y, int32_t *width, int32_t *height);

enum theme_location frame_pointer_motion(struct frame *frame, void *data, int x, int y);
uint32_t frame_status(struct frame *frame);

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
enum theme_location frame_pointer_button(struct frame *frame, void *data, uint32_t btn, enum frame_button_state state);

void frame_status_clear(struct frame *frame, enum frame_status status);
void frame_unset_flag(struct frame *frame, enum frame_flag flag);
void frame_set_flag(struct frame *frame, enum frame_flag flag);

#endif /* HW_XWAYLAND_FRAME_H_ */
