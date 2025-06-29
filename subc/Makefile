CFLAGS 	 = -std=c99 -Werror -Wall -Wno-unused -g
OFLAGS 	 = -std=c99 -Wno-unused -DNDEBUG -O2
CPPFLAGS = -I/opt/local/include
LDFLAGS  = -L/opt/local/lib
LDLIBS   = -lgc -lm

all : main

%.c : %.leg
	leg -o $@ $<

% : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

opt : main.c
	$(CC) $(OFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

test : main
	./main -vv test.txt

eratosthenes : main
	./main -vv mydemo/eratosthenes.c

cast :main
	./main -vv mydemo/cast.c

shl :main
	./main -vv mydemo/shl.c

shr :main
	./main -vv mydemo/shr.c

fisr : main
	./main -vv mydemo/fisr.c

fib : main
	./main -vv mydemo/fib.c

dang : main
	./main -vv demofiles/dangling-pointer.c

pdiff : main
	./main -vv mydemo/pdiff.c
	
unin : main
	./main -vv mydemo/unin.c

pcomp :main
	./main -vv demofiles/pointer-compare.c 

demo : main
	for i in demofiles/*.c; do echo "--------- $$i"; ./main $$i;  done

demov : main
	for i in demofiles/*.c; do echo $$i; ./main -vv $$i;  done

bench : opt
	time ./opt bench.c 28

spotless : clean
	rm -rf *~ *.dSYM

clean : .FORCE
	rm -f main opt

% :main
	./main -vv mydemo/$@.c
.FORCE :
