/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 *
 * FILE:	export.h
 *
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Functions for exporting data to GIS formats.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 *
 ***************************************************************************/


#include <stdio.h>
#include <string.h>

#include "geom.h"
#include "global.h"
#include "options.h"
#include "parser.h"


#ifndef EXPORT_H
#define EXPORT_H


/* maximum width of a double number (DBF) */
#define DBF_MAX_DOUBLE_WIDTH	18

/* constant width for an integer number (DBF) */
#define DBF_INTEGER_WIDTH	9

/* label field positions */
#define LBL_FIELD_POS_TEXT 				1
#define LBL_FIELD_POS_FONT_TYPE			2
#define LBL_FIELD_POS_FONT_STYLE		3
#define LBL_FIELD_POS_FONT_COLOR		4
#define LBL_FIELD_POS_FONT_SIZE			5
#define LBL_FIELD_POS_FONT_ROTATE		6
#define LBL_FIELD_POS_GEOM_TYPE			7

/* label field names */
#define LBL_FIELD_NAME_X				"labelposx"
#define LBL_FIELD_NAME_Y				"labelposy"
#define LBL_FIELD_NAME_TEXT 			"labeltext"
#define LBL_FIELD_NAME_FONT_TYPE 		"fonttype"
#define LBL_FIELD_NAME_FONT_STYLE 		"fontstyle"
#define LBL_FIELD_NAME_FONT_COLOR 		"fontcolor"
#define LBL_FIELD_NAME_FONT_SIZE 		"fontsize"
#define LBL_FIELD_NAME_FONT_ROTATE 		"fontrotate"
#define LBL_FIELD_NAME_GEOM_TYPE 		"geomtype"

/* label field default values */
#define LBL_FIELD_DEFAULT_TEXT			""
#define LBL_FIELD_DEFAULT_FONT_TYPE		"Arial"
#define LBL_FIELD_DEFAULT_FONT_STYLE	0
#define LBL_FIELD_DEFAULT_FONT_COLOR	-16777216
#define LBL_FIELD_DEFAULT_FONT_SIZE		10.0
#define LBL_FIELD_DEFAULT_FONT_ROTATE	0.0

/* geometry types for field "geomtype" */
#define LBL_FIELD_GEOM_TYPE_POINT		0
#define LBL_FIELD_GEOM_TYPE_LINE		1
#define LBL_FIELD_GEOM_TYPE_POLY		2


/* export all data stores to SHP */
int export_SHP ( geom_store *gs, parser_desc *parser, options *opts );

/* export all data stores to DXF */
int export_DXF ( geom_store *gs, parser_desc *parser, options *opts );

/* export all data stores to GeoJSON */
int export_GeoJSON ( geom_store *gs, parser_desc *parser, options *opts );

/* export all data stores to KML */
int export_KML ( geom_store *gs, parser_desc *parser, options *opts );

#endif /* EXPORT_H */
