/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	i18n.c
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


#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <glib.h>

#include "global.h"
#include "errors.h"
#ifdef GUI
#include "gui_conf.h"
#endif
#include "i18n.h"
#include "options.h"
#include "tools.h"


static char *I18N_CURRENT_LOCALE = NULL;

/*
 * CALL THIS FUNCTION IMMEDIATELY AFTER GUI_CONF_INIT()!
 *
 * This function reads the current locale and initializes
 * the internationalization capability via gettext. This
 * can be somewhat complex and depends on what locales are
 * installed on the system at run-time.
 *
 * On Ubuntu, test the German translations like this
 * (provided that support for de_DE.utf8 has been installed):
 *
 * export LANG="de_DE.utf8"
 * export LANGUAGE="de_DE:de"
 *
 * LANGUAGE is responsible for on-screen text, where-as LANG
 * sets locale information, such as the decimal point char.
 *
 * US English defaults are:
 *
 * LANG="en_US.UTF-8"
 * LANGUAGE="en_US:en"
 *
 * setlocale() can be more troublesome on Windows:
 * http://stackoverflow.com/questions/628557/using-gnu-gettext-on-win32
 * http://www.gnu.org/software/gettext/FAQ.html#windows_woe32
 *
 */
void i18n_init ()
{
	/*
	 * BD: Not required. See my comment below.
	struct lconv *locale_info;
	 */

	I18N_DECIMAL_POINT = NULL;
	I18N_THOUSANDS_SEP = NULL;

#ifdef GUI
	/* check if we have a locale preference in GUI settings */
#endif

#ifdef MINGW
	/* convert Windows local string to Posix representation */
	I18N_CURRENT_LOCALE = (char*) g_win32_getlocale ();
#else
	/* get current system locale */
	I18N_CURRENT_LOCALE = setlocale( LC_ALL, "" );
#endif

#ifdef GUI
	/* check if gui language has been set (and saved) by user */	
	char *lang = gui_conf_get_string ( "i18n", "gui_language" );
	if ( lang != NULL ) {
		if ( !strcmp (lang, "DE") ) {
			i18n_set_locale_de_DE();
		}
		if ( !strcmp (lang, "EN") ) {
			i18n_set_locale_en_EN();
		}
	}
#endif

	/* change to supported locale if required (default="en_EN") */
	i18n_force_supported_locale ();

	/* initialize i18n engine */
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "utf-8");
	textdomain(PACKAGE);

	/* get locale information */	
	/* BD: Since nearly 100% of survey data uses the EN notation
	 * for decimal numbers, we set that as the default, and no
	 * longer the national notation (whatever it might be)!
	locale_info = localeconv ();
	if ( locale_info != NULL ) {
		I18N_DECIMAL_POINT = strdup (locale_info->decimal_point);
		I18N_THOUSANDS_SEP = strdup (locale_info->thousands_sep);
	}
	 */

	/* Still NULL? Use default numeric notation! */
	if ( I18N_DECIMAL_POINT == NULL )
		I18N_DECIMAL_POINT = strdup (".");
	if ( I18N_THOUSANDS_SEP == NULL )
		I18N_THOUSANDS_SEP = strdup (",");

	/* Windows multi-byte encodings for cmd.exe output and file paths.
	 * Empty string means "system default". */
	I18N_WIN_CODEPAGE_CONSOLE = strdup ("");
	I18N_WIN_CODEPAGE_FILES = strdup ("");
}


/* Free memory for i18n.
 * CALL ONLY immediately before program exit.
 */
void i18n_free ()
{
	if ( I18N_DECIMAL_POINT  != NULL ) {
		free (I18N_DECIMAL_POINT);
	}

	if ( I18N_THOUSANDS_SEP  != NULL ) {
		free (I18N_THOUSANDS_SEP);
	}
}


/*
 * Returns decimal point as used by current locale.
 *
 * The returned value is an allocated string that must be
 * free'd by the caller.
 * Return value may be null if there is an error.
 */
char *i18n_get_locale_decp () {
	char *result = NULL;

	struct lconv *locale_info = localeconv ();
	if ( locale_info != NULL ) {
		result = strdup (locale_info->decimal_point);
	}

	return ( result );
}


/*
 * Returns thousands separator as used by current locale.
 *
 * The returned value is an allocated string that must be
 * free'd by the caller.
 * Return value may be null if there is an error.
 */
char *i18n_get_locale_tsep () {
	char *result = NULL;

	struct lconv *locale_info = localeconv ();
	if ( locale_info != NULL ) {
		result = strdup (locale_info->thousands_sep);
	}

	return ( result );
}


/* Checks whether the system is running in the POSIX
 * default "C" locale (=English messages).
 */
BOOLEAN i18n_is_locale_C () {
	if ( I18N_CURRENT_LOCALE == NULL ) {
		return FALSE;
	}

	if ( strlen (I18N_CURRENT_LOCALE) == 0 || !strcmp (I18N_CURRENT_LOCALE, "C") ) {
		return TRUE;
	}

	return FALSE;
}


/* Checks whether the system is running in an English
 * locale ("en_*").
 */
BOOLEAN i18n_is_locale_EN () {
	if ( I18N_CURRENT_LOCALE == NULL ) {
		return FALSE;
	}

	if ( strstr (I18N_CURRENT_LOCALE, "en_") ) {
		return TRUE;
	}

	return FALSE;
}


/* Checks whether the system is running in a German
 * locale ("de_*").
 */
BOOLEAN i18n_is_locale_DE () {
	if ( I18N_CURRENT_LOCALE == NULL ) {
		return FALSE;
	}

	if ( strstr (I18N_CURRENT_LOCALE, "de_") ) {
		return TRUE;
	}

	return FALSE;
}


/*
 * Checks whether we are running in a supported locale.
 * If not, sets application locale to "en_EN".
 */
void i18n_force_supported_locale () {
	if ( i18n_is_locale_DE() ) {
		i18n_set_locale_de_DE ();
		return;
	}
	if ( i18n_is_locale_EN() ) {
		i18n_set_locale_en_EN ();
		return;
	}
	/* C = English */
	if ( i18n_is_locale_C() ) {
		i18n_set_locale_en_EN ();
		return;
	}

	/* any other: set to English */
	i18n_set_locale_en_EN ();
}


/*
 * Sets current locale to "locale" (e.g. "en_EN").
 */
void i18n_set_locale ( const char *locale ) {
	if ( locale == NULL ) {
		return;
	}
	setlocale(LC_ALL, locale);
	setlocale(LC_NUMERIC, locale);
	t_setenv("LANGUAGE", locale);
	t_setenv("LANG", locale);
	t_setenv("LC_ALL", locale);
	t_setenv("LC_NUMERIC", locale);
	t_setenv("LC_MESSAGES", locale);

	I18N_CURRENT_LOCALE = (char*) locale;
}


/*
 * Set current locale to English.
 */
void i18n_set_locale_en_EN () {
	i18n_set_locale ( "en_EN" );
}


/*
 * Set current locale to German.
 */
void i18n_set_locale_de_DE () {
	i18n_set_locale ( "de_DE" );
}


/*
 * Force English screen/log messages and
 * numeric format (decimal point and comma
 * as thousands separator)
 */

void i18n_force_english () {

	/* set locale to "En_en" */
	i18n_set_locale_en_EN ();

	/* set numeric notation to English standard */
	I18N_DECIMAL_POINT = strdup (".");
	I18N_THOUSANDS_SEP = strdup (",");

}


/*
 * Sets the decimal point character.
 */
void i18n_set_decimal_point ( const char *dpoint ) {
	I18N_DECIMAL_POINT = strdup (dpoint);
}


/*
 * Sets the thousands separator character.
 */
void i18n_set_thousands_separator ( const char *tsep ) {
	I18N_THOUSANDS_SEP = strdup (tsep);
}


/*
 * Returns the current representation of the decimal point.
 */
char *i18n_get_decimal_point () {
	return ( I18N_DECIMAL_POINT );
}


/*
 * Returns the current representation of the thousands separator.
 */
char *i18n_get_thousands_separator () {
	return ( I18N_THOUSANDS_SEP );
}


/*
 * Returns a default locale string that is _compatible_ with
 * the numeric notation for group and thousands separator,
 * as given in the parameters.
 * The returned locale is _only_ useful for setting LC_NUMERIC,
 * since many languages may share the same numeric format.
 * Note that this method is fuzzy. It often decides only by the
 * decimal point character.
 */
const char *i18n_get_lc_numeric ( const char *decp, const char *tsep ) {
	if ( !strcmp(decp, ".") ) {
		if ( !strcmp(tsep, "'") ) return ("de_CH");
		if ( !strcmp(tsep, " ") ) return ("et_EE");
		/* default locale for "." decimal point is English */
		return ("en_EN" );
	}
	if ( !strcmp(decp, ",") ) {
		if ( !strcmp(tsep, " ") ) return ("fr_FR");
		/* default locale for "," decimal point is German (Germany) */
		return ("de_DE" );
	}
	if ( !strcmp(decp, "/") ) {
		return ("fa_IR" );
	}
	if ( !strcmp(decp, "-") ) {
		return ("kk_KZ" );
	}
	/* default is always English notation */
	return ("en_EN");
}
