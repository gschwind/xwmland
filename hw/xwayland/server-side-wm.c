/*
 * server-side-wm.c
 *
 *  Created on: 28 sept. 2015
 *      Author: gschwind
 */

#include <unistd.h>

#include <X11/Xatom.h>

#include "server-side-wm.h"
#include "client-side-wm.h"
#include "xwl_screen.h"
#include "xwl_window.h"

#include "xwayland.h"

#include <wayland-client-protocol.h>

#include "windowstr.h"
#include "gc.h"
#include "gcstruct.h"

#include "property.h"
#include "propertyst.h"

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif


void xwl_screen_setup_net_supported(struct xwl_screen *xwl_screen) {
	XID supported[8];

	supported[0] = xwl_screen->atom.net_wm_moveresize;
	supported[1] = xwl_screen->atom.net_wm_state;
	supported[2] = xwl_screen->atom.net_wm_state_fullscreen;
	supported[3] = xwl_screen->atom.net_wm_state_maximized_vert;
	supported[4] = xwl_screen->atom.net_wm_state_maximized_horz;
	supported[5] = xwl_screen->atom.net_active_window;
	supported[6] = xwl_screen->atom.net_wm_window_type;
	supported[7] = xwl_screen->atom.net_wm_window_type_normal;

	ChangeWindowProperty(xwl_screen->screen->root,
			xwl_screen->atom.net_supported,
			XA_ATOM,
			32,
			PropModeReplace,
			ARRAY_LENGTH(supported),
			supported,
			True);

}


void ClientLogWrite(int verb, const char *f, ...) {
    va_list args;

    va_start(args, f);
    LogVWrite(verb, f, args);
    va_end(args);
}

void xwl_window_send_focus_window(struct xwl_window *xwl_window)
{
	DeviceIntPtr dev;
	xEvent e;

	if (xwl_window->client_window) {
		uint32_t values[1];

		if (xwl_window->override_redirect)
			return;

		LogWrite(0, "xwl_window_send_focus_window for %d\n", xwl_window->client_window->drawable.id);

		e.u.u.type = ClientMessage;
		e.u.u.detail = 32;
		e.u.clientMessage.window = xwl_window->client_window->drawable.id;
		e.u.clientMessage.u.l.type = xwl_window->xwl_screen->atom.wm_protocols;
		e.u.clientMessage.u.l.longs0 = xwl_window->xwl_screen->atom.wm_take_focus;
		e.u.clientMessage.u.l.longs1 = GetTimeInMillis();
		e.u.clientMessage.u.l.longs2 = 0;
		e.u.clientMessage.u.l.longs3 = 0;
		e.u.clientMessage.u.l.longs4 = 0;

		dev = PickKeyboard(serverClient);
		DeliverEventsToWindow(dev, xwl_window->client_window,
							  &e, 1, SubstructureRedirectMask, NullGrab);

		SetInputFocus(serverClient, dev, xwl_window->client_window->drawable.id,
				RevertToNone, GetTimeInMillis(), 0);

		values[0] = Above;
		ConfigureWindow(xwl_window->client_window, CWStackMode, values, serverClient);
	} else {
		dev = PickKeyboard(serverClient);
		SetInputFocus(serverClient, dev, None, RevertToNone, GetTimeInMillis(), 0);
	}
}

void xwl_window_raise_with_childdren(struct xwl_window * xwl_window) {
	struct xwl_window * iterator;
	XID values = Above;

	LogWrite(0, "restack 0x%x\n", xwl_window->client_window->drawable.id);

	/* Raise the window on top */
	if(xwl_window->frame_window) {
		ConfigureWindow(xwl_window->frame_window, CWStackMode, &values, serverClient);
	} else {
		ConfigureWindow(xwl_window->client_window, CWStackMode, &values, serverClient);
	}

	xorg_list_for_each_entry(iterator, &(xwl_window->list_childdren), link_sibling) {
		xwl_window_raise_with_childdren(iterator);
	}

}

void xwl_screen_window_activate(struct xwl_screen *xwl_screen, struct xwl_window *xwl_window)
{

	if(xwl_screen->net_active_window != NULL) {
		frame_unset_flag(xwl_window->frame, FRAME_FLAG_ACTIVE);
	}

	if(xwl_window && xwl_window->frame) {

		if(xwl_screen->net_active_window != xwl_window) {
			xwl_window_raise_with_childdren(xwl_window);
			xwl_window_send_focus_window(xwl_window);

			ChangeWindowProperty(xwl_screen->screen->root,
					xwl_screen->atom.net_active_window,
					XA_WINDOW,
					32,
					PropModeReplace,
					1,
					&xwl_window->client_window->drawable.id,
					True);
		}
		frame_set_flag(xwl_window->frame, FRAME_FLAG_ACTIVE);
		xwl_window->layout_is_dirty = 1;
	} else if (!xwl_window) {
		XID value = None;

		ChangeWindowProperty(xwl_screen->screen->root,
				xwl_screen->atom.net_active_window,
				XA_WINDOW,
				32,
				PropModeReplace,
				1,
				&value,
				True);

	}

	xwl_screen->net_active_window = xwl_window;

}

Bool xwl_window_is_maximized(struct xwl_window *window)
{
	return window->maximized_horz && window->maximized_vert;
}


void xwl_window_update_layout(struct xwl_window * xwl_window) {
    struct wl_region *region;
    uint32_t x, y, w, h;
    XID values[6];

    if(!xwl_window->frame)
    	return;

    frame_resize_inside(xwl_window->frame,
    		xwl_window->client_window->drawable.width,
			xwl_window->client_window->drawable.height);

	if(!xwl_window->has_32b_visual) {
		DeviceIntPtr dev;
		xEvent e;

		frame_interior(xwl_window->frame, &x, &y, &w, &h);
		region = wl_compositor_create_region(xwl_window->xwl_screen->compositor);
		wl_region_add(region, x, y, w, h);
		wl_surface_set_opaque_region(xwl_window->surface, region);
		wl_region_destroy(region);

		values[0] = x;
		values[1] = y;
		values[2] = w;
		values[3] = h;
		ConfigureWindow(xwl_window->client_window, CWX|CWY|CWWidth|CWHeight, values, serverClient);

		e.u.u.type = ConfigureNotify|0x80;
		e.u.u.detail = 32;
		e.u.configureNotify.window = xwl_window->client_window->drawable.id;
		e.u.configureNotify.aboveSibling = None;
		e.u.configureNotify.borderWidth = 0;
		e.u.configureNotify.height = h;
		e.u.configureNotify.width = w;
		e.u.configureNotify.x = x;
		e.u.configureNotify.y = y;
		e.u.configureNotify.event = xwl_window->client_window->drawable.id;
		e.u.configureNotify.override = FALSE;

		dev = PickPointer(serverClient);
		DeliverEventsToWindow(dev, xwl_window->client_window,
							  &e, 1, StructureNotifyMask, NullGrab);

	} else {
		region = wl_compositor_create_region(xwl_window->xwl_screen->compositor);
		wl_surface_set_opaque_region(xwl_window->surface, region);
		wl_region_destroy(region);
	}

	frame_input_rect(xwl_window->frame, &x, &y, &w, &h);
	region = wl_compositor_create_region(xwl_window->xwl_screen->compositor);
	wl_region_add(region, x, y, w, h);
	wl_surface_set_input_region(xwl_window->surface, region);
	wl_region_destroy(region);



	xwl_window_draw_decoration(xwl_window);



}

int
get_cursor_for_location(enum theme_location location)
{
	// int location = theme_get_location(t, x, y, width, height, 0);

	switch (location) {
		case THEME_LOCATION_RESIZING_TOP:
			return XWM_CURSOR_TOP;
		case THEME_LOCATION_RESIZING_BOTTOM:
			return XWM_CURSOR_BOTTOM;
		case THEME_LOCATION_RESIZING_LEFT:
			return XWM_CURSOR_LEFT;
		case THEME_LOCATION_RESIZING_RIGHT:
			return XWM_CURSOR_RIGHT;
		case THEME_LOCATION_RESIZING_TOP_LEFT:
			return XWM_CURSOR_TOP_LEFT;
		case THEME_LOCATION_RESIZING_TOP_RIGHT:
			return XWM_CURSOR_TOP_RIGHT;
		case THEME_LOCATION_RESIZING_BOTTOM_LEFT:
			return XWM_CURSOR_BOTTOM_LEFT;
		case THEME_LOCATION_RESIZING_BOTTOM_RIGHT:
			return XWM_CURSOR_BOTTOM_RIGHT;
		case THEME_LOCATION_EXTERIOR:
		case THEME_LOCATION_TITLEBAR:
		default:
			return XWM_CURSOR_LEFT_PTR;
	}
}

void
xwl_window_draw_decoration(struct xwl_window *xwl_window) {
	int ret;
	GCPtr gc;
	XID gcid;
	char * data;

	if(xwl_window->frame_window == NULL || xwl_window->frame == NULL) {
		LogWrite(0, "unexpected call of xwl_window_draw_decoration\n");
		return;
	}

	if(xwl_window->frame_window->redirectDraw != RedirectDrawManual) {
		LogWrite(0, "NOT 0 RedirectDrawManual\n");
		return;
	}

	/* TODO: change the API to alloc buffer here */
	data = window_manager_window_draw_frame(xwl_window->frame);

	gcid = FakeClientID(0);
	gc = CreateGC(&(xwl_window->frame_window->drawable), 0, NULL, &ret, gcid, serverClient);

	/* MANDATORY, will finish the creation of the GC */
	ValidateGC(&(xwl_window->frame_window->drawable), gc);

	/* write the buffer to the window */
	(*gc->ops->PutImage)(
			&(xwl_window->frame_window->drawable),
			gc,
			32,
			0,
			0,
			frame_width(xwl_window->frame),
			frame_height(xwl_window->frame),
			0,
			ZPixmap,
			(char*)data
			);

	FreeGC((void*)gc, gcid);
	free(data);

}

void send_wm_delete_window(struct xwl_window * xwl_window) {
	DeviceIntPtr dev;
	xEvent e;

	LogWrite(0, "send_wm_delete_window for %d\n", xwl_window->client_window->drawable.id);

	e.u.u.type = ClientMessage;
	e.u.u.detail = 32;
	e.u.clientMessage.window = xwl_window->client_window->drawable.id;
	e.u.clientMessage.u.l.type = xwl_window->xwl_screen->atom.wm_protocols;
	e.u.clientMessage.u.l.longs0 = xwl_window->xwl_screen->atom.wm_delete_window;
	e.u.clientMessage.u.l.longs1 = GetTimeInMillis();
	e.u.clientMessage.u.l.longs2 = 0;
	e.u.clientMessage.u.l.longs3 = 0;
	e.u.clientMessage.u.l.longs4 = 0;

	dev = PickPointer(serverClient);
	DeliverEventsToWindow(dev, xwl_window->client_window,
						  &e, 1, NoEventMask, NullGrab);

}


void window_manager_get_resources(struct xwl_screen *wm)
{
	int i;

#define F(field) offsetof(struct xwl_screen, field)

	static const struct { const char *name; int offset; } atoms[] = {
		{ "WM_PROTOCOLS",	F(atom.wm_protocols) },
		{ "WM_NORMAL_HINTS",	F(atom.wm_normal_hints) },
		{ "WM_TAKE_FOCUS",	F(atom.wm_take_focus) },
		{ "WM_DELETE_WINDOW",	F(atom.wm_delete_window) },
		{ "WM_STATE",		F(atom.wm_state) },
		{ "WM_S0",		F(atom.wm_s0) },
		{ "WM_CLIENT_MACHINE",	F(atom.wm_client_machine) },
		{ "_NET_WM_CM_S0",	F(atom.net_wm_cm_s0) },
		{ "_NET_WM_NAME",	F(atom.net_wm_name) },
		{ "_NET_WM_PID",	F(atom.net_wm_pid) },
		{ "_NET_WM_ICON",	F(atom.net_wm_icon) },
		{ "_NET_WM_STATE",	F(atom.net_wm_state) },
		{ "_NET_WM_STATE_MAXIMIZED_VERT", F(atom.net_wm_state_maximized_vert) },
		{ "_NET_WM_STATE_MAXIMIZED_HORZ", F(atom.net_wm_state_maximized_horz) },
		{ "_NET_WM_STATE_FULLSCREEN", F(atom.net_wm_state_fullscreen) },
		{ "_NET_WM_USER_TIME", F(atom.net_wm_user_time) },
		{ "_NET_WM_ICON_NAME", F(atom.net_wm_icon_name) },
		{ "_NET_WM_DESKTOP", F(atom.net_wm_desktop) },
		{ "_NET_ACTIVE_WINDOW", F(atom.net_active_window) },
		{ "_NET_WM_WINDOW_TYPE", F(atom.net_wm_window_type) },

		{ "_NET_WM_WINDOW_TYPE_DESKTOP", F(atom.net_wm_window_type_desktop) },
		{ "_NET_WM_WINDOW_TYPE_DOCK", F(atom.net_wm_window_type_dock) },
		{ "_NET_WM_WINDOW_TYPE_TOOLBAR", F(atom.net_wm_window_type_toolbar) },
		{ "_NET_WM_WINDOW_TYPE_MENU", F(atom.net_wm_window_type_menu) },
		{ "_NET_WM_WINDOW_TYPE_UTILITY", F(atom.net_wm_window_type_utility) },
		{ "_NET_WM_WINDOW_TYPE_SPLASH", F(atom.net_wm_window_type_splash) },
		{ "_NET_WM_WINDOW_TYPE_DIALOG", F(atom.net_wm_window_type_dialog) },
		{ "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", F(atom.net_wm_window_type_dropdown) },
		{ "_NET_WM_WINDOW_TYPE_POPUP_MENU", F(atom.net_wm_window_type_popup) },
		{ "_NET_WM_WINDOW_TYPE_TOOLTIP", F(atom.net_wm_window_type_tooltip) },
		{ "_NET_WM_WINDOW_TYPE_NOTIFICATION", F(atom.net_wm_window_type_notification) },
		{ "_NET_WM_WINDOW_TYPE_COMBO", F(atom.net_wm_window_type_combo) },
		{ "_NET_WM_WINDOW_TYPE_DND", F(atom.net_wm_window_type_dnd) },
		{ "_NET_WM_WINDOW_TYPE_NORMAL",	F(atom.net_wm_window_type_normal) },

		{ "_NET_WM_MOVERESIZE", F(atom.net_wm_moveresize) },
		{ "_NET_SUPPORTING_WM_CHECK",
					F(atom.net_supporting_wm_check) },
		{ "_NET_SUPPORTED",     F(atom.net_supported) },
		{ "_NET_CLOSE_WINDOW",     F(atom.net_close_window) },
		{ "_MOTIF_WM_HINTS",	F(atom.motif_wm_hints) },
		{ "CLIPBOARD",		F(atom.clipboard) },
		{ "CLIPBOARD_MANAGER",	F(atom.clipboard_manager) },
		{ "TARGETS",		F(atom.targets) },
		{ "UTF8_STRING",	F(atom.utf8_string) },
		{ "_WL_SELECTION",	F(atom.wl_selection) },
		{ "INCR",		F(atom.incr) },
		{ "TIMESTAMP",		F(atom.timestamp) },
		{ "MULTIPLE",		F(atom.multiple) },
		{ "UTF8_STRING"	,	F(atom.utf8_string) },
		{ "COMPOUND_TEXT",	F(atom.compound_text) },
		{ "TEXT",		F(atom.text) },
		{ "STRING",		F(atom.string) },
		{ "text/plain;charset=utf-8",	F(atom.text_plain_utf8) },
		{ "text/plain",		F(atom.text_plain) },
		{ "XdndSelection",	F(atom.xdnd_selection) },
		{ "XdndAware",		F(atom.xdnd_aware) },
		{ "XdndEnter",		F(atom.xdnd_enter) },
		{ "XdndLeave",		F(atom.xdnd_leave) },
		{ "XdndDrop",		F(atom.xdnd_drop) },
		{ "XdndStatus",		F(atom.xdnd_status) },
		{ "XdndFinished",	F(atom.xdnd_finished) },
		{ "XdndTypeList",	F(atom.xdnd_type_list) },
		{ "XdndActionCopy",	F(atom.xdnd_action_copy) },
		{ "WL_SURFACE_ID",	F(atom.wl_surface_id) }
	};
#undef F

	for (i = 0; i < ARRAY_LENGTH(atoms); i++) {
		*(Atom *) ((char *) wm + atoms[i].offset) = MakeAtom(atoms[i].name, strlen(atoms[i].name), TRUE);
	}

}

Bool visual_is_depth_32(struct xwl_screen *wm, unsigned long visual) {
	int i, j;

	for(i = 0; i < wm->screen->numDepths; ++i) {
		if(wm->screen->allowedDepths[i].depth == 32) {
			DepthPtr depth = &(wm->screen->allowedDepths[i]);
			for(j = 0; j < depth->numVids; ++j) {
				if(visual == depth->vids[j]) {
					return True;
				}
			}
		}
	}

	return False;

}

void window_manager_get_visual_and_colormap(struct xwl_screen *wm)
{
	int i;

	VisualID visual_id = 0;
	for(i = 0; i < wm->screen->numDepths; ++i) {
		LogWrite(0, "DEPTH: depth %d, nvis %d\n",
				(int)wm->screen->allowedDepths[i].depth,
				(int)wm->screen->allowedDepths[i].numVids);
		if(wm->screen->allowedDepths[i].depth == 32) {
			visual_id = wm->screen->allowedDepths[i].vids[0];
			break;
		}
	}

	for(;i < wm->screen->numDepths; ++i) {
		LogWrite(0, "DEPTH: depth %d, nvis %d\n",
				(int)wm->screen->allowedDepths[i].depth,
				(int)wm->screen->allowedDepths[i].numVids);
	}

	LogWrite(0, "Found visual: vid %d\n", (int)visual_id);

	for(i = 0; i < wm->screen->numVisuals; ++i) {
		LogWrite(0, "VISUAL: vid %d, class %d\n", (int)wm->screen->visuals[i].vid, (int)wm->screen->visuals[i].class);
		if((int)wm->screen->visuals[i].vid == (int)visual_id) {
			wm->visual = &(wm->screen->visuals[i]);
			break;
		}
	}

	for(;i < wm->screen->numVisuals; ++i) {
		LogWrite(0, "VISUAL: vid %d, class %d, bits %d\n",
				(int)wm->screen->visuals[i].vid,
				(int)wm->screen->visuals[i].class,
				(int)wm->screen->visuals[i].bitsPerRGBValue);
	}

	/* TODO: check */
	wm->colormap_id = FakeClientID(0);
	CreateColormap(wm->colormap_id, wm->screen, wm->visual,
	               &wm->colormap, AllocNone, 0/*serverClient*/);

}

void window_manager_window_set_wm_state(struct xwl_window *window, int32_t state)
{
	struct xwl_screen *wm = window->xwl_screen;
	uint32_t property[2];

	property[0] = state;
	property[1] = None;
	ChangeWindowProperty(window->client_window,
			wm->atom.wm_state,
			wm->atom.wm_state,
			32,
			PropModeReplace,
			2,
			property,
			True);

}

void window_manager_window_set_net_wm_state(struct xwl_window *window)
{
	struct xwl_screen *wm = window->xwl_screen;
	uint32_t property[3];
	int i;

	i = 0;
	if (window->fullscreen)
		property[i++] = wm->atom.net_wm_state_fullscreen;
	if (window->maximized_vert)
		property[i++] = wm->atom.net_wm_state_maximized_vert;
	if (window->maximized_horz)
		property[i++] = wm->atom.net_wm_state_maximized_horz;

	ChangeWindowProperty(window->client_window,
		    wm->atom.net_wm_state,
			XA_ATOM,
				32,
				PropModeReplace,
				i,
				property,
				True);

}

/*
 * Sets the _NET_WM_DESKTOP property for the window to 'desktop'.
 * Passing a <0 desktop value deletes the property.
 */
void
window_manager_window_set_virtual_desktop(struct xwl_window *window,
				     int desktop)
{
	struct xwl_screen *wm = window->xwl_screen;

	if (desktop >= 0) {
		ChangeWindowProperty(window->client_window,
				wm->atom.net_wm_desktop,
				XA_CARDINAL,
					32,
					PropModeReplace,
					1,
					&desktop,
					True);
	} else {
		DeleteProperty(serverClient, window->client_window, wm->atom.net_wm_desktop);
	}
}


void
window_manager_window_read_properties(struct xwl_window *window)
{
	struct xwl_screen *wm = window->xwl_screen;

#define F(field) offsetof(struct xwl_window, field)
	const struct {
		Atom atom;
		Atom type;
		int offset;
	} props[] = {
		{ XA_WM_CLASS, XA_STRING, F(class_) },
		{ XA_WM_NAME, XA_STRING, F(name) },
		{ XA_WM_TRANSIENT_FOR, XA_WINDOW, F(transient_for) },
		{ wm->atom.wm_protocols, TYPE_WM_PROTOCOLS, F(protocols) },
		{ wm->atom.wm_normal_hints, TYPE_WM_NORMAL_HINTS, F(protocols) },
		{ wm->atom.net_wm_state, TYPE_NET_WM_STATE },
		{ wm->atom.net_wm_window_type, XA_ATOM, F(type) },
		{ wm->atom.net_wm_name, XA_STRING, F(name) },
		{ wm->atom.net_wm_pid, XA_CARDINAL, F(pid) },
		{ wm->atom.motif_wm_hints, TYPE_MOTIF_WM_HINTS, 0 },
		{ wm->atom.wm_client_machine, XA_WM_CLIENT_MACHINE, F(machine) },
	};
#undef F

	void *p;
	uint32_t *xid;
	Atom *atom;
	uint32_t i, k;
	char name[1024];

	if (!window->properties_dirty)
		return;
	window->properties_dirty = 0;

	window->decorate = window->override_redirect ? 0 : MWM_DECOR_EVERYTHING;
	window->size_hints.flags = 0;
	window->motif_hints.flags = 0;
	window->delete_window = 0;

	for (k = 0; k < ARRAY_LENGTH(props); k++) {
		PropertyPtr prop = NULL;
		int rc = 0;

		rc = dixLookupProperty(&prop, window->client_window, props[k].atom, serverClient, DixReadAccess);
		if(rc) {
			//LogWrite(0, "error while reading %s : %d\n", NameForAtom(props[k].atom), rc);
			continue;
		}

		if(!prop) {
			//LogWrite(0, "not found %s : %d\n", NameForAtom(props[k].atom), rc);
			continue;
		}

		//LogWrite(0, "prop found %s(%s)\n", NameForAtom(prop->propertyName), NameForAtom(prop->type));

		if (prop->type == None) {
			/* No such property */
			continue;
		}

		p = ((char *) window + props[k].offset);

		switch (props[k].type) {
		case XA_WM_CLIENT_MACHINE:
		case XA_STRING:
			/* FIXME: We're using this for both string and
			   utf8_string */
			if (*(char **) p)
				free(*(char **) p);

			*(char **) p =
				strndup(prop->data, prop->size);
			break;
		case XA_WINDOW:
			xid = prop->data;
			*(struct xwl_window**)p = xwl_screen_find_window(window->xwl_screen, *xid);

			if (!*(struct xwl_window**)p)
				LogWrite(0, "XCB_ATOM_WINDOW contains window"
					   " id not found in hash table.\n");

			break;
		case XA_CARDINAL:
		case XA_ATOM:
			*(Atom *) p = *((Atom *)prop->data);
			break;
		case TYPE_WM_PROTOCOLS:
			atom = prop->data;
			for (i = 0; i < prop->size; i++)
				if (atom[i] == wm->atom.wm_delete_window) {
					window->delete_window = 1;
					break;
				}
			break;
		case TYPE_WM_NORMAL_HINTS:
			memcpy(&window->size_hints,
			       prop->data,
			       sizeof window->size_hints);
			break;
		case TYPE_NET_WM_STATE:
			window->fullscreen = 0;
			atom = prop->data;
			for (i = 0; i < prop->size; i++) {
				if (atom[i] == wm->atom.net_wm_state_fullscreen)
					window->fullscreen = 1;
				if (atom[i] == wm->atom.net_wm_state_maximized_vert)
					window->maximized_vert = 1;
				if (atom[i] == wm->atom.net_wm_state_maximized_horz)
					window->maximized_horz = 1;
			}
			break;
		case TYPE_MOTIF_WM_HINTS:
			memcpy(&window->motif_hints,
			       prop->data,
			       sizeof window->motif_hints);
			if (window->motif_hints.flags & MWM_HINTS_DECORATIONS) {
				if (window->motif_hints.decorations & MWM_DECOR_ALL)
					/* MWM_DECOR_ALL means all except the other values listed. */
					window->decorate =
						MWM_DECOR_EVERYTHING & (~window->motif_hints.decorations);
				else
					window->decorate =
						window->motif_hints.decorations;
			}
			break;
		default:
			break;
		}
	}

	if (window->pid > 0) {
		gethostname(name, sizeof(name));
		for (i = 0; i < sizeof(name); i++) {
			if (name[i] == '\0')
				break;
		}
		if (i == sizeof(name))
			name[0] = '\0'; /* ignore stupid hostnames */

		/* this is only one heuristic to guess the PID of a client is
		* valid, assuming it's compliant with icccm and ewmh.
		* Non-compliants and remote applications of course fail. */
		if (!window->machine || strcmp(window->machine, name))
			window->pid = 0;
	}

	//if (window->frame && window->name)
	//	wl_shell_surface_set_title(window->shell_surface, window->name);
	//if (window->pid > 0)
	//	wl_shell_surface_set_pid(window->shell_surface, window->pid);

}

static CARD32
add_client_fd(OsTimerPtr timer, CARD32 time, void *arg)
{
	struct window_manager*wm = arg;
    if (!AddClientOnOpenFD(window_manager_client_fd(wm)))
        FatalError("Failed to add wm client\n");
    TimerFree(timer);
    return 0;
}


void register_wm_callback(struct window_manager * wm) {
	RegisterBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);
	TimerSet(NULL, 0, 1, add_client_fd, wm);
}

void
window_manager_block_handler(void *data, struct timeval **tv, void *read_mask)
{

}

void window_manager_conn_wait_block_handler(void *data, struct timeval **tv, void *read_mask)
{
	struct window_manager *wm = data;
	if(pthread_mutex_trylock(window_manager_init_conn(wm)) == 0) {
		LogWrite(0, "%s: success\n", __PRETTY_FUNCTION__);
		RemoveBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);
	    RegisterBlockAndWakeupHandlers(window_manager_block_handler, window_manager_wakeup_handler, wm);
	    pthread_mutex_unlock(window_manager_init_conn(wm));
	}
}

void window_manager_conn_wait_wakeup_handler(void *data, int err, void *read_mask)
{
	struct window_manager *wm = data;
	if(pthread_mutex_trylock(window_manager_init_conn(wm)) == 0) {
		LogWrite(0, "%s: success\n", __PRETTY_FUNCTION__);
		RemoveBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);
	    RegisterBlockAndWakeupHandlers(window_manager_block_handler, window_manager_wakeup_handler, wm);
	    pthread_mutex_unlock(window_manager_init_conn(wm));
	}
}



