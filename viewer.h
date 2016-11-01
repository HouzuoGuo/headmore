#ifndef VIEWER_H
#define VIEWER_H

#include <caca.h>
#include <stdbool.h>
#include <sys/types.h>
#include "geo.h"
#include "vnc.h"

/*
 * The viewer will render frame-buffer content at roughly this many frames per second.
 * It does not take rendering algorithm itself into account, hence the value is a rough estimate at best.
 * The perceived FPS decreases as terminal gets larger.
 * Do not raise the value too high or controls will become very sluggish.
 */
#define VIEWER_FPS 10

/* Render remote frame-buffer on terminal and handle key/mouse IO. */
struct viewer {
	struct vnc *vnc;
	struct geo geo;

	caca_display_t *disp;
	caca_canvas_t *view;
	struct caca_dither *fb_dither;

	suseconds_t last_vnc_esc;
	bool void_backsp, void_tab, void_ret, void_pause, void_esc, void_del;
	bool disp_help, input2vnc;
	bool hold_lctrl, hold_lshift, hold_lalt, hold_lsuper, hold_ralt,
	    hold_rshift, hold_rctrl;
	bool mouse_left, mouse_middle, mouse_right;
};

/* Initialise viewer and its driver for the VNC connection. */
bool viewer_init(struct viewer *v, struct vnc *vnc);
/* Return geometry facts of the viewer. */
struct geo_facts viewer_geo(struct viewer *v);
/* Display a status row at 0,0. */
void viewer_disp_status(struct viewer *v);
/* Display a static help menu at 0,1. */
void viewer_disp_help(struct viewer *v);
/* Redraw the content from the latest frame-buffer of VNC connection. */
void viewer_redraw(struct viewer *v);
/* Handle keyboard input and canvas events. Block caller until quit key is pressed and handled. */
void viewer_ev_loop(struct viewer *v);
/* Click (press and release) a keyboard key in VNC. */
void viewer_vnc_click_key(struct viewer *v, int vnc_key);
/* Hold control key and then click the specified key in VNC, then release control key. */
void viewer_vnc_click_ctrl_key_combo(struct viewer *v, int vnc_key);
/* Toggle (press or release) a keyboard key in VNC. */
void viewer_vnc_toggle_key(struct viewer *v, int vnc_key, bool key_down);
/* Send the latest pointer location and button mask to VNC. */
void viewer_vnc_send_pointer(struct viewer *v);
/* Translate caca key stroke to VNC key symbol and send it over VNC. */
void viewer_input_to_vnc(struct viewer *v, int caca_key);
/* Interpret and act on caca key stroke as a viewer control command. Return false only if viewer should quit. */
bool viewer_handle_control(struct viewer *v, int caca_key);
/* Release all resources held by the viewer, but do not terminate the VNC connection. */
void viewer_terminate(struct viewer *v);

#endif
