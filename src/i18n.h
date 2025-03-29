/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	i18n.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Internationalization support via gettext.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "global.h"

#ifndef I18N_H
#define I18N_H


#ifdef MAIN
/* numeric format information (important!) */
char *I18N_DECIMAL_POINT;
char *I18N_THOUSANDS_SEP;

/* Windows console (cmd.exe) encoding for conversion from/to UTF-8.
 * GetConsoleOutputCP
 * This is the identifier for iconv_open(). For possible choices
 * see: https://www.gnu.org/savannah-checkouts/gnu/libiconv/documentation/libiconv-1.13/iconv_open.3.html
 */
char *I18N_WIN_CODEPAGE_CONSOLE;
/* Windows file system encoding for conversion from/to UTF-8.
 * By default, this is the empty string, which _should_
 * be interpreted correctly as Windows native multi-byte.
 * This is the identifier for iconv_open(). For possible choices
 * see: https://www.gnu.org/savannah-checkouts/gnu/libiconv/documentation/libiconv-1.13/iconv_open.3.html
 */
char *I18N_WIN_CODEPAGE_FILES;
#else
extern char *I18N_DECIMAL_POINT;
extern char *I18N_THOUSANDS_SEP;
extern char *I18N_WIN_CODEPAGE_CONSOLE;
extern char *I18N_WIN_CODEPAGE_FILES;
#endif


void i18n_init ();

void i18n_set_locale_en_EN ();

void i18n_set_locale_de_DE ();

BOOLEAN i18n_is_locale_EN ();

BOOLEAN i18n_is_locale_DE ();

void i18n_force_supported_locale ();

void i18n_force_english ();

void i18n_set_decimal_point ( const char* );

void i18n_set_thousands_separator ( const char* );

char *i18n_get_decimal_point ();

char *i18n_get_thousands_separator ();

char *i18n_get_locale_decp ();

char *i18n_get_locale_tsep ();

const char *i18n_get_lc_numeric ( const char*, const char* );

void i18n_free ();


#endif /* I18H_H */
