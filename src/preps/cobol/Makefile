CC = gcc
CFLAGS =  
SRC = .
OBJS = $(SRC)/cobprep.o 
OBJS2 = $(SRC)/cobbatchprep.o
LIBS = 



.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


cobprep: $(OBJS) 
	$(CC) $(CFLAGS) -o ../../../bin/cobprep $(OBJS) $(LIBS)
	
cobbatchprep: $(OBJS2)
	$(CC) $(CFLAGS) -o ../../../bin/cobbatchprep $(OBJS2) $(LIBS)
	
clean:
	rm -r $(OBJS) $(OBJS2) ../../../bin/cobprep
