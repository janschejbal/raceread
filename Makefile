all: raceread racereaddir

raceread:
	gcc -Wall -O2 -o bin/raceread raceread.c

racereaddir:
	gcc -Wall -O2 -o bin/racereaddir variants/racereaddir.c

clean:
	rm -f bin/raceread bin/racereaddir
