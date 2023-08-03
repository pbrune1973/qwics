Welcome
-----

Welcome to the 

Quick Web-Based Interactive COBOL Service (QWICS), 

an environment to execute transactional COBOL programs written for traditional mainframe transaction processing monitors (TPM) without these as part of any Java EE-compliant application server.

Copyright (C) 2018-2023 by Philipp Brune  Email: Philipp.Brune@qwics.org   

QWICS is free software, licensed either under the terms of the GNU General Public License or the GNU Lesser General Public License (see respective source files for details), both as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. 
You should have received a copy of both licenses along with this project. If not, see <http://www.gnu.org/licenses/>.  


DISCLAIMER:
-----

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.  

USING QWICS WITH DOCKER (RECOMMENDED)
-----

The recommended way of deploying and using QWICS is now by containers using Docker or another container runtime capable of running OCI containers.

QWICS needs three separate containers, started in order as follows:

* Before running the containers, first create a new bridge network in Docker, labeled qwics-network. 

* The first container running the PostgreSQL database, just use the standard image run with the following command (You may customize the database settings to your needs, of course. Please note that you need to adjust the settings in the other conatiners as well):
```c
docker run --name=qwics-postgres --network=qwics-network -e POSTGRES_PASSWORD=postgres -d postgres
```

* The second container running the actual native QWICS server components and GnuCOBOL. There is a Dockerfile provided in the toplevel QWICS project dir. Go there and and run:
```c
docker build -t qwics:latest .
```
and launch it by 
```c
docker run --name qwics-cobol -it -v <YOUR-COPYBOOK-DIR>:/home/qwics/copybooks -v <YOUR-COBOL-SRC-DIR>:/home/qwics/cobsrc -v <YOUR-LOAD-MODULE-DIR>:/home/qwics/loadmod -v <YOUR-VSAM-DATASET-DIR>:/home/qwics/dataset -v <YOUR-BMS-MAP-DIR>:/home/qwics/maps --network=qwics-network -v <YOUR-SOCKET-FILE-DIR>:/home/qwics/comm  qwics:latest
```

* The third container running the Jakarta EE application server using the QWICS JDBC Driver and the QWICS Java Web App. To build the Java parts, first change to the workspace dir, and build the the two Java projects there using Maven and your favorite IDE. Afterwards, run the QwicsWebvApp using the Liberty Maven plugin to correctly set up the Maven target directory. Then stop Liberty and build the Java Docker container from the Dockerfile in the workspace dir using:
```c
docker build -t qwics-jee:latest .
```
and launch it by the command
```c
docker run --name qwics-jee -it --network=qwics-network -v <YOUR-SOCKET-FILE-DIR>:/home/qwics/comm -p 9080:9080 qwics-jee:latest
```

PRE-REQUISITES FOR BUILDING FROM SOURCE:
-----

This software has been written and tested under Linux (for S390/z Systems mainframes, ARM and x86)!

1. Download and extract a copy of the [GnuCOBOL](https://www.gnu.org/software/gnucobol/) sources in its newest release. Modify the following lines the in the file termio.c in the subdirectory libcob/ (preferably, search for the beginning of the function `cob_display(...)` in the file termio.c in the libcob source directory and modify and extend it accordingly):

```c
int (*performEXEC)(char*, void*) = NULL;

void display_cobfield(cob_field *f, FILE *fp) {
    display_common(f,fp);
}


void
cob_display (const int to_stderr,
   const int newline, const int varcnt, 
   ...)
{
        FILE            *fp;
        cob_field       *f;
        int             i;
        int             nlattr;
        cob_u32_t       disp_redirect;
        va_list         args;

// BEGIN OF EXEC HANDLER
        va_start (args, varcnt);
        f = va_arg (args, cob_field * );
        // Ensure string termination with zero for compare
        char dspBuf[100];
	int l = f->size;
	if (l > 99) {
		l = 99;
	}
	memcpy(dspBuf,(char*)f->data,l);
	dspBuf[l] = 0x00;
        if (strstr(dspBuf,"TPMI:")) {
    	    char *cmd = &dspBuf[5];
            if (varcnt > 1) {
                f = va_arg (args, 
                      cob_field * );
            }
            (*performEXEC)(cmd,(void*)f);
            va_end (args);
            return;
        }
        va_end (args);
// END OF EXEC HANDLER
```

In addition, edit the file call.c in the libcob source directory, search for the beginning of the function `cob_resolve_cobol(...)` and modify and extend it as follows:

```c
// BEGIN OF CALL HANDLER
void* (*resolveCALL)(char*) = NULL;
// END OF CALL HANDLER

void *
cob_resolve_cobol (const char *name, const int fold_case, const int errind)
{
        void    *p;
        char    *entry;
        char    *dirent;

// BEGIN OF CALL HANDLER
        p = resolveCALL(name);
        if (p != NULL) {
           return p;
        }
// END OF CALL HANDLER
```

2. Afterwards, build and install GnuCOBOL according to its documentation.

3. Download and install a copy of PostgreSQL database including its C client lib

MANUAL INSTALLATION FROM SOURCE:
-----

Build the preprocessors and the QWICS COBOL execution runtime:

1. Adjust the paths in the following Makefiles to fit your installation:

* <QWICSROOTDIR>/Makefile
* <QWICSROOTDIR>/src/preps/maps/Makefile
* <QWICSROOTDIR>/src/preps/cobol/Makefile

2. Build the binaries, in <QWICSROOTDIR> type the following commands:

```shell
make tpmserver
cd src/preps/maps
make
cd ../cobol
make
```

3. The Java sources of the QWICS JDBC driver and the demo Java EE Web App are provided as Maven projects in the subdirectory workspace. Please import the projects in your IDE with Maven support or use Maven from the command line.

The QwicsWebApp Maven project is configured to use OpenLiberty app server, it will build and run out-of-the-box. 


USING QWICS
-----

1. Deploy your COBOL source files in subdirectory <QWICSROOTDIR>/cobsrc, copy your mapset defs in <QWICSROOTDIR>/maps

```shell
cd maps
../bin/mapprep <MAPFILE>

cd ../cobsrc
../bin/cobp <COBOLMODULENAME>  # without .cob or .cbl suffix!
```

2. Start the PostgreSQL server according to its docs
3. Start the QWICS COBOL runtime, in <QWICSROOTDIR> type the following commands:

```shell
cd bin
/tpmserver 8000
```

4. Start the Java EE application server and use your COBOL code in your EJBs (see example)
 
 
ADDITIONAL CONFIGURATION BY ENVIRONMENT VARIABLES
-----

You may set some or all of the following shell environment variables at runtime to configure the preprocessors as well as the tpmserver to your setup. You can e.g. change the directories, search path for copybooks or the database connection string by 
this. There are default values for all of them (shown in brackets below), so setting nothing defaults to the above settings:

QWICS_MAX_ENQRES - Number of different, named ENQ/DEQ resources available (100)

QWICS_SHM_SIZE - Size in bytes of the shared memory area used (500000)

QWICS_BLOCKNUM - Number of shared memory blocks available. A smaller number increases the performance (960)

QWICS_BLOCKSIZE - Size in bytes of one shared memory block A larger value increases the performance, but may waste required memory (512)

Note: These need to fulfill QWICS_BLOCKNUM * QWICS_BLOCKSIZE < QWICS_SHM_SIZE Tune them to your applications needs!

QWICS_MEM_POOL_SIZE - Number of available memory blocks for GETMAIN/FREEMAIN. Increase if needed (100)

QWICS_DB_CONNECTSTR - Connection string for the PostgreSQL database (dbname=qwics)

QWICS_LOADMODDIR - Directory, where tpmserver searches for load modules (../loadmod)

QWICS_JSDIR - Directory path, where mapprep puts the JSON map files and tpmserver searches them (../copybooks)

QWICS_CBKDIR - Directory path, where mapprep puts the COBOL copybooks generated from the mapsets (../copybooks)

QWICS_DSPDIR - Directory path, where mapprep puts the COBOL DISPLAY includes generated from the mapsets (../copybooks)

QWICS_CBKPATH - Directory paths (with multiple entries separated by a :), where cobprep searches for copybook files (../copybooks)



Have fun!

