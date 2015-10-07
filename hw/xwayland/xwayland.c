/*
 * Copyright © 2011-2014 Intel Corporation
 * Copyright © 2015 Benoit Gschwind <gschwind@gnu-log.net>
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

#include <dix-config.h>

#include "xwayland.h"

#include <pthread.h>
#include <stdio.h>

#include <selection.h>
#include <micmap.h>
#include <misyncshm.h>
#include <compositeext.h>
#include <glx_extinit.h>

#include "window.h"
#include "compint.h"

#include "xwl_event.h"
#include "client-side-wm.h"
#include "server-side-wm.h"
#include "xwm/hash.h"
#include "frame.h"

void
ddxGiveUp(enum ExitCode error)
{
}

void
AbortDDX(enum ExitCode error)
{
    ddxGiveUp(error);
}

void
OsVendorInit(void)
{
}

void
OsVendorFatalError(const char *f, va_list args)
{
}

#if defined(DDXBEFORERESET)
void
ddxBeforeReset(void)
{
    return;
}
#endif

void
ddxUseMsg(void)
{
    ErrorF("-rootless              run rootless, requires wm support\n");
    ErrorF("-wm fd                 create X client for wm on given fd\n");
    ErrorF("-listen fd             add give fd as a listen socket\n");
}

int
ddxProcessArgument(int argc, char *argv[], int i)
{
    if (strcmp(argv[i], "-rootless") == 0) {
        return 1;
    }
    else if (strcmp(argv[i], "-listen") == 0) {
        NoListenAll = TRUE;
        return 2;
    }
    else if (strcmp(argv[i], "-wm") == 0) {
        return 2;
    }
    else if (strcmp(argv[i], "-shm") == 0) {
        return 1;
    }

    return 0;
}

static DevPrivateKeyRec xwl_window_private_key;
static DevPrivateKeyRec xwl_screen_private_key;
static DevPrivateKeyRec xwl_pixmap_private_key;

struct xwl_window *
xwl_screen_find_window(struct xwl_screen * xwl_screen, XID id) {
	struct xwl_window ** data;
	data = ht_find(xwl_screen->clients_window_hash, &id);
	if(data) {
		return *data;
	} else {
		return NULL;
	}
}

void xwl_screen_add_window(struct xwl_screen * xwl_screen, XID id, struct xwl_window * xwl_window) {
	struct xwl_window ** data = ht_add(xwl_screen->clients_window_hash, &id);
	*data = xwl_window;
}

void xwl_screen_remove_window(struct xwl_screen * xwl_screen, XID id) {
	ht_remove(xwl_screen->clients_window_hash, &id);
}

struct xwl_screen *
xwl_screen_get(ScreenPtr screen)
{
    return dixLookupPrivate(&screen->devPrivates, &xwl_screen_private_key);
}

static Bool
xwl_close_screen(ScreenPtr screen)
{
    struct xwl_screen *xwl_screen = xwl_screen_get(screen);
    struct xwl_output *xwl_output, *next_xwl_output;
    struct xwl_seat *xwl_seat, *next_xwl_seat;

    xorg_list_for_each_entry_safe(xwl_output, next_xwl_output,
                                  &xwl_screen->output_list, link)
        xwl_output_destroy(xwl_output);

    xorg_list_for_each_entry_safe(xwl_seat, next_xwl_seat,
                                  &xwl_screen->seat_list, link)
        xwl_seat_destroy(xwl_seat);

    wl_display_disconnect(xwl_screen->display);

    screen->CloseScreen = xwl_screen->CloseScreen;
    free(xwl_screen);

    return screen->CloseScreen(screen);
}

static void
damage_report(DamagePtr pDamage, RegionPtr pRegion, void *data)
{
    struct xwl_window *xwl_window = data;
    struct xwl_screen *xwl_screen = xwl_window->xwl_screen;

    xorg_list_add(&xwl_window->link_damage, &xwl_screen->damage_window_list);
}

static void
damage_destroy(DamagePtr pDamage, void *data)
{
}

void
xwl_pixmap_set_private(PixmapPtr pixmap, struct xwl_pixmap *xwl_pixmap)
{
    dixSetPrivate(&pixmap->devPrivates, &xwl_pixmap_private_key, xwl_pixmap);
}

struct xwl_pixmap *
xwl_pixmap_get(PixmapPtr pixmap)
{
    return dixLookupPrivate(&pixmap->devPrivates, &xwl_pixmap_private_key);
}

//static void
//send_surface_id_event(struct xwl_window *xwl_window)
//{
//    static const char atom_name[] = "WL_SURFACE_ID";
//    static Atom type_atom = None;
//    DeviceIntPtr dev;
//    xEvent e;
//
//	LogWrite(0, "send surface id for %d\n", xwl_window->window->drawable.id);
//
//    if (type_atom == None)
//        type_atom = MakeAtom(atom_name, strlen(atom_name), TRUE);
//
//    e.u.u.type = ClientMessage;
//    e.u.u.detail = 32;
//    e.u.clientMessage.window = xwl_window->window->drawable.id;
//    e.u.clientMessage.u.l.type = type_atom;
//    e.u.clientMessage.u.l.longs0 =
//        wl_proxy_get_id((struct wl_proxy *) xwl_window->surface);
//    e.u.clientMessage.u.l.longs1 = 0;
//    e.u.clientMessage.u.l.longs2 = 0;
//    e.u.clientMessage.u.l.longs3 = 0;
//    e.u.clientMessage.u.l.longs4 = 0;
//
//    dev = PickPointer(serverClient);
//    DeliverEventsToWindow(dev, xwl_window->xwl_screen->screen->root,
//                          &e, 1, SubstructureRedirectMask, NullGrab);
//}

//static void
//xwl_screen_create_frame(struct xwl_screen * screen) {
//
//extern _X_EXPORT WindowPtr CreateWindow(Window /*wid */ ,
//                                        WindowPtr /*pParent */ ,
//                                        int /*x */ ,
//                                        int /*y */ ,
//                                        unsigned int /*w */ ,
//                                        unsigned int /*h */ ,
//                                        unsigned int /*bw */ ,
//                                        unsigned int /*class */ ,
//                                        Mask /*vmask */ ,
//                                        XID * /*vlist */ ,
//                                        int /*depth */ ,
//                                        ClientPtr /*client */ ,
//                                        VisualID /*visual */ ,
//                                        int * /*error */ );
//
//
//xcb_create_window(wm->conn,
//		  32,
//		  window->frame_id,
//		  wm->screen->root,
//		  0, 0,
//		  width, height,
//		  0,
//		  XCB_WINDOW_CLASS_INPUT_OUTPUT,
//		  wm->visual_id,
//		  XCB_CW_BORDER_PIXEL |
//		  XCB_CW_EVENT_MASK |
//		  XCB_CW_COLORMAP, values);
//
//
//XID frame_id = FakeClientID(serverClient->index);
//CreateWindow(XID, screen->root, 0, 0, width, heigth, 0, InputOutput, )
//values[0] = wm->screen->black_pixel;
//values[1] =
//	XCB_EVENT_MASK_KEY_PRESS |
//	XCB_EVENT_MASK_KEY_RELEASE |
//	XCB_EVENT_MASK_BUTTON_PRESS |
//	XCB_EVENT_MASK_BUTTON_RELEASE |
//	XCB_EVENT_MASK_POINTER_MOTION |
//	XCB_EVENT_MASK_ENTER_WINDOW |
//	XCB_EVENT_MASK_LEAVE_WINDOW |
//	XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
//	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
//values[2] = wm->colormap;
//
//
//}

//static void
//weston_wm_window_create_frame(struct weston_wm_window *window)
//{
//	struct weston_wm *wm = window->wm;
//	uint32_t values[3];
//	int x, y, width, height;
//	int buttons = FRAME_BUTTON_CLOSE;
//
//	if (window->decorate & MWM_DECOR_MAXIMIZE)
//		buttons |= FRAME_BUTTON_MAXIMIZE;
//
//	window->frame = frame_create(window->wm->theme,
//				     window->width, window->height,
//				     buttons, window->name);
//	frame_resize_inside(window->frame, window->width, window->height);
//
//	weston_wm_window_get_frame_size(window, &width, &height);
//	weston_wm_window_get_child_position(window, &x, &y);
//
//	values[0] = wm->screen->black_pixel;
//	values[1] =
//		XCB_EVENT_MASK_KEY_PRESS |
//		XCB_EVENT_MASK_KEY_RELEASE |
//		XCB_EVENT_MASK_BUTTON_PRESS |
//		XCB_EVENT_MASK_BUTTON_RELEASE |
//		XCB_EVENT_MASK_POINTER_MOTION |
//		XCB_EVENT_MASK_ENTER_WINDOW |
//		XCB_EVENT_MASK_LEAVE_WINDOW |
//		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
//		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
//	values[2] = wm->colormap;
//
//	window->frame_id = xcb_generate_id(wm->conn);
//	window->base_id = window->frame_id;
//	xcb_create_window(wm->conn,
//			  32,
//			  window->frame_id,
//			  wm->screen->root,
//			  0, 0,
//			  width, height,
//			  0,
//			  XCB_WINDOW_CLASS_INPUT_OUTPUT,
//			  wm->visual_id,
//			  XCB_CW_BORDER_PIXEL |
//			  XCB_CW_EVENT_MASK |
//			  XCB_CW_COLORMAP, values);
//
//	xcb_reparent_window(wm->conn, window->id, window->frame_id, x, y);
//
//	values[0] = 0;
//	xcb_configure_window(wm->conn, window->id,
//			     XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
//
//	window->cairo_surface =
//		cairo_xcb_surface_create_with_xrender_format(wm->conn,
//							     wm->screen,
//							     window->frame_id,
//							     &wm->format_rgba,
//							     width, height);
//
//	hash_table_insert(wm->window_hash, window->frame_id, window);
//
//}

static Bool
xwl_realize_window(WindowPtr window);

static Bool
xwl_unrealize_window(WindowPtr window);

static
void enter(void *data,
	      struct wl_surface *wl_surface,
	      struct wl_output *output) {
	//weston_wm_window_activate(data);
	LogWrite(0, "%s\n", __PRETTY_FUNCTION__);

}

static
void leave(void *data,
	      struct wl_surface *wl_surface,
	      struct wl_output *output) {
	LogWrite(0, "%s\n", __PRETTY_FUNCTION__);

}

static
struct wl_surface_listener surface_listener = {
		enter,
		leave
};

static void
shell_surface_ping(void *data,
                   struct wl_shell_surface *shell_surface, uint32_t serial)
{
	LogWrite(0, "%s\n", __PRETTY_FUNCTION__);
    wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_surface_configure(void *data,
                        struct wl_shell_surface *wl_shell_surface,
                        uint32_t edges, int32_t width, int32_t height)
{
	struct xwl_window * xwl_window = data;
	XID values[4];

	//LogWrite(0, "%s(%d,%d,%d)\n", __PRETTY_FUNCTION__, edges, width, height);

	frame_resize(xwl_window->frame, width, height);
	values[0] = frame_width(xwl_window->frame);
	values[1] = frame_height(xwl_window->frame);
	ConfigureWindow(xwl_window->frame_window, CWWidth|CWHeight, values, serverClient);

	frame_interior(xwl_window->frame, &values[0], &values[1], &values[2], &values[3]);
	ConfigureWindow(xwl_window->client_window, CWX|CWY|CWWidth|CWHeight, values, serverClient);

	xwl_window_dirty_layout(xwl_window);

}

static void
shell_surface_popup_done(void *data, struct wl_shell_surface *wl_shell_surface)
{
	LogWrite(0, "%s\n", __PRETTY_FUNCTION__);
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    shell_surface_ping,
    shell_surface_configure,
    shell_surface_popup_done
};

static void
xwl_window_exposures(WindowPtr pWin, RegionPtr pRegion) {
	ScreenPtr screen = pWin->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;

    //LogWrite(0, "xwl_window_exposures %d\n", pWin->drawable.id);

	xwl_screen = xwl_screen_get(screen);

	/* call legacy WindowExposures */
	if(xwl_screen->WindowExposures) {
		screen->WindowExposures = xwl_screen->WindowExposures;
		(*screen->WindowExposures) (pWin, pRegion);
		xwl_screen->WindowExposures = screen->WindowExposures;
		screen->WindowExposures = xwl_window_exposures;
	}

    if(pWin == screen->root)
    	return;

    xwl_window = xwl_screen_find_window(xwl_screen, pWin->drawable.id);
    if(xwl_window == NULL)
    	return;

    if(xwl_window->frame_window != pWin)
    	return;

    xwl_window_dirty_layout(xwl_window);

}

static Bool
xwl_screen_change_window_attributes(WindowPtr pWin, unsigned long vmask) {
		ScreenPtr screen = pWin->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;

    LogWrite(0, "xwl_screen_change_window_attributes %d (%d,%d,%d,%d)\n",
    		pWin->drawable.id, (int)pWin->drawable.x,  (int)pWin->drawable.y,
			 (int)pWin->drawable.width, (int)pWin->drawable.height);

	xwl_screen = xwl_screen_get(screen);

	/* call legacy ClipNotify */
	if(xwl_screen->ChangeWindowAttributes) {
		screen->ChangeWindowAttributes = xwl_screen->ChangeWindowAttributes;
		(*screen->ChangeWindowAttributes) (pWin, vmask);
		xwl_screen->ChangeWindowAttributes = screen->ChangeWindowAttributes;
		screen->ChangeWindowAttributes = xwl_screen_change_window_attributes;
	}

    if(pWin == screen->root)
    	return FALSE;

    xwl_window = xwl_screen_find_window(xwl_screen, pWin->drawable.id);
    if(xwl_window == NULL)
    	return FALSE;

    if((vmask & (CWX|CWY|CWWidth|CWHeight)) && xwl_window->frame) {
    	uint32_t x, y, w, h;
    	frame_interior(xwl_window->frame, &x, &y, &w, &h);
    	if(pWin->drawable.x != x
    		|| pWin->drawable.y != y
			|| pWin->drawable.width != w
			|| pWin->drawable.height != h
    	) {
    		xwl_window_dirty_layout(xwl_window);
    	}

    }

    if((vmask & (CWBorderWidth)) && xwl_window->frame) {
		xwl_window_dirty_layout(xwl_window);
    }

	return FALSE;
}


/*** NOT IN USE CURRENTLY ***/
static void
xwl_clip_notify(WindowPtr pWin, int dx, int dy) {
	ScreenPtr screen = pWin->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;

    LogWrite(0, "xwl_clip_notify %d\n", pWin->drawable.id);

	xwl_screen = xwl_screen_get(screen);

	/* call legacy ClipNotify */
	if(xwl_screen->ClipNotify) {
		screen->ClipNotify = xwl_screen->ClipNotify;
		(*screen->ClipNotify) (pWin, dx, dy);
		xwl_screen->ClipNotify = screen->ClipNotify;
		screen->ClipNotify = xwl_clip_notify;
	}

    if(pWin == screen->root)
    	return;

    xwl_window = xwl_screen_find_window(xwl_screen, pWin->drawable.id);
    if(xwl_window == NULL)
    	return;


    //xwl_window_draw_decoration(xwl_window);

}

/*** NOT IN USE CURRENTLY ***/
static void
xwl_clear_to_background(WindowPtr pWin, int x, int y, int w, int h, Bool generateExposures)
{
	ScreenPtr screen = pWin->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;

    LogWrite(0, "xwl_clear_to_background\n");

	xwl_screen = xwl_screen_get(screen);

    /* call legacy ClearToBackground */
    screen->ClearToBackground = xwl_screen->ClearToBackground;
    (*screen->ClearToBackground) (pWin, x, y, w, h, generateExposures);
    xwl_screen->ClearToBackground = screen->ClearToBackground;
    screen->ClearToBackground = xwl_clear_to_background;

    xwl_window = xwl_screen_find_window(xwl_screen, pWin->drawable.id);
    if(xwl_window == NULL)
    	return;

    if(xwl_window->frame_window != pWin)
    	return;

    //window_manager_window_draw_decoration(xwl_window);

}

static void
xwl_screen_handle_map_event(struct xwl_screen *xwl_screen, Window window_id)
{
	WindowPtr window;
    struct xwl_window *xwl_window;

	XID values[4];
	int err = 0;
	uint32_t x, y, w, h;
	xGetWindowAttributesReply attr;
	int buttons = FRAME_BUTTON_CLOSE;

	LogWrite(0, "START xwl_handle_map_event 0x%x\n", window_id);

	err = dixLookupWindow(&window, window_id, serverClient, DixWriteAccess);
	if(err) {
		LogWrite(0, "END xwl_handle_map_event 0x%x (Window not found)\n", window_id);
		return;
	}

    if(CLIENT_ID(window_id) == 0) {
    	LogWrite(0, "END xwl_realize_window %d (Server Window)\n", window->drawable.id);
    	return;
    }

    if(window->parent != xwl_screen->screen->root && window->parent != 0) {
    	LogWrite(0, "END xwl_realize_window %d (Not child of root)\n", window->drawable.id);
    	return;
    }

    xwl_window = xwl_screen_find_window(xwl_screen, window->drawable.id);

    if(xwl_window != NULL) {
    	LogWrite(0, "END xwl_realize_window %d (Window already managed)\n", window->drawable.id);
    	return;
    }

	GetWindowAttributes(window, serverClient, &attr);

	LogWrite(0, "Override = %d\n", attr.override);

	/* do not try to manage InputOnly windows */
	if(attr.class == InputOnly) {
		LogWrite(0, "END xwl_realize_window %d (InputOnly)\n", window->drawable.id);
		return;
	}

	/* register this window */
	LogWrite(0, "create xwl_window for %d\n", window->drawable.id);
	xwl_window = calloc(sizeof *xwl_window, 1);
	xwl_window->xwl_screen = xwl_screen;
	xwl_window->client_window = window;
	xwl_window->surface = NULL;
	xwl_window->shell_surface = NULL;
	xwl_window->override_redirect = attr.override;

	xorg_list_init(&(xwl_window->list_childdren));
	xorg_list_init(&(xwl_window->link_sibling));
	xorg_list_init(&(xwl_window->link_cleanup));
	xorg_list_init(&(xwl_window->link_dirty));

	if(!attr.override) {
		DeviceIntPtr dev;
		XID colormap;
		XID visual;

		xwl_window->properties_dirty = 1;
		window_manager_window_read_properties(xwl_window);

		LogWrite(0, "external client %d\n", window->drawable.id);

		if (xwl_window->decorate & MWM_DECOR_MAXIMIZE)
			buttons |= FRAME_BUTTON_MAXIMIZE;

		xwl_window->frame = frame_create(xwl_screen->theme,
				window->drawable.width,
				window->drawable.height, buttons, xwl_window->name);

		frame_resize_inside(xwl_window->frame, window->drawable.width, window->drawable.height);
		frame_interior(xwl_window->frame, &x, &y, &w, &h);

		xwl_window_dirty_layout(xwl_window);

		xwl_window->has_32b_visual = visual_is_depth_32(xwl_screen, attr.visualID);
		if(xwl_window->has_32b_visual) {
			visual = attr.visualID;
			colormap = attr.colormap;
		} else {
			visual = xwl_screen->visual->vid;
			colormap = xwl_screen->colormap_id;
		}

		values[0] = xwl_screen->screen->blackPixel;
		values[1] = xwl_screen->screen->blackPixel;
		values[2] = 0;
		values[3] = colormap;

		LogWrite(0, "Class : %d, XID: %d\n",
				(int)xwl_screen->colormap->class,
				(int)xwl_screen->colormap->mid);


		xwl_window->frame_window = CreateWindow(FakeClientID(0), xwl_screen->screen->root,
				0, 0,
				frame_width(xwl_window->frame),
				frame_height(xwl_window->frame),
				0, InputOutput,
				CWBackPixel|CWBorderPixel|CWBorderWidth|CWColormap, values,
				32, serverClient, visual, &err);

		/* once a resource is created it should be added */
		AddResource(xwl_window->frame_window->drawable.id, RT_WINDOW, (void*)xwl_window->frame_window);

		LogWrite(0, "Frame = %p, err = %d\n", xwl_window->frame_window, err);

		values[0] = 0;
		ConfigureWindow(xwl_window->client_window, CWBorderWidth, values, serverClient);
		ReparentWindow(window, xwl_window->frame_window, x, y, serverClient);
		MapWindow(window, serverClient);
		MapWindow(xwl_window->frame_window, serverClient);

		dev = PickKeyboard(serverClient);
		SetInputFocus(serverClient, dev, xwl_window->client_window->drawable.id, RevertToNone, 0, 0);

		LogWrite(0, "internal window %d\n", window->drawable.id);

		/** Immediately redirect this window **/
		compRedirectWindow(serverClient, xwl_window->frame_window, CompositeRedirectManual);

		window_manager_window_set_wm_state(xwl_window, ICCCM_NORMAL_STATE);
		window_manager_window_set_net_wm_state(xwl_window);
		window_manager_window_set_virtual_desktop(xwl_window, 0);

		xwl_window->surface = wl_compositor_create_surface(xwl_screen->compositor);
		wl_surface_add_listener(xwl_window->surface, &surface_listener, xwl_window);
		xwl_window->shell_surface = wl_shell_get_shell_surface(xwl_screen->shell, xwl_window->surface);
		wl_shell_surface_add_listener(xwl_window->shell_surface, &shell_surface_listener, xwl_window);

		wl_shell_surface_set_toplevel(xwl_window->shell_surface);

		if(xwl_window->name)
			wl_shell_surface_set_title(xwl_window->shell_surface, xwl_window->name);

		wl_display_flush(xwl_screen->display);
		wl_surface_set_user_data(xwl_window->surface, xwl_window);

		LogWrite(0, "add windows to hash table\n");
		xwl_screen_add_window(xwl_screen, window->drawable.id, xwl_window);
		xwl_screen_add_window(xwl_screen, xwl_window->frame_window->drawable.id, xwl_window);
		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);

		xwl_window->damage =
			DamageCreate(damage_report,
					damage_destroy, DamageReportNonEmpty,
					FALSE, xwl_screen->screen, xwl_window);
		DamageRegister(&xwl_window->frame_window->drawable, xwl_window->damage);
		DamageSetReportAfterOp(xwl_window->damage, TRUE);

		xorg_list_init(&xwl_window->link_damage);

		xwl_screen_window_activate(xwl_screen, xwl_window);

	} else {

		xwl_window->properties_dirty = 1;
		window_manager_window_read_properties(xwl_window);
		LogWrite(0, "Transient for %p\n", xwl_window->transient_for);

		MapWindow(window, serverClient);

		xwl_window->frame = NULL;
		xwl_window->frame_window = NULL;

		MapWindow(xwl_window->client_window, serverClient);

		LogWrite(0, "internal window %d\n", window->drawable.id);

		/** Immediately redirect this window **/
		compRedirectWindow(serverClient, xwl_window->client_window, CompositeRedirectManual);

		window_manager_window_set_wm_state(xwl_window, ICCCM_NORMAL_STATE);
		window_manager_window_set_net_wm_state(xwl_window);
		window_manager_window_set_virtual_desktop(xwl_window, 0);

		xwl_window->surface = wl_compositor_create_surface(xwl_screen->compositor);
		wl_surface_add_listener(xwl_window->surface, &surface_listener, xwl_window);

		xwl_window->shell_surface = wl_shell_get_shell_surface(xwl_screen->shell, xwl_window->surface);
		wl_shell_surface_add_listener(xwl_window->shell_surface, &shell_surface_listener, xwl_window);

		if(xwl_window->transient_for) {
			struct xwl_seat * xwl_seat_found;
			struct xwl_seat * seat_iterator;
			int parent_x, parent_y;
			struct xwl_window *window_iterator;

			xwl_seat_found = NULL;

			/**
			 * Find the staking order, if more than ones transient for is found
			 * The current window goes on top of the last one. Else the window
			 * will be above the transient for.
			 **/
			xwl_window->below_me = NULL;

			xorg_list_for_each_entry(window_iterator, &(xwl_window->transient_for->list_childdren), link_sibling) {
				/* if this is a not managed window, go above it */
				if(!window_iterator->frame) {
					xwl_window->below_me = window_iterator;
					break;
				}
			}

			/* if below still unset, go above transient_for */
			if(!xwl_window->below_me) {
				xwl_window->below_me = xwl_window->transient_for;
			}

			/** add current window to the sibling list */
			xorg_list_add(&(xwl_window->link_sibling), &(xwl_window->transient_for->list_childdren));

		    xorg_list_for_each_entry(seat_iterator, &xwl_screen->seat_list, link) {
		    	if((seat_iterator->focus_window == xwl_window->transient_for)
		    		|| (seat_iterator->focus_window == xwl_window->below_me)) {
		    		xwl_seat_found = seat_iterator;
		    		break;
		    	}
		    }

		    if(xwl_window->below_me->frame_window) {
		    	parent_x = xwl_window->below_me->frame_window->origin.x;
		    	parent_y = xwl_window->below_me->frame_window->origin.y;
		    } else {
		    	parent_x = xwl_window->below_me->client_window->origin.x;
		    	parent_y = xwl_window->below_me->client_window->origin.y;
		    }

		    if(xwl_seat_found) {
//		    	wl_shell_surface_set_popup(xwl_window->shell_surface,
//		    			xwl_seat_found->seat,
//						xwl_seat_found->pointer_enter_serial,
//						xwl_window->transient_for->surface,
//						xwl_window->client_window->origin.x - parent_x,
//						xwl_window->client_window->origin.y - parent_y,
//						0);
		    	wl_shell_surface_set_transient(xwl_window->shell_surface,
		    			xwl_window->below_me->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);
		    } else {
		    	wl_shell_surface_set_transient(xwl_window->shell_surface,
		    			xwl_window->below_me->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);
		    }
		} else {
			int parent_x, parent_y;
			/**
			 * if transient_for is not set, we guess this window is a child of
			 * the window under the cursor, if this last window belong to the same
			 * X11 client.
			 */
			struct xwl_seat *seat = xorg_list_first_entry(&(xwl_screen->seat_list), struct xwl_seat, link);

			if(seat->focus_window != NULL) {
			if(CLIENT_ID(seat->focus_window->client_window->drawable.id)
					== CLIENT_ID(xwl_window->client_window->drawable.id)) {

			    if(seat->focus_window->frame_window) {
			    	parent_x = seat->focus_window->frame_window->origin.x;
			    	parent_y = seat->focus_window->frame_window->origin.y;
			    } else {
			    	parent_x = seat->focus_window->client_window->origin.x;
			    	parent_y = seat->focus_window->client_window->origin.y;
			    }

				wl_shell_surface_set_transient(xwl_window->shell_surface,
						seat->focus_window->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);

			} else {
				wl_shell_surface_set_toplevel(xwl_window->shell_surface);
			}

			} else {
				wl_shell_surface_set_toplevel(xwl_window->shell_surface);
			}
		}

		if(xwl_window->name)
			wl_shell_surface_set_title(xwl_window->shell_surface, xwl_window->name);

		wl_display_flush(xwl_screen->display);
		wl_surface_set_user_data(xwl_window->surface, xwl_window);

		LogWrite(0, "add windows to hash table\n");
		xwl_screen_add_window(xwl_screen, window->drawable.id, xwl_window);
		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);

		xwl_window->damage =
			DamageCreate(damage_report,
					damage_destroy, DamageReportNonEmpty,
					FALSE, xwl_screen->screen, xwl_window);
		DamageRegister(&xwl_window->client_window->drawable, xwl_window->damage);
		DamageSetReportAfterOp(xwl_window->damage, TRUE);

		xorg_list_init(&xwl_window->link_damage);

	}

	LogWrite(0, "END xwl_realize_window %d\n", window->drawable.id);
}


static Bool
xwl_realize_window(WindowPtr window)
{
	ScreenPtr screen = window->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_event *xwl_event;
    Bool ret;

	LogWrite(0, "START xwl_realize_window %d (viewable:%s)\n", window->drawable.id, (window->viewable?"True":"False"));

	xwl_screen = xwl_screen_get(screen);

    /* call legacy ReleazeWindow */
    screen->RealizeWindow = xwl_screen->RealizeWindow;
    ret = (*screen->RealizeWindow) (window);
    xwl_screen->RealizeWindow = screen->RealizeWindow;
    screen->RealizeWindow = xwl_realize_window;

	if (xwl_screen->rootless && !window->parent) {
		RegionNull(&window->clipList);
		RegionNull(&window->borderClip);
		RegionNull(&window->winSize);
	}

    if(CLIENT_ID(window->drawable.id) == 0) {
    	LogWrite(0, "END xwl_realize_window %d (Server Window)\n", window->drawable.id);
    	return ret;
    }


	WindowPtr window;
    struct xwl_window *xwl_window;

	XID values[4];
	int err = 0;
	uint32_t x, y, w, h;
	xGetWindowAttributesReply attr;
	int buttons = FRAME_BUTTON_CLOSE;

	LogWrite(0, "START xwl_handle_map_event 0x%x\n", window_id);

	err = dixLookupWindow(&window, window_id, serverClient, DixWriteAccess);
	if(err) {
		LogWrite(0, "END xwl_handle_map_event 0x%x (Window not found)\n", window_id);
		return;
	}

    if(CLIENT_ID(window_id) == 0) {
    	LogWrite(0, "END xwl_realize_window %d (Server Window)\n", window->drawable.id);
    	return;
    }

    if(window->parent != xwl_screen->screen->root && window->parent != 0) {
    	LogWrite(0, "END xwl_realize_window %d (Not child of root)\n", window->drawable.id);
    	return;
    }

    xwl_window = xwl_screen_find_window(xwl_screen, window->drawable.id);

    if(xwl_window != NULL) {
    	LogWrite(0, "END xwl_realize_window %d (Window already managed)\n", window->drawable.id);
    	return;
    }

	GetWindowAttributes(window, serverClient, &attr);

	LogWrite(0, "Override = %d\n", attr.override);

	/* do not try to manage InputOnly windows */
	if(attr.class == InputOnly) {
		LogWrite(0, "END xwl_realize_window %d (InputOnly)\n", window->drawable.id);
		return;
	}

	/* register this window */
	LogWrite(0, "create xwl_window for %d\n", window->drawable.id);
	xwl_window = calloc(sizeof *xwl_window, 1);
	xwl_window->xwl_screen = xwl_screen;
	xwl_window->client_window = window;
	xwl_window->surface = NULL;
	xwl_window->shell_surface = NULL;
	xwl_window->override_redirect = attr.override;

	xorg_list_init(&(xwl_window->list_childdren));
	xorg_list_init(&(xwl_window->link_sibling));
	xorg_list_init(&(xwl_window->link_cleanup));
	xorg_list_init(&(xwl_window->link_dirty));

	if(!attr.override) {
		DeviceIntPtr dev;
		XID colormap;
		XID visual;

		xwl_window->properties_dirty = 1;
		window_manager_window_read_properties(xwl_window);

		LogWrite(0, "external client %d\n", window->drawable.id);

		if (xwl_window->decorate & MWM_DECOR_MAXIMIZE)
			buttons |= FRAME_BUTTON_MAXIMIZE;

		xwl_window->frame = frame_create(xwl_screen->theme,
				window->drawable.width,
				window->drawable.height, buttons, xwl_window->name);

		frame_resize_inside(xwl_window->frame, window->drawable.width, window->drawable.height);
		frame_interior(xwl_window->frame, &x, &y, &w, &h);

		xwl_window_dirty_layout(xwl_window);

		xwl_window->has_32b_visual = visual_is_depth_32(xwl_screen, attr.visualID);
		if(xwl_window->has_32b_visual) {
			visual = attr.visualID;
			colormap = attr.colormap;
		} else {
			visual = xwl_screen->visual->vid;
			colormap = xwl_screen->colormap_id;
		}

		values[0] = xwl_screen->screen->blackPixel;
		values[1] = xwl_screen->screen->blackPixel;
		values[2] = 0;
		values[3] = colormap;

		LogWrite(0, "Class : %d, XID: %d\n",
				(int)xwl_screen->colormap->class,
				(int)xwl_screen->colormap->mid);


		xwl_window->frame_window = CreateWindow(FakeClientID(0), xwl_screen->screen->root,
				0, 0,
				frame_width(xwl_window->frame),
				frame_height(xwl_window->frame),
				0, InputOutput,
				CWBackPixel|CWBorderPixel|CWBorderWidth|CWColormap, values,
				32, serverClient, visual, &err);

		/* once a resource is created it should be added */
		AddResource(xwl_window->frame_window->drawable.id, RT_WINDOW, (void*)xwl_window->frame_window);

		LogWrite(0, "Frame = %p, err = %d\n", xwl_window->frame_window, err);

		values[0] = 0;
		ConfigureWindow(xwl_window->client_window, CWBorderWidth, values, serverClient);
		ReparentWindow(window, xwl_window->frame_window, x, y, serverClient);
		MapWindow(window, serverClient);
		MapWindow(xwl_window->frame_window, serverClient);

		dev = PickKeyboard(serverClient);
		SetInputFocus(serverClient, dev, xwl_window->client_window->drawable.id, RevertToNone, 0, 0);

		LogWrite(0, "internal window %d\n", window->drawable.id);

		/** Immediately redirect this window **/
		compRedirectWindow(serverClient, xwl_window->frame_window, CompositeRedirectManual);

		window_manager_window_set_wm_state(xwl_window, ICCCM_NORMAL_STATE);
		window_manager_window_set_net_wm_state(xwl_window);
		window_manager_window_set_virtual_desktop(xwl_window, 0);

		xwl_window->surface = wl_compositor_create_surface(xwl_screen->compositor);
		wl_surface_add_listener(xwl_window->surface, &surface_listener, xwl_window);
		xwl_window->shell_surface = wl_shell_get_shell_surface(xwl_screen->shell, xwl_window->surface);
		wl_shell_surface_add_listener(xwl_window->shell_surface, &shell_surface_listener, xwl_window);

		wl_shell_surface_set_toplevel(xwl_window->shell_surface);

		if(xwl_window->name)
			wl_shell_surface_set_title(xwl_window->shell_surface, xwl_window->name);

		wl_display_flush(xwl_screen->display);
		wl_surface_set_user_data(xwl_window->surface, xwl_window);

		LogWrite(0, "add windows to hash table\n");
		xwl_screen_add_window(xwl_screen, window->drawable.id, xwl_window);
		xwl_screen_add_window(xwl_screen, xwl_window->frame_window->drawable.id, xwl_window);
		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);

		xwl_window->damage =
			DamageCreate(damage_report,
					damage_destroy, DamageReportNonEmpty,
					FALSE, xwl_screen->screen, xwl_window);
		DamageRegister(&xwl_window->frame_window->drawable, xwl_window->damage);
		DamageSetReportAfterOp(xwl_window->damage, TRUE);

		xorg_list_init(&xwl_window->link_damage);

		xwl_screen_window_activate(xwl_screen, xwl_window);

	} else {

		xwl_window->properties_dirty = 1;
		window_manager_window_read_properties(xwl_window);
		LogWrite(0, "Transient for %p\n", xwl_window->transient_for);

		MapWindow(window, serverClient);

		xwl_window->frame = NULL;
		xwl_window->frame_window = NULL;

		MapWindow(xwl_window->client_window, serverClient);

		LogWrite(0, "internal window %d\n", window->drawable.id);

		/** Immediately redirect this window **/
		compRedirectWindow(serverClient, xwl_window->client_window, CompositeRedirectManual);

		window_manager_window_set_wm_state(xwl_window, ICCCM_NORMAL_STATE);
		window_manager_window_set_net_wm_state(xwl_window);
		window_manager_window_set_virtual_desktop(xwl_window, 0);

		xwl_window->surface = wl_compositor_create_surface(xwl_screen->compositor);
		wl_surface_add_listener(xwl_window->surface, &surface_listener, xwl_window);

		xwl_window->shell_surface = wl_shell_get_shell_surface(xwl_screen->shell, xwl_window->surface);
		wl_shell_surface_add_listener(xwl_window->shell_surface, &shell_surface_listener, xwl_window);

		if(xwl_window->transient_for) {
			struct xwl_seat * xwl_seat_found;
			struct xwl_seat * seat_iterator;
			int parent_x, parent_y;
			struct xwl_window *window_iterator;

			xwl_seat_found = NULL;

			/**
			 * Find the staking order, if more than ones transient for is found
			 * The current window goes on top of the last one. Else the window
			 * will be above the transient for.
			 **/
			xwl_window->below_me = NULL;

			xorg_list_for_each_entry(window_iterator, &(xwl_window->transient_for->list_childdren), link_sibling) {
				/* if this is a not managed window, go above it */
				if(!window_iterator->frame) {
					xwl_window->below_me = window_iterator;
					break;
				}
			}

			/* if below still unset, go above transient_for */
			if(!xwl_window->below_me) {
				xwl_window->below_me = xwl_window->transient_for;
			}

			/** add current window to the sibling list */
			xorg_list_add(&(xwl_window->link_sibling), &(xwl_window->transient_for->list_childdren));

		    xorg_list_for_each_entry(seat_iterator, &xwl_screen->seat_list, link) {
		    	if((seat_iterator->focus_window == xwl_window->transient_for)
		    		|| (seat_iterator->focus_window == xwl_window->below_me)) {
		    		xwl_seat_found = seat_iterator;
		    		break;
		    	}
		    }

		    if(xwl_window->below_me->frame_window) {
		    	parent_x = xwl_window->below_me->frame_window->origin.x;
		    	parent_y = xwl_window->below_me->frame_window->origin.y;
		    } else {
		    	parent_x = xwl_window->below_me->client_window->origin.x;
		    	parent_y = xwl_window->below_me->client_window->origin.y;
		    }

		    if(xwl_seat_found) {
//		    	wl_shell_surface_set_popup(xwl_window->shell_surface,
//		    			xwl_seat_found->seat,
//						xwl_seat_found->pointer_enter_serial,
//						xwl_window->transient_for->surface,
//						xwl_window->client_window->origin.x - parent_x,
//						xwl_window->client_window->origin.y - parent_y,
//						0);
		    	wl_shell_surface_set_transient(xwl_window->shell_surface,
		    			xwl_window->below_me->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);
		    } else {
		    	wl_shell_surface_set_transient(xwl_window->shell_surface,
		    			xwl_window->below_me->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);
		    }
		} else {
			int parent_x, parent_y;
			/**
			 * if transient_for is not set, we guess this window is a child of
			 * the window under the cursor, if this last window belong to the same
			 * X11 client.
			 */
			struct xwl_seat *seat = xorg_list_first_entry(&(xwl_screen->seat_list), struct xwl_seat, link);

			if(seat->focus_window != NULL) {
			if(CLIENT_ID(seat->focus_window->client_window->drawable.id)
					== CLIENT_ID(xwl_window->client_window->drawable.id)) {

			    if(seat->focus_window->frame_window) {
			    	parent_x = seat->focus_window->frame_window->origin.x;
			    	parent_y = seat->focus_window->frame_window->origin.y;
			    } else {
			    	parent_x = seat->focus_window->client_window->origin.x;
			    	parent_y = seat->focus_window->client_window->origin.y;
			    }

				wl_shell_surface_set_transient(xwl_window->shell_surface,
						seat->focus_window->surface,
		    			xwl_window->client_window->origin.x - parent_x,
		    			xwl_window->client_window->origin.y - parent_y,
		    			0);

			} else {
				wl_shell_surface_set_toplevel(xwl_window->shell_surface);
			}

			} else {
				wl_shell_surface_set_toplevel(xwl_window->shell_surface);
			}
		}

		if(xwl_window->name)
			wl_shell_surface_set_title(xwl_window->shell_surface, xwl_window->name);

		wl_display_flush(xwl_screen->display);
		wl_surface_set_user_data(xwl_window->surface, xwl_window);

		LogWrite(0, "add windows to hash table\n");
		xwl_screen_add_window(xwl_screen, window->drawable.id, xwl_window);
		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);

		xwl_window->damage =
			DamageCreate(damage_report,
					damage_destroy, DamageReportNonEmpty,
					FALSE, xwl_screen->screen, xwl_window);
		DamageRegister(&xwl_window->client_window->drawable, xwl_window->damage);
		DamageSetReportAfterOp(xwl_window->damage, TRUE);

		xorg_list_init(&xwl_window->link_damage);

	}

	LogWrite(0, "END xwl_realize_window %d\n", window->drawable.id);


    /** handle realize later **/
    xwl_event = xwl_event_new();
    xwl_event->type = XWL_EVENT_MAP;
    xwl_event->data.map.window = window->drawable.id;
    xwl_event_queue_append(xwl_screen->event_queue, xwl_event);

	LogWrite(0, "END xwl_realize_window %d\n", window->drawable.id);
    return ret;
}

static void
xwl_screen_handle_unmap_window(struct xwl_screen *xwl_screen, struct xwl_window *xwl_window)
{
	WindowPtr child_iterator;
    struct xwl_seat *xwl_seat;
    int err;

	LogWrite(0, "xwl_screen_handle_unmap_window\n");

    if(!xorg_list_is_empty(&xwl_window->link_sibling)) {
    	xorg_list_del(&xwl_window->link_sibling);
    }

    if(!xorg_list_is_empty(&xwl_window->link_dirty)) {
    	xorg_list_del(&xwl_window->link_dirty);
    }

	xorg_list_del(&xwl_window->link_cleanup);
	UnmapWindow(xwl_window->frame_window, FALSE);

	/* remove all children before deleting the window */
	child_iterator = xwl_window->frame_window->firstChild;
	while(child_iterator) {
		WindowPtr next_iterator = child_iterator->nextSib;
		ReparentWindow(child_iterator, xwl_screen->screen->root, 0, 0, serverClient);
		child_iterator = next_iterator;
	}

    DeleteWindow(xwl_window->frame_window, xwl_window->frame_window->drawable.id);
    if(xwl_window->frame)
    	frame_destroy(xwl_window->frame);
	free(xwl_window);

}

static Bool
xwl_unrealize_window(WindowPtr window)
{
    ScreenPtr screen = window->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;
    struct xwl_seat *xwl_seat;
    struct xwl_event *xwl_event;
    Bool ret;

	LogWrite(0, "START xwl_unrealize_window %d (viewable:%s)\n", window->drawable.id, (window->viewable?"True":"False"));

    xwl_screen = xwl_screen_get(screen);

    xorg_list_for_each_entry(xwl_seat, &xwl_screen->seat_list, link) {
        if (xwl_seat->focus_window && xwl_seat->focus_window->client_window == window)
            xwl_seat->focus_window = NULL;
        xwl_seat_clear_touch(xwl_seat, window);
    }

    /** we know what to do with our windows **/
    if(CLIENT_ID(window->drawable.id) == 0) {
    	LogWrite(0, "END xwl_unrealize_window %d (viewable:%s) (Server Window)\n", window->drawable.id, (window->viewable?"True":"False"));
    	goto finish;
    }

    xwl_window = xwl_screen_find_window(xwl_screen, window->drawable.id);

//    xwl_window = dixLookupPrivate(&window->devPrivates, &xwl_window_private_key);

    if (!xwl_window) {
    	LogWrite(0, "END xwl_unrealize_window %d (viewable:%s) (Window Not found)\n", window->drawable.id, (window->viewable?"True":"False"));
    	goto finish;
    }

    dixSetPrivate(&window->devPrivates, &xwl_window_private_key, NULL);

    if(xwl_window->client_window)
     	xwl_screen_remove_window(xwl_screen, xwl_window->client_window->drawable.id);

    if(xwl_window->frame_window)
    	xwl_screen_remove_window(xwl_screen, xwl_window->frame_window->drawable.id);

    if(xwl_window->shell_surface)
    	wl_shell_surface_destroy(xwl_window->shell_surface);

    if (RegionNotEmpty(DamageRegion(xwl_window->damage)))
        xorg_list_del(&xwl_window->link_damage);
    DamageUnregister(xwl_window->damage);
    DamageDestroy(xwl_window->damage);
    if (xwl_window->frame_callback)
        wl_callback_destroy(xwl_window->frame_callback);

    wl_surface_destroy(xwl_window->surface);

    wl_display_flush(xwl_screen->display);

    xwl_event = xwl_event_new();
    xwl_event->type = XWL_EVENT_UNMAP;
    xwl_event->data.unmap.window = xwl_window;
    xwl_event_queue_append(xwl_screen->event_queue, xwl_event);

finish:
    screen->UnrealizeWindow = xwl_screen->UnrealizeWindow;
    ret = (*screen->UnrealizeWindow) (window);
    xwl_screen->UnrealizeWindow = screen->UnrealizeWindow;
    screen->UnrealizeWindow = xwl_unrealize_window;

	LogWrite(0, "END xwl_unrealize_window %d (viewable:%s) (Finish normaly)\n", window->drawable.id, (window->viewable?"True":"False"));
	return ret;
}

static Bool
xwl_save_screen(ScreenPtr pScreen, int on)
{
    return TRUE;
}

static void
frame_callback(void *data,
               struct wl_callback *callback,
               uint32_t time)
{
    struct xwl_window *xwl_window = data;
    xwl_window->frame_callback = NULL;
}

static const struct wl_callback_listener frame_listener = {
    frame_callback
};

static void
xwl_screen_post_damage(struct xwl_screen *xwl_screen)
{
    struct xwl_window *xwl_window, *next_xwl_window;
    RegionPtr region;
    BoxPtr box;
    struct wl_buffer *buffer;
    PixmapPtr pixmap;
    WindowPtr child_iterator;
    struct xorg_list tmp;
    struct xwl_event *event;

    while((event = xwl_event_queue_pop(xwl_screen->event_queue)) != NULL) {
    	switch(event->type) {
    	case XWL_EVENT_MAP:
    		xwl_screen_handle_map_event(xwl_screen, event->data.map.window);
    		break;
    	case XWL_EVENT_UNMAP:
    		xwl_screen_handle_unmap_window(xwl_screen, event->data.unmap.window);
    		break;
    	default:
    		break;
    	}

    	xwl_event_free(event);
    }


    xorg_list_for_each_entry_safe(xwl_window, next_xwl_window,
                                  &xwl_screen->dirty_list, link_dirty) {
    	xorg_list_del(&xwl_window->link_dirty);
        xwl_window_update_layout(xwl_window);
    }

    //LogWrite(0, "process damages\n");
    xorg_list_for_each_entry_safe(xwl_window, next_xwl_window,
                                  &xwl_screen->damage_window_list, link_damage) {

        /* If we're waiting on a frame callback from the server,
         * don't attach a new buffer. */
        if (xwl_window->frame_callback)
            continue;

        region = DamageRegion(xwl_window->damage);
        pixmap = (*xwl_screen->screen->GetWindowPixmap) (xwl_window->client_window);

#if GLAMOR_HAS_GBM
        if (xwl_screen->glamor)
            buffer = xwl_glamor_pixmap_get_wl_buffer(pixmap);
#endif
        if (!xwl_screen->glamor)
            buffer = xwl_shm_pixmap_get_wl_buffer(pixmap);


        wl_surface_attach(xwl_window->surface, buffer, 0, 0);

        box = RegionExtents(region);
        wl_surface_damage(xwl_window->surface, box->x1, box->y1,
                          box->x2 - box->x1, box->y2 - box->y1);
    	//LogWrite(0, "damage (%d,%d,%d,%d)\n", box->x1, box->y1,
         //                 box->x2 - box->x1, box->y2 - box->y1);

        xwl_window->frame_callback = wl_surface_frame(xwl_window->surface);
        wl_callback_add_listener(xwl_window->frame_callback, &frame_listener, xwl_window);

        //LogWrite(0, "commit (%d)\n", xwl_window->window->drawable.id);
        wl_surface_commit(xwl_window->surface);
        DamageEmpty(xwl_window->damage);

        xorg_list_del(&xwl_window->link_damage);
    }
}

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id,
                const char *interface, uint32_t version)
{
    struct xwl_screen *xwl_screen = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        xwl_screen->compositor =
            wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0) {
        xwl_screen->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, "wl_shell") == 0) {
        xwl_screen->shell =
            wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
    else if (strcmp(interface, "wl_output") == 0 && version >= 2) {
        xwl_output_create(xwl_screen, id);
        xwl_screen->expecting_event++;
    }
#ifdef GLAMOR_HAS_GBM
    else if (xwl_screen->glamor &&
             strcmp(interface, "wl_drm") == 0 && version >= 2) {
        xwl_screen_init_glamor(xwl_screen, id, version);
    }
#endif

}

static void
global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    struct xwl_screen *xwl_screen = data;
    struct xwl_output *xwl_output, *tmp_xwl_output;

    xorg_list_for_each_entry_safe(xwl_output, tmp_xwl_output,
                                  &xwl_screen->output_list, link) {
        if (xwl_output->server_output_id == name) {
            xwl_output_destroy(xwl_output);
            break;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    global_remove
};

static void
wakeup_handler(void *data, int err, void *read_mask)
{
    struct xwl_screen *xwl_screen = data;
    int ret;

    if (err < 0)
        return;

    if (!FD_ISSET(xwl_screen->wayland_fd, (fd_set *) read_mask))
        return;

    ret = wl_display_read_events(xwl_screen->display);
    if (ret == -1)
        FatalError("failed to dispatch Wayland events: %s\n", strerror(errno));

    xwl_screen->prepare_read = 0;

    ret = wl_display_dispatch_pending(xwl_screen->display);
    if (ret == -1)
        FatalError("failed to dispatch Wayland events: %s\n", strerror(errno));

}

static void
block_handler(void *data, struct timeval **tv, void *read_mask)
{
    struct xwl_screen *xwl_screen = data;
    int ret;
    xwl_screen_post_damage(xwl_screen);

    while (xwl_screen->prepare_read == 0 &&
           wl_display_prepare_read(xwl_screen->display) == -1) {
        ret = wl_display_dispatch_pending(xwl_screen->display);
        if (ret == -1)
            FatalError("failed to dispatch Wayland events: %s\n",
                       strerror(errno));
    }

    xwl_screen->prepare_read = 1;

    ret = wl_display_flush(xwl_screen->display);
    if (ret == -1)
        FatalError("failed to write to XWayland fd: %s\n", strerror(errno));

}

static CARD32
add_client_fd(OsTimerPtr timer, CARD32 time, void *arg)
{
    struct xwl_screen *xwl_screen = arg;

    if (!AddClientOnOpenFD(xwl_screen->wm_fd))
        FatalError("Failed to add wm client\n");

    TimerFree(timer);

    return 0;
}

static void
listen_on_fds(struct xwl_screen *xwl_screen)
{
    int i;

    for (i = 0; i < xwl_screen->listen_fd_count; i++)
        ListenOnOpenFD(xwl_screen->listen_fds[i], FALSE);
}

static void
wm_selection_callback(CallbackListPtr *p, void *data, void *arg)
{
    SelectionInfoRec *info = arg;
    struct xwl_screen *xwl_screen = data;
    static const char atom_name[] = "WM_S0";
    static Atom atom_wm_s0;

    if (atom_wm_s0 == None)
        atom_wm_s0 = MakeAtom(atom_name, strlen(atom_name), TRUE);
    if (info->selection->selection != atom_wm_s0 ||
        info->kind != SelectionSetOwner)
        return;

    listen_on_fds(xwl_screen);

    DeleteCallback(&SelectionCallback, wm_selection_callback, xwl_screen);
}

static Bool
xwl_screen_init(ScreenPtr pScreen, int argc, char **argv)
{
    struct xwl_screen *xwl_screen;
    Pixel red_mask, blue_mask, green_mask;
    int ret, bpc, green_bpc, i;

	LogWrite(0, "xwl_screen_init (%s)\n", display);

    xwl_screen = calloc(sizeof *xwl_screen, 1);
    if (xwl_screen == NULL)
        return FALSE;
    xwl_screen->lock_count = 0;
    xwl_screen->wm_fd = -1;

    if (!dixRegisterPrivateKey(&xwl_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;
    if (!dixRegisterPrivateKey(&xwl_window_private_key, PRIVATE_WINDOW, 0))
        return FALSE;
    if (!dixRegisterPrivateKey(&xwl_pixmap_private_key, PRIVATE_PIXMAP, 0))
        return FALSE;

    dixSetPrivate(&pScreen->devPrivates, &xwl_screen_private_key, xwl_screen);
    xwl_screen->screen = pScreen;

#ifdef GLAMOR_HAS_GBM
    xwl_screen->glamor = 1;
#endif

    for (i = 1; i < argc; i++) {
    	if (strcmp(argv[i], "-wm") == 0) {
            xwl_screen->wm_fd = atoi(argv[i + 1]);
            i++;
            TimerSet(NULL, 0, 1, add_client_fd, xwl_screen);
        }
        else if (strcmp(argv[i], "-listen") == 0) {
            if (xwl_screen->listen_fd_count ==
                ARRAY_SIZE(xwl_screen->listen_fds))
                FatalError("Too many -listen arguments given, max is %ld\n",
                           ARRAY_SIZE(xwl_screen->listen_fds));

            xwl_screen->listen_fds[xwl_screen->listen_fd_count++] =
                atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-shm") == 0) {
            xwl_screen->glamor = 0;
        }
    }

    /* rootless is mandatory */
    xwl_screen->rootless = 1;

    if (xwl_screen->listen_fd_count > 0) {
        if (xwl_screen->wm_fd >= 0)
            AddCallback(&SelectionCallback, wm_selection_callback, xwl_screen);
        else
            listen_on_fds(xwl_screen);
    }

    xorg_list_init(&xwl_screen->output_list);
    xorg_list_init(&xwl_screen->seat_list);
    xorg_list_init(&xwl_screen->damage_window_list);
    xorg_list_init(&xwl_screen->dirty_list);

    xwl_screen->clients_window_hash = ht_create(sizeof(XID), sizeof(void*), ht_resourceid_hash, ht_resourceid_compare, NULL);
    xwl_screen->depth = 24;

    xwl_screen->display = wl_display_connect(NULL);
    if (xwl_screen->display == NULL) {
        ErrorF("could not connect to wayland server\n");
        return FALSE;
    }

    if (!xwl_screen_init_output(xwl_screen))
        return FALSE;

    xwl_screen->expecting_event = 0;
    xwl_screen->registry = wl_display_get_registry(xwl_screen->display);
    wl_registry_add_listener(xwl_screen->registry,
                             &registry_listener, xwl_screen);
    ret = wl_display_roundtrip(xwl_screen->display);
    if (ret == -1) {
        ErrorF("could not connect to wayland server\n");
        return FALSE;
    }

    while (xwl_screen->expecting_event > 0)
        wl_display_roundtrip(xwl_screen->display);

    bpc = xwl_screen->depth / 3;
    green_bpc = xwl_screen->depth - 2 * bpc;
    blue_mask = (1 << bpc) - 1;
    green_mask = ((1 << green_bpc) - 1) << bpc;
    red_mask = blue_mask << (green_bpc + bpc);

    miSetVisualTypesAndMasks(xwl_screen->depth,
                             ((1 << TrueColor) | (1 << DirectColor)),
                             green_bpc, TrueColor,
                             red_mask, green_mask, blue_mask);

    miSetPixmapDepths();

    ret = fbScreenInit(pScreen, NULL,
                       xwl_screen->width, xwl_screen->height,
                       96, 96, 0,
                       BitsPerPixel(xwl_screen->depth));
    if (!ret)
        return FALSE;

    fbPictureInit(pScreen, 0, 0);

#ifdef HAVE_XSHMFENCE
    if (!miSyncShmScreenInit(pScreen))
        return FALSE;
#endif

    xwl_screen->wayland_fd = wl_display_get_fd(xwl_screen->display);
    AddGeneralSocket(xwl_screen->wayland_fd);
    RegisterBlockAndWakeupHandlers(block_handler, wakeup_handler, xwl_screen);

    pScreen->SaveScreen = xwl_save_screen;

    pScreen->blackPixel = 0;
    pScreen->whitePixel = 1;

    ret = fbCreateDefColormap(pScreen);

    if (!xwl_screen_init_cursor(xwl_screen))
        return FALSE;

#ifdef GLAMOR_HAS_GBM
    if (xwl_screen->glamor && !xwl_glamor_init(xwl_screen)) {
        ErrorF("Failed to initialize glamor, falling back to sw\n");
        xwl_screen->glamor = 0;
    }
#endif

    if (!xwl_screen->glamor) {
        xwl_screen->CreateScreenResources = pScreen->CreateScreenResources;
        pScreen->CreateScreenResources = xwl_shm_create_screen_resources;
        pScreen->CreatePixmap = xwl_shm_create_pixmap;
        pScreen->DestroyPixmap = xwl_shm_destroy_pixmap;
    }

    xwl_screen->RealizeWindow = pScreen->RealizeWindow;
    pScreen->RealizeWindow = xwl_realize_window;

    xwl_screen->UnrealizeWindow = pScreen->UnrealizeWindow;
    pScreen->UnrealizeWindow = xwl_unrealize_window;

//    xwl_screen->ClearToBackground = pScreen->ClearToBackground;
//    pScreen->ClearToBackground = xwl_clear_to_background;
//
//    xwl_screen->ClipNotify = pScreen->ClipNotify;
//    pScreen->ClipNotify = xwl_clip_notify;

    /* Check exposure to ensure frame drawing */
    xwl_screen->WindowExposures = pScreen->WindowExposures;
    pScreen->WindowExposures = xwl_window_exposures;

    xwl_screen->ChangeWindowAttributes = pScreen->ChangeWindowAttributes;
    pScreen->ChangeWindowAttributes = xwl_screen_change_window_attributes;

    xwl_screen->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = xwl_close_screen;

    xwl_screen->net_active_window = NULL;

    return ret;
}

_X_NORETURN
static void _X_ATTRIBUTE_PRINTF(1, 0)
xwl_log_handler(const char *format, va_list args)
{
    char msg[256];

    vsnprintf(msg, sizeof msg, format, args);
    FatalError("%s", msg);
}

static const ExtensionModule xwayland_extensions[] = {
#ifdef GLXEXT
    { GlxExtensionInit, "GLX", &noGlxExtension },
#endif
};

void
InitOutput(ScreenInfo * screen_info, int argc, char **argv)
{
    int depths[] = { 1, 4, 8, 15, 16, 24, 32 };
    int bpp[] =    { 1, 8, 8, 16, 16, 32, 32 };
    int i;

    LogWrite(0, "InitOutput\n");

    for (i = 0; i < ARRAY_SIZE(depths); i++) {
        screen_info->formats[i].depth = depths[i];
        screen_info->formats[i].bitsPerPixel = bpp[i];
        screen_info->formats[i].scanlinePad = BITMAP_SCANLINE_PAD;
    }

    screen_info->imageByteOrder = IMAGE_BYTE_ORDER;
    screen_info->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    screen_info->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    screen_info->bitmapBitOrder = BITMAP_BIT_ORDER;
    screen_info->numPixmapFormats = ARRAY_SIZE(depths);

    LoadExtensionList(xwayland_extensions,
                      ARRAY_SIZE(xwayland_extensions), FALSE);

    /* Cast away warning from missing printf annotation for
     * wl_log_func_t.  Wayland 1.5 will have the annotation, so we can
     * remove the cast and require that when it's released. */
    wl_log_set_handler_client((void *) xwl_log_handler);

    if (AddScreen(xwl_screen_init, argc, argv) == -1) {
        FatalError("Couldn't add screen\n");
    }

    LocalAccessScopeUser();
}

