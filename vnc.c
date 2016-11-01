#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <rfb/keysym.h>
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>
#include <caca.h>
#include "vnc.h"

/* Process RFB server messages, block caller. Return NULL. */
static void *io_loop_fun(void *struct_vnc)
{
	struct vnc *vnc = (struct vnc *)struct_vnc;
	while (true) {
		if (!vnc->cont_io_loop) {
			break;
		}
		if (WaitForMessage(vnc->conn, VNC_POLL_TIMEOUT_USEC) < 0
		    || !HandleRFBServerMessage(vnc->conn)) {
			rfbClientLog
			    ("Error has occurred in the VNC IO routine\n");
			vnc->connected = false;
			break;
		}
	}
	return NULL;
}

bool vnc_init(struct vnc * v, int argc, char **argv)
{
	memset(v, 0, sizeof(struct vnc));
	/*
	 * The connection asks server for 32-bit RGB colours.
	 * Take note that VNC does not use alpha channel, hence the most significant 2 bytes are useless.
	 * I think 256-colours should be more than sufficient for this VNC client, but somehow I could not
	 * get a sufficiently good quality out of dithering process, perhaps I got the bit-masks wrong?
	 */
	v->conn = rfbGetClient(8, 3, 4);
	v->conn->canHandleNewFBSize = FALSE;
	if (!rfbInitClient(v->conn, &argc, argv)) {
		return false;
	}
	v->cont_io_loop = true;
	v->connected = true;
	if (pthread_create(&v->io_loop, NULL, io_loop_fun, (void *)v) != 0) {
		fprintf(stderr, "Failed to create message loop thread\n");
		return false;
	}
	return true;
}

void vnc_destroy(struct vnc *v)
{
	v->cont_io_loop = false;
	v->connected = false;
	if (pthread_join(v->io_loop, NULL) != 0) {
		fprintf(stderr, "Failed to join message loop thread\n");
	}
	uint8_t *fb = v->conn->frameBuffer;
	rfbClientCleanup(v->conn);
	free(fb);
	rfbClientLog("VNC connection has been terminated\n");
}

int cacakey2vnc(int caca_key)
{
	/* Ordinary visible characters in ASCII table do not require translation */
	if (caca_key >= 32 && caca_key <= 126)
		return caca_key;
	/* Translate F1-F15 */
	if (caca_key >= CACA_KEY_F1 && caca_key <= CACA_KEY_F15)
		return caca_key - CACA_KEY_F1 + XK_F1;
	/* More special keys */
	int vnc_key;
	switch (caca_key) {
	case CACA_KEY_BACKSPACE:
		vnc_key = XK_BackSpace;
		break;
	case CACA_KEY_TAB:
		vnc_key = XK_Tab;
		break;
	case CACA_KEY_RETURN:
		vnc_key = XK_Return;
		break;
	case CACA_KEY_PAUSE:
		vnc_key = XK_Pause;
		break;
	case CACA_KEY_ESCAPE:
		vnc_key = XK_Escape;
		break;
	case CACA_KEY_DELETE:
		vnc_key = XK_Delete;
		break;
	case CACA_KEY_UP:
		vnc_key = XK_Up;
		break;
	case CACA_KEY_DOWN:
		vnc_key = XK_Down;
		break;
	case CACA_KEY_RIGHT:
		vnc_key = XK_Right;
		break;
	case CACA_KEY_LEFT:
		vnc_key = XK_Left;
		break;
	case CACA_KEY_INSERT:
		vnc_key = XK_Insert;
		break;
	case CACA_KEY_HOME:
		vnc_key = XK_Home;
		break;
	case CACA_KEY_END:
		vnc_key = XK_End;
		break;
	case CACA_KEY_PAGEUP:
		vnc_key = XK_Page_Up;
		break;
	case CACA_KEY_PAGEDOWN:
		vnc_key = XK_Page_Down;
		break;
	default:
		vnc_key = -1;
	}
	/* Catch other weird low keys just in case */
	if (vnc_key == -1 && caca_key < 0x100) {
		vnc_key = caca_key;
	}
	return vnc_key;
}
