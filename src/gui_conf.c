/***************************************************************************
 *
 * PROGRAM:		Survey2GIS
 * FILE:		gui_conf.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	GUI settings support via a local INI file.
 * 				INI file format is:
 *
 * 				# Comment
 *
 * 				[Group 1]
 * 				Key1=Value
 * 				Key2=Value
 * 				...
 *
 * 				[Group 2]
 * 				...
 *
 * COPYRIGHT:	(C) 2016 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <locale.h>


#include "global.h"
#include "i18n.h"
#include "options.h"
#include "tools.h"


#ifdef GUI

#include <glib.h>



/*
 * Returns the configuration value that matches "key", from the
 * given "group" of the INI file, as a string.
 *
 * Location and name of the INI file are automatically determined.
 *
 * The returned string will be newly allocated and must be free'd
 * by the caller.
 *
 * In case of error or if there is no such group or key, NULL is returned.
 */
char *gui_conf_get_string ( const char* group, const char* key )
{
	char *search_dirs[2]; /* will only search in user settings folder */
	char *file_name = NULL;
	char *path = NULL;
	GKeyFile *keyfile = NULL;
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	BOOLEAN exists = FALSE;
	char *result = NULL;


	if ( group == NULL || key == NULL ) {
		return ( NULL );
	}

	/* compose INI file name */
	int len = strlen ( t_get_prg_name() ) + strlen ( ".ini ") + 1;
	file_name = malloc ( len );
	snprintf ( file_name, len, "%s.ini", t_get_prg_name() );

	/* add user settings folder to search path */
	search_dirs[0] = (char*) g_get_user_config_dir ();
	search_dirs[1] = NULL;

	/* attempt to open key file */
	keyfile = g_key_file_new ();
	exists = g_key_file_load_from_dirs ( keyfile, file_name, (const char **) search_dirs, &path, flags, NULL );
	if ( exists == TRUE ) {
		/* get string value from keyfile */
		result = g_key_file_get_string (keyfile, group, key, NULL);
	}

	/* release resources */
	if ( keyfile != NULL ) {
		g_key_file_free ( keyfile );
	}
	if ( file_name != NULL ) {
		free ( file_name );
	}
	if ( path != NULL ) {
		free ( path );
	}

	return ( result );
}


/*
 * Sets the "value" of "key" in the specified "group" as a string value.
 * The new value will be written to the INI file immediately.
 * If the INI file does not exist then it will be created.
 *
 * Location and name of the INI file are automatically determined.
 *
 * Returns TRUE if the value was successfully set, FALSE on error.
 */
BOOLEAN gui_conf_set_string ( const char* group, const char* key, char *value ) {
	char *search_dirs[2]; /* will only search in user settings folder */
	char *file_name = NULL;
	char *path = NULL;
	GKeyFile *keyfile = NULL;
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	BOOLEAN exists = FALSE;
	BOOLEAN success;


	if ( group == NULL || key == NULL || value == NULL ) {
		return ( FALSE );
	}

	/* compose INI file name */
	int len = strlen ( t_get_prg_name() ) + strlen ( ".ini ") + 1;
	file_name = malloc ( len );
	snprintf ( file_name, len, "%s.ini", t_get_prg_name() );

	/* add user settings folder to search path */
	search_dirs[0] = (char*) g_get_user_config_dir ();
	search_dirs[1] = NULL;

	/* attempt to open key file */
	keyfile = g_key_file_new ();
	exists = g_key_file_load_from_dirs ( keyfile, file_name, (const char **) search_dirs, &path, flags, NULL );

	/* could not open: create new file name and key file */
	if ( exists == FALSE ) {
		int len = strlen (search_dirs[0]) + strlen (file_name) + 2;
		path = malloc ( len );
		snprintf (path, len, "%s%c%s", search_dirs[0], PRG_FILE_SEPARATOR, file_name);
	}

	/* attempt to set the value */
	g_key_file_set_string (keyfile, group, key, value);

	/* check if the value now exists in the key file */
	success = g_key_file_has_key (keyfile, group, key, NULL );
	if ( success == TRUE ) {
		/* write new key file contents to disk */
		success =  g_key_file_save_to_file (keyfile, path, NULL);
	}

	/* release resources */
	if ( keyfile != NULL ) {
		g_key_file_free ( keyfile );
	}
	if ( file_name != NULL ) {
		free ( file_name );
	}
	if ( path != NULL ) {
		free ( path );
	}

	return ( success );
}


#endif


