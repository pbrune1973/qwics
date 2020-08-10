OPENCOBOL = /Users/pbrune/open-cobol-code-1380-branches-gnu-cobol-2.0
POSTGRES = /Applications/Postgres.app/Contents/Versions/latest

CC = gcc
CFLAGS = -I$(OPENCOBOL) -I$(POSTGRES)/include -I/opt/local/include -L$(POSTGRES)/lib -L$(OPENCOBOL)/libcob
TPMSRC = ../tpmserver
SRC = .
OBJS = $(SRC)/cobsql.o $(SRC)/db.o $(TPMSRC)/env/envconf.o
LIBS = -lcob -lpq

%.o : %.c
        $(CC) -c $(CFLAGS) $< -o $@


batchrun: $(OBJS)
        $(CC) $(CFLAGS) -o batchrun $(OBJS) $(LIBS)
        mv batchrun ../../bin

clean:
        rm -r $(OBJS) ../../bin/batchrun  
        
