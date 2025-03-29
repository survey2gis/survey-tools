/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	tools.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Provide convenience functions.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <string.h>

#include "global.h"


#ifndef TOOLS_H
#define TOOLS_H

/* helper function for storing 0 as "0" in string representation */
void t_dbl_to_str ( double value, char *dst );

/* copy max n chars into a new string */
char *t_str_ndup ( const char *str, size_t n );

/* returns TRUE if the "token" is a whitespace character */
BOOLEAN t_str_is_ws ( char token );

/* convert a string to a double if possible */
double t_str_to_dbl ( 	const char *str, const char decp, const char tsep,
		BOOLEAN *error, BOOLEAN *overflow );

/* convert a string to all uppercase */
char * t_str_to_upper ( char *str );

/* convert a string to all lowercase */
char * t_str_to_lower ( char *str );

/* convert a string to an int if possible */
int t_str_to_int ( const char *str, BOOLEAN *error, BOOLEAN *overflow );

/* removes leading and trailing white space */
char *t_str_pack ( const char *str );

/* remove enclosing quotes from string */
char *t_str_del_quotes ( const char *str, char quote_char );

/* converts a list of strings to a string array with NULL terminator */
char **t_str_arr ( const char* elem, ...);

/* check if one of the strings in "arr" equals "str" */
BOOLEAN t_str_eq_arr ( const char* str, const char** arr, BOOLEAN no_case );

/* appends "ext" if not already appended */
char *t_str_ext ( const char *str, const char *ext, const char *valid_ext, ... );

/* converts between different character encodings using iconv() */
int t_str_enc(const char *from, const char* to, char *in, char **out);

/* open a file whose path is given in UTF-8 encoding */
FILE* t_fopen_utf8 ( const char *path, const char* mode );

/* check for legal file path specifier */
BOOLEAN t_is_legal_path (const char *s);

/* check for legal file name specifier */
BOOLEAN t_is_legal_name (const char *s);

/* free memory block (test for NULL) */
void t_free ( void *ref );

/* set environment variable */
void t_setenv ( const char *name, const char *value );

/* set path to data directory with precedence */
char *t_set_data_dir ( const char *env, const char *local, const char *global );

/* return program name string */
const char *t_get_prg_name ();

/* return executable command name string */
const char *t_get_cmd_name ();

/* return program version string */
char *t_get_prg_version ();

/* return program name and version string */
char *t_get_prg_name_and_version ();

#ifdef MINGW
char *index ( const char *str, char token );
char *rindex ( const char *str, char token );
char *realpath(const char *path, char resolved_path[PATH_MAX]);
#endif

#endif /* TOOLS_H */
