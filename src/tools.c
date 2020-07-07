/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	tools.c
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


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <iconv.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef MINGW
#include <windows.h>
#endif

#include "global.h"
#include "i18n.h"
#include "tools.h"


char *t_str_ndup ( const char *str, size_t n )
{
	char *result, *p;
	int i;
	size_t len;

	if ( str == NULL )
		return ( NULL );

	/* determine actual length of string to copy */
	p = (char*) str;
	len = 0;
	while ( ( len < n ) && ( p[len] != '\0' ) ) {
		len ++;
	}

	/* make new string and terminate it */
	result = malloc ( (len+1) * sizeof ( char ) );
	result[len] = '\0';

	/* copy data to new string */
	p = (char*) str;
	i = 0;
	for ( i=0; i < len; i ++ ) {
		result[i] = p[i];
	}

	return ( result );
}


/*
 * Returns a newly allocated string that is an all
 * uppercase conversion of the argument.
 */
char *t_str_to_upper ( char *str )
{
	char *dup = strdup ( str );

	int i = 0;
	for(i = 0; i < strlen(dup); i++){
		dup[i] = toupper(dup[i]);
	}

	return ( dup );
}


/*
 * Returns a newly allocated string that is an all
 * lowercase conversion of the argument.
 */
char *t_str_to_lower ( char *str )
{
	char *dup = strdup ( str );

	int i = 0;
	for(i = 0; i < strlen(dup); i++){
		dup[i] = tolower(dup[i]);
	}

	return ( dup );
}


/*
 * Returns TRUE only if 'c' is a character that
 * is used as part of the numeric notation.
 * Set "is_int" to "TRUE" if parsing an integer number.
 */
BOOLEAN is_allowed_num ( const char c, const char decp, const char tsep, BOOLEAN is_int )
{
	int i;
	char valid[] = { '0','1','2','3','4','5','6','7','8','9','+','-',0 };


	if ( is_int == FALSE ) {
		if ( decp != 0 && c == decp )
			return (TRUE);
		if ( tsep != 0 && c == tsep )
			return (TRUE);
	}

	i = 0;
	while ( valid[i] != 0 ) {
		if ( c == valid[i] ) {
			return ( TRUE );
		}
		i++;
	}

	return ( FALSE );
}


/* Converts a string to a double if possible.
 * If "error" and "overflow" are not NULL, then
 * in case of an error, "error" will be set to "TRUE.
 * If overflow or (!) underflow occurred, then "overflow"
 * will be set to "TRUE".
 *
 * Another pitfall that can occur during conversion from
 * string to double is that the characters used for the
 * decimal point and grouping (thousands separator) of decimals
 * may be different than that of the current locale. If so,
 * pass the INPUT data's symbols for those characters as "decp" and
 * "tsep", and this function will make sure that they are converted
 * to the current locale before conversion by strtod() takes place.
 * Otherwise, pass "decp" and/or "tsep" as "0".
 *
 */
double t_str_to_dbl ( 	const char *str, const char decp, const char tsep,
		BOOLEAN *error, BOOLEAN *overflow )
{
	double result;
	char *endptr;
	char *p, *p2, *p3;
	char *tmp;
	char group;


	/* DEBUG */
	/* fprintf ( stderr, "DEBUG: t_str_to_dbl ('%s', %c, %c, ...)\n", str, decp, tsep ); */

	if ( error != NULL ) *error = FALSE;
	if ( overflow != NULL ) *overflow = FALSE;
	errno = 0;

	if ( str == NULL || strlen ( str ) < 1 ) {
		if ( error != NULL ) *error = TRUE;
		return ( 0 );
	}

	/* check the string for invalid characters */
	char decp_cur = *I18N_DECIMAL_POINT;
	char tsep_cur = *I18N_THOUSANDS_SEP;
	if ( decp != 0 ) decp_cur = decp;
	if ( tsep != 0 ) tsep_cur = tsep;
	p = (char*) str;
	while ( *p != '\0' ) {
		if ( is_allowed_num ( *p, decp_cur, tsep_cur, FALSE ) == FALSE ) {
			if ( error != NULL ) *error = TRUE;
			return ( 0 );
		}
		p += sizeof (char);
	}

	/* DEBUG */
	/*
	fprintf ( stderr, "DEBUG: decp = %c\n", decp );
	fprintf ( stderr, "DEBUG: tsep = %c\n", tsep );
	 */

	tmp = strdup (str);

	/* First we need to get rid of all grouping characters in the
	   input string. */
	if ( tsep != 0 ) {
		group = tsep;
	} else {
		group = *I18N_THOUSANDS_SEP;
	}

	p = tmp;
	while ( *p != '\0' ) {
		if ( *p == group ) {
			p2 = p;
			p3 = p;
			p2 += sizeof(char);
			while ( *p2 != '\0') {
				*p = *p2;
				p += sizeof(char);
				p2 += sizeof(char);
			}
			*p = '\0';
			p = p3;
		}
		p += sizeof(char);
	}

	/* DEBUG */
	/*
	fprintf ( stderr, "DEBUG: w/o group = '%s'\n", tmp );
	 */

	/* If the decimal point symbol of the input data
	   is different from the current locale, we need to change its
	   numeric representation before applying strtod(), which honors
	   the current locale.
	 */
	char *locale_decp = i18n_get_locale_decp ();
	if ( locale_decp != NULL ) {

		/* DEBUG */
		/*
		fprintf ( stderr, "DEBUG: locale decp = %c\n", *locale_decp );
		 */

		if ( decp != 0 ) {
			p = tmp;
			while ( *p != '\0' ) {
				if ( decp != 0 && *p == decp ) {
					if ( *locale_decp != decp ) {
						/* change decimal point symbol to that of locale */
						*p = *locale_decp;
					}
				}
				p += sizeof(char);
			}
		}
		free (locale_decp);
	}

	/* DEBUG */
	/*
	fprintf ( stderr, "DEBUG: change decp = '%s'\n", tmp );
	 */

	result = strtod ( tmp, &endptr );

	/* DEBUG */
	/*
	fprintf ( stderr, "DEBUG: result = '%f'\n", result );
	 */

	if ( errno == ERANGE ) {
		if ( overflow != NULL) *overflow = TRUE;
		if ( error != NULL ) *error = TRUE;
		free ( tmp );
		return ( result );
	}

	if ( result == 0 ) {
		if ( str == endptr ) {
			if ( error != NULL ) *error = TRUE;
			free ( tmp );
			return ( result );
		}
	}

	free ( tmp );
	return ( result );
}


/*
 * Convert a string to an int (base 10) if possible.
 * If "error" and "overflow" are not NULL, then
 * in case of an error, "error" will be set to "TRUE.
 * If overflow or (!) underflow occurred, then "overflow"
 * will be set to "TRUE".
 */
int t_str_to_int ( const char *str, BOOLEAN *error, BOOLEAN *overflow )
{
	long result;
	int compare;
	char *endptr;
	char *p;


	if ( error != NULL ) *error = FALSE;
	if ( overflow != NULL ) *overflow = FALSE;
	errno = 0;

	if ( str == NULL || strlen ( str ) < 1 ) {
		if ( error != NULL ) *error = TRUE;
		return ( 0 );
	}

	/* check the string for invalid characters */
	p = (char*) str;
	while ( *p != '\0' ) {
		if ( is_allowed_num ( *p, 0, 0, TRUE ) == FALSE ) {
			if ( error != NULL ) *error = TRUE;
			return ( 0 );
		}
		p += sizeof (char);
	}

	result = strtol ( str, &endptr, 10 );

	if ( errno == ERANGE ) {
		if ( overflow != NULL) *overflow = TRUE;
		if ( error != NULL ) *error = TRUE;
		return ( result );
	}

	if ( result == 0 ) {
		if ( str == endptr ) {
			if ( error != NULL ) *error = TRUE;
			return ( result );
		}
	}

	/* check if the result can be stored in an int */
	compare = (int) result;
	if ( compare != result ) {
		if ( error != NULL ) *error = TRUE;
		if ( overflow != NULL ) *overflow = TRUE;
		return ( result );
	}

	return ( result );
}


/*
 * Returns TRUE if the "token" is a whitespace character.
 */
BOOLEAN t_str_is_ws ( char token ) {

	if ( token == '\t' ) return ( TRUE );

	if ( token == ' ' ) return ( TRUE );

	if ( token == '\r' ) return ( TRUE );

	if ( token == '\n' ) return ( TRUE );

	return ( FALSE );
}


/*
 * Returns a new string that is a copy of "str", but
 * modified by removing leading and trailing whitespace.
 * Returns an empty string, if there is nothing but whitespace in it.
 * Returns NULL on error.
 */
char *t_str_pack ( const char *str )
{
	const char *start, *end;
	int start_pos;
	int end_pos;
	char *tmp;

	if ( str == NULL || strlen (str) < 1 ) {
		return (NULL);
	}

	start = str;
	start_pos = 0;
	while ( t_str_is_ws (*start) == TRUE && start_pos < strlen (str) ) {
		start = start + sizeof (char);
		start_pos ++;
	}

	end = str + ( sizeof (char) * (strlen(str)-1) );
	end_pos = strlen (str) - 1;
	while ( t_str_is_ws (*end) == TRUE && end_pos > 0 ) {
		end_pos --;
		end -= sizeof (char);
	}

	if ( start_pos > end_pos ) {
		tmp = strdup ("");
		return (tmp);
	}

	tmp = strdup (start);
	tmp[end_pos-start_pos+1] = '\0';

	return ( tmp );
}


/* Creates a new string which is a copy of "str", after
 * removing any enclosing quotation characters.
 * Often, this will be " or '. Whitespace is removed first.
 * If "str" is not correctly quoted, then the new string will be an
 * unmodified copy. Only quotation marks at the very start (after leading
 * whitespace) or very end (before trailing whitespace) will be removed.
 * Returns NULL on error.
 */
char *t_str_del_quotes ( const char *str, char quote_char )
{
	char *start;
	char *tmp;
	char *result;


	if ( str == NULL || strlen (str) < 1 )
		return (NULL);

	tmp = t_str_pack ( str ); /* will have no effect if string has
								 already been "packed" */

	/* after packing, valid quotes can only be the very first and
	   last character */
	if ( strlen (tmp) < 2 || tmp[0] != quote_char
			|| tmp[strlen(tmp)-1] != quote_char )
	{
		free ( tmp );
		tmp = strdup (str);
		return (tmp);
	}

	start = tmp;
	start += sizeof (char);
	tmp[strlen(tmp)-1] = '\0';
	result = strdup (start);
	free (tmp);

	return (result);
}


/*
 * Creates a new string array with NULL terminator from parameter list.
 */
char **t_str_arr ( const char* elem, ...)
{
	int i;
	const char *p;
	va_list argp;
	char **result;


	va_start(argp, elem);
	i = 0;
	for(p = elem; *p != '\0'; p++) {
		i++;
	}
	result = malloc ( sizeof (char*) * (i+1) );
	i = 0;
	for(p = elem; *p != '\0'; p++) {
		result[i] = strdup (p);
		i++;
	}
	result[i] = NULL;
	va_end(argp);

	return ( result );
}


/*
 * Returns TRUE if one of the strings in the array
 * equals "str", FALSE otherwise. Set "no_case" to
 * "TRUE" to enable non-case sensitive comparision,
 * set it to "FALSE" otherwise.
 */
BOOLEAN t_str_eq_arr ( const char* str, const char** arr, BOOLEAN no_case )
{
	const char *p;


	if ( arr == NULL )
		return ( FALSE );

	p = arr[0];
	while ( p != NULL ) {
		if ( no_case == TRUE ) {
			if ( !strcasecmp ( p, str ) ) {
				return ( TRUE );
			} else {
				if ( !strcmp ( p, str ) ) {
					return ( TRUE );
				}
			}
		}
		p++;
	}

	return ( FALSE );
}


/* Returns a copy of "str", after appending "ext" if not already appended.
 * The new string is also packed to eliminate any leading or trailing whitespace.
 * Use this to create file names with valid extensions.
 * The extension string comparison is not case sensitive.
 *
 * !! The last option to this function MUST BE NULL to terminate the list of acceptable
 * extensions !!
 * E.g.:
 *   t_str_ext ( filename, "jpg", "jpg", ".jpeg", NULL);
 */
char *t_str_ext ( const char *str, const char *ext, const char *valid_ext, ... ) {
	char *p;
	char *given_ext;
	va_list argp;
	char *tmp;
	char *result;


	tmp = t_str_pack ( str );

	p = rindex ( (const char*) tmp, '.' );
	if ( p == NULL ) {
		/* no extension: just append "ext" */
		result = malloc ( sizeof (char) * ( strlen ( tmp ) + strlen ( ext ) + 2 ));
		sprintf ( result, "%s.%s", tmp, ext );
		free (tmp);
		return (result);
	}

	p += sizeof (char);
	given_ext = strdup(p);

	/*  there may be a valid extension: let's check */
	va_start(argp, valid_ext);
	while ( valid_ext != NULL ) {
		if ( !strcasecmp ( given_ext, valid_ext ) ) {
			/* got a valid extension: return original string */
			result = strdup (tmp);
			free (tmp);
			free (given_ext);
			return (result);
		}
		valid_ext = va_arg(argp, char *);
	}

	va_end(argp);

	/* no valid extension yet: add one */
	result = malloc ( sizeof (char) * ( strlen ( str ) + strlen ( ext ) + 2 ));
	sprintf ( result, "%s.%s", tmp, ext );
	free (tmp);
	free (given_ext);
	return (result);
}


/*
 * Returns TRUE if "s" is a legal file path specifier,
 * FALSE otherwise.
 */
BOOLEAN t_is_legal_path (const char *s) {

	if ( s == NULL ) {
		return ( FALSE );
	}

	for (; *s; s++) {
		if (*s == '"' ||
				*s == '@' || *s == ',' || *s == '=' || *s == '*' || *s > 0176) {
			return ( FALSE );
		}
	}

	return ( TRUE );
}


/*
 * Returns TRUE if "s" is a legal file name specifier,
 * FALSE otherwise.
 */
BOOLEAN t_is_legal_name (const char *s) {

	if ( s == NULL ) {
		return ( FALSE );
	}

	for (; *s; s++) {
		if (*s == '/' || *s == '"' || *s == '\'' || *s == '\\' ||
				*s == '@' || *s == ',' || *s == '=' || *s == '*' || *s > 0176) {
			return ( FALSE );
		}
	}

	return ( TRUE );
}


/*
 * Free's the block pointed to by "ref", but only if
 * "ref" points to a non-null location.
 */
void t_free ( void *ref ) {
	if ( ref != NULL ) {
		free ( ref );
	}
}


/*
 * Sets an environment variable in a unified manner.
 * The reason we need this is that MinGW/Win32 does
 * provides "putenv" instead of "setenv".
 */
void t_setenv (const char *name, const char *value ) {
#ifdef MINGW
	int len = strlen(name) + 1 + strlen(value) + 1;
	char *str = malloc(len);
	sprintf(str, "%s=%s", name, value);
	putenv(str);	
#else
	setenv(name, value, 1);
#endif
}

/*
 * Converts text encoding of a string using iconv().
 * Returns the length of the output string or -1 on error.
 * On success, new memory will be allocated for the output string
 * and the string will be 0-terminated.
 * On error, iconv() and iconv_open() will set errno and the output
 * string may remain in an undefined state.
 * 
 * Parameters:
 * 
 * from = character encoding of input data (e.g. "UTF-8").
 * to = character encoding of output data (e.g. "ASCII").
 * in = input string
 * out = pointer to output string buffer
 * 
 * For a full list of supported encodings: cf. iconv_open() manual page:
 * https://www.gnu.org/savannah-checkouts/gnu/libiconv/documentation/libiconv-1.13/iconv_open.3.html
 * 
 * NOTE : The default encoding for the operating system's
 * multibyte character representation is the empty string.
 * 
 * The buffer 'out' should not be preallocated, as it will be allocated
 * by this function at the start of the conversion process.
 * 
 */
int t_str_enc(const char *from, const char* to, char *in, char **out)
{
	iconv_t cd;
	char *inbuf;
	char *outbuf;
	size_t inbytesleft, outbytesleft, nchars, buf_len;

	if ( in == NULL ) {
		out = NULL;
		return (-1);
	}

	if ( from == NULL || to == NULL ) {
		return (-1);
	}

	cd = iconv_open(to, from);
	if (cd == (iconv_t)-1) {
		return (-1);
	}

	inbytesleft = strlen(in);
	if (inbytesleft == 0) {
		iconv_close(cd);
		return (-1);
	}
	inbuf = in;
	buf_len = ( 2 * inbytesleft + 1 );
	*out = malloc(buf_len);
	if (!*out) {
		iconv_close(cd);
		return (-1);
	}
	outbytesleft = buf_len;
	outbuf = *out;

	nchars = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	while (nchars == (size_t)-1 && errno == E2BIG) {
		char *ptr;
		size_t increase = 10; /* increase buffer size by one chunk */
		size_t len;
		buf_len += increase;
		outbytesleft += increase;
		ptr = realloc(*out, buf_len);
		if (!ptr) {
			free(*out);
			iconv_close(cd);
			return (-1);
		}
		len = outbuf - *out;
		*out = ptr;
		outbuf = *out + len;
		nchars = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	}
	if (nchars == (size_t)-1) {
		free(*out);
		iconv_close(cd);
		return (-1);
	}

	iconv_close(cd);

	/* Success! 0-terminate output string and return its size. */
	int length = buf_len - outbytesleft;

	char *ptr;
	ptr = *out;
	ptr += length;
	*ptr = '\0';

	return ( buf_len - outbytesleft );
}


/*
 * A simple helper function that takes care that a double value
 * is stored as plain "0" or "1" in its string form, if it is exactly 0 or 1.
 * Since many default option values are 0 or 1, this avoids applying
 * regional decimal format characters to their string representations.
 * 
 * The result is returned in "dst" which has to be preallocated.
 * If dst is NULL then no action will be taken.
 *
 * Note that the length of the result string can never exceed PRG_MAX_STR_LEN.
 */
void t_dbl_to_str ( double value, char *dst ) {
	if ( dst == NULL ) {
		return;
	}
	if ( value == 0 || value == 1 ) {
		if ( value == 0 ) {
			snprintf(dst, PRG_MAX_STR_LEN, "0");
		} else {
			snprintf(dst, PRG_MAX_STR_LEN, "1");
		}
	} else {
		snprintf(dst, PRG_MAX_STR_LEN, "%f", value );
	}
}


/*
 * Sets a directory containing program data in a way that respects
 * common path precedence.
 * This function can be used to set the path to a data directory for
 * 3rd party libraries such as PROJ.4 that allow configurable, global
 * and local data locations.
 *
 * Caller may pass one or more of the following:
 *
 * 'env'    - Name of environment variable that contains the path to
 *            the data directory.
 * 'local'  - Relative path (with respect to the current working directory)
 *            of a local data directory.
 * 'global' - Absolute path of a global data directory.
 *
 * The returned result will be the _first_ directory passed by
 * the caller (i.e. non-NULL) in the order: env -> local -> global.
 *
 * Note that each one of 'env', 'local' and 'global' can be passed
 * as 'NULL', in which case the corresponding location will not be
 * considered.
 *
 * The returned value is either NULL (on error, see below) or a newly
 * allocated string (on success) that must be free'd by the caller.
 *
 * This function does _not_ check whether any of the provided directory
 * paths actually exist and are accessible!
 *
 * This function will return 'NULL' if all three locations are undefined.
 */
char *t_set_data_dir ( const char *env, const char *local, const char *global )
{
	/* All three NULL? Return NULL. */
	if ( env == NULL && local == NULL && global == NULL ) {
		return ( (char*) NULL );
	}

	/* check path in env */
	if( env != NULL && strlen(env) > 0 ) {
		char *p = getenv(env);
		if ( p!= NULL ) {
			return ( strdup (p) );
		}
	}

	/* check local path */
	if ( local != NULL && strlen (local) > 0 ) {
		char buf[PRG_MAX_PATH_LENGTH];
		getcwd(buf, PRG_MAX_PATH_LENGTH);
		strncat(buf, PRG_FILE_SEPARATOR_STR, PRG_MAX_PATH_LENGTH-1);
		strncat(buf, local, PRG_MAX_PATH_LENGTH);
		return ( strdup(buf) );
	}

	/* check global path */
	if ( global != NULL && strlen (global) > 0 ) {
		return ( strdup(global) );
	}

	return ( (char*) NULL );
}


/*
 * Opens a file whose path is provided as a _presumed_ UTF-8 string in a portable way.
 * Takes care of converting to the operating system's default multi-byte encoding
 * for file paths, then opens the file using regular fopen().
 *
 * If the file path is not in valid UTF-8 encoding, then it will be assumed that
 * it already used the OS's native multi-byte encoding and the file path will be
 * opened without any translation of encodings. Note that this means that the result
 * can still be NULL if the invalid UTF-8 sequence has a different cause.
 * 
 * The parameters of this function are identical to those of fopen(), so that it
 * can be used as a straight-forward replacement of the latter.
 * 
 * Parameters:
 * 
 * path: file path (presumed UTF-8 encoding)
 * mode: file opening mode (as passed to fopen).
 * 
 * Returns a valid file handle or NULL on error.
 * Sets errno on error.
 * 
 */
FILE* t_fopen_utf8 ( const char *path, const char* mode ) {
#ifdef MINGW
	/* convert path to Windows' multi-byte representation */
	FILE *f = NULL;
	char *buf_out = NULL;
	int result = t_str_enc("UTF-8", I18N_WIN_CODEPAGE_FILES, (char*) path, &buf_out);
	if ( result < 0 || buf_out == NULL ) {
		if ( buf_out != NULL ) {
			free ( buf_out );
		}
		if ( errno == EILSEQ ) {
			/* Got an illegal byte sequence: maybe the path is
			 * already in Windows' multi-byte encoding. If so:
			 * attempt to open original path now and return handle 
			 * (which may still fail and return NULL!). */ 
			f = fopen ( path, mode );
			return ( f );
		}
		return (NULL);
	}
	f = fopen ( buf_out, mode );
	free ( buf_out );
#else
	FILE *f = fopen ( path, mode );
#endif
	return ( f );
}


#ifdef MINGW

/*
 * If we have MinGW, then we must provide a few functions that
 * are not part of its system library.
 */
char *index ( const char *str, char token ) {
	char *p;
	int i;


	if ( str == NULL )
		return ( NULL );

	p = (char*) str;
	i = 0;
	while ( i < strlen ( str ) ) {
		if ( *p == token )
			return ( p );
		i ++;
		p ++;
	}

	return ( NULL );
}


char *rindex ( const char *str, char token ) {
	char *p;
	int i;


	if ( str == NULL )
		return ( NULL );

	p = (char*) str;
	for ( i=0; i < strlen (str); i++ )
		p ++;

	i = strlen (str);
	while ( i > 0 ) {
		if ( *p == token )
			return ( p );
		i --;
		p --;
	}

	return ( NULL );
}

/*
 * https://sourceforge.net/p/mingw/patches/256/
 */
char *realpath(const char *path, char resolved_path[PATH_MAX])
{
  char *return_path = 0;

  if (path) //Else EINVAL
  {
    if (resolved_path)
    {
      return_path = resolved_path;
    }
    else
    {
      //Non standard extension that glibc uses
      return_path = malloc(PATH_MAX);
    }

    if (return_path) //Else EINVAL
    {
      //This is a Win32 API function similar to what realpath() is supposed to do
      size_t size = GetFullPathNameA(path, PATH_MAX, return_path, 0);

      //GetFullPathNameA() returns a size larger than buffer if buffer is too small
      if (size > PATH_MAX)
      {
        if (return_path != resolved_path) //Malloc'd buffer - Unstandard extension retry
        {
          size_t new_size;

          free(return_path);
          return_path = malloc(size);

          if (return_path)
          {
            new_size = GetFullPathNameA(path, size, return_path, 0); //Try again

            if (new_size > size) //If it's still too large, we have a problem, don't try again
            {
              free(return_path);
              return_path = 0;
              errno = ENAMETOOLONG;
            }
            else
            {
              size = new_size;
            }
          }
          else
          {
            //I wasn't sure what to return here, but the standard does say to return EINVAL
            //if resolved_path is null, and in this case we couldn't malloc large enough buffer
            errno = EINVAL;
          }
        }
        else //resolved_path buffer isn't big enough
        {
          return_path = 0;
          errno = ENAMETOOLONG;
        }
      }

      //GetFullPathNameA() returns 0 if some path resolve problem occured
      if (!size)
      {
        if (return_path != resolved_path) //Malloc'd buffer
        {
          free(return_path);
        }

        return_path = 0;

        //Convert MS errors into standard errors
        switch (GetLastError())
        {
          case ERROR_FILE_NOT_FOUND:
            errno = ENOENT;
            break;

          case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
            errno = ENOTDIR;
            break;

          case ERROR_ACCESS_DENIED:
            errno = EACCES;
            break;

          default: //Unknown Error
            errno = EIO;
            break;
        }
      }

      //If we get to here with a valid return_path, we're still doing good
      if (return_path)
      {
        struct stat stat_buffer;

        //Make sure path exists, stat() returns 0 on success
        if (stat(return_path, &stat_buffer))
        {
          if (return_path != resolved_path)
          {
            free(return_path);
          }

          return_path = 0;
          //stat() will set the correct errno for us
        }
        //else we succeeded!
      }
    }
    else
    {
      errno = EINVAL;
    }
  }
  else
  {
    errno = EINVAL;
  }

  return return_path;
}

#endif


/*
 * Returns a string that contains the current program name.
 *
 * The returned string is a pointer to a string constant defined in 'global.h'.
 */
const char *t_get_prg_name () {
	return ((const char*)PRG_NAME);
}

/*
 * Returns a string that contains the current program version as defined in 'global.h'.
 *
 * Memory for the return value is allocated and must be free'd by caller.
 */
char *t_get_prg_version () {
	char *str = malloc ( sizeof (char) * PRG_MAX_STR_LEN);
	if ( PRG_VERSION_BETA > 0 ) {
		snprintf(str,PRG_MAX_STR_LEN,"%i.%i.%i BETA %i", PRG_VERSION_MAJOR, PRG_VERSION_MINOR,
				PRG_VERSION_REVISION, PRG_VERSION_BETA );
	} else {
		snprintf(str,PRG_MAX_STR_LEN,"%i.%i.%i", PRG_VERSION_MAJOR, PRG_VERSION_MINOR,
				PRG_VERSION_REVISION );
	}
	return (str);
}

/*
 * Returns a string that contains the current program name and version as defined in 'global.h'.
 *
 * Memory for the return value is allocated and must be free'd by caller.
 */
char *t_get_prg_name_and_version () {
	char *str = malloc ( sizeof (char) * PRG_MAX_STR_LEN);
	if ( PRG_VERSION_BETA > 0 ) {
		snprintf(str,PRG_MAX_STR_LEN,"%s %i.%i.%i BETA %i", PRG_NAME, PRG_VERSION_MAJOR, PRG_VERSION_MINOR,
				PRG_VERSION_REVISION, PRG_VERSION_BETA );
	} else {
		snprintf(str,PRG_MAX_STR_LEN,"%s %i.%i.%i", PRG_NAME, PRG_VERSION_MAJOR, PRG_VERSION_MINOR,
				PRG_VERSION_REVISION );
	}
	return (str);
}

