OPENCOBOL = /Users/pbrune/open-cobol-code-1380-branches-gnu-cobol-2.0
POSTGRES = /Applications/Postgres.app/Contents/Versions/latest

CC = gcc
CFLAGS = -I$(OPENCOBOL) -I$(POSTGRES)/include -I/opt/local/include -L$(POSTGRES)/lib -L$(OPENCOBOL)/libcob -L. -Wl,--export-dynamic
TPMSRC = ../tpmserver
SRC = .
OBJS = $(SRC)/cobsql.o $(SRC)/db.o $(SRC)/datasetfh.o $(TPMSRC)/env/envconf.o
LIBS = -lcob -lpq -ljclpars

%.o : %.c
        $(CC) -c $(CFLAGS) $< -o $@


batchrun: $(OBJS)
        $(CC) $(CFLAGS) -o batchrun $(OBJS) $(LIBS)
        mv batchrun ../../bin

jclpars:
        $(CC) -fPIC -c libjclpars.c
        #$(CC) -dynamiclib -undefined suppress -flat_namespace libjclpars.o -o libjclpars.dylib
        $(CC) -shared libjclpars.o -o libjclpars.so
        #mv libjclpars.dylib ../../bin
        mv libjclpars.so ../../bin

clean:
        rm -r $(OBJS) ../../bin/batchrun  
        
