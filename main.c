#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "vnc.h"
#include "viewer.h"

int main(int argc, char **argv)
{
	struct vnc vnc;
	struct viewer viewer;
	if (!vnc_init(&vnc, argc, argv)) {
		fprintf(stderr,
			"Failed to establish VNC connection (bad authentication?).\n");
		return 1;
	}
	if (!viewer_init(&viewer, &vnc)) {
		fprintf(stderr, "Failed to initialise viewer display.\n");
		return 1;
	}
	viewer_ev_loop(&viewer);
	viewer_terminate(&viewer);
	vnc_destroy(&vnc);
	return 0;
}
