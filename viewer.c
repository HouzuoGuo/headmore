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
	"`     Toggle input to viewer/VNC   ",
	"~     Click back-tick in VNC       ",
	"wasd  Pan viewer                   ",
	"q/e   Zoom out/in                  ",
	"Space Toggle display mouse pointer ",
	"============ RIGHT HAND ===========",
	"F10  Quit                          ",
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
static struct _rfbClient *rfb(struct viewer *v)
{
	return v->vnc->conn;
}

bool viewer_init(struct viewer * v, struct vnc * vnc)
{
	/* All bool switches are off by default */
	memset(v, 0, sizeof(struct viewer));
	/* Initialise visuals */
	v->view = caca_create_canvas(0, 0);
	if (!v->view) {
		fprintf(stderr, "Failed to create caca canvas\n");
		return false;
	}
	v->disp = caca_create_display_with_driver(v->view, "ncurses");
	if (!v->disp) {
		fprintf(stderr, "Failed to create caca display\n");
		return false;
	}
	v->vnc = vnc;
	caca_set_display_title(v->disp, rfb(v)->desktopName);

	/* Initialise parameters for geometry calculation */
	geo_init(&v->geo, viewer_geo(v));

	/* Tell VNC to place mouse pointer to default position */
	viewer_vnc_send_pointer(v);
	return true;
}

struct geo_facts viewer_geo(struct viewer *v)
{
	return geo_facts_of(v->vnc, v->disp, v->view);
}

void viewer_disp_status(struct viewer *v)
{
	caca_set_color_ansi(v->view, CACA_WHITE, CACA_BLUE);
	char *conn_remark = "";
	if (!v->vnc->connected) {
		conn_remark = "(Disconnected)";
	}
	char *who_has_input = "Input to viewer";
	if (v->input2vnc) {
		who_has_input = "Input to VNC (~ to disengage)";
	}
	char held_controls[80] = { 0 };
	bool disp_held_controls = false;
	if (v->mouse_left) {
		disp_held_controls = true;
		strcat(held_controls, " LMouse");
	}
	if (v->mouse_middle) {
		disp_held_controls = true;
		strcat(held_controls, " MMouse");
	}
	if (v->mouse_right) {
		disp_held_controls = true;
		strcat(held_controls, " RMouse");
	}
	if (v->hold_lctrl) {
		disp_held_controls = true;
		strcat(held_controls, " LCtrl");
	}
	if (v->hold_lshift) {
		disp_held_controls = true;
		strcat(held_controls, " LShift");
	}
	if (v->hold_lalt) {
		disp_held_controls = true;
		strcat(held_controls, " LAlt");
	}
	if (v->hold_lsuper) {
		disp_held_controls = true;
		strcat(held_controls, " LSuper");
	}
	if (v->hold_ralt) {
		disp_held_controls = true;
		strcat(held_controls, " RAlt");
	}
	if (v->hold_rshift) {
		disp_held_controls = true;
		strcat(held_controls, " RShift");
	}
	if (v->hold_rctrl) {
		disp_held_controls = true;
		strcat(held_controls, " RCtrl");
	}
	char held_controls_msg[100] = { 0 };
	if (disp_held_controls) {
		strcat(held_controls_msg, "| Holding down:");
		strcat(held_controls_msg, held_controls);
	}
	caca_printf(v->view, 0, 0, "h:Help | %s:%d%s | %s %s",
		    rfb(v)->serverHost, rfb(v)->serverPort, conn_remark,
		    who_has_input, held_controls_msg);
}

void viewer_disp_help(struct viewer *v)
{
	caca_set_color_ansi(v->view, CACA_WHITE, CACA_BLUE);
	int i;
	for (i = 0; viewer_help[i] != NULL; i++) {
		caca_put_str(v->view, 0, 1 + i, viewer_help[i]);
	}
}

void viewer_redraw(struct viewer *v)
{
	caca_clear_canvas(v->view);
	/*
	 * Run the latest frame-buffer content through Floyd–Steinberg algorithm -
	 * it seems to offer higher quality over other algorithm choices.
	 * The dither parameters are tailored for a connection on 32-bit RGB colours.
	 */
	if (v->fb_dither != NULL) {
		caca_free_dither(v->fb_dither);
	}
	struct geo_facts facts = viewer_geo(v);
	v->fb_dither =
	    caca_create_dither(32, facts.vnc_width, facts.vnc_height,
			       facts.vnc_width * 4, 0x000000ff, 0x0000ff00,
			       0x00ff0000, 0);
	caca_set_dither_algorithm(v->fb_dither, "fstein");
	caca_set_dither_gamma(v->fb_dither, 1.0);
	struct geo_dither_params params =
	    geo_get_dither_params(&v->geo, viewer_geo(v));
	caca_dither_bitmap(v->view, params.x, params.y, params.width,
			   params.height, v->fb_dither, rfb(v)->frameBuffer);
	/*
	 * Mouse cursors are usually wider than 14 pixels. If it will not take
	 * more than 5 characters to draw the cusor, then consider it very
	 * difficult to spot on the VNC canvas, and draw a red block right there.
	 */
	int mouse_ch_x = geo_dither_ch_px_x(&params, v->geo.mouse_x);
	int mouse_ch_y = geo_dither_ch_px_y(&params, v->geo.mouse_y);
	if (geo_dither_numch_x(&params, 12) < 5) {
		caca_set_color_ansi(v->view, CACA_WHITE, CACA_RED);
		caca_fill_box(v->view, mouse_ch_x - 1, mouse_ch_y - 1, 3, 3,
			      '*');
	}
	/* Draw local mouse pointer */
	if (v->draw_mouse_pointer) {
		caca_set_color_ansi(v->view, CACA_WHITE, CACA_RED);
		caca_put_char(v->view, mouse_ch_x, mouse_ch_y, '*');
	}
	viewer_disp_status(v);
	if (v->disp_help) {
		viewer_disp_help(v);
	}
	caca_refresh_display(v->disp);
}

void viewer_ev_loop(struct viewer *v)
{
	int ev_accept =
	    CACA_EVENT_KEY_PRESS | CACA_EVENT_RESIZE | CACA_EVENT_QUIT;
	while (true) {
		/* Listen to the latest event */
		caca_event_t ev;
		caca_get_event(v->disp, ev_accept, &ev,
			       VIEWER_FRAME_INTVL_USEC);
		/* Certain types of events are caca calling quit */
		enum caca_event_type ev_type = caca_get_event_type(&ev);
		if (ev_type & CACA_EVENT_QUIT || ev_type & CACA_EVENT_NONE) {
			return;
		}
		/* Handle previously banked escape key (VNC input), send it to VNC. */
		if (v->last_vnc_esc != 0
		    && get_time_usec() - v->last_vnc_esc >=
		    VIEWER_FRAME_INTVL_USEC) {
			v->last_vnc_esc = 0;
			viewer_vnc_click_key(v, cacakey2vnc(CACA_KEY_ESCAPE));
		}
		/* Redraw at a constant frame rate when there is no key input */
		if (!(ev_type & CACA_EVENT_KEY_PRESS)) {
			viewer_redraw(v);
			continue;
		}
		int ev_char = caca_get_event_key_ch(&ev);
		/* Input never gets directed at VNC if it is disconnected */
		if (!v->vnc->connected) {
			v->input2vnc = false;
		}
		/* A key input is directed at either VNC or viewer controls */
		if (v->input2vnc && ev_char != '`') {
			viewer_input_to_vnc(v, ev_char);
		} else if (!viewer_handle_control(v, ev_char)) {
			return;
		}
	}
}

void viewer_vnc_click_key(struct viewer *v, int vnc_key)
{
	SendKeyEvent(rfb(v), vnc_key, TRUE);
	SendKeyEvent(rfb(v), vnc_key, FALSE);
}

void viewer_vnc_click_ctrl_key_combo(struct viewer *v, int vnc_key)
{
	SendKeyEvent(rfb(v), XK_Control_L, TRUE);
	SendKeyEvent(rfb(v), vnc_key, TRUE);
	SendKeyEvent(rfb(v), vnc_key, FALSE);
	SendKeyEvent(rfb(v), XK_Control_L, FALSE);
}

void viewer_vnc_toggle_key(struct viewer *v, int vnc_key, bool key_down)
{
	SendKeyEvent(rfb(v), vnc_key, key_down ? TRUE : FALSE);
}

void viewer_vnc_send_pointer(struct viewer *v)
{
	int mask = 0;
	if (v->mouse_left) {
		mask |= rfbButton1Mask;
	}
	if (v->mouse_middle) {
		mask |= rfbButton2Mask;
	}
	if (v->mouse_right) {
		mask |= rfbButton3Mask;
	}
	SendPointerEvent(rfb(v), v->geo.mouse_x, v->geo.mouse_y, mask);
}

void viewer_input_to_vnc(struct viewer *v, int caca_key)
{
	/*
	 * Escape key could mean two things, either it is a key press by itself, or it
	 * could be an Alt key combination. An Alt key combination is sent in two key
	 * events, first event is an escape key, second is the combo key.
	 * To distinguish between the two meanings, the escape key is banked here and
	 * dealt with later.
	 */
	if (caca_key == CACA_KEY_ESCAPE) {
		v->last_vnc_esc = get_time_usec();
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
			viewer_vnc_click_ctrl_key_combo(v, caca_key - 1 + 'a');
			return;
		case 8:	/* H and an extra backspace */
			v->void_backsp = true;
			viewer_vnc_click_ctrl_key_combo(v, 'h');
			return;
		case 9:	/* I and an extra tab */
			v->void_tab = true;
			viewer_vnc_click_ctrl_key_combo(v, 'i');
			return;
		case 13:	/* M and an extra enter */
			v->void_ret = true;
			viewer_vnc_click_ctrl_key_combo(v, 'm');
			return;
		case 19:	/* S and an extra pause */
			v->void_pause = true;
			viewer_vnc_click_ctrl_key_combo(v, 's');
			return;
		}
	}
	/*
	 * If the previous key was a control sequence that came with an extra key
	 * event, discard it and do not send it over VNC.
	 */
	switch (caca_key) {
	case CACA_KEY_BACKSPACE:
		if (v->void_backsp) {
			v->void_backsp = false;
			return;
		}
		break;
	case CACA_KEY_TAB:
		if (v->void_tab) {
			v->void_tab = false;
			return;
		}
		break;
	case CACA_KEY_RETURN:
		if (v->void_ret) {
			v->void_ret = false;
			return;
		}
	case CACA_KEY_PAUSE:
		if (v->void_pause) {
			v->void_pause = false;
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
	 * In case there was a banked escape key, the Alt combination key shall
	 * arrive pretty soon, and most definitely before the next frame refresh.
	 * Occasionally it takes even longer to arrive but there is no way to
	 * work around it.
	 */
	suseconds_t elapsed = get_time_usec() - v->last_vnc_esc;
	if (elapsed < VIEWER_FRAME_INTVL_USEC
	    && elapsed < VIEWER_MAX_INPUT_INTVL_USEC) {
		viewer_vnc_toggle_key(v, XK_Alt_L, true);
		viewer_vnc_click_key(v, translated_ch);
		viewer_vnc_toggle_key(v, XK_Alt_L, false);
		v->last_vnc_esc = 0;
		return;
	}
	/* Finally the key code can be sent to VNC! */
	viewer_vnc_click_key(v, translated_ch);
}

bool viewer_handle_control(struct viewer * v, int caca_key)
{
	/*
	 * When the canvas is too busy panning/zooming, causing very low FPS,
	 * the terminal occasionally generates repeatitive key input seemingly
	 * out of nowhere, in very short successions. To work around it, this
	 * condition caps number of viewe controls to approximately 10 per second.
	 */
	suseconds_t elapsed = get_time_usec() - v->last_viewer_control;
	if (v->last_viewer_control != 0
	    && elapsed < VIEWER_MAX_INPUT_INTVL_USEC) {
		return true;
	}
	/*
	 * In order to avoid redrawing too rapidly, only viewer zoom/pan
	 * actions redraw immediately.
	 * VNC input has to wait for main loop to redraw at an interval.
	 */
	switch (caca_key) {
	case 'h':
	case 'H':
		v->disp_help = !v->disp_help;
		break;
	case CACA_KEY_F10:
		return false;
		/* Left hand */
	case 'w':
	case 'W':
		geo_pan(&v->geo, 0, -1);
		viewer_redraw(v);
		break;
	case 'a':
	case 'A':
		geo_pan(&v->geo, -1, 0);
		viewer_redraw(v);
		break;
	case 's':
	case 'S':
		geo_pan(&v->geo, 0, 1);
		viewer_redraw(v);
		break;
	case 'd':
	case 'D':
		geo_pan(&v->geo, 1, 0);
		viewer_redraw(v);
		break;
	case 'q':
	case 'Q':
		geo_zoom(&v->geo, viewer_geo(v), -1);
		viewer_redraw(v);
		break;
	case 'e':
	case 'E':
		geo_zoom(&v->geo, viewer_geo(v), 1);
		viewer_redraw(v);
		break;
	case '`':
		if (v->vnc->connected) {
			v->input2vnc = !v->input2vnc;
		}
		break;
	case '~':
		/* Click back-tick (96 in ASCII) in VNC */
		viewer_vnc_click_key(v, 96);
		break;
	case ' ':
		v->draw_mouse_pointer = !v->draw_mouse_pointer;
		break;
		/* Right hand */
	case 'i':
	case 'I':
		geo_move_mouse(&v->geo, viewer_geo(v), 0, -1);
		viewer_vnc_send_pointer(v);
		break;
	case 'j':
	case 'J':
		geo_move_mouse(&v->geo, viewer_geo(v), -1, 0);
		viewer_vnc_send_pointer(v);
		break;
	case 'k':
	case 'K':
		geo_move_mouse(&v->geo, viewer_geo(v), 0, 1);
		viewer_vnc_send_pointer(v);
		break;
	case 'l':
	case 'L':
		geo_move_mouse(&v->geo, viewer_geo(v), 1, 0);
		viewer_vnc_send_pointer(v);
		break;
	case 'u':
	case 'U':
		v->mouse_left = true;
		viewer_vnc_send_pointer(v);
		v->mouse_left = false;
		viewer_vnc_send_pointer(v);
		break;
	case 'o':
	case 'O':
		v->mouse_right = true;
		viewer_vnc_send_pointer(v);
		v->mouse_right = false;
		viewer_vnc_send_pointer(v);
		break;
	case '7':
		v->mouse_left = !v->mouse_left;
		viewer_vnc_send_pointer(v);
		break;
	case '8':
		v->mouse_middle = !v->mouse_middle;
		viewer_vnc_send_pointer(v);
		break;
	case '9':
		v->mouse_right = !v->mouse_right;
		viewer_vnc_send_pointer(v);
		break;
	case '0':
		v->mouse_middle = true;
		viewer_vnc_send_pointer(v);
		v->mouse_middle = false;
		viewer_vnc_send_pointer(v);
		break;
	case 'p':
	case 'P':
		geo_zoom_to_cursor(&v->geo, viewer_geo(v));
		break;
		/* Toggle keys */
	case 'z':
	case 'Z':
		v->hold_lctrl = !v->hold_lctrl;
		viewer_vnc_toggle_key(v, XK_Control_L, v->hold_lctrl);
		break;
	case 'm':
	case 'M':
		v->hold_rctrl = !v->hold_rctrl;
		viewer_vnc_toggle_key(v, XK_Control_R, v->hold_rctrl);
		break;
	case 'x':
	case 'X':
		v->hold_lshift = !v->hold_lshift;
		viewer_vnc_toggle_key(v, XK_Shift_L, v->hold_lshift);
		break;
	case 'n':
	case 'N':
		v->hold_rshift = !v->hold_rshift;
		viewer_vnc_toggle_key(v, XK_Shift_R, v->hold_rshift);
		break;
	case 'c':
	case 'C':
		v->hold_lalt = !v->hold_lalt;
		viewer_vnc_toggle_key(v, XK_Alt_L, v->hold_lalt);
		break;
	case 'b':
	case 'B':
		v->hold_ralt = !v->hold_ralt;
		viewer_vnc_toggle_key(v, XK_Alt_R, v->hold_ralt);
		break;
	case 'v':
	case 'V':
		v->hold_lsuper = !v->hold_lsuper;
		viewer_vnc_toggle_key(v, XK_Super_L, v->hold_lsuper);
		break;
	}
	v->last_viewer_control = get_time_usec();
	return true;
}

void viewer_terminate(struct viewer *v)
{
	if (v->fb_dither != NULL) {
		caca_free_dither(v->fb_dither);
	}
	if (v->disp != NULL) {
		caca_free_display(v->disp);
	}
	if (v->view != NULL) {
		caca_free_canvas(v->view);
	}
}
