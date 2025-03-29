
/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	tests/test-platform.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Run some cross-platform portability function tests.
 *
 * COPYRIGHT:	(C) 2016 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../global.h"

#include "../i18n.h"
#include "../tools.h"


/* These are just need because some function in tools.c require
 * them to be defined.
 */
char *I18N_DECIMAL_POINT;
char *I18N_THOUSANDS_SEP;


/*
 * Test character encoding conversion using iconv().
 */
void test_iconv () {
	
	fprintf ( stdout, "*** Test: character encoding conversion using iconv() ***\n" );
	
	char *buf_in;
	char *buf_out;
	const char *enc_in = "UTF-8";
	const char *enc_out = "ASCII";
	
	fprintf ( stdout, "Translating from '%s' to '%s'.\n", enc_in, enc_out );
		
	buf_in = strdup ("abcde.123456");
		
	int result;
		
	result = t_str_enc(enc_in, enc_out, buf_in, &buf_out);
	
	if ( result < 0 ) {
		fprintf ( stdout, "Failed. Reason: %s.\n", strerror (errno));
		return;
	}
	
	buf_out[result] = '\0';	
	fprintf ( stdout, "Success: Output is '%s' (length '%i').\n", buf_out, result );
	
	/* inverse translation */
	
	fprintf ( stdout, "Translating from '%s' to '%s'.\n", enc_out, enc_in );
	
	result = t_str_enc(enc_out, enc_in, buf_in, &buf_out);
	
	
	if ( result < 0 ) {
		fprintf ( stdout, "Failed. Reason: %s.\n", strerror (errno));
		return;
	}
	
	buf_out[result] = '\0';
	fprintf ( stdout, "Success: Output is '%s' (length '%i').\n", buf_out, result );
	
	return;
}


/*
 *
 * MAIN FUNCTION
 * Runs all tests.
 *
 */
int main(int argc, char *argv[])
{

/* Windows: Set CLI codepage to the closest thing to UTF-8 */
#ifdef MINGW
	SetConsoleOutputCP(65001);
#endif

	/* set default number format options */
	I18N_DECIMAL_POINT = strdup (".");
	I18N_THOUSANDS_SEP = strdup (",");

	/* test character encoding conversion using iconv */
	test_iconv ();

	return (PRG_EXIT_OK);
}
