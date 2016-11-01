#include <string.h>
#include "geo.h"

struct geo_facts geo_facts_of(struct vnc *vnc, caca_display_t * disp,
			      caca_canvas_t * view)
{
	struct geo_facts ret;
	ret.disp_width = caca_get_display_width(disp);
	ret.disp_height = caca_get_display_height(disp);
	ret.term_width = caca_get_canvas_width(view);
	ret.term_height = caca_get_canvas_height(view);
	ret.vnc_width = vnc->conn->width;
	ret.vnc_height = vnc->conn->height;
	return ret;
}

void geo_init(struct geo *g, struct geo_facts facts)
{
	memset(g, 0, sizeof(struct geo));
	/* Fill the zoom table */
	int i;
	g->zoom_lvls[0] = 1.0;
	for (i = 0; i < GEO_ZOOM_MAX_LVL; i++) {
		g->zoom_lvls[i + 1] = g->zoom_lvls[i] * GEO_ZOOM_STEP;
	}
	/*
	 * Mouse speed is the inverse of zoom.
	 * At maximum zoom, mouse moves slowest at speed of 1px/move.
	 * Each zoom step lower speeds up mouse movement by 2px/move.
	 */
	for (i = 0; i < GEO_ZOOM_MAX_LVL + 1; i++) {
		g->mouse_speed[i] = 1 + (GEO_ZOOM_MAX_LVL - i) * 2;
	}

	/* Begin by placing image right at the centre */
	g->view_x = 0.5;
	g->view_y = 0.5;
	geo_zoom(g, facts, 0);

	/* Place mouse pointer at centre of screen and depress all buttons */
	g->mouse_x = facts.vnc_width / 2;
	g->mouse_y = facts.vnc_height / 2;
}

void geo_zoom(struct geo *g, struct geo_facts facts, int offset)
{
	g->zoom += offset;
	if (g->zoom < 0) {
		g->zoom = 0;
	} else if (g->zoom > GEO_ZOOM_MAX_LVL) {
		g->zoom = GEO_ZOOM_MAX_LVL;
	}

	g->zoom_x =
	    (g->zoom <
	     0) ? 1.0 / g->zoom_lvls[-g->zoom] : g->zoom_lvls[g->zoom];
	g->zoom_y =
	    g->zoom_x * facts.term_width / facts.term_height *
	    facts.vnc_height / facts.vnc_width * facts.term_height /
	    facts.term_width * facts.disp_width / facts.disp_height;

	if (g->zoom_y > g->zoom_x) {
		float tmp = g->zoom_x;
		g->zoom_x = tmp * tmp / g->zoom_y;
		g->zoom_y = tmp;
	}
}

void geo_pan(struct geo *g, int pan_x, int pan_y)
{
	if (pan_x != 0) {
		if (g->zoom_x > 1.0) {
			g->view_x += pan_x * (GEO_PAN_STEP / g->zoom_x);
		}
		if (g->view_x < 0.0) {
			g->view_x = 0.0;
		} else if (g->view_x > 1.0) {
			g->view_x = 1.0;
		}
	}
	if (pan_y != 0) {
		if (g->zoom_y > 1.0) {
			g->view_y += pan_y * (GEO_PAN_STEP / g->zoom_y);
		}
		if (g->view_y < 0.0) {
			g->view_y = 0.0;
		} else if (g->view_y > 1.0) {
			g->view_y = 1.0;
		}
	}
}

void geo_move_mouse(struct geo *g, struct geo_facts facts, int step_x,
		    int step_y)
{
	int speed = g->mouse_speed[g->zoom];
	g->mouse_x += step_x * speed;
	if (g->mouse_x < 0) {
		g->mouse_x = 0;
	} else if (g->mouse_x >= facts.vnc_width) {
		g->mouse_x = facts.vnc_width - 1;
	}
	g->mouse_y += step_y * speed;
	if (g->mouse_y < 0) {
		g->mouse_y = 0;
	} else if (g->mouse_y >= facts.vnc_height) {
		g->mouse_y = facts.vnc_height - 1;
	}
}

void geo_zoom_to_cursor(struct geo *g, struct geo_facts facts)
{
	/* Zoom in to approximately 67% of maximum zoom level */
	geo_zoom(g, facts, GEO_ZOOM_CURSOR_LVL - g->zoom);
	/* Calculate position of mouse cursor relative to the entire canvas */
	g->view_x = (float)g->mouse_x / (float)facts.vnc_width;
	g->view_y = (float)g->mouse_y / (float)facts.vnc_height;
}

struct geo_dither_params geo_get_dither_params(struct geo *g,
					       struct geo_facts facts)
{
	float delta_x = (g->zoom_x > 1.0) ? g->view_x : 0.5;
	float delta_y = (g->zoom_y > 1.0) ? g->view_y : 0.5;
	struct geo_dither_params ret;
	ret.x = facts.term_width * (1.0 - g->zoom_x) * delta_x;
	ret.y = facts.term_height * (1.0 - g->zoom_y) * delta_y;
	ret.width = facts.term_width * g->zoom_x + 1;
	ret.height = facts.term_height * g->zoom_y + 1;
	return ret;
}
