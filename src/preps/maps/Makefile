CC = gcc
CFLAGS =  
SRC = .
OBJS = $(SRC)/mapprep.o 
LIBS = 



.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


mapprep: $(OBJS) 
	$(CC) $(CFLAGS) -o ../../../bin/mapprep $(OBJS) $(LIBS)
	
	
clean:
	rm -r $(OBJS) ../../../bin/mapprep
