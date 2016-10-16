all:
	gcc -g -O3 -Wall -o headmore `pkg-config --cflags --libs libvncclient caca` vnc.c viewer.c main.c 

clean:
	rm headmore
