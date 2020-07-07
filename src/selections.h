/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	selections.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Functions to validate and apply field content based selections.
 *
 * COPYRIGHT:	(C) 2015 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "global.h"
#include "geom.h"
#include "options.h"
#include "parser.h"


#ifndef SELECTIONS_H
#define SELECTIONS_H


/* selection token separators */
#define SELECTION_TOKEN_SEP				":"
#define SELECTION_RANGE_SEP				";"

/* selection type IDs and names */
#define NUM_SELECTION_TYPES				10 /* number of types */
#define SELECTION_TYPE_INVALID 			-1
#define SELECTION_TYPE_EQ 				0 /* equal */
#define SELECTION_TYPE_EQ_NAME 			"eq"
#define SELECTION_TYPE_EQ_NAME_FULL 	"Equal (eq)"
#define SELECTION_TYPE_NEQ 				1 /* not equal */
#define SELECTION_TYPE_NEQ_NAME 		"neq"
#define SELECTION_TYPE_NEQ_NAME_FULL 	"Not equal (neq)"
#define SELECTION_TYPE_LT 				2 /* less than */
#define SELECTION_TYPE_LT_NAME 			"lt"
#define SELECTION_TYPE_LT_NAME_FULL 	"Less than (lt)"
#define SELECTION_TYPE_GT				3 /* greater than */
#define SELECTION_TYPE_GT_NAME			"gt"
#define SELECTION_TYPE_GT_NAME_FULL		"Greater than (gt)"
#define SELECTION_TYPE_LTE 				4 /* less than or equal */
#define SELECTION_TYPE_LTE_NAME 		"lte"
#define SELECTION_TYPE_LTE_NAME_FULL 	"Less than or equal (lte)"
#define SELECTION_TYPE_GTE 				5 /* greater than or equal */
#define SELECTION_TYPE_GTE_NAME 		"gte"
#define SELECTION_TYPE_GTE_NAME_FULL 	"Greater than or equal (gte)"
#define SELECTION_TYPE_SUB 				6 /* substring */
#define SELECTION_TYPE_SUB_NAME			"sub"
#define SELECTION_TYPE_SUB_NAME_FULL	"Substring (sub)"
#define SELECTION_TYPE_REGEXP 			7 /* regular expression */
#define SELECTION_TYPE_REGEXP_NAME		"regexp"
#define SELECTION_TYPE_REGEXP_NAME_FULL	"Regular expression (regexp)"
#define SELECTION_TYPE_RANGE 			8 /* numeric range */
#define SELECTION_TYPE_RANGE_NAME		"range"
#define SELECTION_TYPE_RANGE_NAME_FULL	"Range (range)"
#define SELECTION_TYPE_ALL 				9 /* all records */
#define SELECTION_TYPE_ALL_NAME 		"all"
#define SELECTION_TYPE_ALL_NAME_FULL 	"All (all)"

/* selection type names */
extern char *SELECTION_TYPE_NAME[];
extern char *SELECTION_TYPE_NAME_FULL[];

/* selection type names incl. modifiers */
extern char *SELECTION_TYPE_NAME_ADD[];

extern char *SELECTION_TYPE_NAME_SUB[];

/* modifier chars */
#define SELECTION_MOD_REPLACE		'' /* replace current selection */
#define SELECTION_MOD_REPLACE_NAME	"Replace selection"
#define SELECTION_MOD_INV			'*' /* invert effect of selection expression */
#define SELECTION_MOD_INV_STR		"*"
#define SELECTION_MOD_INV_NAME		"Invert selection (*)"
#define SELECTION_MOD_ADD			'+' /* add to existing selection */
#define SELECTION_MOD_ADD_STR		"+"
#define SELECTION_MOD_ADD_NAME		"Add to selection (+)"
#define SELECTION_MOD_SUB			'-' /* subtract from existing selection */
#define SELECTION_MOD_SUB_STR		"-"
#define SELECTION_MOD_SUB_NAME		"Subtract from selection (-)"

/* geometry type IDs and names */
#define NUM_SELECTION_GEOMS				5 /* number of supported geom types */
#define SELECTION_GEOM_INVALID			-1
#define SELECTION_GEOM_POINT			0 /* point */
#define SELECTION_GEOM_POINT_NAME		"point"
#define SELECTION_GEOM_POINT_NAME_FULL	"Points (point)"
#define SELECTION_GEOM_RAW				1 /* raw point/vertex */
#define SELECTION_GEOM_RAW_NAME			"raw"
#define SELECTION_GEOM_RAW_NAME_FULL	"'Raw' points/vertices (raw)"
#define SELECTION_GEOM_LINE				2 /* line */
#define SELECTION_GEOM_LINE_NAME		"line"
#define SELECTION_GEOM_LINE_NAME_FULL	"Lines (line)"
#define SELECTION_GEOM_POLY				3 /* polygon */
#define SELECTION_GEOM_POLY_NAME		"poly"
#define SELECTION_GEOM_POLY_NAME_FULL	"Polygons (poly)"
#define SELECTION_GEOM_ALL				4 /* any of the above */
#define SELECTION_GEOM_ALL_NAME			"all"
#define SELECTION_GEOM_ALL_NAME_FULL	"All (all)"

/* geometry type names */
extern char *SELECTION_GEOM_TYPE_NAME[];
extern char *SELECTION_GEOM_TYPE_NAME_FULL[];


/* Add a selection to list of selections */
BOOLEAN selection_add ( char *expr, options *opt );

/* Return number of selections on list */
unsigned int selections_get_count ( options *opt );

/* Validate all specified selections */
BOOLEAN selections_validate ( options *opt, parser_desc *parser );

/* Check only syntax of one selection expression */
BOOLEAN selection_is_valid_syntax ( char *selection );

/* Check state of selection modifiers */
BOOLEAN selection_is_mod_add ( char *seltype );
BOOLEAN selection_is_mod_sub ( char *seltype );
BOOLEAN selection_is_mod_inv ( char *seltype );

/* Get selection type index from selection type name string */
short selection_get_seltype ( char *seltype );

/* Get selection geometry type index from selection geometry type name string */
short selection_get_geomtype ( char *seltype );

/* Apply all specified selections to a geometry store */
void selections_apply_all ( options *opt, parser_desc *parser, geom_store *gs );

/* Return total number of selected geometry of a specified type */
int selections_get_num_selected ( short geom_type, geom_store *gs );

#endif /* SELECTIONS_H */

