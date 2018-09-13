Welcome to the 

Quick Web Information Control System (QWICS), 

an environment to execute transactional COBOL programs written for traditional mainframe transaction processing monitors (TPM) without these as part of any Java EE-complient appliaction server.

Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@qwics.org   

QWICS is free software, licensed either under the terms of the GNU General Public License or the GNU Lesser General Public License (see respectivesource fiules for details), both as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. 
You should have received a copy of both licenses along with this project. If not, see <http://www.gnu.org/licenses/>.  


DISCLAIMER:

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.  


INSTALLATION:

This software has been written and tested under Linux (for S390/z Systems mainframes and x86)!

0. Download and extract a copy of the GnuCOBOL compiler sources in its newest release. Modify the following lines the in the file termio.c in the subbdirectory libcob/ (preferably, search for the beginning of the function cob_display(...) and modify and extend it accordingly):

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
        if (strstr((char*)f->data,
        			    "TPMI:")) {
           char *cmd 
                = (char*)(f->data+5);
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

Afterwards, build and install the GnuCOBOL compiler according to its documentation.

1. Download and insatll a copy of PostgreSQL database including its C client lib

2. Build the preprocessors and the QWICS COBOL execution runtime:

2.1  Adjust the paths in the following Makefiles to fit your installation:

<QWICSROOTDIR>/Makefile
<QWICSROOTDIR>/src/preps/maps/Makefile
<QWICSROOTDIR>/src/preps/cobol/Makefile

2.2 Build the binaries, in <QWICSROOTDIR> type the following commands:

make tpmserver
cd src/preps/maps
make
cd ../cobol
make

2.3 The Java sources of the QWICS JDBC driver and the demo Java EE Web App are provided as Eclipse IDE projects in the subdirectory workspace. Please import the projects in your own workspace (using menu items "Import... --> Existing projects into workspace"). Please see http://www.eclipse.org for further information on Eclipse.

2.4 Download a Java EE-compliant appliaction server and (e.g. JBoss WildFly, see http://wildfly.org) and deploy the JDBC driver with a XA datasource and the Web appliaction there.


USING QWICS

1. Deploy your COBOL source files in subdirectory <QWICSROOTDIR>/cobsrc, copy your mapset defs in <QWICSROOTDIR>/maps

cd maps
../bin/mapprep <MAPFILE>

cd ../cobsrc
../bin/cobp <COBOLMODULENAME>   (withour .cob or .cbl suffix!) 


2. Start the PostgreSQL server according to its docs
3. Start the QWICS COBOL runtime, in <QWICSROOTDIR> type the following commands:

cd bin
/tpmserver 8000

4. Start the Java EE appliaction server and use your COBOL code in your EJBs (see example)


Have fun!

