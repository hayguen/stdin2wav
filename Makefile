
all:	stdin2wav

.PHONY:	clean

clean:
	-rm stdin2wav

stdin2wav:	stdin2wav.c
	gcc -std=c99 -g -o stdin2wav stdin2wav.c -lsndfile


#	gcc -std=c99 -g -o stdin2wav stdin2wav.c /usr/lib/arm-linux-gnueabihf/libsndfile.so
#	gcc -std=c99 -g -o stdin2wav -lsndfile stdin2wav.c
