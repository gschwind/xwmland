/*
 * Copyright © 2011 Intel Corporation
 * Copyright © 2015 Benoit Gschwind <gschwind@gnu-log.net>
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


#include "wm.h"
#include "xwm/hash.h"

#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <selection.h>
#include <micmap.h>
#include <misyncshm.h>
#include <compositeext.h>
#include <glx_extinit.h>
#include <screenint.h>
#include <propertyst.h>

static void
window_manager_get_resources(struct window_manager *wm)
{
	int i;

#define F(field) offsetof(struct window_manager, field)

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

/* We reuse some predefined, but otherwise useles atoms */
#define TYPE_WM_PROTOCOLS	XCB_ATOM_CUT_BUFFER0
#define TYPE_MOTIF_WM_HINTS	XCB_ATOM_CUT_BUFFER1
#define TYPE_NET_WM_STATE	XCB_ATOM_CUT_BUFFER2
#define TYPE_WM_NORMAL_HINTS	XCB_ATOM_CUT_BUFFER3

void
window_manager_window_read_properties(struct xwl_window *window)
{
	struct window_manager *wm = window->xwl_screen->wm;

#define F(field) offsetof(struct xwl_window, field)
	const struct {
		xcb_atom_t atom;
		xcb_atom_t type;
		int offset;
	} props[] = {
		{ XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, F(class) },
		{ XCB_ATOM_WM_NAME, XCB_ATOM_STRING, F(name) },
		{ XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, F(transient_for) },
		{ wm->atom.wm_protocols, TYPE_WM_PROTOCOLS, F(protocols) },
		{ wm->atom.wm_normal_hints, TYPE_WM_NORMAL_HINTS, F(protocols) },
		{ wm->atom.net_wm_state, TYPE_NET_WM_STATE },
		{ wm->atom.net_wm_window_type, XCB_ATOM_ATOM, F(type) },
		{ wm->atom.net_wm_name, XCB_ATOM_STRING, F(name) },
		{ wm->atom.net_wm_pid, XCB_ATOM_CARDINAL, F(pid) },
		{ wm->atom.motif_wm_hints, TYPE_MOTIF_WM_HINTS, 0 },
		{ wm->atom.wm_client_machine, XCB_ATOM_WM_CLIENT_MACHINE, F(machine) },
	};
#undef F

	void *p;
	uint32_t *xid;
	xcb_atom_t *atom;
	uint32_t i;
	char name[1024];

	if (!window->properties_dirty)
		return;
	window->properties_dirty = 0;

	window->decorate = window->override_redirect ? 0 : MWM_DECOR_EVERYTHING;
	window->size_hints.flags = 0;
	window->motif_hints.flags = 0;
	window->delete_window = 0;

	for (i = 0; i < ARRAY_LENGTH(props); i++) {
		PropertyPtr prop;
		dixLookupProperty(&prop, window->window, props[i].atom, serverClient, DixReadAccess);

		if(!prop)
			continue;

		if (prop->type == XCB_ATOM_NONE) {
			/* No such property */
			continue;
		}

		p = ((char *) window + props[i].offset);

		switch (props[i].type) {
		case XCB_ATOM_WM_CLIENT_MACHINE:
		case XCB_ATOM_STRING:
			/* FIXME: We're using this for both string and
			   utf8_string */
			if (*(char **) p)
				free(*(char **) p);

			*(char **) p =
				strndup(prop->data, prop->size);
			break;
		case XCB_ATOM_WINDOW:
			xid = prop->data;
			*(struct xwl_window**)p = hash_table_lookup(window->xwl_screen->window_hash, *xid);
			if (!*(struct xwl_window**)p)
				LogWrite(0, "XCB_ATOM_WINDOW contains window"
					   " id not found in hash table.\n");
			break;
		case XCB_ATOM_CARDINAL:
		case XCB_ATOM_ATOM:
			*(xcb_atom_t *) p = *((xcb_atom_t *)prop->data);
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

static void
window_manager_get_visual_and_colormap(struct window_manager *wm)
{
	int i;

	VisualID visual_id = 0;
	for(i = 0; i < wm->screen->numDepths; ++i) {
		LogWrite(0, "DEPTH: depth %d, nvis %d\n", wm->screen->allowedDepths[i].depth, wm->screen->allowedDepths[i].numVids);
		if(wm->screen->allowedDepths[i].depth == 32) {
			visual_id = wm->screen->allowedDepths[i].vids[0];
			break;
		}
	}

	LogWrite(0, "Found visual: vid %d\n", (int)visual_id);

	for(i = 0; i < wm->screen->numVisuals; ++i) {
		LogWrite(0, "VISUAL: vid %d, class %d\n", (int)wm->screen->visuals[i].vid, wm->screen->visuals[i].class);
		if((int)wm->screen->visuals[i].vid == (int)visual_id) {
			wm->visual = &(wm->screen->visuals[i]);
			break;
		}
	}

	/* TODO: check */
	wm->colormap_id = FakeClientID(0);
	CreateColormap(wm->colormap_id, wm->screen, wm->visual,
	               &wm->colormap, AllocNone, 0/*serverClient*/);

}

void
initialize_connection(struct window_manager *wm) {

}

static void
window_manager_block_handler(void *data, struct timeval **tv, void *read_mask)
{

}

static void
window_manager_handle_event(struct window_manager *wm)
{
	xcb_generic_event_t *event;
	while ((event = xcb_poll_for_event(wm->conn)) != 0) {
//		if (weston_wm_handle_selection_event(wm, event)) {
//			free(event);
//			count++;
//			continue;
//		}
//
//		if (weston_wm_handle_dnd_event(wm, event)) {
//			free(event);
//			count++;
//			continue;
//		}
//
		switch (event->response_type) {
//		case XCB_BUTTON_PRESS:
//		case XCB_BUTTON_RELEASE:
//			weston_wm_handle_button(wm, event);
//			break;
//		case XCB_ENTER_NOTIFY:
//			weston_wm_handle_enter(wm, event);
//			break;
//		case XCB_LEAVE_NOTIFY:
//			weston_wm_handle_leave(wm, event);
//			break;
//		case XCB_MOTION_NOTIFY:
//			weston_wm_handle_motion(wm, event);
//			break;
//		case XCB_CREATE_NOTIFY:
//			weston_wm_handle_create_notify(wm, event);
//			break;
//		case XCB_MAP_REQUEST:
//			weston_wm_handle_map_request(wm, event);
//			break;
//		case XCB_MAP_NOTIFY:
//			weston_wm_handle_map_notify(wm, event);
//			break;
//		case XCB_UNMAP_NOTIFY:
//			weston_wm_handle_unmap_notify(wm, event);
//			break;
//		case XCB_REPARENT_NOTIFY:
//			weston_wm_handle_reparent_notify(wm, event);
//			break;
//		case XCB_CONFIGURE_REQUEST:
//			weston_wm_handle_configure_request(wm, event);
//			break;
//		case XCB_CONFIGURE_NOTIFY:
//			weston_wm_handle_configure_notify(wm, event);
//			break;
//		case XCB_DESTROY_NOTIFY:
//			weston_wm_handle_destroy_notify(wm, event);
//			break;
//		case XCB_MAPPING_NOTIFY:
//			LogWrite(0, "XCB_MAPPING_NOTIFY\n");
//			break;
//		case XCB_PROPERTY_NOTIFY:
//			weston_wm_handle_property_notify(wm, event);
//			break;
//		case XCB_CLIENT_MESSAGE:
//			weston_wm_handle_client_message(wm, event);
//			break;
//		case XCB_FOCUS_IN:
//			weston_wm_handle_focus_in(wm, event);
//			break;
		case 0:
			LogWrite(0, "Error ocurrent\n");
			break;
		}

		xcb_flush(wm->conn);
		free(event);
	}

}


static void
window_manager_wakeup_handler(void *data, int err, void *read_mask)
{
	struct window_manager *wm = data;

    if (err < 0)
        return;

    window_manager_handle_event(wm);

}

static void
window_manager_conn_wait_block_handler(void *data, struct timeval **tv, void *read_mask);


static void
window_manager_conn_wait_wakeup_handler(void *data, int err, void *read_mask);

static void
window_manager_conn_wait_block_handler(void *data, struct timeval **tv, void *read_mask)
{
	struct window_manager *wm = data;
	if(pthread_mutex_trylock(&wm->init_conn) == 0) {
		LogWrite(0, "%s: success\n", __PRETTY_FUNCTION__);
		RemoveBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);
	    RegisterBlockAndWakeupHandlers(window_manager_block_handler, window_manager_wakeup_handler, wm);
	    pthread_mutex_unlock(&wm->init_conn);
	}
}

static void
window_manager_conn_wait_wakeup_handler(void *data, int err, void *read_mask)
{
	struct window_manager *wm = data;
	if(pthread_mutex_trylock(&wm->init_conn) == 0) {
		LogWrite(0, "%s: success\n", __PRETTY_FUNCTION__);
		RemoveBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);
	    RegisterBlockAndWakeupHandlers(window_manager_block_handler, window_manager_wakeup_handler, wm);
	    pthread_mutex_unlock(&wm->init_conn);
	}
}

static void *
window_manager_init_conn(void * data) {
	struct window_manager *wm = data;
	uint32_t values = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_screen_iterator_t s;
	xcb_screen_t * screen;
	int err;
	wm->conn = xcb_connect_to_fd(wm->fd[0], NULL);
	if((err = xcb_connection_has_error(wm->conn)) > 0) {
		LogWrite(0, "Error %d\n", err);
	}
	s = xcb_setup_roots_iterator(xcb_get_setup(wm->conn));
	screen = s.data;
	xcb_change_window_attributes(wm->conn, screen->root, XCB_CW_EVENT_MASK, &values);
	xcb_flush(wm->conn);
	pthread_mutex_unlock(&wm->init_conn);
	return NULL;
}

static CARD32
add_client_fd(OsTimerPtr timer, CARD32 time, void *arg)
{
	struct window_manager*wm = arg;

    if (!AddClientOnOpenFD(wm->fd[1]))
        FatalError("Failed to add wm client\n");

    TimerFree(timer);

    return 0;
}

struct window_manager *
window_manager_create(ScreenPtr screen)
{
	struct window_manager *wm;

	wm = (struct window_manager *)calloc(1, sizeof *wm);
	if (wm == NULL)
		return NULL;

	wm->screen = screen;

	window_manager_get_resources(wm);
	window_manager_get_visual_and_colormap(wm);

	wm->theme = theme_create();

	pthread_mutex_init(&wm->init_conn, NULL);
	pthread_mutex_lock(&wm->init_conn);
	pthread_create(&wm->init_conn_thread, NULL, window_manager_init_conn, wm);
	pthread_detach(wm->init_conn_thread);

	if(socketpair(AF_UNIX, SOCK_NONBLOCK | SOCK_STREAM | SOCK_CLOEXEC, 0, wm->fd) < 0) {
		LogWrite(0, "Cannot pair socket\n");
	}
    RegisterBlockAndWakeupHandlers(window_manager_conn_wait_block_handler, window_manager_conn_wait_wakeup_handler, wm);

    TimerSet(NULL, 0, 1, add_client_fd, wm);
	return wm;
}


