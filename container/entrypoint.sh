#!/bin/bash

export QWICS_DB_CONNECTSTR="host=qwics-postgres user=postgres password=postgres dbname=postgres"
export QWICS_DATASET_DIR=/home/qwics/dataset
export QWICS_LOADMODDIR=/home/qwics/loadmod
export QWICS_JSDIR=/home/qwics/copybooks

/home/qwics/bin/tpmserver 8000 
