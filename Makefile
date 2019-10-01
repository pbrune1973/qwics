
OPENCOBOL = /bigdata3/brune/gnucobol-2.2
POSTGRES = /bigdata3/bin

CC = gcc
CFLAGS = -I$(OPENCOBOL) -I$(POSTGRES)/include -I/opt/local/include -L$(OPENCOBOL)/libcob -L$(POSTGRES)/lib 
TPMSRC = src/tpmserver
TPMOBJS = $(TPMSRC)/tpmserver.o $(TPMSRC)/cobexec.o $(TPMSRC)/db/conpool.o
LIBS = -lcob -lpthread -lpq -ldl



.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


tpmserver: $(TPMOBJS) 
	$(CC) $(CFLAGS) -o bin/tpmserver $(TPMOBJS) $(LIBS)
	
	
clean:
	rm -r $(TPMOBJS) bin/tpmserver
