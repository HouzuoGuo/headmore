all:
	gcc -g -O3 -Wall -o headmore *.c -lpthread `pkg-config --cflags --libs caca libvncclient`

clean:
	rm headmore
