HEADER = $(shell find . -name "*.h")

CC     = icc
CFLAGS = -O2 -g -DPAPI
LINKFLAGS = -O2 -lm -lpapi -g

ifndef NPAD
  NPAD = 7 
endif

.PHONY: default clean cleanall


default: run

run: cache-analyse
	./$<

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@ -DNPAD=$(NPAD)
  

cache-analyse: cache-analyse.o
	$(CC) $(LINKFLAGS) -o $@ $< 


clean:
	rm -f *.o

cleanall: clean
	rm -f cache-analyse
