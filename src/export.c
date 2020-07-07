/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 *
 * FILE:	export.c
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


#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "global.h"
#include "errors.h"
#include "export.h"
#include "geom.h"
#include "i18n.h"
#include "options.h"
#include "parser.h"
#include "selections.h"
#include "tools.h"


/* strings with standard DXF layer names */
static const char DXF_LAYER_NAME_RAW[] = 				"001_raw_points";
static const char DXF_LAYER_NAME_RAW_LABELS[] = 		"002_raw_point_coords";
static const char DXF_LAYER_NAME_POINT[] = 				"003_points";
static const char DXF_LAYER_NAME_POINT_LABELS[] = 		"004_point_ids";
static const char DXF_LAYER_NAME_LINE[] = 				"005_lines";
static const char DXF_LAYER_NAME_LINE_LABELS[] = 		"006_line_ids";
static const char DXF_LAYER_NAME_AREA[] = 				"007_areas";
static const char DXF_LAYER_NAME_AREA_LABELS[] = 		"008_area_ids";
static const char DXF_LAYER_NAME_LABELS[] =				"009_labels";

/* first layer number to use when composing DXF layer names for field value labels */
static int DXF_LAYER_FIRST_FIELD = 10;

/* symbology settings */
static double DXF_LABEL_SIZE_RAW = 0.08;
static double DXF_LABEL_SIZE_POINT = 0.12;
static double DXF_LABEL_SIZE_LINE = 0.12;
static double DXF_LABEL_SIZE_AREA = 0.12;
static double DXF_LABEL_SIZE_USER = 0.12;

/* forward declarations of functions */
char *export_float_to_str ( double f );


/* Checks whether a value can be stored in a DBase
   field of a given type.

   type should be one of:

   PARSER_FIELD_TYPE_TEXT			0
   PARSER_FIELD_TYPE_INT 			1
   PARSER_FIELD_TYPE_DOUBLE 		2

   Returns TRUE if so, FALSE otherwise.
   Caller needs to decide what to do in case the value cannot be stored.

 */
BOOLEAN export_SHP_DBF_field_width_OK ( char *val, int type, options *opts )
{
	if ( type == PARSER_FIELD_TYPE_TEXT ) {
		if ( strlen ( val ) > PRG_MAX_STR_LEN )
			return ( FALSE );
	}

	if ( type == PARSER_FIELD_TYPE_INT ) {
		if ( strlen ( val ) > DBF_INTEGER_WIDTH )
			return ( FALSE );
	}

	/* double values are more tricky */
	if ( type == PARSER_FIELD_TYPE_DOUBLE ) {
		char *p;
		char *buf;

		buf = strdup ( val );
		p = strstr ( buf, I18N_DECIMAL_POINT );
		if ( p!= NULL ) {
			*p = '\0';
			if ( ( strlen (p) + strlen (I18N_DECIMAL_POINT) + opts->decimal_places ) > DBF_MAX_DOUBLE_WIDTH)
				return ( FALSE );
		}
		free ( buf );
	}

	return ( TRUE );
}


/*
 * Writes the attribute data row for a given record as a
 * new row to a Shapefile DBF attribute table.
 * If there is a problem with the data, a (row of) NULL field(s) will be
 * written to the attribute table and a warning issued.
 *
 * "pk" is the primary key, i.e. an integer index from 0 .. n, which
 * points directly into the points(_raw)[], lines[], or polygons[]
 * array of the geometry store "gs".
 *
 * The return value is the total number of attribute data errors.
 */
int export_SHP_write_atts ( 	DBFHandle dbf, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int obj, parser_desc *parser,
		options *opts )
{
	unsigned int err_count;
	int i_value;
	double d_value;
	int field_num;
	int pos;
	char *NULL_str = NULL;
	BOOLEAN c_error;
	BOOLEAN c_overflow;
	BOOLEAN fits;
	BOOLEAN store_null;
	BOOLEAN success;
	char *input;
	unsigned int line;
	char **atts;


	if ( gs == NULL || parser == NULL || opts == NULL )
		return ( 1 );

	if ( 	GEOM_TYPE != GEOM_TYPE_POINT && GEOM_TYPE != GEOM_TYPE_POINT_RAW &&
			GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nGeometry type unknown.\nNo attribute data written.") );
		return ( 1 );
	}

	/* string representation of user-defined NULL value */
	if ( parser->empty_val_set == TRUE ) {
		NULL_str = (char*) malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
		snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
	}

	/* get name of input source */
	input = NULL;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		input = strdup (gs->points[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		input = strdup (gs->points_raw[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		input = strdup (gs->lines[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		input = strdup (gs->polygons[pk].source);
	if ( input == NULL )  {
		input = strdup ("<NULL>");
	} else {
		if ( !strcmp ( "-", input ) ) {
			free ( input );
			input = strdup ("<console input stream>");
		}
	}

	/* get line of input file from which this geom was read */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		line = gs->points[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		line = gs->points_raw[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		line = gs->lines[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		line = gs->polygons[pk].line;

	/* get pointer to attribute field contents (strings) */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		atts = gs->points[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		atts = gs->points_raw[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		atts = gs->lines[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		atts = gs->polygons[pk].atts;

	/* reset error count to "0" */
	err_count = 0;

	/* first field is always the primary key */
	success = DBFWriteIntegerAttribute( dbf, obj, 0, pk );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write primary key '%i' into '%s'."),
				input, line, pk, PRG_RESERVED_FIELD_NAMES[0] );
		err_count ++;
	}

	field_num = 0;
	pos = 1;
	while ( parser->fields[field_num] != NULL ) {
		if ( parser->fields[field_num]->skip == FALSE  ) {

			/* TEXT field */
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_TEXT ) {
				/*** TEXT ***/
				fits = export_SHP_DBF_field_width_OK ( atts[field_num], PARSER_FIELD_TYPE_TEXT, opts );
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( fits == FALSE ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' does not fit into a text field.\nNULL data written instead."),
							input, line, parser->fields[field_num]->name);
					store_null = TRUE;
					err_count ++;
				} else {
					if ( atts[field_num] == NULL ) {
						store_null = TRUE;
					}
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default XBase NULL value */
						success = DBFWriteNULLAttribute( dbf, obj, pos );
					} else {
						/* write user-defined NULL value */
						success = DBFWriteStringAttribute( dbf, obj, pos, NULL_str );
					}
				} else {
					/* write actual field contents */
					success = DBFWriteStringAttribute( dbf, obj, pos, atts[field_num] );
				}
			}

			/* INT field */
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_INT ) {
				/*** INTEGER ***/
				i_value = t_str_to_int ( atts[field_num], &c_error, &c_overflow );
				fits = export_SHP_DBF_field_width_OK ( atts[field_num], PARSER_FIELD_TYPE_INT, opts );
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( c_error == TRUE && c_overflow == FALSE ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i\nValue for attribute '%s' is not a valid integer number\nNULL data written instead."),
							input, line, parser->fields[field_num]->name);
					store_null = TRUE;
					err_count ++;
				} else {
					if ( c_overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
								input, line, parser->fields[field_num]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( fits == FALSE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' does not fit into an integer field.\nNULL data written instead."),
									input, line, parser->fields[field_num]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[field_num] == NULL ) {
								store_null = TRUE;
							}
						}
					}
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default XBase NULL value */
						success = DBFWriteNULLAttribute( dbf, obj, pos );
					} else {
						/* write user-defined NULL value */
						success = DBFWriteIntegerAttribute( dbf, obj, pos, parser->empty_val );
					}
				} else {
					/* write actual attribute value */
					success = DBFWriteIntegerAttribute( dbf, obj, pos, i_value );
				}
				if ( success == FALSE ) {
					/* attribute write error */
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write attribute '%s'."),
							input, line, parser->fields[field_num]->name);
					err_count ++;
				}
			}

			/* DOUBLE field */
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_DOUBLE ) {

#ifdef MINGW
				/* DEBUG
				//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				  THIS IS A TERRIBLE WORK-AROUND FOR A PROBLEM WITH THE WINDOWS-
				  VERSION OF SHAPELIB (INCL. 1.3.0)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				  Basically, Shapelib uses its own atod() on Windows and that
				  screws up decimal point character conversion.
				  As a work-around, we try to guess the correct numeric format
				  by looking at the user-provided characters and then we set
				  LC_NUMERIC accordingly. We reset LC_NUMERIC, as soon as the
				  DBF export is done (see end of this codeblock)!
				  The same is true for the global vars I18N_DECIMAL_POINT/I18N_THOUSANDS_SEP
				 */
				if ( !strcmp("en_EN", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup (".");
					I18N_THOUSANDS_SEP = strdup (",");
				}
				else if ( !strcmp("de_CH", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup (".");
					I18N_THOUSANDS_SEP = strdup ("'");
				}
				else if ( !strcmp("et_EE", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup (".");
					I18N_THOUSANDS_SEP = strdup (" ");
				}
				else if ( !strcmp("fr_FR", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup (",");
					I18N_THOUSANDS_SEP = strdup (" ");
				}
				else if ( !strcmp("de_DE", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup (",");
					I18N_THOUSANDS_SEP = strdup (".");
				}
				else if ( !strcmp("fa_IR", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup ("/");
					I18N_THOUSANDS_SEP = strdup ("'");
				}
				else if ( !strcmp("kk_KZ", i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group )) ) {
					I18N_DECIMAL_POINT = strdup ("-");
					I18N_THOUSANDS_SEP = strdup (".");
				}
				setlocale( LC_NUMERIC, i18n_get_lc_numeric ( opts->decimal_point, opts->decimal_group ) );
				bindtextdomain ("", "");
				textdomain ("");
#endif

				/*** DOUBLE ***/
				d_value = t_str_to_dbl ( atts[field_num], opts->decimal_point[0], opts->decimal_group[0], &c_error, &c_overflow );
				fits = export_SHP_DBF_field_width_OK ( atts[field_num], PARSER_FIELD_TYPE_DOUBLE, opts );
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( c_error == TRUE && c_overflow == FALSE ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is not a valid number\nNULL data written instead."),
							input, line, parser->fields[field_num]->name);
					store_null = TRUE;
					err_count ++;
				} else {
					if ( c_overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
								input, line, parser->fields[field_num]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( fits == FALSE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' does not fit into a numeric field.\nNULL data written instead."),
									input, line, parser->fields[field_num]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[field_num] == NULL ) {
								store_null = TRUE;
							}
						}
					}
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default XBase NULL value */
						success = DBFWriteNULLAttribute( dbf, obj, pos );
					} else {
						/* write user-defined NULL value */
						success = DBFWriteDoubleAttribute( dbf, obj, pos, (double) parser->empty_val );
					}
				} else {

					/* write actual attribute value */
					success = DBFWriteDoubleAttribute( dbf, obj, pos, d_value );

				}
				if ( success == FALSE ) {
					/* attribute write error */
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write attribute '%s'."),
							input, line, parser->fields[field_num]->name);
					err_count ++;
				}

#ifdef MINGW
				/* DEBUG
				  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				  THIS IS A TERRIBLE WORK-AROUND FOR A PROBLEM WITH THE WINDOWS-
				  VERSION OF SHAPELIB (INCL. 1.3.0)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				  (SEE BEGINNING OF THIS CODEBLOCK FOR MORE INFO!)
				  This resets the i18n engine's numerical notation to the OS' settings.
				*/
				/* setlocale( LC_NUMERIC, "" ); */
				setlocale( LC_NUMERIC, PACKAGE );
				/* bindtextdomain ("", ""); */
				bindtextdomain(PACKAGE, LOCALEDIR);
				/* textdomain (""); */
				textdomain (PACKAGE);
				I18N_DECIMAL_POINT = strdup (opts->decimal_point);
				I18N_THOUSANDS_SEP = strdup (opts->decimal_group);
#endif

			}
			pos ++;
		}
		field_num ++;
	}

	if ( NULL_str != NULL )
		free ( NULL_str );
	free ( input );
	return (err_count);
}


/*
 * This is a modified version of export_SHP_write_atts_labels() (see above) that
 * writes the attribute field values for the labels layer, which are very
 * different from those written for measurement geometries.
 *
 * The return value is the total number of attribute data errors.
 */
int export_SHP_write_atts_labels ( 	DBFHandle dbf, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int obj, parser_desc *parser,
		options *opts )
{
	unsigned int err_count;
	int field_num;
	char *NULL_str = NULL;
	BOOLEAN fits;
	BOOLEAN store_null;
	BOOLEAN success;
	char *input;
	unsigned int line;
	char **atts;


	if ( gs == NULL || parser == NULL || opts == NULL )
		return ( 1 );

	if ( 	GEOM_TYPE != GEOM_TYPE_POINT && GEOM_TYPE != GEOM_TYPE_POINT_RAW &&
			GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nGeometry type unknown.\nNo attribute data written.") );
		return ( 1 );
	}

	/* string representation of user-defined NULL value */
	if ( parser->empty_val_set == TRUE ) {
		NULL_str = (char*) malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
		snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
	}

	/* get name of input source */
	input = NULL;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		input = strdup (gs->points[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		input = strdup (gs->points_raw[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		input = strdup (gs->lines[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		input = strdup (gs->polygons[pk].source);
	if ( input == NULL )  {
		input = strdup ("<NULL>");
	} else {
		if ( !strcmp ( "-", input ) ) {
			free ( input );
			input = strdup ("<console input stream>");
		}
	}

	/* get line of input file from which this geom was read */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		line = gs->points[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		line = gs->points_raw[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		line = gs->lines[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		line = gs->polygons[pk].line;

	/* get pointer to attribute field contents (strings) */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		atts = gs->points[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		atts = gs->points_raw[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		atts = gs->lines[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		atts = gs->polygons[pk].atts;

	/* reset error count to "0" */
	err_count = 0;

	/* first field is always the primary key */
	success = DBFWriteIntegerAttribute( dbf, obj, 0, pk );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write primary key '%i' into '%s'."),
				input, line, pk, PRG_RESERVED_FIELD_NAMES[0] );
		err_count ++;
	}

	field_num = 0;
	while ( parser->fields[field_num] != NULL ) {
		/* Find label field and copy its text into the attribute "labeltext" */
		if ( !strcasecmp (parser->fields[field_num]->name, opts->label_field)  ) {
			fits = export_SHP_DBF_field_width_OK ( atts[field_num], PARSER_FIELD_TYPE_TEXT, opts );
			store_null = FALSE;
			/* check for all conditions under which NULL data will be written */
			if ( fits == FALSE ) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' does not fit into a text field.\nNULL data written instead."),
						input, line, parser->fields[field_num]->name);
				store_null = TRUE;
				err_count ++;
			} else {
				if ( atts[field_num] == NULL ) {
					store_null = TRUE;
				}
			}
			if ( store_null == TRUE ) {
				/* store "no data" representation */
				if ( parser->empty_val_set == FALSE ) {
					/* write default XBase NULL value */
					success = DBFWriteNULLAttribute( dbf, obj, LBL_FIELD_POS_TEXT );
				} else {
					/* write user-defined NULL value */
					success = DBFWriteStringAttribute( dbf, obj, LBL_FIELD_POS_TEXT, NULL_str );
				}
			} else {
				/* write actual field contents */
				success = DBFWriteStringAttribute( dbf, obj, LBL_FIELD_POS_TEXT, atts[field_num] );
			}
			if ( success == FALSE ) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
						input, line, LBL_FIELD_NAME_TEXT );
				err_count ++;
			}
		}
		field_num ++;
	}

	/* Write default values into all other fields */
	success = DBFWriteStringAttribute( dbf, obj, LBL_FIELD_POS_FONT_TYPE, LBL_FIELD_DEFAULT_FONT_TYPE );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_TYPE );
		err_count ++;
	}
	success = DBFWriteIntegerAttribute( dbf, obj, LBL_FIELD_POS_FONT_STYLE, LBL_FIELD_DEFAULT_FONT_STYLE );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_STYLE );
		err_count ++;
	}
	success = DBFWriteIntegerAttribute( dbf, obj, LBL_FIELD_POS_FONT_COLOR, LBL_FIELD_DEFAULT_FONT_COLOR );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_COLOR );
		err_count ++;
	}
	success = DBFWriteDoubleAttribute( dbf, obj, LBL_FIELD_POS_FONT_SIZE, LBL_FIELD_DEFAULT_FONT_SIZE );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_SIZE );
		err_count ++;
	}
	success = DBFWriteDoubleAttribute( dbf, obj, LBL_FIELD_POS_FONT_ROTATE, LBL_FIELD_DEFAULT_FONT_ROTATE );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_ROTATE );
		err_count ++;
	}
	/* encode geometry type */
	int geom_type = LBL_FIELD_GEOM_TYPE_POINT;
	if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
		geom_type = LBL_FIELD_GEOM_TYPE_LINE;
	} else if ( GEOM_TYPE == GEOM_TYPE_POLY ) {
		geom_type = LBL_FIELD_GEOM_TYPE_POLY;
	}
	success = DBFWriteIntegerAttribute( dbf, obj, LBL_FIELD_POS_GEOM_TYPE, geom_type );
	if ( success == FALSE ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nUnable to write label attribute '%s'."),
				input, line, LBL_FIELD_NAME_FONT_ROTATE );
		err_count ++;
	}

	if ( NULL_str != NULL ) {
		free ( NULL_str );
	}
	free ( input );

	return (err_count);
}


/*
 * Helper function for export_SHP (below).
 * Creates the attribute table for the shapefile in
 * DBase (rather: XBase) format.
 *
 * Returns -1 on error, 0 if OK.
 */
int export_SHP_make_DBF ( DBFHandle dbf, parser_desc *parser, options *opts )
{
	int error;
	int field_num;
	int field_type;
	int field_width;
	int field_decpl;


	/* First attribute is always the geometry ID key field */
	error = DBFAddField( dbf, PRG_RESERVED_FIELD_NAMES[0], FTInteger, DBF_INTEGER_WIDTH, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to write DBF attribute table schema.") );
		return (-1);
	}

	/* now add all other fields that were defined in the parser */
	field_num = 0;
	while ( parser->fields[field_num] != NULL ) {
		if ( parser->fields[field_num]->skip == FALSE ) {
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_TEXT ) {
				field_type = FTString;
				field_width = PRG_MAX_STR_LEN;
				field_decpl = 0;
			}
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_INT ) {
				field_type = FTInteger;
				field_width = DBF_INTEGER_WIDTH;
				field_decpl = 0;
			}
			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_DOUBLE ) {
				field_type = FTDouble;
				field_width = DBF_MAX_DOUBLE_WIDTH-opts->decimal_places;
				field_decpl = opts->decimal_places;
			}
			error = DBFAddField( dbf, parser->fields[field_num]->name, field_type, field_width, field_decpl );
			if ( error < 0 ) {
				err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema."));
				return (-1);
			}
		}
		field_num ++;
	}

	return (0);
}


/*
 * A modified version of export_SHP_make_DBF() (see above) that creates the DBF
 * schema for label points, which is different from that used for storing
 * measured geometries.
 *
 * Attribute schema:
 *
 * labeltext (varchar)
 * fonttype (varchar)
 * fontstyle (integer)
 * fontcolor (integer)
 * fontsize (double)
 * fontrotate (double)
 * geomtype (integer)
 *
 * Returns -1 on error, 0 if OK.
 */
int export_SHP_make_DBF_labels ( DBFHandle dbf, parser_desc *parser, options *opts )
{
	int error;

	/* First attribute is always the geometry ID key field */
	error = DBFAddField( dbf, PRG_RESERVED_FIELD_NAMES[0], FTInteger, DBF_INTEGER_WIDTH, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to write DBF attribute table schema.") );
		return (-1);
	}

	/* label text */
	error = DBFAddField( dbf, LBL_FIELD_NAME_TEXT, FTString, PRG_MAX_STR_LEN, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* font type (e.g. "Arial") */
	error = DBFAddField( dbf, LBL_FIELD_NAME_FONT_TYPE, FTString, PRG_MAX_STR_LEN, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* font style (0=normal, etc.) */
	error = DBFAddField( dbf, LBL_FIELD_NAME_FONT_STYLE, FTInteger, DBF_INTEGER_WIDTH, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* font color (integer-coded RGB value) */
	error = DBFAddField( dbf, LBL_FIELD_NAME_FONT_COLOR, FTInteger, DBF_INTEGER_WIDTH, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* font size (may be interpreted as pixel or real-world units) */
	error = DBFAddField( dbf, LBL_FIELD_NAME_FONT_SIZE, FTDouble, DBF_MAX_DOUBLE_WIDTH, 9 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* font rotation (double-precision angle) */
	error = DBFAddField( dbf, LBL_FIELD_NAME_FONT_ROTATE, FTDouble, DBF_MAX_DOUBLE_WIDTH, 3 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	/* geometry type (integer code) */
	error = DBFAddField( dbf, LBL_FIELD_NAME_GEOM_TYPE, FTInteger, DBF_INTEGER_WIDTH, 0 );
	if ( error < 0 ) {
		err_show ( ERR_EXIT, _("Failed to create DBF attribute table schema for labels layer."));
		return (-1);
	}

	return (0);
}


/*
 * Helper function for export_SHP().
 * Creates the special ".gva" annotation settings file used
 * by gvSIG CE to identify label layers.
 * In its basic form, this is just an empty file.
 *
 * Returns -1 on error, 0 on success
 */
int export_SHP_make_labels_GVA ( const char* path )
{
	FILE *ft;

	/* attempt to open text file for writing */
	ft = t_fopen_utf8 ( path, "w+");
	if ( ft == NULL ) {
		return ( -1 );
	}

	/* write header with field names */
	/* GEOM_ID will always be the first field */
	fprintf ( ft, "\n" );
	fclose ( ft );

	return ( 0 );
}


/*
 * Cross-plattform file path helper function:
 * 
 * This function takes care of translating a file path given in
 * UTF-8 to Windows multi-byte.
 * If UTF-8 translation fails, then the raw, untranslated path
 * is copied. On systems other than Windows, no translation takes
 * place and the unmodified path is returned.
 * 
 * This translation is necessary on Windows, because the Windows
 * version of shapelib expects paths to be in Windows multi-byte
 * encoding.
 * 
 * Returns a newly allocated string which must be free'd by the
 * caller when no longer needed. May return NULL if the path
 * given is NULL or there is an error other than an invalid
 * byte sequence.
 * 
 */
char* export_SHP_path_utf8_to_native ( const char* path ) {
	if ( path == NULL) {
		return ( NULL );
	}
#ifdef MINGW
	char *convbuf = NULL;
	int result = t_str_enc("UTF-8", I18N_WIN_CODEPAGE_FILES, (char*)path, &convbuf);
	if ( convbuf == NULL || result < 0 ) {
		if ( errno == EILSEQ ) {
			/* Got an illegal byte sequence: maybe the
			 * path is already in UTF-8 encoding. If so:
			 * Return the original path unmodified.
			 */ 
			return ( strdup (path) );
		} else {
			return ( NULL );
		}		
	} else {
		return ( convbuf );
	}
#else
	return ( strdup (path) );
#endif
}


/*
 * Cross-platform helper function for shapefile creation:
 * 
 * This function just makes sure that file paths are translated
 * to the operating system's multi-byte representation before
 * shapelib's own SHPCreate() is invoked on the path.
 * 
 */
SHPHandle export_SHP_SHPCreate ( const char* pszShapeFile, int nShapeType )
{	
	SHPHandle h;
	char *path;

	path = export_SHP_path_utf8_to_native (pszShapeFile);
	h = SHPCreate ( (const char*)path, nShapeType );
	if ( path != NULL ) {
		free ( path );
	}

	return ( h );
}


/*
 * Cross-platform helper function for DBase file creation:
 * 
 * This function just makes sure that file paths are translated
 * to the operating system's multi-byte representation before
 * shapelib's own DBFCreate() is invoked on the path.
 * 
 */
DBFHandle export_SHP_DBFCreate ( const char* pszDBFFile )
{	
	DBFHandle h;
	char *path;


	path = export_SHP_path_utf8_to_native (pszDBFFile);
	h = DBFCreate ( (const char*)path );
	if ( path != NULL ) {
		free ( path );
	}

	return ( h );
}


/*
 * Creates the attribute data ("properties") for a GeoJSON feature
 * and writes it to the file pointed to by argument 'ft'.
 * The file must be open and have write permissions.
 *
 * "pk" is the primary key, i.e. an integer index from 0 .. n, which
 * points directly into the points(_raw)[], lines[], or polygons[]
 * array of the geometry store "gs".
 * "geom_id" is a unique ID for each row of attribute table output.
 * This is required because the geom IDs in the geometry store "gs" use
 * separate IDs, but we want to have only one ID in the text output file.
 *
 * The return value is the total number of attribute data errors.
 *
 * TODO: GeoJSON allows only one "properties" member per feature.
 *       Therefore, we can only store one set of label properties.
 *       Currently, "part_id" is always as "0".
 */
int export_GeoJSON_write_properties ( FILE *ft, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int geom_id, int part_id,
		parser_desc *parser, options *opts )
{
	unsigned int err_count;
	char *NULL_str = NULL;
	BOOLEAN store_null;
	BOOLEAN c_error;
	BOOLEAN c_overflow;
	char *input;
	unsigned int line;
	char **atts;
	int i;


	if ( gs == NULL || parser == NULL || opts == NULL ) {
		return ( 1 );
	}

	if ( 	GEOM_TYPE != GEOM_TYPE_POINT && GEOM_TYPE != GEOM_TYPE_POINT_RAW &&
			GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nGeometry type unknown.\nNo attribute data written.") );
		return ( 1 );
	}

	/* string representation of user-defined NULL value */
	if ( parser->empty_val_set == TRUE ) {
		NULL_str = malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
		snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
	}

	/* get name of input source */
	input = NULL;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		input = strdup (gs->points[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		input = strdup (gs->points_raw[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		input = strdup (gs->lines[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		input = strdup (gs->polygons[pk].source);
	if ( input == NULL )  {
		input = strdup ("<NULL>");
	} else {
		if ( !strcmp ( "-", input ) ) {
			free ( input );
			input = strdup ("<console input stream>");
		}
	}

	/* get line of input file from which this geom was read */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		line = gs->points[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		line = gs->points_raw[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		line = gs->lines[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		line = gs->polygons[pk].line;

	/* get pointer to attribute field contents (strings) */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		atts = gs->points[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		atts = gs->points_raw[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		atts = gs->lines[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		atts = gs->polygons[pk].atts;

	/* reset error count to "0" */
	err_count = 0;

	fprintf ( ft, "      \"properties\": {\n" );

	/* first property is always the geom ID */
	fprintf ( ft, "        \"geom_id\": %u", geom_id - 1);

	/* handle label properties */
	BOOLEAN has_label = FALSE;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		has_label = gs->points[pk].has_label;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		has_label = gs->points_raw[pk].has_label;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		has_label = gs->lines[pk].parts[part_id].has_label;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		has_label = gs->polygons[pk].parts[part_id].has_label;

	if ( has_label == TRUE ) {
		double label_x = 0.0;
		double label_y = 0.0;

		if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
			label_x = gs->points[pk].label_x;
			label_y = gs->points[pk].label_y;
		}
		if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW ) {
			label_x = gs->points_raw[pk].label_x;
			label_y = gs->points_raw[pk].label_y;
		}
		if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
			/* TODO: We are only transferring the label data for the first geometry part at the moment! */
			label_x = gs->lines[pk].parts[part_id].label_x;
			label_y = gs->lines[pk].parts[part_id].label_y;
		}
		if ( GEOM_TYPE == GEOM_TYPE_POLY ) {
			/* TODO: We are only transferring the label data for the first geometry part at the moment! */
			label_x = gs->polygons[pk].parts[part_id].label_x;
			label_y = gs->polygons[pk].parts[part_id].label_y;
		}

		fprintf ( ft, "," );
		fprintf ( ft, "\n" );
		char *label_x_str = export_float_to_str ( label_x );
		char *label_y_str = export_float_to_str ( label_y );
		fprintf ( ft, "        \"%s\": %s,\n", LBL_FIELD_NAME_X, label_x_str );
		fprintf ( ft, "        \"%s\": %s,\n", LBL_FIELD_NAME_Y, label_y_str );
		free (label_x_str);
		free (label_y_str);
		/* get label field and its contents */
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( !strcasecmp (parser->fields[i]->name, opts->label_field)  ) {
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( atts[i] == NULL ) {
					store_null = TRUE;
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write NULL value as empty string */
						fprintf ( ft, "        \"%s\": \"\",\n", LBL_FIELD_NAME_TEXT );
					} else {
						/* write user-defined NULL value */
						fprintf ( ft, "        \"%s\": \"%s\",\n", LBL_FIELD_NAME_TEXT, NULL_str);
					}
				} else {
					/* write actual field contents */
					fprintf ( ft, "        \"%s\": \"%s\",\n", LBL_FIELD_NAME_TEXT, atts[i]);
				}
				break;
			}
			i ++;
		}
		/* write default label properties */
		fprintf ( ft, "        \"%s\": \"%s\",\n", LBL_FIELD_NAME_FONT_TYPE, LBL_FIELD_DEFAULT_FONT_TYPE);
		fprintf ( ft, "        \"%s\": %i,\n", LBL_FIELD_NAME_FONT_STYLE, LBL_FIELD_DEFAULT_FONT_STYLE);
		fprintf ( ft, "        \"%s\": %i,\n", LBL_FIELD_NAME_FONT_COLOR, LBL_FIELD_DEFAULT_FONT_COLOR);
		char *lbl_field_default_str = export_float_to_str ( LBL_FIELD_DEFAULT_FONT_SIZE );
		fprintf ( ft, "        \"%s\": %s,\n", LBL_FIELD_NAME_FONT_SIZE, lbl_field_default_str );
		free ( lbl_field_default_str );
		char *lbl_field_font_str = export_float_to_str ( LBL_FIELD_DEFAULT_FONT_ROTATE );
		fprintf ( ft, "        \"%s\": %s", LBL_FIELD_NAME_FONT_ROTATE, lbl_field_font_str );
		free ( lbl_field_font_str );
	}

	int num_properties = 0;
	while ( parser->fields[num_properties] != NULL ) {
		num_properties ++;
	}
	if ( num_properties > 0 ) {
		fprintf ( ft, "," );
		fprintf ( ft, "\n" );
		for ( i = 0; i < num_properties; i ++ ) {
			if ( parser->fields[i]->skip == FALSE ) {
				if ( !strcasecmp (parser->fields[i]->name,"id") ) {
					/* "id" is reserved for primary key in GeoJSON! */
					fprintf ( ft, "        \"_%s\": ", parser->fields[i]->name );
				} else {
					fprintf ( ft, "        \"%s\": ", parser->fields[i]->name );
				}
				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
					/*** TEXT ***/
					if ( atts[i] == NULL ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default NULL value */
							fprintf (ft, "\"\"");
						} else {
							/* write user-defined NULL value */
							fprintf (ft, "\"%s\"", NULL_str);
						}
					} else {
						/* write actual field contents */
						fprintf (ft, "\"%s\"", atts[i]);
					}
				}
				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_INT ) {
					/*** INTEGER ***/
					int i_value = t_str_to_int ( atts[i], &c_error, &c_overflow );
					store_null = FALSE;
					/* check for all conditions under which NULL data will be written */
					if ( c_error == TRUE && c_overflow == FALSE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i\nValue for attribute '%s' is not a valid integer number\nNULL data written instead."),
								input, line, parser->fields[i]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( c_overflow == TRUE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
									input, line, parser->fields[i]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[i] == NULL ) {
								store_null = TRUE;
							}
						}
					}
					if ( store_null == TRUE ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default integer NULL value */
							fprintf (ft, "0");
						} else {
							/* write user-defined NULL value */
							fprintf (ft, "%i", parser->empty_val);
						}
					} else {
						/* write actual attribute value */
						fprintf (ft, "%i", i_value);
					}
				}

				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_DOUBLE ) {
					/*** DOUBLE ***/
					double d_value = t_str_to_dbl ( atts[i], opts->decimal_point[0], opts->decimal_group[0], &c_error, &c_overflow );
					store_null = FALSE;
					/* check for all conditions under which NULL data will be written */
					if ( c_error == TRUE && c_overflow == FALSE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is not a valid number\nNULL data written instead."),
								input, line, parser->fields[i]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( c_overflow == TRUE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
									input, line, parser->fields[i]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[i] == NULL ) {
								store_null = TRUE;
							}
						}
					}
					if ( store_null == TRUE ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default double NULL value */
							fprintf (ft, "0.0");
						} else {
							/* write user-defined NULL value */
							fprintf (ft, "%i.0", parser->empty_val);
						}
					} else {
						/* write actual attribute value */
						if ( d_value == 0 ) {
							fprintf (ft, "0.0");
						} else {
							char *d_str = export_float_to_str ( d_value );
							fprintf (ft, "%s", d_str);
							free (d_str);
						}
					}
				}

				if ( i < (num_properties-1) ) {
					fprintf ( ft, "," );
				}
				fprintf ( ft, "\n" );
			}
		}
	} else {
		fprintf ( ft, "\n" );
	}

	if ( NULL_str != NULL ) {
		free ( NULL_str );
	}

	return (err_count);
}


/*
 * Writes geometry store objects into a new, single GeoJSON object file
 * with several layers, one for each geometry type, if required.
 *
 * By the time this function is called, "gs" must be a valid geometry
 * store and must contain only fully checked and built, valid geometries,
 * ready to be written to disk.
 *
 * If "force 2D" has been set, then all Z coordinate values will be "0.0".
 *
 * Returns number of attribute field errors.
 * */
int export_GeoJSON ( geom_store *gs, parser_desc *parser, options *opts )
{
	int num_errors = 0;
	int num_points = 0;
	int num_points_raw = 0;
	int num_lines = 0;
	FILE *fp = NULL;
	BOOLEAN exist_id = FALSE;
	BOOLEAN exist_id_renamed = FALSE;


	/* Check if there is anything to export! */
	if ( gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons < 1 )  {
		err_show (ERR_NOTE,"");
		err_show (ERR_WARN, _("\nNo valid geometries found. No output produced."));
		return ( 0 );
	}

	/* Check that we don't have issues with an existing "id" field! */
	exist_id = FALSE;
	exist_id_renamed = FALSE;
	int i = 0;
	while ( parser->fields[i] != NULL ) {
		if ( parser->fields[i]->skip == FALSE ) {
			if ( !strcasecmp (parser->fields[i]->name,"id") ) {
				exist_id = TRUE;
			}
			if ( !strcasecmp (parser->fields[i]->name,"_id") ) {
				exist_id_renamed = TRUE;
			}
		}
		i ++;
	}
	if ( exist_id == TRUE && exist_id_renamed == TRUE ) {
		err_show (ERR_NOTE,"");
		err_show (ERR_EXIT, _("\nBoth fields 'id' and '_id' exist in input data. Cannot create GeoJSON primary key."));
	}

	/* Get total number of (selected) features for each type:
	 * Important for correct comma separation of records. */
	num_points = 0;
	for ( i = 0; i < gs->num_points; i ++ ) {
		if ( gs->points[i].is_selected == TRUE ) {
			num_points ++;
		}
	}
	num_points_raw = 0;
	for ( i = 0; i < gs->num_points_raw; i ++ ) {
		if ( gs->points_raw[i].is_selected == TRUE ) {
			num_points_raw ++;
		}
	}
	num_lines = 0;
	for ( i = 0; i < gs->num_lines; i ++ ) {
		if ( gs->lines[i].is_selected == TRUE ) {
			num_lines ++;
		}
	}

	/* Attempt to create output file. */
	fp = t_fopen_utf8 (gs->path_all, "w+");
	if ( fp != NULL )
	{
		int geom_id = 1;
		double X,Y,Z;
		char *xf,*yf,*zf;
		int i, j, k, l, m;

		/* write header to output file */
		fprintf ( fp, "{ \"type\": \"FeatureCollection\",\n" );
		fprintf ( fp, "  \"features\": [\n" );

		/* POLYGONS
		 *
		 * Polygons in GeoJSON are quite tricky, because for each part we need
		 * store the boundaries/rings in a well-defined order: first the outer
		 * boundary, then all holes.
		 */
		for (i = 0; i < gs->num_polygons; i ++ ) {
			if ( gs->polygons[i].is_selected == TRUE ) {
				/* Deciding between GeoJSON geometry types is also a little trickier:
				 * A polygon is a MultiPolygon if it has at least two parts that are
				 * _not_ holes. Otherwise it's a simple Polygon. */
				BOOLEAN is_multi_part = FALSE;
				int num_non_holes = 0;
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					if ( gs->polygons[i].parts[j].is_hole == FALSE ) {
						num_non_holes ++;
					}
				}
				if ( num_non_holes > 1 ) {
					is_multi_part = TRUE;
				}

				fprintf ( fp, "    { \"type\": \"Feature\", \"id\": %i,\n", (geom_id)-1 );
				fprintf ( fp, "      \"geometry\": {\n" );
				if ( is_multi_part ) {
					fprintf	( fp, "        \"type\": \"MultiPolygon\",\n" );
					fprintf	( fp, "        \"coordinates\": [\n" );
				} else {
					fprintf	( fp, "        \"type\": \"Polygon\",\n" );
					fprintf	( fp, "        \"coordinates\": [\n" );
				}
				BOOLEAN hole_added = FALSE;
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					/* Iterate over all polygon parts that are _not_ holes */
					if ( gs->polygons[i].parts[j].is_hole == FALSE ) {
						hole_added = FALSE;
						if ( is_multi_part ) {
							fprintf	( fp, "          [\n" );
							fprintf	( fp, "            [\n" );
						} else {
							fprintf	( fp, "          [\n" );
						}
						for ( k = 0; k < gs->polygons[i].parts[j].num_vertices; k++ ) {
							X = gs->polygons[i].parts[j].X[k];
							Y = gs->polygons[i].parts[j].Y[k];
							xf = export_float_to_str(X);
							yf = export_float_to_str(Y);
							if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
								Z = gs->polygons[i].parts[j].Z[k];
							} else {
								Z = 0.0;
							}
							zf = export_float_to_str(Z);

							if ( is_multi_part ) {
								fprintf	( fp, "              [%s, %s, %s]", xf, yf, zf );
							} else {
								fprintf	( fp, "             [%s, %s, %s]", xf, yf, zf );
							}
							if ( k < (gs->polygons[i].parts[j].num_vertices-1) ) {
								fprintf	( fp, "," );
							}
							fprintf	( fp, "\n" );
							free ( xf );
							free ( yf );
							free ( zf );
						}
						if ( is_multi_part ) {
							fprintf	( fp, "            ]" );
						} else {
							fprintf	( fp, "          ]" );
						}
						/* Iterate over all holes to see which ones are in this part. */
						for ( k = 0; k < gs->polygons[i].num_parts; k++ ) {
							if ( gs->polygons[i].parts[k].is_hole == TRUE ) {
								geom_part *A = &gs->polygons[i].parts[k];
								geom_part *B = &gs->polygons[i].parts[j];
								if ( geom_tools_part_in_part_2D ( A, B ) == TRUE ) {
									BOOLEAN lies_inside_hole = FALSE;
									/* Eliminate holes that lie within another hole. */
									for ( l = 0; l < gs->num_polygons && lies_inside_hole == FALSE; l++ ) {
										for ( m = 0; m < gs->polygons[l].num_parts && lies_inside_hole == FALSE; m++ ) {
											if ( gs->polygons[l].parts[m].is_hole == TRUE ) {
												B = &gs->polygons[l].parts[m];
												if ( geom_tools_part_in_part_2D ( A, B ) == TRUE ) {
													lies_inside_hole = TRUE;
												}
											}
										}
									}
									if ( lies_inside_hole == FALSE ) {
										/* Add hole to current part. */
										fprintf	( fp, ", [\n" );
										for ( l = 0; l < gs->polygons[i].parts[k].num_vertices; l++ ) {
											X = gs->polygons[i].parts[k].X[l];
											Y = gs->polygons[i].parts[k].Y[l];
											xf = export_float_to_str(X);
											yf = export_float_to_str(Y);
											if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
												Z = gs->polygons[i].parts[k].Z[l];
											} else {
												Z = 0.0;
											}
											zf = export_float_to_str(Z);

											if ( is_multi_part ) {
												fprintf	( fp, "             [%s, %s, %s]", xf, yf, zf );
											} else {
												fprintf	( fp, "             [%s, %s, %s]", xf, yf, zf );
											}

											if ( l < (gs->polygons[i].parts[k].num_vertices-1) ) {
												fprintf	( fp, "," );
											}
											fprintf	( fp, "\n" );
											free ( xf );
											free ( yf );
											free ( zf );
										}
										hole_added = TRUE; /* for line break in output file */
										if ( is_multi_part ) {
											fprintf	( fp, "            ]" );
										} else {
											fprintf	( fp, "          ]" );
										}
									}
								}
							}
						}
					} /* DONE (adding all holes to this part */
					if ( hole_added == FALSE || j == gs->polygons[i].num_parts-1 ) {
						fprintf	( fp, "\n" );
						if ( is_multi_part == TRUE ) {
							if ( j < gs->polygons[i].num_parts-1 ) {
								fprintf	( fp, "          ],\n" );
							} else {
								fprintf	( fp, "          ]\n" );
							}
						}
					}
				} /* DONE (adding all parts) */
				fprintf	( fp, "        ]\n" );
				fprintf ( fp, "      },\n" );
				/* write attributes (=properties) */
				int att_errors =
						export_GeoJSON_write_properties ( fp, gs, GEOM_TYPE_POLY,
								i, geom_id, 0, parser, opts );
				fprintf ( fp, "      }\n" );
				fprintf ( fp, "    }" );
				if ( i < gs->num_polygons-1 ) {
					fprintf ( fp, "," );
				} else {
					if ( num_lines > 0 || num_points > 0 ) {
						fprintf ( fp, "," );
					}
				}
				fprintf ( fp, "\n" );
				num_errors += att_errors;
				geom_id ++;
			}
		}

		/* LINES */
		for (i = 0; i < gs->num_lines; i ++ ) {
			if ( gs->lines[i].is_selected == TRUE ) {
				BOOLEAN is_multi_part = FALSE;
				if ( gs->lines[i].num_parts > 1 ) {
					is_multi_part = TRUE;
				}
				fprintf ( fp, "    { \"type\": \"Feature\", \"id\": %i,\n", (geom_id)-1 );
				fprintf ( fp, "      \"geometry\": {\n" );
				if ( is_multi_part == TRUE ) {
					fprintf	( fp, "        \"type\": \"MultiLineString\",\n" );
					fprintf	( fp, "        \"coordinates\": [\n" );
				} else {
					fprintf	( fp, "        \"type\": \"LineString\",\n" );
					fprintf	( fp, "        \"coordinates\":\n" );
				}
				for ( j = 0; j < gs->lines[i].num_parts; j++ ) {
					if ( is_multi_part == TRUE ) {
						fprintf	( fp, "          [\n" );
					} else {
						fprintf	( fp, "        [\n" );
					}
					for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k++ ) {
						X = gs->lines[i].parts[j].X[k];
						Y = gs->lines[i].parts[j].Y[k];
						xf = export_float_to_str(X);
						yf = export_float_to_str(Y);
						if ( gs->lines[i].is_3D == TRUE && opts->force_2d == FALSE ) {
							Z = gs->lines[i].parts[j].Z[k];
						} else {
							Z = 0.0;
						}
						zf = export_float_to_str(Z);
						if ( is_multi_part == TRUE ) {
							fprintf	( fp, "             [%s, %s, %s]", xf, yf, zf );
						} else {
							fprintf	( fp, "           [%s, %s, %s]", xf, yf, zf );
						}
						if ( k < (gs->lines[i].parts[j].num_vertices-1) ) {
							fprintf	( fp, "," );
						}
						fprintf	( fp, "\n" );
						free ( xf );
						free ( yf );
						free ( zf );
					}
					if ( is_multi_part == TRUE ) {
						if ( j < gs->lines[i].num_parts-1 ) {
							fprintf	( fp, "          ],\n" );
						} else {
							fprintf	( fp, "          ]\n" );
						}
					}
				}
				/* DONE (adding all parts) */
				fprintf	( fp, "        ]\n" );
				fprintf ( fp, "      },\n" );
				/* write attributes (=properties) */
				int att_errors =
						export_GeoJSON_write_properties ( fp, gs, GEOM_TYPE_LINE,
								i, geom_id, 0, parser, opts );
				fprintf ( fp, "      }\n" );
				fprintf ( fp, "    }" );
				if ( i < ( gs->num_lines - 1 ) ) {
					fprintf ( fp, "," );
				}  else {
					if ( num_points > 0 ) {
						fprintf ( fp, "," );
					}
				}
				fprintf ( fp, "\n" );
				num_errors += att_errors;
				geom_id ++;
			}
		}

		/* POINTS */
		for (i = 0; i < gs->num_points; i ++ ) {
			if ( gs->points[i].is_selected == TRUE ) {
				X = gs->points[i].X;
				Y = gs->points[i].Y;
				xf = export_float_to_str(X);
				yf = export_float_to_str(Y);
				if ( gs->points[i].is_3D == TRUE && opts->force_2d == FALSE ) {
					Z = gs->points[i].Z;
				} else {
					Z = 0.0;
				}
				zf = export_float_to_str(Z);
				/* write point geometry */
				fprintf ( fp, "    { \"type\": \"Feature\", \"id\": %i,\n", (geom_id)-1 );
				fprintf ( fp, "      \"geometry\": {\n" );
				fprintf	( fp, "        \"type\": \"Point\",\n" );
				fprintf	( fp, "        \"coordinates\": [%s, %s, %s]\n", xf, yf, zf );
				fprintf ( fp, "      },\n" );
				free ( xf );
				free ( yf );
				free ( zf );
				/* write attributes (=properties) */
				int att_errors =
						export_GeoJSON_write_properties ( fp, gs, GEOM_TYPE_POINT,
								i, geom_id, 0, parser, opts );
				fprintf ( fp, "      }\n" );
				fprintf ( fp, "    }" );
				if ( i < ( gs->num_points - 1 ) ) {
					fprintf ( fp, "," );
				}  else {
					if ( num_points_raw > 0 ) {
						fprintf ( fp, "," );
					}
				}
				fprintf ( fp, "\n" );
				num_errors += att_errors;
				geom_id ++;
			}
		}

		/* RAW VERTICES */
		for (i = 0; i < gs->num_points_raw; i ++ ) {
			if ( gs->points_raw[i].is_selected == TRUE ) {
				X = gs->points_raw[i].X;
				Y = gs->points_raw[i].Y;
				xf = export_float_to_str(X);
				yf = export_float_to_str(Y);
				if ( gs->points_raw[i].is_3D == TRUE && opts->force_2d == FALSE ) {
					Z = gs->points_raw[i].Z;
				} else {
					Z = 0.0;
				}
				zf = export_float_to_str(Z);
				/* write point geometry */
				fprintf ( fp, "    { \"type\": \"Feature\", \"id\": %i,\n", (geom_id)-1 );
				fprintf ( fp, "      \"geometry\": {\n" );
				fprintf	( fp, "        \"type\": \"Point\",\n" );
				fprintf	( fp, "        \"coordinates\": [\n" );
				fprintf	( fp, "           %s, %s, %s\n", xf, yf, zf );
				fprintf	( fp, "        ]\n" );
				fprintf ( fp, "      },\n" );
				free ( xf );
				free ( yf );
				free ( zf );
				/* write attributes (=properties) */
				int att_errors =
						export_GeoJSON_write_properties ( fp, gs, GEOM_TYPE_POINT_RAW,
								i, geom_id, 0, parser, opts );
				fprintf ( fp, "      }\n" );
				fprintf ( fp, "    }" );
				if ( i < ( gs->num_points_raw - 1 ) ) {
					fprintf ( fp, "," );
				}
				fprintf ( fp, "\n" );
				num_errors += att_errors;
				geom_id ++;
			}
		}
		/* write footer to output file */
		fprintf ( fp, "  ]\n" );
		fprintf ( fp, "}\n" );
	} else {
		return ( 0 );
	}

	/* Close output file. */
	if ( fp != NULL ) {
		fclose (fp);
	}

	return ( num_errors );
}


/*
 * Creates content for the <description> member of KML Placemark.
 */
void export_KML_write_description ( FILE *ft, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int geom_id, int part_id,
		parser_desc *parser, options *opts )
{
	if ( gs == NULL || parser == NULL || opts == NULL ) {
		return;
	}

	fprintf ( ft, "        <description>\n" );
	fprintf ( ft, "          <![CDATA[\n" );
	fprintf ( ft, "            Generated by %s.<p>\n", t_get_prg_name_and_version());
	fprintf ( ft, "          ]]>\n" );
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW ) {
		fprintf ( ft, "          <![CDATA[" );
		fprintf ( ft, "            Source:<b> %s</b><br>\n", gs->points_raw[pk].source );
		fprintf ( ft, "            Line:<b> %u</b><br>\n", gs->points_raw[pk].line );
		fprintf ( ft, "            True 3D:<b> %i</b><br>\n", gs->points_raw[pk].is_3D );
		fprintf ( ft, "            X:<b> %.6f</b><br>\n", gs->points_raw[pk].X );
		fprintf ( ft, "            Y:<b> %.6f</b><br>\n", gs->points_raw[pk].Y );
		fprintf ( ft, "            Z:<b> %.6f</b>\n", gs->points_raw[pk].Z );
		fprintf ( ft, "          ]]>\n" );
	}

	if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
		fprintf ( ft, "          <![CDATA[" );
		fprintf ( ft, "            Source:<b> %s</b><br>\n", gs->points[pk].source );
		fprintf ( ft, "            Line:<b> %u</b><br>\n", gs->points[pk].line );
		fprintf ( ft, "            True 3D:<b> %i</b><br>\n", gs->points[pk].is_3D );
		fprintf ( ft, "            X:<b> %.6f</b><br>\n", gs->points[pk].X );
		fprintf ( ft, "            Y:<b> %.6f</b><br>\n", gs->points[pk].Y );
		fprintf ( ft, "            Z:<b> %.6f</b>\n", gs->points[pk].Z );
		fprintf ( ft, "          ]]>\n" );
	}

	if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
		fprintf ( ft, "          <![CDATA[" );
		fprintf ( ft, "            Source:<b> %s</b><br>\n", gs->lines[pk].source );
		fprintf ( ft, "            Line:<b> %u</b><br>\n", gs->lines[pk].line );
		fprintf ( ft, "            True 3D:<b> %i</b><br>\n", gs->lines[pk].is_3D );
		fprintf ( ft, "            Parts:<b> %u</b>\n", gs->lines[pk].num_parts );
		fprintf ( ft, "          ]]>\n" );
	}

	if ( GEOM_TYPE == GEOM_TYPE_POLY ) {
		fprintf ( ft, "          <![CDATA[" );
		fprintf ( ft, "            Source:<b> %s</b><br>\n", gs->polygons[pk].source );
		fprintf ( ft, "            Line:<b> %u</b><br>\n", gs->polygons[pk].line );
		fprintf ( ft, "            True 3D:<b> %i</b><br>\n", gs->polygons[pk].is_3D );
		int num_parts = 0;
		int num_holes = 0;
		int i;
		for ( i = 0; i < gs->polygons[pk].num_parts; i++ ) {
			if ( gs->polygons[pk].parts[i].is_hole == TRUE ) {
				num_holes ++;
			} else {
				num_parts ++;
			}
		}
		fprintf ( ft, "            Parts:<b> %i</b><br>\n", num_parts );
		fprintf ( ft, "            Holes:<b> %i</b>\n", num_holes );
		fprintf ( ft, "          ]]>\n" );
	}
	fprintf ( ft, "        </description>\n" );
}


/*
 * Creates the attribute data ("data") for a KML feature
 * and writes it to the file pointed to by argument 'ft'.
 * The file must be open and have write permissions.
 *
 * "pk" is the primary key, i.e. an integer index from 0 .. n, which
 * points directly into the points(_raw)[], lines[], or polygons[]
 * array of the geometry store "gs".
 * "geom_id" is a unique ID for each row of attribute table output.
 * This is required because the geom IDs in the geometry store "gs" use
 * separate IDs, but we want to have only one ID in the text output file.
 *
 * The return value is the total number of attribute data errors.
 */
int export_KML_write_data ( FILE *ft, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int geom_id, int part_id,
		parser_desc *parser, options *opts )
{
	unsigned int err_count;
	char *NULL_str = NULL;
	BOOLEAN c_error;
	BOOLEAN c_overflow;
	char *input;
	unsigned int line;
	char **atts;

	if ( gs == NULL || parser == NULL || opts == NULL ) {
		return ( 1 );
	}

	if ( 	GEOM_TYPE != GEOM_TYPE_POINT && GEOM_TYPE != GEOM_TYPE_POINT_RAW &&
			GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nGeometry type unknown.\nNo attribute data written.") );
		return ( 1 );
	}

	/* string representation of user-defined NULL value */
	if ( parser->empty_val_set == TRUE ) {
		NULL_str = malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
		snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
	}

	/* get name of input source */
	input = NULL;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		input = strdup (gs->points[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		input = strdup (gs->points_raw[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		input = strdup (gs->lines[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		input = strdup (gs->polygons[pk].source);
	if ( input == NULL )  {
		input = strdup ("<NULL>");
	} else {
		if ( !strcmp ( "-", input ) ) {
			free ( input );
			input = strdup ("<console input stream>");
		}
	}

	/* get line of input file from which this geom was read */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		line = gs->points[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		line = gs->points_raw[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		line = gs->lines[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		line = gs->polygons[pk].line;

	/* get pointer to attribute field contents (strings) */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		atts = gs->points[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		atts = gs->points_raw[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		atts = gs->lines[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		atts = gs->polygons[pk].atts;

	/* reset error count to "0" */
	err_count = 0;

	fprintf ( ft, "        <ExtendedData>\n" );
	fprintf ( ft, "          <SchemaData schemaUrl=\"#attributeTypeId\">\n" );

	/* first property is always the geom ID */
	fprintf ( ft, "            <SimpleData name=\"%s\">%u</SimpleData>\n", "geom_id", geom_id - 1 );

	int num_properties = 0;
	while ( parser->fields[num_properties] != NULL ) {
		num_properties ++;
	}
	if ( num_properties > 0 ) {
		int i;
		for ( i = 0; i < num_properties; i ++ ) {
			if ( parser->fields[i]->skip == FALSE ) {
				char field_name[PRG_MAX_FIELD_LEN+1];
				snprintf ( field_name, PRG_MAX_FIELD_LEN+1, parser->fields[i]->name);

				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
					/*** TEXT ***/
					if ( atts[i] == NULL ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">\"\"</SimpleData>\n", field_name );
						} else {
							/* write user-defined NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">\"%s\"</SimpleData>\n", field_name, NULL_str );
						}
					} else {
						/* write actual field contents */
						fprintf ( ft, "            <SimpleData name=\"%s\">\"%s\"</SimpleData>\n", field_name, atts[i] );
					}
				}
				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_INT ) {
					/*** INTEGER ***/
					int i_value = t_str_to_int ( atts[i], &c_error, &c_overflow );
					BOOLEAN store_null = FALSE;
					/* check for all conditions under which NULL data will be written */
					if ( c_error == TRUE && c_overflow == FALSE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i\nValue for attribute '%s' is not a valid integer number\nNULL data written instead."),
								input, line, parser->fields[i]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( c_overflow == TRUE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
									input, line, parser->fields[i]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[i] == NULL ) {
								store_null = TRUE;
							}
						}
					}
					if ( store_null == TRUE ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default integer NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">0</SimpleData>\n", field_name );
						} else {
							/* write user-defined NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">%i</SimpleData>\n", field_name, parser->empty_val );
						}
					} else {
						/* write actual attribute value */
						fprintf ( ft, "            <SimpleData name=\"%s\">%i</SimpleData>\n", field_name, i_value );
					}
				}

				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_DOUBLE ) {
					/*** DOUBLE ***/
					double d_value = t_str_to_dbl ( atts[i], opts->decimal_point[0], opts->decimal_group[0], &c_error, &c_overflow );
					BOOLEAN store_null = FALSE;
					/* check for all conditions under which NULL data will be written */
					if ( c_error == TRUE && c_overflow == FALSE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is not a valid number\nNULL data written instead."),
								input, line, parser->fields[i]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( c_overflow == TRUE ) {
							err_show ( ERR_NOTE, "" );
							err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
									input, line, parser->fields[i]->name);
							store_null = TRUE;
							err_count ++;
						} else {
							if ( atts[i] == NULL ) {
								store_null = TRUE;
							}
						}
					}
					if ( store_null == TRUE ) {
						/* store "no data" representation */
						if ( parser->empty_val_set == FALSE ) {
							/* write default double NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">0.0</SimpleData>\n", field_name );
						} else {
							/* write user-defined NULL value */
							fprintf ( ft, "            <SimpleData name=\"%s\">%i.0</SimpleData>\n", field_name, parser->empty_val );
						}
					} else {
						/* write actual attribute value */
						if ( d_value == 0 ) {
							fprintf ( ft, "            <SimpleData name=\"%s\">0.0</SimpleData>\n", field_name );
						} else {
							char *d_str = export_float_to_str ( d_value );
							fprintf ( ft, "            <SimpleData name=\"%s\">%s</SimpleData>\n", field_name, d_str );
							free ( d_str );
						}
					}
				}
			}
		}
		fprintf ( ft, "          </SchemaData>\n" );
		fprintf ( ft, "        </ExtendedData>\n" );
	}

	if ( NULL_str != NULL ) {
		free ( NULL_str );
	}

	return (err_count);
}


/*
 * Writes geometry store objects into a new, single KML XML file
 * with several layers, one for each geometry type, if required.
 *
 * By the time this function is called, "gs" must be a valid geometry
 * store and must contain only fully checked and built, valid geometries,
 * ready to be written to disk.
 *
 * If "force 2D" has been set, then all Z coordinate values will be "0.0".
 *
 * Returns number of attribute field errors.
 * */
int export_KML ( geom_store *gs, parser_desc *parser, options *opts )
{
	int num_errors = 0;
	int num_points = 0;
	int num_points_raw = 0;
	int num_lines = 0;
	int num_polygons = 0;

	FILE *fp = NULL;


	/* Check if there is anything to export! */
	if ( gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons < 1 )  {
		err_show (ERR_NOTE,"");
		err_show (ERR_WARN, _("\nNo valid geometries found. No output produced."));
		return ( 0 );
	}

	/* Get total number of (selected) features for each type:
	 * Important for KML folder structure. */
	num_points = 0;
	int i;
	for ( i = 0; i < gs->num_points; i ++ ) {
		if ( gs->points[i].is_selected == TRUE ) {
			num_points ++;
		}
	}
	num_points_raw = 0;
	for ( i = 0; i < gs->num_points_raw; i ++ ) {
		if ( gs->points_raw[i].is_selected == TRUE ) {
			num_points_raw ++;
		}
	}
	num_lines = 0;
	for ( i = 0; i < gs->num_lines; i ++ ) {
		if ( gs->lines[i].is_selected == TRUE ) {
			num_lines ++;
		}
	}
	num_polygons = 0;
	for ( i = 0; i < gs->num_polygons; i ++ ) {
		if ( gs->polygons[i].is_selected == TRUE ) {
			num_polygons ++;
		}
	}

	/* Attempt to create output file. */
	fp = t_fopen_utf8 (gs->path_all, "w+");
	if ( fp != NULL ) {
		int geom_id = 1;
		double X,Y,Z;
		char *xf,*yf,*zf;
		int i, j, k, l, m;

		/* write header to output file */
		fprintf ( fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
		fprintf ( fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n" );
		fprintf ( fp, "  <Document>\n" );

		fprintf ( fp, "    <name>%s_%s.kml (Survey2GIS KML export)</name> \n", opts->base, GEOM_TYPE_NAMES[GEOM_TYPE_ALL] );

		/* build attribute schema */
		fprintf ( fp, "\n    <Schema name=\"attributeType\" id=\"attributeTypeId\">\n" );
		fprintf ( fp, "      <SimpleField type=\"string\" name=\"geom_id\">\n" );
		fprintf ( fp, "        <displayName><![CDATA[%s]]></displayName>\n", "geom_id (uint)" );
		fprintf ( fp, "      </SimpleField>\n" );
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( parser->fields[i]->skip == FALSE ) {
				fprintf ( fp, "      <SimpleField type=\"%s\" name=\"%s\">\n",
						PARSER_FIELD_TYPE_NAMES_KML[parser->fields[i]->type], parser->fields[i]->name );
				fprintf ( fp, "        <displayName><![CDATA[%s (%s)]]></displayName>\n", parser->fields[i]->name,
						PARSER_FIELD_TYPE_NAMES[parser->fields[i]->type]);
				fprintf ( fp, "      </SimpleField>\n" );
			}
			i ++;
		}
		fprintf ( fp, "    </Schema>\n" );

		/* geometry styles
		 * NOTE KML colour coding is quite braindead:
		 *
		 * aabbggrr, where aa=alpha
		 *
		 * with each tuple 00 (min) to ff (max)
		 *
		 */
		/* polygons */
		fprintf ( fp, "\n    <Style id=\"polygon\">\n" );
		fprintf ( fp, "      <LineStyle>\n" );
		fprintf ( fp, "        <width>1.0</width>\n" );
		fprintf ( fp, "        <color>ff595959</color>\n" );
		fprintf ( fp, "      </LineStyle>\n" );
		fprintf ( fp, "      <PolyStyle>\n" );
		fprintf ( fp, "        <color>7d595959</color>\n" );
		fprintf ( fp, "      </PolyStyle>\n" );
		fprintf ( fp, "      <BalloonStyle>\n" );
		fprintf ( fp, "        <text>\n" );
		fprintf ( fp, "          <![CDATA[\n" );
		fprintf ( fp, "            <h2>$[name]</h2>\n" );
		fprintf ( fp, "            <h3>Description</h3>\n" );
		fprintf ( fp, "            $[description]\n" );
		fprintf ( fp, "            <h3>Data</h3>\n" );
		fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n", "geom_id", "geom_id" );
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( parser->fields[i]->skip == FALSE ) {
				fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n",
						parser->fields[i]->name,
						parser->fields[i]->name );
			}
			i ++;
		}
		fprintf ( fp, "          ]]>\n" );
		fprintf ( fp, "        </text>\n" );
		fprintf ( fp, "      </BalloonStyle>\n" );
		fprintf ( fp, "    </Style>\n" );
		/* lines */
		fprintf ( fp, "    <Style id=\"line\">\n" );
		fprintf ( fp, "      <LineStyle>\n" );
		fprintf ( fp, "        <width>1.0</width>\n" );
		fprintf ( fp, "        <color>ffffffff</color>\n" );
		fprintf ( fp, "      </LineStyle>\n" );
		fprintf ( fp, "      <BalloonStyle>\n" );
		fprintf ( fp, "        <text>\n" );
		fprintf ( fp, "          <![CDATA[\n" );
		fprintf ( fp, "            <h2>$[name]</h2>\n" );
		fprintf ( fp, "            <h3>Description</h3>\n" );
		fprintf ( fp, "            $[description]\n" );
		fprintf ( fp, "            <h3>Data</h3>\n" );
		fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n", "geom_id", "geom_id" );
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( parser->fields[i]->skip == FALSE ) {
				fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n",
						parser->fields[i]->name,
						parser->fields[i]->name );
			}
			i ++;
		}
		fprintf ( fp, "          ]]>\n" );
		fprintf ( fp, "        </text>\n" );
		fprintf ( fp, "      </BalloonStyle>\n" );
		fprintf ( fp, "    </Style>\n" );
		/* points */
		fprintf ( fp, "    <Style id=\"point\">\n" );
		fprintf ( fp, "      <LabelStyle>\n" );
		fprintf ( fp, "        <scale>0.0</scale>\n" );
		fprintf ( fp, "      </LabelStyle>\n" );
		fprintf ( fp, "      <IconStyle>\n" );
		fprintf ( fp, "        <scale>0.75</scale>\n" );
		fprintf ( fp, "        <Icon>\n" );
		fprintf ( fp, "          <href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href>\n" );
		fprintf ( fp, "        </Icon>\n" );
		fprintf ( fp, "      </IconStyle>\n" );
		fprintf ( fp, "      <BalloonStyle>\n" );
		fprintf ( fp, "        <text>\n" );
		fprintf ( fp, "          <![CDATA[\n" );
		fprintf ( fp, "            <h2>$[name]</h2>\n" );
		fprintf ( fp, "            <h3>Description</h3>\n" );
		fprintf ( fp, "            $[description]\n" );
		fprintf ( fp, "            <h3>Data</h3>\n" );
		fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n", "geom_id", "geom_id" );
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( parser->fields[i]->skip == FALSE ) {
				fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n",
						parser->fields[i]->name,
						parser->fields[i]->name );
			}
			i ++;
		}
		fprintf ( fp, "          ]]>\n" );
		fprintf ( fp, "        </text>\n" );
		fprintf ( fp, "      </BalloonStyle>\n" );
		fprintf ( fp, "    </Style>\n" );

		/* vertices */
		fprintf ( fp, "    <Style id=\"vertex\">\n" );
		fprintf ( fp, "      <LabelStyle>\n" );
		fprintf ( fp, "        <scale>0.5</scale>\n" );
		fprintf ( fp, "      </LabelStyle>\n" );
		fprintf ( fp, "      <IconStyle>\n" );
		fprintf ( fp, "        <Icon>\n" );
		fprintf ( fp, "          <href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle_highlight.png</href>\n" );
		fprintf ( fp, "        </Icon>\n" );
		fprintf ( fp, "        <scale>0.5</scale>\n" );
		fprintf ( fp, "      </IconStyle>\n" );
		fprintf ( fp, "      <BalloonStyle>\n" );
		fprintf ( fp, "        <text>\n" );
		fprintf ( fp, "          <![CDATA[\n" );
		fprintf ( fp, "            <h2>Vertex $[name]</h2>\n" );
		fprintf ( fp, "            <h3>Description</h3>\n" );
		fprintf ( fp, "            $[description]\n" );
		fprintf ( fp, "            <h3>Data</h3>\n" );
		fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n", "geom_id", "geom_id" );
		i = 0;
		while ( parser->fields[i] != NULL ) {
			if ( parser->fields[i]->skip == FALSE ) {
				fprintf ( fp, "            $[attributeType/%s/displayName]:<b> $[attributeType/%s]</b><br/>\n",
						parser->fields[i]->name,
						parser->fields[i]->name );
			}
			i ++;
		}
		fprintf ( fp, "          ]]>\n" );
		fprintf ( fp, "        </text>\n" );
		fprintf ( fp, "      </BalloonStyle>\n" );
		fprintf ( fp, "    </Style>\n" );
		/* labels */
		fprintf ( fp, "    <Style id=\"label\">\n" );
		fprintf ( fp, "      <LabelStyle>\n" );
		fprintf ( fp, "        <scale>0.75</scale>\n" );
		fprintf ( fp, "        <color>ff6dfffa</color>\n" );
		fprintf ( fp, "      </LabelStyle>\n" );
		fprintf ( fp, "      <IconStyle>\n" );
		fprintf ( fp, "        <scale>0.0</scale>\n" );
		fprintf ( fp, "      </IconStyle>\n" );
		fprintf ( fp, "    </Style>\n" );

		/* POINTS */
		if ( num_points > 0 ) {
			fprintf ( fp, "\n    <Folder>\n" );
			fprintf ( fp, "      <name>Points (%i)</name>\n", num_points );
		}
		for (i = 0; i < gs->num_points; i ++ ) {
			if ( gs->points[i].is_selected == TRUE ) {
				/* start new Point placemark */
				fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
				fprintf ( fp, "        <name>Point %i</name>\n", i+1 );
				fprintf ( fp, "        <styleUrl>#point</styleUrl>\n" );
				/* write kml description member */
				export_KML_write_description ( fp, gs, GEOM_TYPE_POINT, i, geom_id, 0, parser, opts );
				/* attribute data */
				int att_errors = export_KML_write_data ( fp, gs, GEOM_TYPE_POINT, i, geom_id, 0, parser, opts );
				num_errors += att_errors;
				/* DEBUG */
				/*
				X = ( gs->points[i].X - 3513040.0 );
				Y = ( gs->points[i].Y - 5279880.0 );
				X = X / 2.0;
				Y = Y / 2.0;
				 */
				X = gs->points[i].X;
				Y = gs->points[i].Y;
				xf = export_float_to_str(X);
				yf = export_float_to_str(Y);
				if ( gs->points[i].is_3D == TRUE && opts->force_2d == FALSE ) {
					Z = gs->points[i].Z;
				} else {
					Z = 0.0;
				}
				zf = export_float_to_str(Z);
				/* write point geometry */
				fprintf ( fp, "        <Point>\n" );
				fprintf ( fp, "          <coordinates>%s,%s,%s</coordinates>\n", xf, yf, zf );
				fprintf ( fp, "        </Point>\n" );
				free ( xf );
				free ( yf );
				free ( zf );
				fprintf ( fp, "      </Placemark>\n" );
				fprintf ( fp, "\n" );
				geom_id ++;
			}
		}
		if ( num_points > 0 ) {
			fprintf ( fp, "    </Folder>\n" );
		}

		/* LINES */
		if ( num_lines > 0 ) {
			fprintf ( fp, "\n    <Folder>\n" );
			fprintf ( fp, "      <name>Lines (%i)</name>\n", num_lines );
		}
		for (i = 0; i < gs->num_lines; i ++ ) {
			if ( gs->lines[i].is_selected == TRUE ) {
				BOOLEAN is_multi_part = FALSE;
				if ( gs->lines[i].num_parts > 1 ) {
					is_multi_part = TRUE;
				}
				/* start new LineString placemark */
				fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
				if ( is_multi_part == FALSE ) {
					fprintf ( fp, "        <name>Line %i (single part)</name>\n", i+1 );
				} else {
					fprintf ( fp, "        <name>Line %i (multi part)</name>\n", i+1 );
				}
				fprintf ( fp, "        <styleUrl>#line</styleUrl>\n" );
				/* write kml description member */
				export_KML_write_description ( fp, gs, GEOM_TYPE_LINE, i, geom_id, 0, parser, opts );
				/* attribute data */
				int att_errors = export_KML_write_data ( fp, gs, GEOM_TYPE_LINE, i, geom_id, 0, parser, opts );
				num_errors += att_errors;
				for ( j = 0; j < gs->lines[i].num_parts; j++ ) {
					fprintf ( fp, "        <LineString>\n" );
					fprintf ( fp, "          <altitudeMode>absolute</altitudeMode>\n" );
					fprintf ( fp, "          <coordinates>\n" );
					for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k++ ) {
						/* DEBUG */
						/*
						X = ( gs->lines[i].parts[j].X[k] - 3513040.0 );
						Y = ( gs->lines[i].parts[j].Y[k] - 5279880.0 );
						X = X / 2.0;
						Y = Y / 2.0;
						 */
						X = gs->lines[i].parts[j].X[k];
						Y = gs->lines[i].parts[j].Y[k];
						xf = export_float_to_str(X);
						yf = export_float_to_str(Y);
						if ( gs->lines[i].is_3D == TRUE && opts->force_2d == FALSE ) {
							Z = gs->lines[i].parts[j].Z[k];
						} else {
							Z = 0.0;
						}
						zf = export_float_to_str(Z);
						fprintf	( fp, "            %s,%s,%s\n", xf, yf, zf );
						free ( xf );
						free ( yf );
						free ( zf );
					}
					fprintf ( fp, "          </coordinates>\n" );
				}
				fprintf ( fp, "        </LineString>\n" );
				fprintf ( fp, "      </Placemark>\n" );
				fprintf ( fp, "\n" );
				geom_id ++;
			}
		}
		if ( num_polygons > 0 ) {
			fprintf ( fp, "    </Folder>\n" );
		}

		/* POLYGONS
		 *
		 * Polygons in KML are quite tricky, because for each part we need
		 * store the boundaries/rings in a well-defined order: first the outer
		 * boundary, then all holes.
		 */
		if ( num_polygons > 0 ) {
			fprintf ( fp, "\n    <Folder>\n" );
			fprintf ( fp, "      <name>Polygons (%i)</name>\n", num_polygons );
		}
		for (i = 0; i < gs->num_polygons; i ++ ) {
			if ( gs->polygons[i].is_selected == TRUE ) {

				/* Multi-part polygons are represented by collecting :
				 * A polygon is a MultiPolygon if it has at least two parts that are
				 * _not_ holes. Otherwise it's a simple Polygon. */
				BOOLEAN is_multi_part = FALSE;
				int num_non_holes = 0;
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					if ( gs->polygons[i].parts[j].is_hole == FALSE ) {
						num_non_holes ++;
					}
				}
				if ( num_non_holes > 1 ) {
					is_multi_part = TRUE;
				}
				/* start new Polygon placemark */
				fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
				if ( is_multi_part == FALSE ) {
					fprintf ( fp, "        <name>Polygon %i (single part)</name>\n", i+1 );
				} else {
					fprintf ( fp, "        <name>Polygon %i (multi part)</name>\n", i+1 );
				}
				fprintf ( fp, "        <styleUrl>#polygon</styleUrl>\n" );
				/* write kml description member */
				export_KML_write_description ( fp, gs, GEOM_TYPE_POLY, i, geom_id, 0, parser, opts );
				/* attribute data */
				int att_errors = export_KML_write_data ( fp, gs, GEOM_TYPE_POLY, i, geom_id, 0, parser, opts );
				num_errors += att_errors;
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					/* Iterate over all polygon parts that are _not_ holes */
					if ( gs->polygons[i].parts[j].is_hole == FALSE ) {
						fprintf ( fp, "        <Polygon>\n" );
						fprintf ( fp, "          <altitudeMode>absolute</altitudeMode>\n" );
						fprintf ( fp, "          <outerBoundaryIs>\n" );
						fprintf ( fp, "            <LinearRing>\n" );
						fprintf ( fp, "              <coordinates>\n" );
						for ( k = 0; k < gs->polygons[i].parts[j].num_vertices; k++ ) {
							/* DEBUG */
							/*
							X = gs->polygons[i].parts[j].X[k] - 3513040.0;
							Y = gs->polygons[i].parts[j].Y[k] - 5279880.0;
							X = X / 2.0;
							Y = Y / 2.0;
							 */
							X = gs->polygons[i].parts[j].X[k];
							Y = gs->polygons[i].parts[j].Y[k];
							xf = export_float_to_str(X);
							yf = export_float_to_str(Y);
							if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
								Z = gs->polygons[i].parts[j].Z[k];
							} else {
								Z = 0.0;
							}
							zf = export_float_to_str(Z);
							fprintf	( fp, "                %s,%s,%s\n", xf, yf, zf );
							free ( xf );
							free ( yf );
							free ( zf );
						}
						fprintf ( fp, "              </coordinates>\n" );
						fprintf ( fp, "            </LinearRing>\n" );
						fprintf ( fp, "          </outerBoundaryIs>\n" );
						/* Iterate over all holes to see which ones are in this part. */
						/* for ( k = 0; k < 0; k++ ) { */
						for ( k = 0; k < gs->polygons[i].num_parts; k++ ) {
							if ( gs->polygons[i].parts[k].is_hole == TRUE ) {
								geom_part *A = &gs->polygons[i].parts[k];
								geom_part *B = &gs->polygons[i].parts[j];
								if ( geom_tools_part_in_part_2D ( A, B ) == TRUE ) {
									BOOLEAN lies_inside_hole = FALSE;
									/* Eliminate holes that lie within another hole. */
									for ( l = 0; l < gs->num_polygons && lies_inside_hole == FALSE; l++ ) {
										for ( m = 0; m < gs->polygons[l].num_parts && lies_inside_hole == FALSE; m++ ) {
											if ( gs->polygons[l].parts[m].is_hole == TRUE ) {
												B = &gs->polygons[l].parts[m];
												if ( geom_tools_part_in_part_2D ( A, B ) == TRUE ) {
													lies_inside_hole = TRUE;
												}
											}
										}
									}
									if ( lies_inside_hole == FALSE ) {
										/* Add hole to current part. */
										fprintf ( fp, "          <innerBoundaryIs>\n" );
										fprintf ( fp, "            <LinearRing>\n" );
										fprintf ( fp, "              <coordinates>\n" );
										for ( l = 0; l < gs->polygons[i].parts[k].num_vertices; l++ ) {
											/* DEBUG */
											/*
											X = gs->polygons[i].parts[k].X[l] - 3513040.0;
											Y = gs->polygons[i].parts[k].Y[l] - 5279880.0;
											X = X / 2.0;
											Y = Y / 2.0;
											 */
											X = gs->polygons[i].parts[k].X[l];
											Y = gs->polygons[i].parts[k].Y[l];
											xf = export_float_to_str(X);
											yf = export_float_to_str(Y);
											if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
												Z = gs->polygons[i].parts[k].Z[l];
											} else {
												Z = 0.0;
											}
											zf = export_float_to_str(Z);
											fprintf	( fp, "                %s,%s,%s\n", xf, yf, zf );
											free ( xf );
											free ( yf );
											free ( zf );
										}
										fprintf ( fp, "              </coordinates>\n" );
										fprintf ( fp, "            </LinearRing>\n" );
										fprintf ( fp, "          </innerBoundaryIs>\n" );
									}
								}
							}
						}
					} /* DONE (adding all holes to this part */
				} /* DONE (adding all parts) */
				fprintf ( fp, "        </Polygon>\n" );
				fprintf ( fp, "      </Placemark>\n" );
				fprintf ( fp, "\n" );
				geom_id ++;
			}
		}
		if ( num_polygons > 0 ) {
			fprintf ( fp, "    </Folder>\n" );
		}

		/* RAW VERTICES */
		if ( num_points_raw > 0 ) {
			fprintf ( fp, "\n    <Folder>\n" );
			fprintf ( fp, "      <name>Vertices (%i)</name>\n", num_points_raw );
		}
		for (i = 0; i < gs->num_points_raw; i ++ ) {
			if ( gs->points_raw[i].is_selected == TRUE ) {
				/* start new Point placemark */
				fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
				fprintf ( fp, "        <name>%i</name>\n", i+1 );
				fprintf ( fp, "        <visibility>0</visibility>\n"); /* hidden by default */
				fprintf ( fp, "        <styleUrl>#vertex</styleUrl>\n" );
				/* write kml description member */
				export_KML_write_description ( fp, gs, GEOM_TYPE_POINT_RAW, i, geom_id, 0, parser, opts );
				/* attribute data */
				int att_errors = export_KML_write_data ( fp, gs, GEOM_TYPE_POINT_RAW, i, geom_id, 0, parser, opts );
				num_errors += att_errors;
				/* DEBUG */
				/*
				X = ( gs->points_raw[i].X - 3513040.0 );
				Y = ( gs->points_raw[i].Y - 5279880.0 );
				X = X / 2.0;
				Y = Y / 2.0;
				 */
				X = gs->points_raw[i].X;
				Y = gs->points_raw[i].Y;
				xf = export_float_to_str(X);
				yf = export_float_to_str(Y);
				if ( gs->points_raw[i].is_3D == TRUE && opts->force_2d == FALSE ) {
					Z = gs->points_raw[i].Z;
				} else {
					Z = 0.0;
				}
				zf = export_float_to_str(Z);
				/* write point geometry */
				fprintf ( fp, "        <Point>\n" );
				fprintf ( fp, "          <coordinates>%s,%s,%s</coordinates>\n", xf, yf, zf );
				fprintf ( fp, "        </Point>\n" );
				free ( xf );
				free ( yf );
				free ( zf );
				fprintf ( fp, "      </Placemark>\n" );
				fprintf ( fp, "\n" );
				geom_id ++;
			}
		}
		if ( num_points_raw > 0 ) {
			fprintf ( fp, "    </Folder>\n" );
		}

		/* LABELS */
		int num_labels = 0;
		for ( i = 0; i < gs->num_points; i ++ ) {
			if ( gs->points[i].is_selected == TRUE ) {
				if ( gs->points[i].has_label == TRUE ) {
					num_labels ++;
				}
			}
		}
		for ( i = 0; i < gs->num_lines; i ++ ) {
			if ( gs->lines[i].is_selected == TRUE ) {
				for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
					if ( gs->lines[i].parts[j].has_label == TRUE ) {
						num_labels ++;
					}
				}
			}
		}
		for ( i = 0; i < gs->num_polygons; i ++ ) {
			if ( gs->polygons[i].is_selected == TRUE ) {
				for ( j = 0; j < gs->polygons[i].num_parts; j ++ ) {
					if ( gs->polygons[i].parts[j].has_label == TRUE ) {
						num_labels ++;
					}
				}
			}
		}
		if ( num_labels > 0 ) {
			char **atts = NULL;
			char *NULL_str = NULL;
			int label_field_idx = 0;
			BOOLEAN store_null = FALSE;

			/* string representation of user-defined NULL value */
			if ( parser->empty_val_set == TRUE ) {
				NULL_str = malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
				snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
			}
			/* get label field */
			i = 0;
			while ( parser->fields[i] != NULL ) {
				if ( !strcasecmp (parser->fields[i]->name, opts->label_field)  ) {
					label_field_idx = i;
					break;
				}
				i ++;
			}
			fprintf ( fp, "\n    <Folder>\n" );
			fprintf ( fp, "      <name>Labels (%i)</name>\n", num_labels );
			/* labels for all geometries/parts go into one and the same KML folder */
			for ( i = 0; i < gs->num_points; i ++ ) {
				if ( gs->points[i].is_selected == TRUE ) {
					if ( gs->points[i].has_label == TRUE ) {
						atts = gs->points[i].atts;
						fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
						store_null = FALSE;
						/* check for all conditions under which NULL data will be written */
						if ( atts[label_field_idx] == NULL ) {
							store_null = TRUE;
						}
						if ( store_null == TRUE ) {
							/* store "no data" representation */
							if ( parser->empty_val_set == FALSE ) {
								fprintf ( fp, "        <name></name>\n" ); /* write NULL value as empty string */
							} else {
								fprintf ( fp, "        <name>%s</name>\n", NULL_str ); /* write user-defined NULL value */
							}
						} else {
							fprintf ( fp, "        <name>%s</name>\n", atts[label_field_idx] ); /* write actual field contents */
						}
						fprintf ( fp, "        <styleUrl>#label</styleUrl>\n" );
						/* DEBUG */
						/*
						X = ( gs->points[i].label_x - 3513040.0 );
						Y = ( gs->points[i].label_y - 5279880.0 );
						X = X / 2.0;
						Y = Y / 2.0;
						 */
						X = gs->points[i].label_x;
						Y = gs->points[i].label_y;
						xf = export_float_to_str(X);
						yf = export_float_to_str(Y);
						if ( gs->points[i].is_3D == TRUE && opts->force_2d == FALSE ) {
							Z = gs->points[i].Z;
						} else {
							Z = 0.0;
						}
						zf = export_float_to_str(Z);
						/* write point geometry */
						fprintf ( fp, "        <Point>\n" );
						fprintf ( fp, "          <coordinates>%s,%s</coordinates>\n", xf, yf );
						fprintf ( fp, "        </Point>\n" );
						free ( xf );
						free ( yf );
						fprintf ( fp, "      </Placemark>\n" );
						fprintf ( fp, "\n" );
						geom_id ++;
					}
				}
			}
			for ( i = 0; i < gs->num_lines; i ++ ) {
				if ( gs->lines[i].is_selected == TRUE ) {
					for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
						if ( gs->lines[i].parts[j].has_label == TRUE ) {
							atts = gs->lines[i].atts;
							fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
							store_null = FALSE;
							/* check for all conditions under which NULL data will be written */
							if ( atts[label_field_idx] == NULL ) {
								store_null = TRUE;
							}
							if ( store_null == TRUE ) {
								/* store "no data" representation */
								if ( parser->empty_val_set == FALSE ) {
									fprintf ( fp, "        <name></name>\n" ); /* write NULL value as empty string */
								} else {
									fprintf ( fp, "        <name>%s</name>\n", NULL_str ); /* write user-defined NULL value */
								}
							} else {
								fprintf ( fp, "        <name>%s</name>\n", atts[label_field_idx] ); /* write actual field contents */
							}
							fprintf ( fp, "        <styleUrl>#label</styleUrl>\n" );
							/* DEBUG */
							/*
							X = ( gs->lines[i].parts[j].label_x - 3513040.0 );
							Y = ( gs->lines[i].parts[j].label_y - 5279880.0 );
							X = X / 2.0;
							Y = Y / 2.0;
							 */
							X = gs->lines[i].parts[j].label_x;
							Y = gs->lines[i].parts[j].label_y;
							xf = export_float_to_str(X);
							yf = export_float_to_str(Y);
							/* write point geometry */
							fprintf ( fp, "        <Point>\n" );
							fprintf ( fp, "          <coordinates>%s,%s</coordinates>\n", xf, yf );
							fprintf ( fp, "        </Point>\n" );
							free ( xf );
							free ( yf );
							fprintf ( fp, "      </Placemark>\n" );
							fprintf ( fp, "\n" );
							geom_id ++;
						}
					}
				}
			}
			for ( i = 0; i < gs->num_polygons; i ++ ) {
				if ( gs->polygons[i].is_selected == TRUE ) {
					for ( j = 0; j < gs->polygons[i].num_parts; j ++ ) {
						if ( gs->polygons[i].parts[j].has_label == TRUE ) {
							atts = gs->polygons[i].atts;
							fprintf ( fp, "      <Placemark id=\"%i\">\n", geom_id-1 );
							store_null = FALSE;
							/* check for all conditions under which NULL data will be written */
							if ( atts[label_field_idx] == NULL ) {
								store_null = TRUE;
							}
							if ( store_null == TRUE ) {
								/* store "no data" representation */
								if ( parser->empty_val_set == FALSE ) {
									fprintf ( fp, "        <name></name>\n" ); /* write NULL value as empty string */
								} else {
									fprintf ( fp, "        <name>%s</name>\n", NULL_str ); /* write user-defined NULL value */
								}
							} else {
								fprintf ( fp, "        <name>%s</name>\n", atts[label_field_idx] ); /* write actual field contents */
							}
							fprintf ( fp, "        <styleUrl>#label</styleUrl>\n" );
							/* DEBUG */
							/*
							X = ( gs->polygons[i].parts[j].label_x - 3513040.0 );
							Y = ( gs->polygons[i].parts[j].label_y - 5279880.0 );
							X = X / 2.0;
							Y = Y / 2.0;
							 */
							X = gs->polygons[i].parts[j].label_x;
							Y = gs->polygons[i].parts[j].label_y;
							xf = export_float_to_str(X);
							yf = export_float_to_str(Y);
							/* write point geometry */
							fprintf ( fp, "        <Point>\n" );
							fprintf ( fp, "          <coordinates>%s,%s</coordinates>\n", xf, yf );
							fprintf ( fp, "        </Point>\n" );
							free ( xf );
							free ( yf );
							fprintf ( fp, "      </Placemark>\n" );
							fprintf ( fp, "\n" );
							geom_id ++;
						}
					}
				}
			}
			fprintf ( fp, "    </Folder>\n" );
			if ( NULL_str != NULL ) {
				free ( NULL_str );
			}
		}


		/* write footer to output file */
		fprintf ( fp, "  </Document>\n" );
		fprintf ( fp, "</kml>\n" );
	} else {
		return ( 0 );
	}

	/* Close output file. */
	if ( fp != NULL ) {
		fclose (fp);
	}

	return ( num_errors );
}


/*
 * Writes geometry store objects into new Shapefile(s), separated
 * by geometry type. Calls "export_shape_write_atts()" to create
 * attribute DBF data.
 *
 * By the time this function is called, "gs" must be a valid geometry
 * store and must contain only fully checked and built, valid geometries,
 * ready to be written to disk.
 *
 * Returns number of attribute field errors.
 * */
int export_SHP ( geom_store *gs, parser_desc *parser, options *opts )
{
	double *X,*Y,*Z;
	SHPHandle points, points_raw, lines, polygons, labels;
	DBFHandle points_atts, points_raw_atts, lines_atts, polygons_atts, labels_atts;
	SHPObject *geom;
	int i, j, k, num_errors;
	int num_vertices;


	/* Check if there is anything to export! */
	if ( gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons < 1 )  {
		err_show (ERR_NOTE,"");
		err_show (ERR_WARN, _("\nNo valid geometries found. No output produced."));
		return ( 0 );
	}

	/* CREATE ALL REQUIRED OUTPUT FILES */

	/* Points */
	if ( selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ) {
		if ( gs->points[0].is_3D == TRUE && opts->force_2d == FALSE ) {
			points = export_SHP_SHPCreate ( gs->path_points, SHPT_POINTZ );
		} else {
			points = export_SHP_SHPCreate ( gs->path_points, SHPT_POINT );
		}
		points_atts = export_SHP_DBFCreate( gs->path_points_atts );
		if ( points_atts == NULL ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF output file for point data\n(%s)."),
					gs->path_points_atts );
			return ( 0 );
		}
		if ( export_SHP_make_DBF ( points_atts, parser, opts ) < 0 ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF schema for point data\n(%s)."),
					gs->path_points_atts );
			return ( 0 );
		}
	}
	/* Raw points */
	if ( selections_get_num_selected ( GEOM_TYPE_POINT_RAW, gs ) > 0 ) {
		if ( gs->points_raw[0].is_3D == TRUE && opts->force_2d == FALSE ) {
			points_raw = export_SHP_SHPCreate ( gs->path_points_raw, SHPT_POINTZ );
		} else {
			points_raw = export_SHP_SHPCreate ( gs->path_points_raw, SHPT_POINT );
		}
		points_raw_atts = export_SHP_DBFCreate( gs->path_points_raw_atts );
		if ( points_raw_atts == NULL ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF output file for raw vertex data\n(%s)."),
					gs->path_points_atts );
			return ( 0 );
		}
		if ( export_SHP_make_DBF ( points_raw_atts, parser, opts ) < 0 ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF schema for raw vertex data\n(%s)."),
					gs->path_points_raw_atts );
			return ( 0 );
		}
	}
	/* Lines */
	if ( selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ) {
		if ( gs->lines[0].is_3D == TRUE && opts->force_2d == FALSE ) {
			lines = export_SHP_SHPCreate ( gs->path_lines, SHPT_ARCZ );
		} else {
			lines = export_SHP_SHPCreate ( gs->path_lines, SHPT_ARC );
		}
		lines_atts = export_SHP_DBFCreate( gs->path_lines_atts );
		if ( lines_atts == NULL ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF output file for line data\n(%s)."),
					gs->path_points_atts );
			return ( 0 );
		}
		if ( export_SHP_make_DBF ( lines_atts, parser, opts ) < 0 ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF schema for line data\n(%s)."),
					gs->path_lines_atts );
			return ( 0 );
		}
	}
	/* Polygons */
	if ( selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0 ) {
		if ( gs->polygons[0].is_3D == TRUE && opts->force_2d == FALSE ) {
			polygons = export_SHP_SHPCreate ( gs->path_polys, SHPT_POLYGONZ );
		} else {
			polygons = export_SHP_SHPCreate ( gs->path_polys, SHPT_POLYGON );
		}
		polygons_atts = export_SHP_DBFCreate( gs->path_polys_atts );
		if ( polygons_atts == NULL ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF output file for polygon data\n(%s)."),
					gs->path_points_atts );
			return ( 0 );
		}
		if ( export_SHP_make_DBF ( polygons_atts, parser, opts ) < 0 ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_EXIT, _("\nError creating DBF schema for polygon data\n(%s)."),
					gs->path_polys_atts );
			return ( 0 );
		}
	}
	/* Labels (points) */
	if ( opts->label_field != NULL ) {
		if ( 	selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ||
				selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ||
				selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0
		)
		{
			labels = export_SHP_SHPCreate ( gs->path_labels, SHPT_POINT );
			labels_atts = export_SHP_DBFCreate( gs->path_labels_atts );
			if ( labels_atts == NULL ) {
				err_show (ERR_NOTE,"");
				err_show (ERR_EXIT, _("\nError creating DBF output file for label data\n(%s)."),
						gs->path_labels_atts );
				return ( 0 );
			}
			if ( export_SHP_make_DBF_labels ( labels_atts, parser, opts ) < 0 ) {
				err_show (ERR_NOTE,"");
				err_show (ERR_EXIT, _("\nError creating DBF schema for label data\n(%s)."),
						gs->path_labels_atts );
				return ( 0 );
			}
		}
	}

	/* WRITE GEOMETRIES TO SHAPEFILE(S) */

	num_errors = 0;

	/* Points */
	int obj = -1; /* Separate, unbroken DBF/SHP index. */
	for (i = 0; i < gs->num_points; i ++ ) {
		if ( gs->points[i].is_selected == TRUE ) {
			obj ++;
			X = malloc ( sizeof ( double ) );
			Y = malloc ( sizeof ( double ) );
			Z = malloc ( sizeof ( double ) );
			X[0] = gs->points[i].X;
			Y[0] = gs->points[i].Y;
			Z[0] = gs->points[i].Z;
			if ( gs->points[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				geom = SHPCreateSimpleObject ( SHPT_POINTZ, 1, X, Y, Z );
			} else {
				geom = SHPCreateSimpleObject ( SHPT_POINT, 1, X, Y, NULL );
			}
			SHPWriteObject( points, -1, geom );
			SHPDestroyObject( geom );
			free ( X );
			free ( Y );
			free ( Z );
			num_errors += export_SHP_write_atts (points_atts, gs, GEOM_TYPE_POINT, i, obj, parser, opts);
		}
	}
	/* Raw points */
	obj = -1;
	for (i = 0; i < gs->num_points_raw; i ++ ) {
		if ( gs->points_raw[i].is_selected == TRUE ) {
			obj ++;
			X = malloc ( sizeof ( double ) );
			Y = malloc ( sizeof ( double ) );
			Z = malloc ( sizeof ( double ) );
			X[0] = gs->points_raw[i].X;
			Y[0] = gs->points_raw[i].Y;
			Z[0] = gs->points_raw[i].Z;
			if ( gs->points_raw[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				geom = SHPCreateSimpleObject ( SHPT_POINTZ, 1, X, Y, Z );
			} else {
				geom = SHPCreateSimpleObject ( SHPT_POINT, 1, X, Y, Z );
			}
			SHPWriteObject( points_raw, -1, geom );
			SHPDestroyObject( geom );
			free ( X );
			free ( Y );
			free ( Z );
			num_errors += export_SHP_write_atts (points_raw_atts, gs, GEOM_TYPE_POINT_RAW, i, obj, parser, opts);
		}
	}
	/* Lines */
	obj = -1;
	for (i = 0; i < gs->num_lines; i ++ ) {
		if ( gs->lines[i].is_selected == TRUE ) {
			int *start;
			int vertex;

			obj ++;
			start = malloc ( sizeof (int) * gs->lines[i].num_parts );
			num_vertices = 0;
			/* get total number of vertices and starting index for each
		       part of a multi-part line string. */
			for ( j=0; j < gs->lines[i].num_parts; j++ ) {
				start[j] = num_vertices;
				num_vertices += gs->lines[i].parts[j].num_vertices;
			}
			X = malloc ( sizeof ( double ) * num_vertices );
			Y = malloc ( sizeof ( double ) * num_vertices );
			Z = malloc ( sizeof ( double ) * num_vertices );
			vertex = 0;
			for ( j=0; j < gs->lines[i].num_parts; j++ ) {
				for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k++ ) {
					X[vertex] = gs->lines[i].parts[j].X[k];
					Y[vertex] = gs->lines[i].parts[j].Y[k];
					if ( gs->lines[i].is_3D == TRUE && opts->force_2d == FALSE ) {
						Z[vertex] = gs->lines[i].parts[j].Z[k];
					} else {
						Z[vertex] = 0.0;
					}
					vertex ++;
				}
			}
			if ( gs->lines[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				geom = SHPCreateObject ( SHPT_ARCZ, obj, gs->lines[i].num_parts, start, NULL, num_vertices, X, Y, Z, NULL );
			} else {
				geom = SHPCreateObject ( SHPT_ARC, obj, gs->lines[i].num_parts, start, NULL, num_vertices, X, Y, NULL, NULL );
			}
			SHPWriteObject( lines, -1, geom );
			SHPDestroyObject( geom );
			free ( X );
			free ( Y );
			free ( Z );
			free ( start );
			num_errors += export_SHP_write_atts (lines_atts, gs, GEOM_TYPE_LINE, i, obj, parser, opts);
		}
	}
	/* Polygons */
	obj = -1;
	for (i = 0; i < gs->num_polygons; i ++ ) {
		if ( gs->polygons[i].is_selected == TRUE ) {
			int *start;
			int vertex;

			obj ++;
			start = malloc ( sizeof (int) * gs->polygons[i].num_parts );
			num_vertices = 0;
			/* get total number of vertices and starting index for each
		       part of a multi-part polygon. */
			for ( j=0; j < gs->polygons[i].num_parts; j++ ) {
				start[j] = num_vertices;
				num_vertices += gs->polygons[i].parts[j].num_vertices;
			}
			X = malloc ( sizeof ( double ) * num_vertices );
			Y = malloc ( sizeof ( double ) * num_vertices );
			Z = malloc ( sizeof ( double ) * num_vertices );
			vertex = 0;
			for ( j=0; j < gs->polygons[i].num_parts; j++ ) {
				for ( k = 0; k < gs->polygons[i].parts[j].num_vertices; k++ ) {
					X[vertex] = gs->polygons[i].parts[j].X[k];
					Y[vertex] = gs->polygons[i].parts[j].Y[k];
					if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
						Z[vertex] = gs->polygons[i].parts[j].Z[k];
					} else {
						Z[vertex] = 0.0;
					}
					vertex ++;
				}
			}
			if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				/* shapelib will automatically figure out, whether a part is an outer or inner (hole) ring */
				geom = SHPCreateObject ( SHPT_POLYGONZ, obj, gs->polygons[i].num_parts, start, NULL, num_vertices, X, Y, Z, NULL );
			} else {
				/* shapelib will automatically figure out, whether a part is an outer or inner (hole) ring */
				geom = SHPCreateObject ( SHPT_POLYGON, obj, gs->polygons[i].num_parts, start, NULL, num_vertices, X, Y, NULL, NULL );
			}
			SHPRewindObject( polygons, geom ); /* correct order of vertices in polygon */
			SHPWriteObject( polygons, -1, geom );
			SHPDestroyObject( geom );
			free ( X );
			free ( Y );
			free ( Z );
			free ( start );
			num_errors += export_SHP_write_atts (polygons_atts, gs, GEOM_TYPE_POLY, i, obj, parser, opts);
		}
	}
	/* Labels (points) */
	if ( opts->label_field != NULL ) {
		obj = -1;
		/* Labels for points. */
		if ( opts->label_mode_point != OPTIONS_LABEL_MODE_NONE ) {
			for (i = 0; i < gs->num_points; i ++ ) {
				if ( gs->points[i].is_selected == TRUE ) {
					if ( gs->points[i].has_label == TRUE ) {
						obj ++;
						X = malloc ( sizeof ( double ) );
						Y = malloc ( sizeof ( double ) );
						Z = malloc ( sizeof ( double ) );
						X[0] = gs->points[i].label_x;
						Y[0] = gs->points[i].label_y;
						Z[0] = 0.0;
						geom = SHPCreateSimpleObject ( SHPT_POINT, 1, X, Y, NULL );
						SHPWriteObject( labels, -1, geom );
						SHPDestroyObject( geom );
						free ( X );
						free ( Y );
						free ( Z );
						num_errors += export_SHP_write_atts_labels (labels_atts, gs, GEOM_TYPE_POINT, i, obj, parser, opts);
					} else {
						/* Warn: no label for this point. */
						err_show (ERR_NOTE,"");
						err_show (ERR_WARN, _("\nFailed to place label at point #%i."), i );
					}
				}
			}
		}
		/* Labels for lines. */
		if ( opts->label_mode_line != OPTIONS_LABEL_MODE_NONE ) {
			for (i = 0; i < gs->num_lines; i ++ ) {
				if ( gs->lines[i].is_selected == TRUE ) {
					int p;
					for ( p = 0; p < gs->lines[i].num_parts; p++ ) {
						if ( gs->lines[i].parts[p].has_label == TRUE ) {
							obj ++;
							X = malloc ( sizeof ( double ) );
							Y = malloc ( sizeof ( double ) );
							Z = malloc ( sizeof ( double ) );
							X[0] = gs->lines[i].parts[p].label_x;
							Y[0] = gs->lines[i].parts[p].label_y;
							Z[0] = 0.0;
							geom = SHPCreateSimpleObject ( SHPT_POINT, 1, X, Y, NULL );
							SHPWriteObject( labels, -1, geom );
							SHPDestroyObject( geom );
							free ( X );
							free ( Y );
							free ( Z );
							num_errors += export_SHP_write_atts_labels (labels_atts, gs, GEOM_TYPE_LINE, i, obj, parser, opts);
						} else {
							/* Warn: no label for this line (part). */
							if ( opts->label_mode_line == OPTIONS_LABEL_MODE_CENTER ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at center of line #%i (part #%i)."), i, p );
							}
							if ( opts->label_mode_line == OPTIONS_LABEL_MODE_FIRST ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at first vertex of line #%i (part #%i)."), i, p );
							}
							if ( opts->label_mode_line == OPTIONS_LABEL_MODE_LAST ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at last vertex of line #%i (part #%i)."), i, p );
							}
						}
					}
				}
			}
		}
		/* Labels for polygons. */
		if ( opts->label_mode_poly != OPTIONS_LABEL_MODE_NONE ) {
			for (i = 0; i < gs->num_polygons; i ++ ) {
				if ( gs->polygons[i].is_selected == TRUE ) {
					int p;
					for ( p = 0; p < gs->polygons[i].num_parts; p++ ) {
						if ( gs->polygons[i].parts[p].has_label == TRUE ) {
							obj ++;
							X = malloc ( sizeof ( double ) );
							Y = malloc ( sizeof ( double ) );
							Z = malloc ( sizeof ( double ) );
							X[0] = gs->polygons[i].parts[p].label_x;
							Y[0] = gs->polygons[i].parts[p].label_y;
							Z[0] = 0.0;
							geom = SHPCreateSimpleObject ( SHPT_POINT, 1, X, Y, NULL );
							SHPWriteObject( labels, -1, geom );
							SHPDestroyObject( geom );
							free ( X );
							free ( Y );
							free ( Z );
							num_errors += export_SHP_write_atts_labels (labels_atts, gs, GEOM_TYPE_POLY, i, obj, parser, opts);
						} else {
							if ( gs->polygons[i].parts[p].is_hole == FALSE ) {
								/* Warn: no label for this polygon (part), even though it is not a hole. */
								if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_CENTER ) {
									err_show (ERR_NOTE,"");
									err_show (ERR_WARN, _("\nFailed to place label at center of polygon #%i (part #%i)."), i, p );
								}
								if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_FIRST ) {
									err_show (ERR_NOTE,"");
									err_show (ERR_WARN, _("\nFailed to place label at first vertex of polygon #%i (part #%i)."), i, p );
								}
								if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_LAST ) {
									err_show (ERR_NOTE,"");
									err_show (ERR_WARN, _("\nFailed to place label at last vertex of polygon #%i (part #%i)."), i, p );
								}
							}
						}
					}
				}
			}
		}
	}


	/* Warn if no labels were written */
	if ( 	opts->label_mode_point == OPTIONS_LABEL_MODE_NONE &&
			opts->label_mode_line == OPTIONS_LABEL_MODE_NONE &&
			opts->label_mode_poly == OPTIONS_LABEL_MODE_NONE
	)
	{
		err_show (ERR_NOTE,"");
		err_show (ERR_WARN, _("\nLabel mode for all geometries set to '%s'.\nLabel layer will be empty."),
				OPTIONS_LABEL_MODE_NAMES[OPTIONS_LABEL_MODE_NONE]);
	}

	/* CREATE .GVA FILE FOR LABEL POINTS (OPTIONAL) */
	if ( opts->label_field != NULL && gs->path_labels_gva != NULL ) {
		int error = export_SHP_make_labels_GVA (gs->path_labels_gva);
		if ( error < 0 ) {
			err_show (ERR_NOTE,"");
			err_show (ERR_WARN, _("\nFailed to create annotation settings file ('%s')."), gs->path_labels_gva);
		}
	}

	/* CLOSE ALL OUTPUT FILES */

	/* Points */
	if ( selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ) {
		SHPClose ( points );
		DBFClose ( points_atts );
	}
	/* Raw points */
	if ( selections_get_num_selected ( GEOM_TYPE_POINT_RAW, gs ) > 0 ) {
		SHPClose ( points_raw );
		DBFClose ( points_raw_atts );
	}
	/* Lines */
	if ( selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ) {
		SHPClose ( lines );
		DBFClose ( lines_atts );
	}
	/* Polygons */
	if ( selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0 ) {
		SHPClose ( polygons );
		DBFClose ( polygons_atts );
	}
	/* Labels (points) */
	if ( opts->label_field != NULL ) {
		if ( 	selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ||
				selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ||
				selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0
		)
		{
			SHPClose ( labels );
			DBFClose ( labels_atts );
		}
	}

	return ( num_errors );
}


/*
 * Helper function for all export formats that must write floating
 * point numbers to text files (DXF, KML, GeoJSON) and need to
 * ensure that the number always use English number format and no
 * grouping characters.
 * 
 * Takes a floating point number and makes a new string from it
 * that can be written into the exported text file.
 *
 * Return value is _always_ a newly allocated char* that must be free'd
 * by the caller!
 */
char *export_float_to_str ( double f ) {
	char buf[PRG_MAX_STR_LEN];
	char *p;
	char *val;
	double integral;
	double fractional;

	val = malloc ( sizeof (char) * PRG_MAX_STR_LEN);

	if ( f == 0 ) {
		snprintf (val,PRG_MAX_STR_LEN,"0");
		return ( val );
	}

	fractional = modf(f, &integral);

	snprintf (buf,PRG_MAX_STR_LEN,"%f",fractional);
	p = buf;
	p++;
	p++;
	snprintf (val,PRG_MAX_STR_LEN,"%i.%s",(int)integral,p);

	return ( val );
}


/*
 * Writes the attribute data row for a given record as a
 * new row to a simple text attribute table (for DXF output).
 * If there is a problem with the data, a (row of) NULL field(s) will be
 * written to the attribute table and a warning issued.
 *
 * "pk" is the primary key, i.e. an integer index from 0 .. n, which
 * points directly into the points(_raw)[], lines[], or polygons[]
 * array of the geometry store "gs".
 * "geom_id" is a unique ID for each row of attribute table output.
 * This is required because the geom IDs in the geometry store "gs" use
 * separate IDs, but we want to have only one ID in the text output file.
 *
 * The return value is the total number of attribute data errors.
 */
int export_DXF_write_atts ( 	FILE *ft, geom_store *gs, int GEOM_TYPE,
		unsigned int pk, unsigned int geom_id,
		parser_desc *parser, options *opts )
{
	unsigned int err_count;
	int i_value;
	double d_value;
	int field_num;
	char *NULL_str = NULL;
	BOOLEAN store_null;
	BOOLEAN c_error;
	BOOLEAN c_overflow;
	char *input;
	unsigned int line;
	char **atts;


	if ( gs == NULL || parser == NULL || opts == NULL )
		return ( 1 );

	if ( 	GEOM_TYPE != GEOM_TYPE_POINT && GEOM_TYPE != GEOM_TYPE_POINT_RAW &&
			GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		err_show ( ERR_NOTE, "" );
		err_show ( ERR_WARN, _("\nGeometry type unknown.\nNo attribute data written.") );
		return ( 1 );
	}

	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW ) {
		/* TODO: Theoretically, we could also output full attributes for
				raw point. However, the geom_id of the latter are not
		        unique! Instead, they are the IDs of the geometries of
				the points are vertices. So we would need to generate a
				new, unique key for DXF object handles AND TXT attributes!
		 */
		return (0);
	}

	/* string representation of user-defined NULL value */
	if ( parser->empty_val_set == TRUE ) {
		NULL_str = malloc ( sizeof ( char ) * ( DBF_INTEGER_WIDTH + 1)  );
		snprintf ( NULL_str, DBF_INTEGER_WIDTH, "%i", parser->empty_val );
	}

	/* get name of input source */
	input = NULL;
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		input = strdup (gs->points[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		input = strdup (gs->points_raw[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		input = strdup (gs->lines[pk].source);
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		input = strdup (gs->polygons[pk].source);
	if ( input == NULL )  {
		input = strdup ("<NULL>");
	} else {
		if ( !strcmp ( "-", input ) ) {
			free ( input );
			input = strdup ("<console input stream>");
		}
	}

	/* get line of input file from which this geom was read */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		line = gs->points[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		line = gs->points_raw[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		line = gs->lines[pk].line;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		line = gs->polygons[pk].line;

	/* get point to attribute field contents (strings) */
	if ( GEOM_TYPE == GEOM_TYPE_POINT )
		atts = gs->points[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POINT_RAW )
		atts = gs->points_raw[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_LINE )
		atts = gs->lines[pk].atts;
	if ( GEOM_TYPE == GEOM_TYPE_POLY )
		atts = gs->polygons[pk].atts;

	/* reset error count to "0" */
	err_count = 0;

	/* first field is always the geom ID */
	fprintf (ft, "%u", geom_id-1); /* geom ID starts at "0" in text output */

	/* now all other fields */
	field_num = 0;
	while ( parser->fields[field_num] != NULL ) {
		if ( parser->fields[field_num]->skip == FALSE ) {

			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_TEXT ) {
				/*** TEXT ***/
				if ( atts[field_num] == NULL ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default NULL value */
						fprintf (ft, ";\"\"");
					} else {
						/* write user-defined NULL value */
						fprintf (ft, ";\"%s\"", NULL_str);
					}
				} else {
					/* write actual field contents */
					fprintf (ft, ";\"%s\"", atts[field_num]);
				}
			}

			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_INT ) {
				/*** INTEGER ***/
				i_value = t_str_to_int ( atts[field_num], &c_error, &c_overflow );
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( c_error == TRUE && c_overflow == FALSE ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i\nValue for attribute '%s' is not a valid integer number\nNULL data written instead."),
							input, line, parser->fields[field_num]->name);
					store_null = TRUE;
					err_count ++;
				} else {
					if ( c_overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
								input, line, parser->fields[field_num]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( atts[field_num] == NULL ) {
							store_null = TRUE;
						}
					}
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default integer NULL value */
						fprintf (ft, ";0");
					} else {
						/* write user-defined NULL value */
						fprintf (ft, ";%i", parser->empty_val);
					}
				} else {
					/* write actual attribute value */
					fprintf (ft, ";%i", i_value);
				}
			}

			if ( parser->fields[field_num]->type == PARSER_FIELD_TYPE_DOUBLE ) {
				/*** DOUBLE ***/
				d_value = t_str_to_dbl ( atts[field_num], opts->decimal_point[0], opts->decimal_group[0], &c_error, &c_overflow );
				store_null = FALSE;
				/* check for all conditions under which NULL data will be written */
				if ( c_error == TRUE && c_overflow == FALSE ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is not a valid number\nNULL data written instead."),
							input, line, parser->fields[field_num]->name);
					store_null = TRUE;
					err_count ++;
				} else {
					if ( c_overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nRecord read from '%s', line %i:\nValue for attribute '%s' is too large (overflow).\nNULL data written instead."),
								input, line, parser->fields[field_num]->name);
						store_null = TRUE;
						err_count ++;
					} else {
						if ( atts[field_num] == NULL ) {
							store_null = TRUE;
						}
					}
				}
				if ( store_null == TRUE ) {
					/* store "no data" representation */
					if ( parser->empty_val_set == FALSE ) {
						/* write default double NULL value */
						fprintf (ft, ";0");
					} else {
						/* write user-defined NULL value */
						fprintf (ft, ";%i", parser->empty_val);
					}
				} else {
					/* write actual attribute value */
					if ( d_value == 0 ) {
						fprintf (ft, ";0");
					} else {
						char *d_str = export_float_to_str ( d_value );
						fprintf (ft, ";%s", d_str );
						free ( d_str );
					}
				}
			}
		}
		field_num ++;
	}

	fprintf (ft, "\n");

	if ( NULL_str != NULL ) {
		free ( NULL_str );
	}

	return (err_count);
}


/*
 * Helper function for export_DXF (below).
 * Creates the attribute table for the DXF file in simple
 * text format.
 *
 * Returns NULL on error, otherwise a valid file handle that can be
 * use to write to the attribute table.
 */
FILE *export_DXF_make_TXT ( parser_desc *parser, char *path )
{
	FILE *ft;
	int field_num;


	/* attempt to open text file for writing */
	ft = t_fopen_utf8 (path, "w+");
	if ( ft == NULL ) return (ft);

	/* write header with field names */
	/* GEOM_ID will always be the first field */
	fprintf (ft, "geom_id");
	field_num = 0;
	while ( parser->fields[field_num] != NULL ) {
		if ( parser->fields[field_num]->skip == FALSE ) {
			fprintf (ft, ";%s", parser->fields[field_num]->name);
		}
		field_num ++;
	}
	fprintf (ft, "\n");

	return (ft);
}


/*
 * Helper function for export_DXF():
 * Returns a valid, open file handle on success, NULL on error
 */
FILE *dxf_write_header ( geom_store *gs, char *path ) {
	FILE *fp;
	fp = t_fopen_utf8 (path, "w+");

	/* write DXF header to output file */
	if ( fp != NULL ) {
		char *f;
		/* DXF producer ID */
		fprintf ( fp, "999\n" );
		fprintf ( fp, "DXF by %s\n", t_get_prg_name_and_version() );

		/* DXF header start */
		fprintf ( fp, "  0\n" );
		fprintf ( fp, "SECTION\n" );
		fprintf ( fp, "  2\n" );
		fprintf ( fp, "HEADER\n" );

		/* AutoCAD DB maint. version  */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$ACADMAINTVER\n" );
		fprintf ( fp, " 70\n" );
		fprintf ( fp, "  6\n" ); /* maint. release 6 */

		/* fill mode */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$FILLMODE\n" );
		fprintf ( fp, " 70\n" );
		fprintf ( fp, "  1\n" ); /* fill hatched areas if conditions are met */

		/* viewport limits (2D setting) */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$LIMMIN\n" );
		fprintf ( fp, " 10\n" );
		f = export_float_to_str(gs->min_x); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 20\n" );
		f = export_float_to_str(gs->min_y); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$LIMMAX\n" );
		fprintf ( fp, " 10\n" );
		f = export_float_to_str(gs->max_x); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 20\n" );
		f = export_float_to_str(gs->max_y); fprintf ( fp, "%s\n", f); free (f);

		/* block size limits (3D setting) */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$EXTMIN\n" );
		fprintf ( fp, " 10\n" );
		f = export_float_to_str(gs->min_x); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 20\n" );
		f = export_float_to_str(gs->min_y); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 30\n" );
		f = export_float_to_str(gs->min_z); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$EXTMAX\n" );
		fprintf ( fp, " 10\n" );
		f = export_float_to_str(gs->max_x); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 20\n" );
		f = export_float_to_str(gs->max_y); fprintf ( fp, "%s\n", f); free (f);
		fprintf ( fp, " 30\n" );
		f = export_float_to_str(gs->max_z); fprintf ( fp, "%s\n", f); free (f);

		/* global point size */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$PDSIZE\n" );
		fprintf ( fp, " 40\n" );
		fprintf ( fp, " 0.0\n" );

		/* insertion anchor for block */
		fprintf ( fp, "  9\n" );
		fprintf ( fp, "$INSBASE\n" );
		fprintf ( fp, " 10\n" );
		fprintf ( fp, "0.0\n" ); /* anchor at 0/0/0 */
		fprintf ( fp, " 20\n" );
		fprintf ( fp, "0.0\n" ); /* anchor at 0/0/0 */
		fprintf ( fp, " 30\n" );
		fprintf ( fp, "0.0\n" ); /* anchor at 0/0/0 */

		/* end of header */
		fprintf ( fp, "  0\n" );
		fprintf ( fp, "ENDSEC\n" );

	}

	return ( fp );
}


/*
 * Helper function for DXF_write_tables():
 * Returns the DXF layer name that matches the labels for field "field_num".
 * Returned field name is a newly allocated string that must be free'd by the caller.
 * Returns NULL on error.
 */
char *dxf_get_field_layer_name ( parser_desc *parser, int field_num ) {
	int layer_idx;
	int dxf_layer_name_len;
	char *dxf_layer_name;


	if ( parser->fields[field_num] == NULL )
		return NULL;

	layer_idx = DXF_LAYER_FIRST_FIELD + field_num;

	dxf_layer_name_len = PRG_MAX_FIELD_LEN+100;

	dxf_layer_name = malloc (sizeof(char) * dxf_layer_name_len);

	if ( layer_idx < 10 ) {
		snprintf(dxf_layer_name,dxf_layer_name_len,"00%i_%s",layer_idx,parser->fields[field_num]->name);
	}
	if ( layer_idx >= 10 && layer_idx < 100 ) {
		snprintf(dxf_layer_name,dxf_layer_name_len,"0%i_%s",layer_idx,parser->fields[field_num]->name);
	}
	if ( layer_idx >= 100 ) {
		snprintf(dxf_layer_name,dxf_layer_name_len,"%i_%s",layer_idx,parser->fields[field_num]->name);
	}

	return (dxf_layer_name);
}


/*
 * Helper function for DXF_write_tables():
 * Writes a DXF "LAYER" table entry.
 * "layer" must be one of the constant strings defined in "export.c" ("DXF_LAYER_NAME_*")
 * OR "0" for the mandatory 0 layer.
 * "color" is just any integer > 0. Use "-1" to hide the layer by default (not all CAD
 * respect this).
 */
void dxf_write_layer ( const char layer[], int color, FILE *fp ) {
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "LAYER\n" );
	fprintf ( fp, "  2\n" );
	fprintf ( fp, "%s\n", layer );
	fprintf ( fp, " 70\n" );
	fprintf ( fp, "    64\n" );
	fprintf ( fp, " 62\n" );
	fprintf ( fp, "     %i\n", color );
	fprintf ( fp, "  6\n" );
	fprintf ( fp, "CONTINUOUS\n" );
}


/*
 * Helper function for export_DXF():
 * Writes "TABLES" section to DXF.
 * This part of the DXF contains entries for line styles
 * and layers that are referenced later by the actual
 * drawing objects ("ENTITIES") that will be created by export_DXF().
 */
void dxf_write_tables ( geom_store *gs, parser_desc *parser, FILE *fp, options *opts ) {
	int num_layers;
	int color;
	int i;

	if ( fp == NULL )
		return;

	/* find out how many layers we will need */
	num_layers = 1; /* there's always layer "0" */
	if ( gs->num_points > 0 )
		num_layers += 2;
	if ( gs->num_points_raw > 0 )
		num_layers += 2;
	if ( gs->num_lines > 0 )
		num_layers += 3;
	if ( gs->num_polygons > 0 )
		num_layers += 2;

	/* random colour index for layers */
	color = 1;

	/* LINE STYLES */
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "SECTION\n" );
	fprintf ( fp, "  2\n" );
	fprintf ( fp, "TABLES\n" );
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "TABLE\n" );
	fprintf ( fp, "  2\n" );
	fprintf ( fp, "LTYPE\n" );
	fprintf ( fp, " 70\n" );
	fprintf ( fp, "     1\n" );
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "LTYPE\n" );
	fprintf ( fp, "  2\n" );
	fprintf ( fp, "CONTINUOUS\n" );
	fprintf ( fp, " 70\n" );
	fprintf ( fp, "    64\n" );
	fprintf ( fp, "  3\n" );
	fprintf ( fp, "Solid line\n" );
	fprintf ( fp, " 72\n" );
	fprintf ( fp, "    65\n" );
	fprintf ( fp, " 73\n" );
	fprintf ( fp, "     0\n" );
	fprintf ( fp, " 40\n" );
	fprintf ( fp, "0.0\n" );
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "ENDTAB\n" );

	/* LAYERS (as required) */
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "TABLE\n" );
	fprintf ( fp, "  2\n" );
	fprintf ( fp, "LAYER\n" );
	fprintf ( fp, " 70\n" );
	fprintf ( fp, "      %i\n", num_layers ); /* layer "0" included */
	/* layer "0" (mandatory) */
	dxf_write_layer ( "0", color, fp );
	color ++;
	/* raw points (+labels) layer? */
	if ( gs->num_points_raw > 0 ) {
		/* raw point labels */
		dxf_write_layer ( _(DXF_LAYER_NAME_RAW_LABELS), -1, fp );
		color ++;
		/* raw  points */
		dxf_write_layer ( _(DXF_LAYER_NAME_RAW), color++, fp );
	}
	/* points (+labels) layer? */
	if ( gs->num_points > 0 ) {
		/* point labels */
		dxf_write_layer ( _(DXF_LAYER_NAME_POINT_LABELS), -1, fp );
		/* points */
		dxf_write_layer ( _(DXF_LAYER_NAME_POINT), color++, fp );
	}
	/* lines (+labels) layer? */
	if ( gs->num_lines > 0 ) {
		/* line labels */
		dxf_write_layer ( _(DXF_LAYER_NAME_LINE_LABELS), -1, fp );
		/* lines */
		dxf_write_layer ( _(DXF_LAYER_NAME_LINE), color++, fp );
	}
	/* areas/polygons (+labels) layer? */
	if ( gs->num_polygons > 0 ) {
		/* area labels */
		dxf_write_layer ( _(DXF_LAYER_NAME_AREA_LABELS), -1, fp );
		/* areas */
		dxf_write_layer ( _(DXF_LAYER_NAME_AREA), color++, fp );
	}
	if ( opts->label_field != NULL ) {
		/* user-defined labels */
		dxf_write_layer ( _(DXF_LAYER_NAME_LABELS), -1, fp );
	}
	/* add layers for all fields */
	i = 0;
	while ( parser->fields[i] != NULL ) {
		char *dxf_layer_name = dxf_get_field_layer_name ( parser, i );
		if ( dxf_layer_name != NULL ) {
			dxf_write_layer ( dxf_layer_name, color*(-1), fp );
			color ++;
			free ( dxf_layer_name );
		}
		i ++;
	}

	/* close TABLES section */
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "ENDTAB\n" );
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "ENDSEC\n" );
}


/*
 * Helper function for export_DXF():
 * Writes terminating lines (EOF) to DXF and closes file handle.
 */
void dxf_write_footer ( FILE *fp ) {

	if ( fp == NULL )
		return;

	fprintf ( fp, "  0\n" );
	fprintf ( fp, "EOF\n" );

	fclose (fp);
}


/*
 * Helper function for export_DXF():
 * Writes a simple coordinate label at position X/Y/Z.
 * "layer" must be one of the constant strings defined in "export.c" ("DXF_LAYER_NAME_*")
 * or one of the auto-generated layer names for field label layers.
 * Default is to label by "geom_id"; if "geom_id" is passed as "-1", then the point is
 * labeled using its coordinates instead (useful for raw point labels).
 */
void dxf_write_label ( FILE *fp, const char layer[],
		int geom_id, double size, double X, double Y, double Z ) {
	char *xf,*yf,*zf,*sf;


	xf = export_float_to_str(X);
	yf = export_float_to_str(Y);
	zf = export_float_to_str(Z);
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "TEXT\n" );
	fprintf ( fp, "  8\n" );
	fprintf ( fp, "%s\n", layer );
	fprintf ( fp, " 10\n" );
	fprintf ( fp, "%s\n", xf );
	fprintf ( fp, " 20\n" );
	fprintf ( fp, "%s\n", yf );
	fprintf ( fp, " 30\n" );
	fprintf ( fp, "%s\n", zf );
	fprintf ( fp, " 40\n" );
	sf = export_float_to_str(size);
	fprintf ( fp, "%s\n", sf ); /* font size */
	free (sf);
	fprintf ( fp, "  1\n" );
	if ( geom_id > -1 ) {
		/* label by geom_id */
		fprintf ( fp, "%i\n", geom_id );
	} else {
		/* label by coordinates */
		fprintf ( fp, "%s; %s; %s\n", xf, yf, zf ); /* label string */
	}
	fprintf ( fp, " 72\n" );
	fprintf ( fp, "     4\n" );
	fprintf ( fp, " 11\n" );
	fprintf ( fp, "%s\n", xf );
	fprintf ( fp, " 21\n" );
	fprintf ( fp, "%s\n", yf );
	fprintf ( fp, " 31\n" );
	fprintf ( fp, "%s\n", zf );
	free (xf);
	free (yf);
	free (zf);
}


/*
 * Helper function for export_DXF():
 * Writes a simple text "label" at position X/Y/Z.
 * "layer" must be one of the constant strings defined in "export.c" ("DXF_LAYER_NAME_*").
 * or one of the auto-generated layer names for field label layers.
 */
void dxf_write_label_text ( FILE *fp, const char layer[],
		const char *label, double size, double X, double Y, double Z ) {
	char *xf,*yf,*zf,*sf;

	xf = export_float_to_str(X);
	yf = export_float_to_str(Y);
	zf = export_float_to_str(Z);
	fprintf ( fp, "  0\n" );
	fprintf ( fp, "TEXT\n" );
	fprintf ( fp, "  8\n" );
	fprintf ( fp, "%s\n", layer );
	fprintf ( fp, " 10\n" );
	fprintf ( fp, "%s\n", xf );
	fprintf ( fp, " 20\n" );
	fprintf ( fp, "%s\n", yf );
	fprintf ( fp, " 30\n" );
	fprintf ( fp, "%s\n", zf );
	fprintf ( fp, " 40\n" );
	sf = export_float_to_str(size);
	fprintf ( fp, "%s\n", sf ); /* font size */
	free (sf);
	fprintf ( fp, "  1\n" );
	fprintf ( fp, "%s\n", label ); /* label string */
	fprintf ( fp, " 72\n" );
	fprintf ( fp, "     4\n" );
	fprintf ( fp, " 11\n" );
	fprintf ( fp, "%s\n", xf );
	fprintf ( fp, " 21\n" );
	fprintf ( fp, "%s\n", yf );
	fprintf ( fp, " 31\n" );
	fprintf ( fp, "%s\n", zf );
	free (xf);
	free (yf);
	free (zf);
}


/*
 * Writes geometry store objects into a new, single DXF file, with several
 * layers, one for each geometry type, if required. Calls "export_dxf_write_atts()"
 * to create a simple text file with associated attribute data.
 *
 * By the time this function is called, "gs" must be a valid geometry
 * store and must contain only fully checked and built, valid geometries,
 * ready to be written to disk.
 *
 * If "force 2D" has been set, then all Z coordinate values will be "0.0".
 *
 * Returns number of attribute field errors.
 * */
int export_DXF ( geom_store *gs, parser_desc *parser, options *opts )
{
	double *X,*Y,*Z;
	double X_mean, Y_mean, Z_mean;
	char *xf,*yf,*zf;
	int i, j, k, l, v, num_errors, att_errors;
	char *dxf_layer_name;
	int num_vertices;
	FILE *fatts;
	FILE *fdxf;
	char **atts;


	if ( gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons < 1 )  {
		err_show (ERR_NOTE,"");
		err_show (ERR_WARN, _("\nNo valid geometries found. No output produced."));
		return ( 0 );
	}

	/* create all required output files */

	/*
	 * create text output files for attributes
	 */
	fatts = export_DXF_make_TXT ( parser, gs->path_all_atts );
	if (  fatts == NULL ) {
		err_show (ERR_NOTE,"");
		err_show (ERR_EXIT, _("\nError creating TXT output file for attribute data\n(%s)."),
				gs->path_all_atts );
		return ( 0 );
	}

	/* write DXF header (global settings) */
	fdxf = dxf_write_header ( gs, gs->path_all );
	if ( fdxf == NULL ) {
		err_show (ERR_NOTE,"");
		err_show (ERR_EXIT, _("\nError creating DXF output file\n(%s)."),
				gs->path_all );
		return ( 0 );
	}

	/* write "TABLES" section of DXF */
	dxf_write_tables ( gs, parser, fdxf, opts );

	/* WRITE GEOMETRIES TO DXF */

	/* start DXF section "ENTITIES" (drawing objects) */
	fprintf ( fdxf, "  0\n" );
	fprintf ( fdxf, "SECTION\n" );
	fprintf ( fdxf, "  2\n" );
	fprintf ( fdxf, "ENTITIES\n" );

	num_errors = 0;

	/* POLYGONS */
	for (i = 0; i < gs->num_polygons; i ++ ) {
		if ( gs->polygons[i].is_selected == TRUE ) {
			int *start;
			int vertex;

			start = malloc ( sizeof (int) * gs->polygons[i].num_parts );
			num_vertices = 0;
			/* get total number of vertices and starting index for each
		   part of a multi-part polygon. */
			for ( j=0; j < gs->polygons[i].num_parts; j++ ) {
				start[j] = num_vertices;
				num_vertices += gs->polygons[i].parts[j].num_vertices;
			}
			X = malloc ( sizeof ( double ) * num_vertices );
			Y = malloc ( sizeof ( double ) * num_vertices );
			Z = malloc ( sizeof ( double ) * num_vertices );
			vertex = 0;
			for ( j=0; j < gs->polygons[i].num_parts; j++ ) {
				if (gs->polygons[i].parts[j].is_hole == FALSE ) { /* we do not process polygon holes in DXF */
					fprintf ( fdxf, "  0\n" );
					fprintf ( fdxf, "POLYLINE\n" );
					fprintf ( fdxf, "  5\n" );
					fprintf ( fdxf, "%u\n", gs->polygons[i].geom_id ); /* OBJECT HANDLE */
					fprintf ( fdxf, "  8\n" );
					fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_AREA) );
					fprintf ( fdxf, " 66\n" );
					fprintf ( fdxf, "  1\n" );
					for ( k = 0; k < gs->polygons[i].parts[j].num_vertices; k++ ) {
						X[vertex] = gs->polygons[i].parts[j].X[k];
						Y[vertex] = gs->polygons[i].parts[j].Y[k];
						xf = export_float_to_str(X[vertex]);
						yf = export_float_to_str(Y[vertex]);
						if ( gs->polygons[i].is_3D == TRUE && opts->force_2d == FALSE ) {
							Z[vertex] = gs->polygons[i].parts[j].Z[k];
						} else {
							Z[vertex] = 0.0;
						}
						zf = export_float_to_str(Z[vertex]);
						fprintf ( fdxf, "  0\n" );
						fprintf ( fdxf, "VERTEX\n" );
						fprintf ( fdxf, "  8\n" );
						fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_AREA) );
						fprintf ( fdxf, " 10\n" );
						fprintf ( fdxf, "%s\n", xf );
						fprintf ( fdxf, " 20\n" );
						fprintf ( fdxf, "%s\n", yf );
						fprintf ( fdxf, " 30\n" );
						fprintf ( fdxf, "%s\n", zf );
						free ( xf );
						free ( yf );
						free ( zf );
						vertex ++;
					}
					fprintf ( fdxf, "  0\n" );
					fprintf ( fdxf, "SEQEND\n" );
					fprintf ( fdxf, "  8\n" );
					fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_AREA) );
					/* area label (per part): place label at centroid */
					X_mean = 0;
					Y_mean = 0;
					Z_mean = 0;
					for ( v = 0; v < gs->polygons[i].parts[j].num_vertices; v ++ ) {
						X_mean = X_mean + X [ v ];
						Y_mean = Y_mean + Y [ v ];
						Z_mean = Z_mean + Z [ v ];
					}
					X_mean = X_mean / gs->polygons[i].parts[j].num_vertices;
					Y_mean = Y_mean / gs->polygons[i].parts[j].num_vertices;
					Z_mean = Z_mean / gs->polygons[i].parts[j].num_vertices;
					dxf_write_label ( fdxf, _(DXF_LAYER_NAME_AREA_LABELS),
							gs->polygons[i].geom_id-1, DXF_LABEL_SIZE_AREA, X_mean, Y_mean, Z_mean );
					/* attributes (to text file) */
					att_errors = num_errors += export_DXF_write_atts (fatts, gs, GEOM_TYPE_POLY, i, gs->polygons[i].geom_id, parser, opts);
					num_errors += att_errors;
					/* export all fields as labels */
					if ( att_errors == 0 ) {
						atts = gs->polygons[i].atts;
						l = 0;
						while (parser->fields[l] != NULL ) {
							if ( atts[l] != NULL ) {
								dxf_layer_name = dxf_get_field_layer_name ( parser, l );
								if ( dxf_layer_name != NULL ) {
									dxf_write_label_text ( fdxf, dxf_layer_name, atts[l], DXF_LABEL_SIZE_AREA, X_mean, Y_mean, Z_mean );
									free ( dxf_layer_name );
								}
							}
							l++;
						}
					}
				}
			}
			free ( X );
			free ( Y );
			free ( Z );
			free ( start );
		}
	}

	/* LINES */
	for (i = 0; i < gs->num_lines; i ++ ) {
		if ( gs->lines[i].is_selected == TRUE ) {
			int *start;
			int vertex;

			start = malloc ( sizeof (int) * gs->lines[i].num_parts );
			num_vertices = 0;
			/* get total number of vertices and starting index for each
		   part of a multi-part line string. */
			for ( j=0; j < gs->lines[i].num_parts; j++ ) {
				start[j] = num_vertices;
				num_vertices += gs->lines[i].parts[j].num_vertices;
			}
			X = malloc ( sizeof ( double ) * num_vertices );
			Y = malloc ( sizeof ( double ) * num_vertices );
			Z = malloc ( sizeof ( double ) * num_vertices );
			vertex = 0;
			for ( j=0; j < gs->lines[i].num_parts; j++ ) {
				fprintf ( fdxf, "  0\n" );
				fprintf ( fdxf, "POLYLINE\n" );
				fprintf ( fdxf, "  5\n" );
				fprintf ( fdxf, "%u\n", gs->lines[i].geom_id ); /* OBJECT HANDLE */
				fprintf ( fdxf, "  8\n" );
				fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_LINE) );
				fprintf ( fdxf, " 66\n" );
				fprintf ( fdxf, "  1\n" );
				for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k++ ) {
					X[vertex] = gs->lines[i].parts[j].X[k];
					Y[vertex] = gs->lines[i].parts[j].Y[k];
					xf = export_float_to_str(X[vertex]);
					yf = export_float_to_str(Y[vertex]);
					if ( gs->lines[i].is_3D == TRUE && opts->force_2d == FALSE ) {
						Z[vertex] = gs->lines[i].parts[j].Z[k];
					} else {
						Z[vertex] = 0.0;
					}
					zf = export_float_to_str(Z[vertex]);
					fprintf ( fdxf, "  0\n" );
					fprintf ( fdxf, "VERTEX\n" );
					fprintf ( fdxf, "  8\n" );
					fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_LINE) );
					fprintf ( fdxf, " 10\n" );
					fprintf ( fdxf, "%s\n", xf );
					fprintf ( fdxf, " 20\n" );
					fprintf ( fdxf, "%s\n", yf );
					fprintf ( fdxf, " 30\n" );
					fprintf ( fdxf, "%s\n", zf );
					free ( xf );
					free ( yf );
					free ( zf );
					vertex ++;
				}
				fprintf ( fdxf, "  0\n" );
				fprintf ( fdxf, "SEQEND\n" );
				fprintf ( fdxf, "  8\n" );
				fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_LINE) );
				/* line label (per part): place label at central vertex */
				X_mean = X[(gs->lines[i].parts[j].num_vertices / 2) - 1];
				if ( X_mean < 0 ) X_mean = 0;
				Y_mean = Y[(gs->lines[i].parts[j].num_vertices / 2) - 1];
				if ( Y_mean < 0 ) Y_mean = 0;
				Z_mean = Z[(gs->lines[i].parts[j].num_vertices / 2) - 1];
				if ( Z_mean < 0 ) Z_mean = 0;
				dxf_write_label ( fdxf, _(DXF_LAYER_NAME_LINE_LABELS),
						gs->lines[i].geom_id-1, DXF_LABEL_SIZE_LINE, X_mean, Y_mean, Z_mean );
				/* attributes (to text file) */
				att_errors = num_errors += export_DXF_write_atts (fatts, gs, GEOM_TYPE_LINE, i, gs->lines[i].geom_id, parser, opts);
				num_errors += att_errors;
				/* export all fields as labels */
				if ( att_errors == 0 ) {
					atts = gs->lines[i].atts;
					l = 0;
					while (parser->fields[l] != NULL ) {
						if ( atts[l] != NULL ) {
							dxf_layer_name = dxf_get_field_layer_name ( parser, l );
							if ( dxf_layer_name != NULL ) {
								dxf_write_label_text ( fdxf, dxf_layer_name, atts[l], DXF_LABEL_SIZE_LINE, X_mean, Y_mean, Z_mean );
								free ( dxf_layer_name );
							}
						}
						l++;
					}
				}
			}
			free ( X );
			free ( Y );
			free ( Z );
			free ( start );
		}
	}

	/* POINTS */
	for (i = 0; i < gs->num_points; i ++ ) {
		if ( gs->points[i].is_selected == TRUE ) {
			X = malloc ( sizeof ( double ) );
			Y = malloc ( sizeof ( double ) );
			Z = malloc ( sizeof ( double ) );
			X[0] = gs->points[i].X;
			Y[0] = gs->points[i].Y;
			xf = export_float_to_str(X[0]);
			yf = export_float_to_str(Y[0]);
			if ( gs->points[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				Z[0] = gs->points[i].Z;
			} else {
				Z[0] = 0.0;
			}
			zf = export_float_to_str(Z[0]);
			/* actual points */
			fprintf ( fdxf, "  0\n" );
			fprintf ( fdxf, "POINT\n" );
			fprintf ( fdxf, "  5\n" );
			fprintf ( fdxf, "%u\n", gs->points[i].geom_id ); /* OBJECT HANDLE */
			fprintf ( fdxf, "  8\n" );
			fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_POINT) );
			fprintf ( fdxf, " 10\n" );
			fprintf ( fdxf, "%s\n", xf );
			fprintf ( fdxf, " 20\n" );
			fprintf ( fdxf, "%s\n", yf );
			fprintf ( fdxf, " 30\n" );
			fprintf ( fdxf, "%s\n", zf );
			free ( xf );
			free ( yf );
			free ( zf );
			/* point labels (attach geom ID) */
			dxf_write_label ( fdxf, _(DXF_LAYER_NAME_POINT_LABELS),
					gs->points[i].geom_id-1, DXF_LABEL_SIZE_POINT, X[0], Y[0], Z[0] );
			/* attributes (to text file) */
			att_errors = export_DXF_write_atts (fatts, gs, GEOM_TYPE_POINT, i, gs->points[i].geom_id, parser, opts);
			num_errors += att_errors;
			/* export all fields as labels */
			if ( att_errors == 0 ) {
				atts = gs->points[i].atts;
				l = 0;
				while (parser->fields[l] != NULL ) {
					if ( atts[l] != NULL ) {
						dxf_layer_name = dxf_get_field_layer_name ( parser, l );
						if ( dxf_layer_name != NULL ) {
							dxf_write_label_text ( fdxf, dxf_layer_name, atts[l], DXF_LABEL_SIZE_POINT, X[0], Y[0], Z[0] );
							free ( dxf_layer_name );
						}
					}
					l++;
				}
			}
			free ( X );
			free ( Y );
			free ( Z );
		}
	}

	/* RAW VERTICES */
	for (i = 0; i < gs->num_points_raw; i ++ ) {
		if ( gs->points_raw[i].is_selected == TRUE ) {
			X = malloc ( sizeof ( double ) );
			Y = malloc ( sizeof ( double ) );
			Z = malloc ( sizeof ( double ) );
			X[0] = gs->points_raw[i].X;
			Y[0] = gs->points_raw[i].Y;
			xf = export_float_to_str(X[0]);
			yf = export_float_to_str(Y[0]);
			if ( gs->points_raw[i].is_3D == TRUE && opts->force_2d == FALSE ) {
				Z[0] = gs->points_raw[i].Z;
			} else {
				Z[0] = 0.0;
			}
			zf = export_float_to_str(Z[0]);
			/* actual raw points */
			fprintf ( fdxf, "  0\n" );
			fprintf ( fdxf, "POINT\n" );
			fprintf ( fdxf, "  5\n" );
			fprintf ( fdxf, "%u\n", gs->points_raw[i].geom_id ); /* OBJECT HANDLE */
			fprintf ( fdxf, "  8\n" );
			fprintf ( fdxf, "%s\n", _(DXF_LAYER_NAME_RAW) );
			fprintf ( fdxf, " 10\n" );
			fprintf ( fdxf, "%s\n", xf );
			fprintf ( fdxf, " 20\n" );
			fprintf ( fdxf, "%s\n", yf );
			fprintf ( fdxf, " 30\n" );
			fprintf ( fdxf, "%s\n", zf );
			free ( xf );
			free ( yf );
			free ( zf );
			/* raw point labels (attach coordinates) */
			dxf_write_label ( fdxf, _(DXF_LAYER_NAME_RAW_LABELS),
					-1, DXF_LABEL_SIZE_RAW, X[0], Y[0], Z[0] );
			/* attributes (to text file) */
			att_errors = export_DXF_write_atts (fatts, gs, GEOM_TYPE_POINT_RAW, i, gs->points_raw[i].geom_id, parser, opts);
			num_errors += att_errors;
			free ( X );
			free ( Y );
			free ( Z );
			/* we do not output attributes for raw points */
		}
	}

	/* LABELS */
	if ( opts->label_field != NULL ) {
		/* Find label test field */
		int field_num = 0;
		while ( parser->fields[field_num] != NULL ) {
			if ( !strcasecmp (parser->fields[field_num]->name, opts->label_field)  ) {
				break;
			}
			field_num ++;
		}
		for (i = 0; i < gs->num_points; i ++ ) {
			if ( gs->points[i].is_selected == TRUE || gs->points[i].has_label == TRUE ) {
				atts = gs->points[i].atts;
				double X = gs->points[i].label_x;
				double Y = gs->points[i].label_y;
				double Z = 0.0;
				dxf_write_label_text ( fdxf, DXF_LAYER_NAME_LABELS, gs->points[i].atts[field_num], DXF_LABEL_SIZE_USER, X, Y, Z );
			} else {
				/* Warn: no label for this point. */
				err_show (ERR_NOTE,"");
				err_show (ERR_WARN, _("\nFailed to place label at point #%i."), i );
			}
		}
		for (i = 0; i < gs->num_lines; i ++ ) {
			if ( gs->lines[i].is_selected == TRUE ) {
				atts = gs->lines[i].atts;
				int p;
				for (p = 0; p < gs->lines[i].num_parts; p ++ ) {
					if ( gs->lines[i].parts[p].has_label == TRUE ) {
						double X = gs->lines[i].parts[p].label_x;
						double Y = gs->lines[i].parts[p].label_y;
						double Z = 0.0;
						dxf_write_label_text ( fdxf, DXF_LAYER_NAME_LABELS, gs->lines[i].atts[field_num], DXF_LABEL_SIZE_USER, X, Y, Z );
					} else {
						/* Warn: no label for this line (part). */
						if ( opts->label_mode_line == OPTIONS_LABEL_MODE_CENTER ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nFailed to place label at center of line #%i (part #%i)."), i, p );
						}
						if ( opts->label_mode_line == OPTIONS_LABEL_MODE_FIRST ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nFailed to place label at first vertex of line #%i (part #%i)."), i, p );
						}
						if ( opts->label_mode_line == OPTIONS_LABEL_MODE_LAST ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nFailed to place label at last vertex of line #%i (part #%i)."), i, p );
						}
					}
				}
			}
		}
		for (i = 0; i < gs->num_polygons; i ++ ) {
			if ( gs->polygons[i].is_selected == TRUE ) {
				atts = gs->polygons[i].atts;
				int p;
				for (p = 0; p < gs->polygons[i].num_parts; p ++ ) {
					if ( gs->polygons[i].parts[p].has_label == TRUE ) {
						double X = gs->polygons[i].parts[p].label_x;
						double Y = gs->polygons[i].parts[p].label_y;
						double Z = 0.0;
						dxf_write_label_text ( fdxf, DXF_LAYER_NAME_LABELS, gs->polygons[i].atts[field_num], DXF_LABEL_SIZE_USER, X, Y, Z );
					} else {
						if ( gs->polygons[i].parts[p].is_hole == FALSE ) {
							/* Warn: no label for this polygon (part), even though it is not a hole. */
							if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_CENTER ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at center of polygon #%i (part #%i)."), i, p );
							}
							if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_FIRST ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at first vertex of polygon #%i (part #%i)."), i, p );
							}
							if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_LAST ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nFailed to place label at last vertex of polygon #%i (part #%i)."), i, p );
							}
						}
					}
				}
			}
		}
	}

	/* close DXF section "ENTITIES" (drawing objects) */
	fprintf ( fdxf, "  0\n" );
	fprintf ( fdxf, "ENDSEC\n" );

	/* close attributes output file */
	fclose ( fatts );

	/* write DXF footer and close */
	dxf_write_footer ( fdxf );

	return ( num_errors );
}
