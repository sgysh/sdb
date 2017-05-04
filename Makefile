TARGETS := sdb sample

.PHONY : all clean

all: $(TARGETS)

sdb: sdb.cc
	g++ -std=c++11 -o sdb sdb.cc

sample: sample.c
	gcc -std=c99 -o sample sample.c

clean:
	rm -v $(TARGETS)

