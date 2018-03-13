OPENCOBOL = /Users/pbrune/open-cobol-code-1380-branches-gnu-cobol-2.0
POSTGRES = /Applications/Postgres.app/Contents/Versions/latest

CC = gcc
CFLAGS =  
SRC = .
OBJS = $(SRC)/mapprep.o 
LIBS = 



.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


cobprep: $(OBJS) 
	$(CC) $(CFLAGS) -o ../../../bin/mapprep $(OBJS) $(LIBS)
	
	
clean:
	rm -r $(OBJS) ../../../bin/mapprep
