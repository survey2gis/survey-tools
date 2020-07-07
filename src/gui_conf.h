/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	gui_conf.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	GUI settings support via an INI file.
 *
 * COPYRIGHT:	(C) 2016 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "global.h"


#ifndef GUI_CONF_H
#define GUI_CONF_H

/* get a string value from the INI file */
char *gui_conf_get_string ( const char* group, const char* key );

/* set a string value in the INI file */
BOOLEAN gui_conf_set_string ( const char* group, const char* key, char *value );

#endif /* GUI_CONF_H */

