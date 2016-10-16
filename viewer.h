#ifndef VIEWER_H
#define VIEWER_H

#include <stdbool.h>
#include <caca.h>
#include <sys/types.h>
#include "vnc.h"

#define VIEWER_PAN_STEP 0.20
#define VIEWER_ZOOM_STEP 1.20f
#define VIEWER_ZOOM_MAX_LVL 15
#define VIEWER_FPS 10		/* too high FPS will make control sluggish, never go above 50. */

/* Draw remote frame-buffer and handle key/mouse IO. */
struct viewer {
	struct vnc *vnc;
	caca_display_t *disp;

	caca_canvas_t *view;
	struct caca_dither *fb_dither;
	float view_x, zoom_x, view_y, zoom_y;
	int zoom_lvl;
	float zoom_lvls[VIEWER_ZOOM_MAX_LVL + 1];

	int mouse_speed[VIEWER_ZOOM_MAX_LVL + 1];
	int mouse_x, mouse_y;
	bool mouse_left, mouse_middle, mouse_right;

	suseconds_t last_esc_key;
	bool void_backsp, void_tab, void_ret, void_pause, void_esc, void_del;
	bool disp_help, input2vnc;
	bool hold_lctrl, hold_lshift, hold_lalt, hold_lsuper, hold_ralt,
	    hold_rshift, hold_rctrl;
};

/* Initialise canvas and its driver for the VNC connection. */
bool viewer_init(struct viewer *viewer, struct vnc *vnc);
/* Display a status row at 0,0. */
void viewer_disp_status(struct viewer *viewer);
/* Display a static help menu at 0,1. */
void viewer_disp_help(struct viewer *viewer);
/* Zoom in (+) or out (-) on the canvas. Remember to redraw! */
void viewer_zoom(struct viewer *viewer, int offset);
/* Pan the canvas several steps up (-y), down (+y), left (-x), or right (+x). Remember to redraw! */
void viewer_pan(struct viewer *viewer, int pan_x, int pan_y);
/* Redraw the content from the latest frame-buffer of VNC connection. */
void viewer_redraw(struct viewer *viewer);
/* Handle keyboard input and canvas events. Block caller until quit key is pressed and handled. */
void viewer_ev_loop(struct viewer *viewer);
/* Click (press and release) a keyboard key in VNC. */
void viewer_vnc_click_key(struct viewer *viewer, int vnc_key);
/* Hold control key and then click the specified key in VNC, then release control key. */
void viewer_vnc_click_ctrl_key_combo(struct viewer *viewer, int vnc_key);
/* Toggle (press or release) a keyboard key in VNC. */
void viewer_vnc_toggle_key(struct viewer *viewer, int vnc_key, bool key_down);
/* Send the latest pointer location and button mask to VNC. */
void viewer_vnc_send_pointer(struct viewer *viewer);
/* Move mouse pointer several steps up (-y) down (+y), left (-x) or right (+x).*/
void viewer_move_mouse(struct viewer *viewer, int step_x, int step_y);
/* Move view to the location of mouse cursor and zoom in there to a pre-defined level. */
void viewer_zoom_to_cursor(struct viewer *viewer);
/* Translate caca key stroke to VNC key symbol and send it over VNC. */
void viewer_input_to_vnc(struct viewer *viewer, int caca_key);
/* Interpret and act on caca key stroke as a viewer control command. Return false only if viewer should quit. */
bool viewer_handle_control(struct viewer *viewer, int caca_key);
/* Release all resources held by the viewer, but do not terminate the VNC connection. */
void viewer_terminate(struct viewer *viewer);
#endif
