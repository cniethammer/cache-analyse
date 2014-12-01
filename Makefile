CC     = gcc
CFLAGS = -O2
LDFLAGS = -O2 -lm

ifdef PAPI
  CFLAGS += " -DPAPI"
  LDFLAGS += " -lpapi"
endif

ifndef NPAD
  NPAD = 7 
endif

CFLAGS+= -DNPAD=$(NPAD)

.PHONY: default clean cleanall


default: run

run: cache-analyse
	./$<

  


clean:
	rm -f *.o

cleanall: clean
	rm -f cache-analyse
