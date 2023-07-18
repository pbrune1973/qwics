#!/bin/bash

export QWICS_DB_CONNECTSTR="host=qwics-postgres user=postgres password=postgres dbname=postgres"

/home/qwics/bin/tpmserver 8000 
