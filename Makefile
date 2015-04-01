chord: chord.c
	gcc -pedantic -Werror -Wall -pthread -std=gnu99 -o chord chord.c chord.h sha1.c sha1.h -lm

clean:
	rm *~
