/*******************************************************************************************/
/*   QWICS Server COBOL embedded SQL executor                                              */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 20.08.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2020 by Philipp Brune  Email: Philipp.Brune@qwics.org            */
/*                                                                                         */
/*   This file is part of of the QWICS Server project.                                     */
/*                                                                                         */
/*   QWICS Server is free software: you can redistribute it and/or modify it under the     */
/*   terms of the GNU General Public License as published by the Free Software Foundation, */
/*   either version 3 of the License, or (at your option) any later version.               */
/*   It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;       */
/*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/*   PURPOSE.  See the GNU General Public License for more details.                        */
/*                                                                                         */
/*   You should have received a copy of the GNU General Public License                     */
/*   along with this project. If not, see <http://www.gnu.org/licenses/>.                  */
/*******************************************************************************************/


#include <stdio.h>
#include "cobsql.h"


int main(int argc, char *argv[]) {
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr," Usage: batchrun <loadmod> [<jobname> <step> <pgm>]\n");
        return 1;
    } else {
        if ((argc > 2) && !(argc == 5)) {
            fprintf(stderr," Usage: batchrun <loadmod> [<jobname> <step> <pgm>]\n");
            return 1;
        }
    }

    return batchrun(argc,argv);
}
