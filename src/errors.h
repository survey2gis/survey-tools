/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	errors.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Facilities for displaying and logging errors, warnings
 * 				and status messages.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <stdio.h>
#include <string.h>

#include "global.h"
#include "options.h"

#ifndef ERRORS_H
#define ERRORS_H

/* the different types of errors and warnings */
#define ERR_EXIT 	0 /* an error that leads to program exit */
#define ERR_WARN 	1 /* a warning */
#define ERR_DBUG 	2 /* a debug message */
#define ERR_NOTE 	3 /* a notification message */

/* maximum length of an error message string */
#define ERR_MSG_LENGTH	1000

#ifdef MAIN
char err_msg[ERR_MSG_LENGTH]; /* global buffer for error messages */
int ERR_STATUS;
int WARN_STATUS;
#else
extern char err_msg[ERR_MSG_LENGTH];
extern int ERR_STATUS;
extern int WARN_STATUS;
#endif

/* store error message in global message buffer */
void err_msg_set ( const char *msg );

/* clear global error message buffer */
void err_msg_clear ();

/* display an error message straight to the console */
void err_show ( unsigned short type, const char *format, ... );

/* initialize message logging facility */
void err_log_init ( options *opts );

/* close error log file */
void err_close ();

#endif /* ERRORS_H */
