#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/qwics/bin

export QWICS_DB_CONNECTSTR="host=qwics-postgres user=postgres password=postgres dbname=postgres"
export QWICS_DATASET_DIR=/home/qwics/dataset
export QWICS_LOADMODDIR=/home/qwics/loadmod
export QWICS_JSDIR=/home/qwics/copybooks
export QWICS_BATCH_CONFIG=/home/qwics/src/jobentry/config
export QWICS_BATCH_SPOOLDIR=/home/qwics/spool
export QWICS_BATCH_WORKDIR=/home/qwics/work
export QWICS_READER_SOCKETFILE=/home/qwics/comm/reader.sock

/home/qwics/bin/jobentry &
/home/qwics/bin/tpmserver /home/qwics/comm/sock
