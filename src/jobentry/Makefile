OPENCOBOL = /home/qwics/gnucobol-3.1.2
POSTGRES = /usr

OBJS = spool/JCLParser.o spool/SpoolingSystem.o spool/JobClass.o spool/OutputClass.o \
       spool/JobClassQueue.o spool/OutputClassQueue.o spool/Initiator.o spool/OutputWriter.o \
       spool/CardReader.o spool/semaphore.o card/RuntimeContext.o card/Parameters.o card/JobCard.o \
	   card/JOB.o card/EXEC.o card/DD.o card/PROC.o card/SET.o card/OUTPUT.o \
	   dataset/LockManager.o dataset/DataSet.o dataset/TOC.o dataset/Member.o dataset/ebcdic.o \
	   dataset/PartitionedDataSet.o dataset/TOCDataSet.o dataset/Concatenation.o dataset/Catalog.o \
	   dataset/DataSetDef.o env/envconf.o jobentry.o 


DEFINES = 

CC     = gcc
CPP    = g++
TPMSRC = ../tpmserver
CFLAGS = -I$(OPENCOBOL) -I$(POSTGRES)/include/postgresql -L/usr/local/lib -L$(POSTGRES)/lib -L/home/qwics/bin -fPIC -pie -Wl,--export-dynamic
LIBS   = -lpthread -lcob -lpq -ljclpars

.cpp.o:
	$(CPP) $(DEFINES) $(CFLAGS) -c $<

.c.o:
	$(CPP) $(DEFINES) $(CFLAGS) -c $<

jobentry: $(OBJS) 
	$(CPP) $(CFLAGS) *.o ../batchrun/cobsql.o ../batchrun/db.o ../batchrun/datasetfh.o $(TPMSRC)/env/envconf.o $(TPMSRC)/shm/shmtpm.o -o ../../bin/jobentry $(LIBS) 

clean:
	rm -f *.o
	rm -f ../../bin/jobentry
