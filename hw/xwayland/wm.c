/*
 * wm.c
 *
 *  Created on: 26 sept. 2015
 *      Author: gschwind
 */

#include "wm.h"

#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <selection.h>
#include <micmap.h>
#include <misyncshm.h>
#include <compositeext.h>
#include <glx_extinit.h>
#include <screenint.h>

extern const char *display;

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
window_manager_wakeup_handler(void *data, int err, void *read_mask)
{
	struct window_manager *wm = data;
	xcb_generic_event_t * event;

    if (err < 0)
        return;

	while ((event = xcb_poll_for_event(wm->conn)) != 0) {
		LogWrite(0, "event\n");
		free(event);
	}

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


