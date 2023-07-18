
OPENCOBOL = /home/qwics/gnucobol-3.1.2
POSTGRES = /usr

CC = gcc
CFLAGS = -I$(OPENCOBOL) -I$(POSTGRES)/include/postgresql -L/usr/local/lib -L$(POSTGRES)/lib 
TPMSRC = src/tpmserver
TPMOBJS = $(TPMSRC)/tpmserver.o $(TPMSRC)/cobexec.o $(TPMSRC)/db/conpool.o $(TPMSRC)/msg/queueman.o $(TPMSRC)/shm/shmtpm.o $(TPMSRC)/enqdeq/enqdeq.o $(TPMSRC)/env/envconf.o 
LIBS = -lcob -lpthread -lpq -ldl


.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


tpmserver: $(TPMOBJS) 
	$(CC) $(CFLAGS) -o bin/tpmserver $(TPMOBJS) $(LIBS)
	
	
clean:
	rm -r $(TPMOBJS) bin/tpmserver
