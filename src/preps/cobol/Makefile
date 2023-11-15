CC = gcc
CFLAGS =  
SRC = .
OBJS = $(SRC)/cobprep.o 
OBJS2 = $(SRC)/cobbatchprep.o
OBJS3 = $(SRC)/p3prep.o
LIBS = 



.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


cobprep: $(OBJS) 
	$(CC) $(CFLAGS) -o ../../../bin/cobprep $(OBJS) $(LIBS)
	
cobbatchprep: $(OBJS2)
	$(CC) $(CFLAGS) -o ../../../bin/cobbatchprep $(OBJS2) $(LIBS)

p3prep: $(OBJS3)
	$(CC) $(CFLAGS) -o ../../../bin/p3prep $(OBJS3) $(LIBS)
	
clean:
	rm -r $(OBJS) $(OBJS2) ../../../bin/cobprep
