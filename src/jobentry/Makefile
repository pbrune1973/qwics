OBJS = spool/JCLParser.o spool/SpoolingSystem.o spool/JobClass.o spool/OutputClass.o \
       spool/JobClassQueue.o spool/OutputClassQueue.o spool/Initiator.o spool/OutputWriter.o \
       spool/CardReader.o spool/semaphore.o card/RuntimeContext.o card/Parameters.o card/JobCard.o \
	   card/JOB.o card/EXEC.o card/DD.o card/PROC.o card/SET.o card/OUTPUT.o \
	   dataset/LockManager.o dataset/DataSet.o dataset/TOC.o dataset/Member.o dataset/ebcdic.o \
	   dataset/PartitionedDataSet.o dataset/Concatenation.o dataset/Catalog.o dataset/DataSetDef.o \
	   env/envconf.o jobentry.o 


DEFINES = 

CC     = gcc
CPP    = g++
CFLAGS = -Wno-deprecated
LIBS   = -lpthread

.cpp.o:
	$(CPP) $(DEFINES) $(CFLAGS) -c $<

.c.o:
	$(CPP) $(DEFINES) $(CFLAGS) -c $<

jobentry: $(OBJS) 
	$(CPP) $(LIBS) *.o -o ../../bin/jobentry

clean:
	rm -f *.o
	rm -f ../../bin/jobentry
