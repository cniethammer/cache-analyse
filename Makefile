CC      = gcc
CFLAGS  = -O2 -Wall -Wunused
LDFLAGS = -O2 -lm

ifdef DEBUG
  CFLAGS += " -g"
  LDFLAGS += " -g"
endif
ifdef PAPI
  CFLAGS += " -DPAPI"
  LDFLAGS += " -lpapi"
endif

ifndef NPAD
  NPAD = 0
endif

CFLAGS+= -DNPAD=$(NPAD)

.PHONY: default clean cleanall

default: cache-analyse

run: cache-analyse
	./$<


clean:
	rm -f *.o

cleanall: clean
	rm -f cache-analyse
