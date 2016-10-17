#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <rfb/keysym.h>
#include <rfb/rfbclient.h>
#include "viewer.h"

/* Return current wall clock time in microseconds. */
static suseconds_t get_time_usec()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec * 1000000 + now.tv_usec;
}

/* Messages to display in a static help menu. */
static char const *viewer_help[] = {
	"============ LEFT HAND ============",
	"Esc   Disconnect and quit          ",
	"`     Toggle input to viewer/VNC   ",
	"~     Click back-tick in VNC       ",
	"wasd  Pan viewer                   ",
	"q/e   Zoom out/in                  ",
	"============ RIGHT HAND ===========",
	"ijkl Move mouse cursor             ",
	"u/o  Click L/R mouse button        ",
	"789  Toggle hold L/M/R mouse button",
	"0    Click middle mouse button     ",
	"p    Find mouse pointer            ",
	"=========== TOGGLE KEYS ===========",
	"z/m    Toggle hold L/R Control     ",
	"x/n    Toggle hold L/R Shift       ",
	"c/b    Toggle hold L/R Alt         ",
	"v      Toggle hold L Meta          ",
	NULL
};

/* Return the RFB client the viewer is connected to. */
static struct _rfbClient *rfb(struct viewer *viewer)
{
	return viewer->vnc->conn;
}

bool viewer_init(struct viewer * viewer, struct vnc * vnc)
{
	memset(viewer, 0, sizeof(struct viewer));
	/* Fill the zoom table */
	int i;
	viewer->zoom_lvls[0] = 1.0;
	for (i = 0; i < VIEWER_ZOOM_MAX_LVL; i++) {
		viewer->zoom_lvls[i + 1] =
		    viewer->zoom_lvls[i] * VIEWER_ZOOM_STEP;
	}
	/*
	 * Mouse speed is the inverse of zoom.
	 * At maximum zoom, mouse moves slowest at speed of 1px/move.
	 * Each zoom step lower speeds up mouse movement by 2px/move.
	 */
	for (i = 0; i < VIEWER_ZOOM_MAX_LVL + 1; i++) {
		viewer->mouse_speed[i] = 1 + (VIEWER_ZOOM_MAX_LVL - i) * 2;
	}
	viewer->view = caca_create_canvas(0, 0);
	if (!viewer->view) {
		fprintf(stderr, "Failed to create caca canvas\n");
		return false;
	}
	viewer->disp = caca_create_display_with_driver(viewer->view, "ncurses");
	if (!viewer->disp) {
		fprintf(stderr, "Failed to create caca display\n");
		return false;
	}
	viewer->vnc = vnc;
	caca_set_display_title(viewer->disp, rfb(viewer)->desktopName);

	/* Begin by placing image right at the centre */
	viewer->view_x = 0.5;
	viewer->view_y = 0.5;
	viewer_zoom(viewer, 0);

	/* Place mouse pointer at centre of screen and depress all buttons */
	viewer->mouse_x = rfb(viewer)->width / 2;
	viewer->mouse_y = rfb(viewer)->height / 2;
	viewer_vnc_send_pointer(viewer);
	return true;
}

void viewer_disp_status(struct viewer *viewer)
{
	caca_set_color_ansi(viewer->view, CACA_WHITE, CACA_BLUE);
	char *conn_remark = "";
	if (!viewer->vnc->connected) {
		conn_remark = "(Disconnected)";
	}
	char *who_has_input = "Input to viewer";
	if (viewer->input2vnc) {
		who_has_input = "Input to VNC (~ to disengage)";
	}
	char held_controls[80] = { 0 };
	bool disp_held_controls = false;
	if (viewer->mouse_left) {
		disp_held_controls = true;
		strcat(held_controls, " LMouse");
	}
	if (viewer->mouse_middle) {
		disp_held_controls = true;
		strcat(held_controls, " MMouse");
	}
	if (viewer->mouse_right) {
		disp_held_controls = true;
		strcat(held_controls, " RMouse");
	}
	if (viewer->hold_lctrl) {
		disp_held_controls = true;
		strcat(held_controls, " LCtrl");
	}
	if (viewer->hold_lshift) {
		disp_held_controls = true;
		strcat(held_controls, " LShift");
	}
	if (viewer->hold_lalt) {
		disp_held_controls = true;
		strcat(held_controls, " LAlt");
	}
	if (viewer->hold_lsuper) {
		disp_held_controls = true;
		strcat(held_controls, " LSuper");
	}
	if (viewer->hold_ralt) {
		disp_held_controls = true;
		strcat(held_controls, " RAlt");
	}
	if (viewer->hold_rshift) {
		disp_held_controls = true;
		strcat(held_controls, " RShift");
	}
	if (viewer->hold_rctrl) {
		disp_held_controls = true;
		strcat(held_controls, " RCtrl");
	}
	char held_controls_msg[100] = { 0 };
	if (disp_held_controls) {
		strcat(held_controls_msg, "| Holding down:");
		strcat(held_controls_msg, held_controls);
	}
	caca_printf(viewer->view, 0, 0, "h:Help | %s:%d%s | %s %s",
		    rfb(viewer)->serverHost, rfb(viewer)->serverPort,
		    conn_remark, who_has_input, held_controls_msg);
}

void viewer_disp_help(struct viewer *viewer)
{
	caca_set_color_ansi(viewer->view, CACA_WHITE, CACA_BLUE);
	int i;
	for (i = 0; viewer_help[i] != NULL; i++) {
		caca_put_str(viewer->view, 0, 1 + i, viewer_help[i]);
	}
}

void viewer_zoom(struct viewer *viewer, int offset)
{
	viewer->zoom_lvl += offset;
	if (viewer->zoom_lvl < 0) {
		viewer->zoom_lvl = 0;
	} else if (viewer->zoom_lvl > VIEWER_ZOOM_MAX_LVL) {
		viewer->zoom_lvl = VIEWER_ZOOM_MAX_LVL;
	}

	int viewer_width = caca_get_canvas_width(viewer->view);
	int viewer_height = caca_get_canvas_height(viewer->view);
	viewer->zoom_x =
	    (viewer->zoom_lvl <
	     0) ? 1.0 /
	    viewer->zoom_lvls[-viewer->zoom_lvl] : viewer->
	    zoom_lvls[viewer->zoom_lvl];
	viewer->zoom_y =
	    viewer->zoom_x * viewer_width / viewer_height *
	    rfb(viewer)->height / rfb(viewer)->width * viewer_height /
	    viewer_width * caca_get_display_width(viewer->disp) /
	    caca_get_display_height(viewer->disp);

	if (viewer->zoom_y > viewer->zoom_x) {
		float tmp = viewer->zoom_x;
		viewer->zoom_x = tmp * tmp / viewer->zoom_y;
		viewer->zoom_y = tmp;
	}
}

void viewer_pan(struct viewer *viewer, int pan_x, int pan_y)
{
	if (pan_x != 0) {
		if (viewer->zoom_x > 1.0) {
			viewer->view_x +=
			    pan_x * (VIEWER_PAN_STEP / viewer->zoom_x);
		}
		if (viewer->view_x < 0.0) {
			viewer->view_x = 0.0;
		} else if (viewer->view_x > 1.0) {
			viewer->view_x = 1.0;
		}
	}
	if (pan_y != 0) {
		if (viewer->zoom_y > 1.0) {
			viewer->view_y +=
			    pan_y * (VIEWER_PAN_STEP / viewer->zoom_y);
		}
		if (viewer->view_y < 0.0) {
			viewer->view_y = 0.0;
		} else if (viewer->view_y > 1.0) {
			viewer->view_y = 1.0;
		}
	}
}

void viewer_redraw(struct viewer *viewer)
{
	caca_set_color_ansi(viewer->view, CACA_WHITE, CACA_BLACK);
	caca_clear_canvas(viewer->view);
	/*
	 * Run the latest frame-buffer content through Floyd–Steinberg algorithm -
	 * it seems to offer higher quality over other algorithm choices.
	 * The dither parameters are tailored for a connection on 32-bit RGB colours.
	 */
	if (viewer->fb_dither != NULL) {
		caca_free_dither(viewer->fb_dither);
	}
	viewer->fb_dither =
	    caca_create_dither(32, rfb(viewer)->width,
			       rfb(viewer)->height,
			       rfb(viewer)->width * 4, 0x000000ff,
			       0x0000ff00, 0x00ff0000, 0);
	caca_set_dither_algorithm(viewer->fb_dither, "fstein");
	caca_set_dither_gamma(viewer->fb_dither, 1.0);
	int viewer_width = caca_get_canvas_width(viewer->view);
	int viewer_height = caca_get_canvas_height(viewer->view);
	float delta_x = (viewer->zoom_x > 1.0) ? viewer->view_x : 0.5;
	float delta_y = (viewer->zoom_y > 1.0) ? viewer->view_y : 0.5;

	/* Draw frame-buffer and other things on the canvas */
	caca_dither_bitmap(viewer->view,
			   viewer_width * (1.0 -
					   viewer->zoom_x) * delta_x,
			   viewer_height * (1.0 -
					    viewer->zoom_y) * delta_y,
			   viewer_width * viewer->zoom_x + 1,
			   viewer_height * viewer->zoom_y + 1,
			   viewer->fb_dither, rfb(viewer)->frameBuffer);
	viewer_disp_status(viewer);
	if (viewer->disp_help) {
		viewer_disp_help(viewer);
	}
	caca_refresh_display(viewer->disp);
}

void viewer_ev_loop(struct viewer *viewer)
{
	while (true) {
		/* Listen to the latest event */
		caca_event_t ev;
		caca_get_event(viewer->disp,
			       CACA_EVENT_KEY_PRESS | CACA_EVENT_RESIZE |
			       CACA_EVENT_QUIT, &ev, 1000000 / VIEWER_FPS);
		/* Certain types of events are caca calling quit */
		enum caca_event_type ev_type = caca_get_event_type(&ev);
		if (ev_type & CACA_EVENT_QUIT || ev_type & CACA_EVENT_NONE) {
			return;
		}
		/* Handle previously banked escape key (VNC input), send it to VNC. */
		if (viewer->last_vnc_esc != 0
		    && get_time_usec() - viewer->last_vnc_esc >=
		    1000000 / VIEWER_FPS) {
			viewer->last_vnc_esc = 0;
			viewer_vnc_click_key(viewer,
					     cacakey2vnc(CACA_KEY_ESCAPE));
		}
		/* Handle previously banked escape key (viewer control), stop the viewer. */
		if (viewer->last_viewer_esc != 0
		    && get_time_usec() - viewer->last_viewer_esc >=
		    1000000 / VIEWER_FPS) {
			return;
		}
		/* Redraw at a constant frame rate when there is no key input */
		if (!(ev_type & CACA_EVENT_KEY_PRESS)) {
			viewer_redraw(viewer);
			continue;
		}
		int ev_char = caca_get_event_key_ch(&ev);
		/* Input never gets directed at VNC if it is disconnected */
		if (!viewer->vnc->connected) {
			viewer->input2vnc = false;
		}
		/* A key input is directed at either VNC or viewer controls */
		if (viewer->input2vnc && ev_char != '`') {
			viewer_input_to_vnc(viewer, ev_char);
		} else {
			viewer_handle_control(viewer, ev_char);
		}
	}
}

void viewer_vnc_click_key(struct viewer *viewer, int vnc_key)
{
	SendKeyEvent(rfb(viewer), vnc_key, TRUE);
	SendKeyEvent(rfb(viewer), vnc_key, FALSE);
}

void viewer_vnc_click_ctrl_key_combo(struct viewer *viewer, int vnc_key)
{
	SendKeyEvent(rfb(viewer), XK_Control_L, TRUE);
	SendKeyEvent(rfb(viewer), vnc_key, TRUE);
	SendKeyEvent(rfb(viewer), vnc_key, FALSE);
	SendKeyEvent(rfb(viewer), XK_Control_L, FALSE);
}

void viewer_vnc_toggle_key(struct viewer *viewer, int vnc_key, bool key_down)
{
	SendKeyEvent(rfb(viewer), vnc_key, key_down ? TRUE : FALSE);
}

void viewer_vnc_send_pointer(struct viewer *viewer)
{
	int mask = 0;
	if (viewer->mouse_left) {
		mask |= rfbButton1Mask;
	}
	if (viewer->mouse_middle) {
		mask |= rfbButton2Mask;
	}
	if (viewer->mouse_right) {
		mask |= rfbButton3Mask;
	}
	SendPointerEvent(rfb(viewer), viewer->mouse_x, viewer->mouse_y, mask);
}

void viewer_move_mouse(struct viewer *viewer, int step_x, int step_y)
{
	int speed = viewer->mouse_speed[viewer->zoom_lvl];
	viewer->mouse_x += step_x * speed;
	viewer->mouse_y += step_y * speed;
	if (viewer->mouse_x < 0) {
		viewer->mouse_x = 0;
	} else if (viewer->mouse_x > rfb(viewer)->width - 1) {
		viewer->mouse_x = rfb(viewer)->width - 1;
	}
	if (viewer->mouse_y < 0) {
		viewer->mouse_y = 0;
	} else if (viewer->mouse_y > rfb(viewer)->height - 1) {
		viewer->mouse_y = rfb(viewer)->height - 1;
	}
	viewer_vnc_send_pointer(viewer);
}

void viewer_zoom_to_cursor(struct viewer *viewer)
{
	/* Zoom in to approximately 67% of maximum zoom level */
	viewer_zoom(viewer, VIEWER_ZOOM_MAX_LVL * 2 / 3 - viewer->zoom_lvl);
	/* Calculate position of mouse cursor relative to the entire canvas */
	viewer->view_x = (float)viewer->mouse_x / (float)rfb(viewer)->width;
	viewer->view_y = (float)viewer->mouse_y / (float)rfb(viewer)->height;
	/* Pan several steps to place the mouse cursor closer to view's centre */
	viewer_redraw(viewer);
}

void viewer_input_to_vnc(struct viewer *viewer, int caca_key)
{
	/*
	 * Escape key could mean two things, either it is a key press by itself, or it
	 * could be an Alt key combination. An Alt key combination is sent in two key
	 * events, first event is an escape key, second is the combo key.
	 * To distinguish between the two meanings, the escape key is banked here and
	 * dealt with later.
	 */
	if (caca_key == CACA_KEY_ESCAPE) {
		viewer->last_vnc_esc = get_time_usec();
		return;
	}
	/*
	 * Control sequences can only be seen by terminal in lower case letters a-z,
	 * their key code ranges from 1(a) to 26(z). In four cases the control sequence
	 * also generates an extra key event that has to be discarded.
	 */
	if (caca_key >= 1 && caca_key <= 26) {
		switch (caca_key) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:	/* Ctrl+ABCDEFG */
		case 10:
		case 11:
		case 12:	/* (missing H,I) Ctrl+JKL */
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:	/* (missing M) Ctrl+NOPQR */
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:	/* (missing S) Ctrl+TUVWXYZ */
			viewer_vnc_click_ctrl_key_combo(viewer,
							caca_key - 1 + 'a');
			return;
		case 8:	/* H and an extra backspace */
			viewer->void_backsp = true;
			viewer_vnc_click_ctrl_key_combo(viewer, 'h');
			return;
		case 9:	/* I and an extra tab */
			viewer->void_tab = true;
			viewer_vnc_click_ctrl_key_combo(viewer, 'i');
			return;
		case 13:	/* M and an extra enter */
			viewer->void_ret = true;
			viewer_vnc_click_ctrl_key_combo(viewer, 'm');
			return;
		case 19:	/* S and an extra pause */
			viewer->void_pause = true;
			viewer_vnc_click_ctrl_key_combo(viewer, 's');
			return;
		}
	}
	/*
	 * If the previous key was a control sequence that came with an extra key
	 * event, discard it and do not send it over VNC.
	 */
	switch (caca_key) {
	case CACA_KEY_BACKSPACE:
		if (viewer->void_backsp) {
			viewer->void_backsp = false;
			return;
		}
		break;
	case CACA_KEY_TAB:
		if (viewer->void_tab) {
			viewer->void_tab = false;
			return;
		}
		break;
	case CACA_KEY_RETURN:
		if (viewer->void_ret) {
			viewer->void_ret = false;
			return;
		}
	case CACA_KEY_PAUSE:
		if (viewer->void_pause) {
			viewer->void_pause = false;
			return;
		}
	}
	/* Some key codes from caca must be translated to VNC key codes before use */
	int translated_ch = cacakey2vnc(caca_key);
	if (translated_ch == -1) {
		rfbClientErr("Unknown key %d is not sent to VNC\n", caca_key);
		return;
	}
	/*
	 * In case there was a banked escape key, the Alt combination key shall arrive
	 * in an instant within 2000 microseconds.
	 */
	if (get_time_usec() - viewer->last_vnc_esc < 2000) {
		viewer_vnc_toggle_key(viewer, XK_Alt_L, true);
		viewer_vnc_click_key(viewer, translated_ch);
		viewer_vnc_toggle_key(viewer, XK_Alt_L, false);
		viewer->last_vnc_esc = 0;
		return;
	}
	/* Finally the key code can be sent to VNC! */
	viewer_vnc_click_key(viewer, translated_ch);
}

void viewer_handle_control(struct viewer *viewer, int caca_key)
{
	/*
	 * Similar to the case with viewer_input_to_vnc, if user accidentally types an
	 * Alt key combination in the viewer, the first key event - an escape key will
	 * mislead viewer into quitting.
	 * Instead of quitting immediately, an escape key will be banked and the viewer
	 * awaits one more key event to arrive. If it indeed arrives and within 2000
	 * microseconds, then the user must have mistakenly typed Alt key combination,
	 * in which case the viewer shall not quit.
	 * If the latter key event does not arrive, the escape key must have been an
	 * intention to quit the viewer and event loop will do that job.
	 */
	if (get_time_usec() - viewer->last_viewer_esc < 2000) {
		viewer->last_viewer_esc = 0;
	}
	switch (caca_key) {
	case 'h':
	case 'H':
		viewer->disp_help = !viewer->disp_help;
		break;
	case CACA_KEY_ESCAPE:
		/* Escape key is banked, its meaning will be determined soon later, */
		viewer->last_viewer_esc = get_time_usec();
		break;
		/* Left hand */
	case 'w':
	case 'W':
		viewer_pan(viewer, 0, -1);
		break;
	case 'a':
	case 'A':
		viewer_pan(viewer, -1, 0);
		break;
	case 's':
	case 'S':
		viewer_pan(viewer, 0, 1);
		break;
	case 'd':
	case 'D':
		viewer_pan(viewer, 1, 0);
		break;
	case 'q':
	case 'Q':
		viewer_zoom(viewer, -1);
		break;
	case 'e':
	case 'E':
		viewer_zoom(viewer, 1);
		break;
	case '`':
		if (viewer->vnc->connected) {
			viewer->input2vnc = !viewer->input2vnc;
		}
		break;
	case '~':
		viewer_vnc_click_key(viewer, 96);	/* click back-tick (96 in ASCII) in VNC */
		break;
		/* Right hand */
	case 'i':
	case 'I':
		viewer_move_mouse(viewer, 0, -1);
		break;
	case 'j':
	case 'J':
		viewer_move_mouse(viewer, -1, 0);
		break;
	case 'k':
	case 'K':
		viewer_move_mouse(viewer, 0, 1);
		break;
	case 'l':
	case 'L':
		viewer_move_mouse(viewer, 1, 0);
		break;
	case 'u':
	case 'U':
		viewer->mouse_left = true;
		viewer_vnc_send_pointer(viewer);
		viewer->mouse_left = false;
		viewer_vnc_send_pointer(viewer);
		break;
	case 'o':
	case 'O':
		viewer->mouse_right = true;
		viewer_vnc_send_pointer(viewer);
		viewer->mouse_right = false;
		viewer_vnc_send_pointer(viewer);
		break;
	case '7':
		viewer->mouse_left = !viewer->mouse_left;
		viewer_vnc_send_pointer(viewer);
		break;
	case '8':
		viewer->mouse_middle = !viewer->mouse_middle;
		viewer_vnc_send_pointer(viewer);
		break;
	case '9':
		viewer->mouse_right = !viewer->mouse_right;
		viewer_vnc_send_pointer(viewer);
		break;
	case '0':
		viewer->mouse_middle = true;
		viewer_vnc_send_pointer(viewer);
		viewer->mouse_middle = false;
		viewer_vnc_send_pointer(viewer);
		break;
	case 'p':
	case 'P':
		viewer_zoom_to_cursor(viewer);
		break;
		/* Toggle keys */
	case 'z':
	case 'Z':
		viewer->hold_lctrl = !viewer->hold_lctrl;
		viewer_vnc_toggle_key(viewer, XK_Control_L, viewer->hold_lctrl);
		break;
	case 'm':
	case 'M':
		viewer->hold_rctrl = !viewer->hold_rctrl;
		viewer_vnc_toggle_key(viewer, XK_Control_R, viewer->hold_rctrl);
		break;
	case 'x':
	case 'X':
		viewer->hold_lshift = !viewer->hold_lshift;
		viewer_vnc_toggle_key(viewer, XK_Shift_L, viewer->hold_lshift);
		break;
	case 'n':
	case 'N':
		viewer->hold_rshift = !viewer->hold_rshift;
		viewer_vnc_toggle_key(viewer, XK_Shift_R, viewer->hold_rshift);
		break;
	case 'c':
	case 'C':
		viewer->hold_lalt = !viewer->hold_lalt;
		viewer_vnc_toggle_key(viewer, XK_Alt_L, viewer->hold_lalt);
		break;
	case 'b':
	case 'B':
		viewer->hold_ralt = !viewer->hold_ralt;
		viewer_vnc_toggle_key(viewer, XK_Alt_R, viewer->hold_ralt);
		break;
	case 'v':
	case 'V':
		viewer->hold_lsuper = !viewer->hold_lsuper;
		viewer_vnc_toggle_key(viewer, XK_Super_L, viewer->hold_lsuper);
		break;
	}
}

void viewer_terminate(struct viewer *viewer)
{
	caca_free_display(viewer->disp);
	caca_free_canvas(viewer->view);
	if (viewer->fb_dither != NULL) {
		caca_free_dither(viewer->fb_dither);
	}
}
