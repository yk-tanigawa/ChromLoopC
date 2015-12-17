CC = gcc
LD = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = #-lpthread -lm
SRCS := $(wildcard *.c) # wildcard
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.dep)
EXEC = $(SRCS:.c=)
RM = rm -f


all: $(EXEC)

prep: prep.o
	$(LD) $(LDFLAGS) -o $@ $^

kmerFreqOdds: kmerFreqOdds.o
	$(LD) $(LDFLAGS) -o $@ $^

kmerPairBoost: kmerPairBoost.o
	$(LD) $(LDFLAGS) -o $@ $^

kmerPairBoost.o: adaboost.h calloc_errchk.h io.h diffSec.h bit_op.h

adaboost.h: calloc_errchk.h diffSec.h bit_op.h

chrom: chrom.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(OBJS) $(EXEC) *~

.PHONY:
	all clean
