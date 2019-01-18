/*******************************************************************************************/
/*   QWICS Server Java EE Web Application                                                  */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 21.11.2018                                  */
/*                                                                                         */
/*   Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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

package ejb;

import javax.ejb.ApplicationException;

@ApplicationException(rollback = true)
public class QwicsException extends RuntimeException {
	private static final long serialVersionUID = 3551523670703011297L;


	public QwicsException() {
		super();
		// TODO Auto-generated constructor stub
	}

	public QwicsException(String message, Throwable cause, boolean enableSuppression, boolean writableStackTrace) {
		super(message, cause, enableSuppression, writableStackTrace);
		// TODO Auto-generated constructor stub
	}

	public QwicsException(String message, Throwable cause) {
		super(message, cause);
		// TODO Auto-generated constructor stub
	}

	public QwicsException(String message) {
		super(message);
		// TODO Auto-generated constructor stub
	}

	public QwicsException(Throwable cause) {
		super(cause);
		// TODO Auto-generated constructor stub
	}
	
}

