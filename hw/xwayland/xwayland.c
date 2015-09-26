/*
 * Copyright © 2011-2014 Intel Corporation
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

#include "xwm/hash.h"

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

static void
shell_surface_ping(void *data,
                   struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_surface_configure(void *data,
                        struct wl_shell_surface *wl_shell_surface,
                        uint32_t edges, int32_t width, int32_t height)
{
}

static void
shell_surface_popup_done(void *data, struct wl_shell_surface *wl_shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    shell_surface_ping,
    shell_surface_configure,
    shell_surface_popup_done
};

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

static void
send_surface_id_event(struct xwl_window *xwl_window)
{
    static const char atom_name[] = "WL_SURFACE_ID";
    static Atom type_atom = None;
    DeviceIntPtr dev;
    xEvent e;

	LogWrite(0, "send surface id for %d\n", xwl_window->window->drawable.id);

    if (type_atom == None)
        type_atom = MakeAtom(atom_name, strlen(atom_name), TRUE);

    e.u.u.type = ClientMessage;
    e.u.u.detail = 32;
    e.u.clientMessage.window = xwl_window->window->drawable.id;
    e.u.clientMessage.u.l.type = type_atom;
    e.u.clientMessage.u.l.longs0 =
        wl_proxy_get_id((struct wl_proxy *) xwl_window->surface);
    e.u.clientMessage.u.l.longs1 = 0;
    e.u.clientMessage.u.l.longs2 = 0;
    e.u.clientMessage.u.l.longs3 = 0;
    e.u.clientMessage.u.l.longs4 = 0;

    dev = PickPointer(serverClient);
    DeliverEventsToWindow(dev, xwl_window->xwl_screen->screen->root,
                          &e, 1, SubstructureRedirectMask, NullGrab);
}

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
xwl_realize_window(WindowPtr window)
{
	ScreenPtr screen = window->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;
    struct wl_region *region;
    Bool ret;

	LogWrite(0, "xwl_realize_window %d\n", window->drawable.id);

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

    if(window->parent != screen->root && window->parent != 0)
    	return ret;

    xwl_window = hash_table_lookup(xwl_screen->window_hash, window->drawable.id);



    if(CLIENT_ID(window->drawable.id) != 0 && !xwl_window) {
    	XID values[4];
    	int err = 0;
    	int x, y;

		/* register this window */
		LogWrite(0, "create xwl_window for %d\n", window->drawable.id);
		xwl_window = calloc(sizeof *xwl_window, 1);

    	LogWrite(0, "external client %d\n", window->drawable.id);

    	xwl_window->frame = frame_create(xwl_screen->wm->theme,
    			window->drawable.width,
				window->drawable.height, 0, "title");

    	frame_resize_inside(xwl_window->frame, window->drawable.width, window->drawable.height);
    	frame_interior(xwl_window->frame, &x, &y, NULL, NULL);

    	xGetWindowAttributesReply attr;
    	GetWindowAttributes(window, serverClient, &attr);

    	LogWrite(0, "Override = %d\n", attr.override);

    	if(!attr.override) {
			values[0] = screen->blackPixel;
			values[1] = screen->blackPixel;
			values[2] =
					KeyPressMask|
					KeyReleaseMask|
					ButtonPressMask|
					ButtonReleaseMask|
					PointerMotionMask|
					EnterWindowMask|
					LeaveWindowMask|
					SubstructureNotifyMask|
					SubstructureRedirectMask;

			values[3] = xwl_screen->wm->colormap_id;

			xwl_window->frame_window = CreateWindow(FakeClientID(0), screen->root,
					0, 0,
					frame_width(xwl_window->frame),
					frame_height(xwl_window->frame),
					0, InputOutput,
					CWBackPixel|CWBorderPixel|CWEventMask|CWColormap, values,
					32, serverClient, xwl_screen->wm->visual->vid, &err);

			LogWrite(0, "Frame = %p, err = %d\n", xwl_window->frame_window, err);


			xwl_window->xwl_screen = xwl_screen;
			xwl_window->window = window;
			xwl_window->surface = NULL;
			xwl_window->shell_surface = NULL;

			ReparentWindow(window, xwl_window->frame_window, x, y, serverClient);
			MapWindow(window, serverClient);

			hash_table_insert(xwl_screen->window_hash, window->drawable.id, xwl_window);
			hash_table_insert(xwl_screen->window_hash, xwl_window->frame_window->drawable.id, xwl_window);

			MapWindow(xwl_window->frame_window, serverClient);

    	}
	} else if (xwl_window && !xwl_window->surface) {
		cairo_surface_t * sbuff;
		cairo_surface_t * out;
		cairo_t * cr;
		pixman_box16_t * rects;
		int nrect, i;
		PixmapPtr pixmap;
		struct xwl_pixmap * xwl_pixmap;

		LogWrite(0, "internal window %d\n", window->drawable.id);

		/** Immediately redirect this window **/
		compRedirectWindow(serverClient, xwl_window->frame_window, CompositeRedirectManual);

		if (window->redirectDraw != RedirectDrawManual) {
			LogWrite(0, "unexpected redirect: ");
			switch(window->redirectDraw) {
			case RedirectDrawNone:
				LogWrite(0, "RedirectDrawNone\n");
				break;
			case RedirectDrawAutomatic:
				LogWrite(0, "RedirectDrawAutomatic\n");
				break;
			case RedirectDrawManual:
				LogWrite(0, "RedirectDrawManual\n");
				break;
			}
			return ret;
		}

		sbuff = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, frame_width(xwl_window->frame), frame_height(xwl_window->frame));
		cr = cairo_create(sbuff);
		frame_repaint(xwl_window->frame, cr);
		cairo_destroy(cr);

		pixmap = (*screen->GetWindowPixmap)(xwl_window->frame_window);
		xwl_pixmap = xwl_pixmap_get(pixmap);

		out = cairo_image_surface_create_for_data(xwl_pixmap->data, CAIRO_FORMAT_ARGB32, pixmap->drawable.width, pixmap->drawable.height, pixmap->devKind);
		cr = cairo_create(out);
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_paint(cr);

		rects = pixman_region_rectangles(&(xwl_window->frame_window->clipList), &nrect);
		for(i = 0; i < nrect; ++i) {
			cairo_rectangle(cr, rects[i].x1, rects[i].y1, rects[i].x2 - rects[i].x1, rects[i].y2 - rects[i].y1);
			cairo_clip(cr);
			cairo_set_source_surface(cr, sbuff, 0, 0);
			cairo_paint(cr);
		}

		cairo_destroy(cr);
		cairo_surface_destroy(out);
		cairo_surface_destroy(sbuff);

		xwl_window->surface = wl_compositor_create_surface(xwl_screen->compositor);
		xwl_window->shell_surface = wl_shell_get_shell_surface(xwl_screen->shell, xwl_window->surface);
		wl_shell_surface_set_toplevel(xwl_window->shell_surface);

		wl_display_flush(xwl_screen->display);
		wl_surface_set_user_data(xwl_window->surface, xwl_window);

		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);

		xwl_window->damage =
			DamageCreate(damage_report, damage_destroy, DamageReportNonEmpty,
						 FALSE, screen, xwl_window);
		DamageRegister(&window->drawable, xwl_window->damage);
		DamageSetReportAfterOp(xwl_window->damage, TRUE);

		xorg_list_init(&xwl_window->link_damage);


//
//		if (xwl_window->surface == NULL) {
//			ErrorF("wl_display_create_surface failed\n");
//			return FALSE;
//		}
//
//		wl_display_flush(xwl_screen->display);
//		wl_surface_set_user_data(xwl_window->surface, xwl_window);
//
//		dixSetPrivate(&window->devPrivates, &xwl_window_private_key, xwl_window);
//
//		xwl_window->damage =
//			DamageCreate(damage_report, damage_destroy, DamageReportNonEmpty,
//						 FALSE, screen, xwl_window);
//		DamageRegister(&window->drawable, xwl_window->damage);
//		DamageSetReportAfterOp(xwl_window->damage, TRUE);
//
//		xorg_list_init(&xwl_window->link_damage);
//		hash_table_insert(xwl_screen->window_hash, xwl_window->window->drawable.id, xwl_window);
		//send_surface_id_event(xwl_window);

	}
    return ret;
}

static Bool
xwl_unrealize_window(WindowPtr window)
{
    ScreenPtr screen = window->drawable.pScreen;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;
    struct xwl_seat *xwl_seat;
    Bool ret;

    xwl_screen = xwl_screen_get(screen);

	LogWrite(0, "xwl_unrealize_window %d\n", window->drawable.id);

    xorg_list_for_each_entry(xwl_seat, &xwl_screen->seat_list, link) {
        if (xwl_seat->focus_window && xwl_seat->focus_window->window == window)
            xwl_seat->focus_window = NULL;

        xwl_seat_clear_touch(xwl_seat, window);
    }

    screen->UnrealizeWindow = xwl_screen->UnrealizeWindow;
    ret = (*screen->UnrealizeWindow) (window);
    xwl_screen->UnrealizeWindow = screen->UnrealizeWindow;
    screen->UnrealizeWindow = xwl_unrealize_window;

    xwl_window =
        dixLookupPrivate(&window->devPrivates, &xwl_window_private_key);
    if (!xwl_window) {
        return ret;
    }

    hash_table_remove(xwl_screen->window_hash, xwl_window->window->drawable.id);
    hash_table_remove(xwl_screen->window_hash, xwl_window->frame_window->drawable.id);

    if(xwl_window->shell_surface)
    	wl_shell_surface_destroy(xwl_window->shell_surface);

    wl_surface_destroy(xwl_window->surface);
    if (RegionNotEmpty(DamageRegion(xwl_window->damage)))
        xorg_list_del(&xwl_window->link_damage);
    DamageUnregister(xwl_window->damage);
    DamageDestroy(xwl_window->damage);
    if (xwl_window->frame_callback)
        wl_callback_destroy(xwl_window->frame_callback);

    free(xwl_window);
    dixSetPrivate(&window->devPrivates, &xwl_window_private_key, NULL);
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

    //LogWrite(0, "process damages\n");
    xorg_list_for_each_entry_safe(xwl_window, next_xwl_window,
                                  &xwl_screen->damage_window_list, link_damage) {

        /* If we're waiting on a frame callback from the server,
         * don't attach a new buffer. */
        if (xwl_window->frame_callback)
            continue;

        region = DamageRegion(xwl_window->damage);
        pixmap = (*xwl_screen->screen->GetWindowPixmap) (xwl_window->window);

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

    pthread_mutex_init(&(xwl_screen->window_hash_lock), NULL);
    xwl_screen->window_hash = hash_table_create();
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

    xwl_screen->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = xwl_close_screen;

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

