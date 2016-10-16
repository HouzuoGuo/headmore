#ifndef VNC_H
#define VNC_H

#include <rfb/rfbclient.h>
#include <stdbool.h>

#define VNC_POLL_TIMEOUT_USEC 100000	/* keep it under a second */
#define VNC_DBG_LOG ".headmore.log"	/* name of log file under user's home directory */
#define VNC_PASS_MAX 256	/* maximum length of input password */

/* Connect to remote frame-buffer and handle control/image IO. */
struct vnc {
	struct _rfbClient *conn;
	bool connected, cont_io_loop;
	pthread_t io_loop;
};

/* Connect to server and immediately begin message loop in a separate thread. Return false only on failure. */
bool vnc_init(struct vnc *vnc, int argc, char **argv);
/* Close VNC connection and free all resources, including the VNC client itself. */
void vnc_destroy(struct vnc *vnc);
/* Translate a key code as read by libcaca to its corresponding VNC key code. Return -1 only if no translation. */
int cacakey2vnc(int keych);

#endif
