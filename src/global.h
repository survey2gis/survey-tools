/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	global.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Global definitions.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.h"

#include <iconv.h>
#include <limits.h>
#include <unistd.h>

#ifdef MINGW
#include <windows.h>
#endif

#ifndef MINGW
typedef unsigned short BOOLEAN;
#endif

/* file separator used by OS */
#ifdef MINGW
#define PRG_FILE_SEPARATOR		'\\'
#define PRG_FILE_SEPARATOR_STR	"\\"
#else
#define PRG_FILE_SEPARATOR		'/'
#define PRG_FILE_SEPARATOR_STR	"/"
#endif

#ifndef FALSE
#define	FALSE	0
#endif

#ifndef TRUE
#define	TRUE	1
#endif

#define NULL_CHAR 	0

/* i18n support via GNU gettext */
#ifndef DARWIN
#include <libintl.h>
#endif
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)


/* exit status */
#define PRG_EXIT_OK		EXIT_SUCCESS
#define PRG_EXIT_ERR	EXIT_FAILURE

/* general program information */
#define PRG_NAME 			"Survey2GIS"
#define PRG_VERSION_MAJOR		1
#define PRG_VERSION_MINOR		5
#define PRG_VERSION_REVISION	0
/* a value > 0 indicates a beta quality release */
#define PRG_VERSION_BETA		0

/* supported output file formats */
#define PRG_OUTPUT_SHP			0
#define PRG_OUTPUT_DXF			1
#define PRG_OUTPUT_GEOJSON		2
#define PRG_OUTPUT_KML			3

#define PRG_OUTPUT_DEFAULT		PRG_OUTPUT_SHP

static const char PRG_OUTPUT_EXT[][8] =
{ "shp", "dxf", "geojson", "kml", "" };

static const char PRG_OUTPUT_DESC[][255] =
{ "Esri(tm) Shapefile", "AutoCAD(tm) Drawing Exchange Format", "GeoJSON Object", "Keyhole Markup Language", "" };

/* Parsing limits */
#define PRG_MAX_SELECTIONS		255 /* Maximum number of selections */

/* Precision limits */
#define PRG_MAX_DECIMAL_PLACES	16

/* Some limits imposed by Shapefile format restrictions */
#define PRG_MAX_FIELDS 			251 /* Maximum number of user fields in parser/attribute table.
								   	   Theoretically, this is 255, but we reserve one field for
								   	   the primary key "GEOM_ID", and two for GRASS' "cat" and
								   	   "cat_" keys, plus one for whatever other primary feature
								   	   key the target GIS application may want to use. */
#define PRG_MAX_FIELD_LEN 		10 /* maximum length of any field name in chars */
#define PRG_MAX_STR_LEN 		254 /* maximum length of any string */

/* a string that contains the complete set of characters allowed in a field name */
#define PRG_FIELD_NAME_CHARS 	"0123456789_ abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* reserved field name */
static const char PRG_RESERVED_FIELD_NAMES[][PRG_MAX_FIELD_LEN+1] =
{ "geom_id", "cat", "cat_", "index", "where", "join",
		"labelposx", "labelposy", "labeltext",
		"fonttype", "fontstyle", "fontcolor", "fontsize", "fontrotate", "geomtype",
		""
};

/* maximum number of characters in any file path handled by this software */
#define PRG_MAX_PATH_LENGTH				PATH_MAX

#define LBL_FIELD_NAME_X				"labelposx"
#define LBL_FIELD_NAME_Y				"labelposy"
#define LBL_FIELD_NAME_TEXT 			"labeltext"
#define LBL_FIELD_NAME_FONT_TYPE 		"fonttype"
#define LBL_FIELD_NAME_FONT_STYLE 		"fontstyle"
#define LBL_FIELD_NAME_FONT_COLOR 		"fontcolor"
#define LBL_FIELD_NAME_FONT_SIZE 		"fontsize"
#define LBL_FIELD_NAME_FONT_ROTATE 		"fontrotate"
#define LBL_FIELD_NAME_GEOM_TYPE 		"geomtype"

/* extension for settings files */
static const char PRG_SETTINGS_FILE_EXT[] = ".s2g";
/* pattern for file selector widget */
static const char PRG_SETTINGS_FILE_PATTERN[] = "*.s2g";
/* description for file selector widget */
static const char PRG_SETTINGS_FILE_DESC[] = "Survey2GIS (*.s2g)";
/* name of file with default settings */
static const char PRG_SETTINGS_FILE_DEFAULT[] = "default.s2g";
/* list separator for settings file content */
static const char PRG_SETTINGS_LIST_SEP = '|';
/* alternative list separator for settings file content */
static const char PRG_SETTINGS_LIST_SEP_ALT = 030; /* ASCII #030 = RS (Record Separator) */

#endif /* GLOBAL_H */
