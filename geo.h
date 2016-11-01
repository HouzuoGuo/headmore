#ifndef GEO_H
#define GEO_H

#include <caca.h>
#include "vnc.h"

#define GEO_PAN_STEP 0.20
#define GEO_ZOOM_STEP 1.20f
#define GEO_ZOOM_MAX_LVL 15
#define GEO_ZOOM_CURSOR_LVL 9	/* zoom level for when zooming into mouse cursor */

/* Geometry facts of terminal emulator and VNC connection. */
struct geo_facts {
	int disp_width, disp_height;
	int term_width, term_height;
	int vnc_width, vnc_height;
};

/* Return geometry facts of the VNC connection and caca terminal. */
struct geo_facts geo_facts_of(struct vnc *vnc, caca_display_t * disp,
			      caca_canvas_t * canvas);

/* Keep track of geometry of remote frame-buffer and canvas. */
struct geo {
	float view_x, zoom_x, view_y, zoom_y;
	int zoom;
	float zoom_lvls[GEO_ZOOM_MAX_LVL + 1];

	int mouse_speed[GEO_ZOOM_MAX_LVL + 1];
	int mouse_x, mouse_y;
};

/* Initialise geometry structure. */
void geo_init(struct geo *g, struct geo_facts facts);
/* Calculate geometry after zooming in (+) or out (-). */
void geo_zoom(struct geo *g, struct geo_facts facts, int offset);
/* Pan the canvas several steps up (-y), down (+y), left (-x), or right (+x). */
void geo_pan(struct geo *g, int pan_x, int pan_y);
/* Move mouse pointer several steps up (-y) down (+y), left (-x) or right (+x).*/
void geo_move_mouse(struct geo *g, struct geo_facts facts, int step_x,
		    int step_y);
/* Move view to the location of mouse cursor and zoom in there. */
void geo_zoom_to_cursor(struct geo *g, struct geo_facts facts);

/* Calculated parameters for dithering algorithm. */
struct geo_dither_params {
	int x, y, width, height;
};
/* Calculate and return input parameters for dithering algorithm. */
struct geo_dither_params geo_get_dither_params(struct geo *g,
					       struct geo_facts facts);

#endif
