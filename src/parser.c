/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	parser.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	ASCII parser for geometries and attributes.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "global.h"
#include "errors.h"
#include "geom.h"
#include "i18n.h"
#include "options.h"
#include "parser.h"
#include "tools.h"


#ifdef GUI
#include <gtk/gtk.h>
#include "gui_form.h"
#endif

/* PRIVATE functions */
void parser_field_destroy ( parser_field *field );


/*
 * Set the contents of a parser record to default values.
 */
void parser_record_init ( parser_record *rec ) {
	rec->line = -1;
	rec->contents = NULL;
	rec->skip = NULL;
	rec->x = 0.0;
	rec->y = 0.0;
	rec->z = 0.0;
	rec->geom_id = 0;
	rec->part_id = 0;
	rec->tag = NULL;
	rec->key = NULL;
	rec->geom_type = GEOM_TYPE_NONE;
	rec->written_out = FALSE;
	rec->is_valid = FALSE;
	rec->is_empty = TRUE;
}


/*
 * Creates a new data store for a parser.
 * Its initial storage capacity is PARSER_DATA_STORE_CHUNK records.
 *
 * Returns NULL on error, otherwise a new, empty
 * data store with default settings.
 */
parser_data_store *parser_data_store_create ( const char *input, parser_desc *parser,  options *opts )
{
	parser_data_store *ds;
	int i;


	ds = NULL;

	if ( input == NULL || parser == NULL || opts == NULL )
		return ( NULL );

	if ( strlen ( input ) < 1 )
		return ( NULL );

	ds = malloc ( sizeof ( parser_data_store ) );
	if ( ds == NULL )
		return ( NULL );
	ds->slot = 0;
	ds->input = strdup ( input );
	ds->offset_x = opts->offset_x;
	ds->offset_y = opts->offset_y;
	ds->offset_z = opts->offset_z;
	i = 0;
	while ( parser->fields[i] != NULL)
		i++;
	ds->num_fields = i;
	ds->num_records = PARSER_DATA_STORE_CHUNK;
	ds->num_points = 0;
	ds->num_lines = 0;
	ds->num_polygons = 0;
	ds->space_left = PARSER_DATA_STORE_CHUNK;
	ds->records = malloc ( sizeof (parser_record) * PARSER_DATA_STORE_CHUNK );
	if ( ds->records == NULL ) {
		free ( ds );
		return ( NULL );
	}
	for ( i = 0; i < PARSER_DATA_STORE_CHUNK; i++ ) {
		parser_record_init ( &ds->records[i] );
	}
	ds->output_points = NULL;
	ds->output_points_raw = NULL;
	ds->output_lines = NULL;
	ds->output_polygons = NULL;
	return ( ds );
}


/*
 * Release memory for a parser data store and all records
 * associated with it.
 */
void parser_data_store_destroy ( parser_data_store *ds )
{
	int i, j;


	if ( ds->input != NULL )
		free ( ds->input );

	for ( i = 0; i < ds->num_records; i ++ ) {
		if ( ds->records[i].is_empty == TRUE ) {
			free ( ds->records[i].contents );
		} else {
			if ( ds->records[i].contents != NULL ) {
				for ( j = 0; j < ds->num_fields; j ++ ) {
					if ( ds->records[i].contents[j] != NULL ) {
						free ( ds->records[i].contents[j] );
					}
				}
				free ( ds->records[i].contents );
			}
			if ( ds->records[i].tag != NULL )
				free ( ds->records[i].tag );
			if ( ds->records[i].skip != NULL ) {
				free ( ds->records[i].skip );
			}
		}
	}
	free ( ds->records );
	if ( ds->output_points != NULL)
		free ( ds->output_points );
	if ( ds->output_points_raw != NULL)
		free ( ds->output_points_raw );
	if ( ds->output_lines != NULL)
		free ( ds->output_lines );
	if ( ds->output_polygons != NULL)
		free ( ds->output_polygons );
	free ( ds );
}


/*
 * Stores a new record in a data store.
 * The data store's capacity will be increased as needed,
 * in chunks of PARSER_DATA_STORE_CHUNK size.
 * This function does intensive error checking.
 *
 * Returns 0 if the data was stored OK, 1 otherwise.
 * In case of error, a more descriptive error message
 * is also stored in the global "err_message" char[].
 */
int parser_record_store ( 	const char **contents, int num_fields_read, unsigned int line_no,
		parser_data_store *ds, parser_desc *parser, options *opts )
{
	int i,j;
	char *p;
	BOOLEAN DEBUG = FALSE;


	if ( contents == NULL || ds == NULL || opts == NULL ) {
		err_msg_set (_("Record to be stored contains no data."));
		return ( 1 );
	}

	/* memory management */
	if ( ds->space_left < 1 ) {
		if ( DEBUG == TRUE ) {
			fprintf ( stderr, "STORE: allocating %i more slots.\n", PARSER_DATA_STORE_CHUNK );
		}
		/* out of storage space: get more mem */
		void *new_mem;
		new_mem = realloc ( (void*) ds->records, sizeof ( parser_record ) *
				( ds->num_records + PARSER_DATA_STORE_CHUNK ) );
		if ( new_mem == NULL ) {
			err_msg_set (_("Out of storage memory."));
			return ( 1 );
		}
		ds->records = (parser_record*) new_mem;
		/* initialize new empty records */
		for ( i=ds->num_records; i < PARSER_DATA_STORE_CHUNK+ds->num_records; i ++ ) {
			parser_record_init ( &ds->records[i] );
		}
		ds->space_left = PARSER_DATA_STORE_CHUNK;
		ds->num_records += PARSER_DATA_STORE_CHUNK;
		if ( DEBUG == TRUE ) {
			fprintf ( stderr, "STORE: new capacity: %ui.\n", ds->num_records );
		}
	}

	if ( ds->records[ds->slot].is_empty == FALSE ) {
		/* slot taken: this is an error */
		err_msg_set (_("Storage slot already contains data."));
		return ( 1 );
	}

	/* store record in current available slot */
	ds->records[ds->slot].contents = malloc ( sizeof (char*) * ds->num_fields );
	if ( ds->records[ds->slot].contents == NULL ) {
		err_msg_set (_("Out of storage memory."));
		return ( 1 );
	}
	for ( i = 0; i < ds->num_fields; i ++ ) {
		if ( contents[i] != NULL ) {
			ds->records[ds->slot].contents[i] = strdup ( contents[i] );
			if ( parser->fields[i]->conversion_mode_set == TRUE ) {
				/* convert to all upper or all lower characters */
				p = ds->records[ds->slot].contents[i];
				while ( *p != '\0' ) {
					if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_UPPER ) {
						*p = toupper ((int) *p);
					}
					if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_LOWER ) {
						*p = tolower ((int) *p);
					}
					p += sizeof ( char );
				}
			}
			if ( parser->fields[i]->has_lookup == TRUE ) {
				/* replace string value with lookup value */
				j = 0;
				while ( parser->fields[i]->lookup_old[j] != NULL ) {
					if ( !strcasecmp ( parser->fields[i]->lookup_old[j],
							ds->records[ds->slot].contents[i] ) )
					{
						if ( ds->records[ds->slot].contents[i] != NULL )
							free ( ds->records[ds->slot].contents[i] );
						ds->records[ds->slot].contents[i] =
								strdup (parser->fields[i]->lookup_new[j]);
						break;
					}
					j ++;
				}
			}
		} else {
			ds->records[ds->slot].contents[i] = NULL;
		}
	}
	ds->records[ds->slot].line = line_no;
	ds->records[ds->slot].is_empty = FALSE;

	ds->records[ds->slot].skip = malloc (sizeof(BOOLEAN) * ds->num_fields);
	for ( i = 0; i < ds->num_fields; i ++ ) {
		ds->records[ds->slot].skip[i] = FALSE;
	}

	/* if tag mode is "min", then we might have just read a reduced number
	   of fields and we now need to reshuffle */
	if ( parser->tag_mode == PARSER_TAG_MODE_MIN ) {
		/* in tag mode "min", there may be reduced lines */
		int num_fields_required = 0;
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( parser->fields[i]->persistent == TRUE ) {
				num_fields_required ++;
			}
		}
		if ( num_fields_required == num_fields_read ) {
			/* It looks likely that this is a valid but reduced record:
			   reshuffle fields to their intended positions! */
			int pos = 0;
			char *val = NULL;
			for ( i = 0; i < ds->num_fields; i ++ ) {
				if ( ds->records[ds->slot].contents[pos] != NULL ) {
					val = strdup (ds->records[ds->slot].contents[pos]);
					free ( ds->records[ds->slot].contents[pos] );
					ds->records[ds->slot].contents[pos] = NULL;
				}
				if ( parser->fields[i] != NULL ) {
					if ( 	parser->fields[i]->persistent ||
							( parser->coor_x != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_x ) ) ||
							( parser->coor_y != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_y ) ) ||
							( parser->coor_z != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_z ) )
					)
					{
						if ( val != NULL ) {
							ds->records[ds->slot].contents[pos] = strdup (val);
							free (val);
							val = NULL;
							ds->records[ds->slot].skip[i] = FALSE; /* will be included in validation */
						}
					} else {
						ds->records[ds->slot].contents[pos] = NULL;
						ds->records[ds->slot].skip[i] = TRUE; /* will be excluded from validation */
					}
				}
				pos ++;
			}
		}
	}

	if ( DEBUG == TRUE ) {
		fprintf ( stderr, "\nSTORE: line no. %ui: [", line_no );
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( i < ds->num_fields-1 ) {
				if ( ds->records[ds->slot].contents[i] != NULL ) {
					fprintf ( stderr, "%s, ", ds->records[ds->slot].contents[i] );
				} else {
					fprintf ( stderr, "(null), " );
				}
			}
			else {
				if ( ds->records[ds->slot].contents[i] != NULL ) {
					fprintf ( stderr, "%s", ds->records[ds->slot].contents[i] );
				} else {
					fprintf ( stderr, "(null)" );
				}
			}
		}
		fprintf ( stderr, "]\n" );
	}

	/* advance pointer to next free slot */
	ds->slot ++;
	/* decrease free capacity */
	ds->space_left --;

	if ( DEBUG == TRUE ) {
		fprintf ( stderr, "STORE: free capacity: %ui.\n", ds->space_left );
	}

	return ( 0 );
}


/*
 * Validates the contents of one record (line) read from
 * an input file by the parser. Basically, this checks
 * whether all the fields on the line exist and have the
 * data types required by the parser's schema definition.
 *
 * In addition, this function extracts coordinates from
 * the records and stores them (including any offsets)
 * in the dedicated double type coordinate fields.
 *
 * Returns "0" on success, otherwise an int > 0.
 */
int parser_record_validate_store_coords ( 	unsigned int slot, int num_fields_read,
		parser_data_store *ds, parser_desc *parser, options *opts )

{
	int i;
	char *input;
	double dvalue;
	int num_fields_required;
	BOOLEAN error;
	BOOLEAN overflow;
	BOOLEAN valid = TRUE;
	BOOLEAN DEBUG = FALSE;

	if ( ds == NULL ) {
		err_msg_set (_("Uninitialized data store specified."));
		return ( 1 );
	}

	if ( slot >= ds->num_records ) {
		err_msg_set (_("Slot number out of bounds."));
		return ( 1 );
	}

	if ( num_fields_read < 1 || num_fields_read >= PRG_MAX_FIELDS ) {
		err_msg_set (_("Invalid number of fields read."));
		return ( 1 );
	}

	if ( opts == NULL ) {
		err_msg_set (_("Uninitialized option set specified."));
		return ( 1 );
	}

	if ( parser == NULL ) {
		err_msg_set (_("Uninitialized parser specified."));
		return ( 1 );
	}


	/* replace "-" with proper name */
	if ( !strcmp ("-",ds->input) ) {
		input = strdup ("<console input stream>");
	} else {
		input = strdup (ds->input);
	}

	if ( DEBUG == TRUE ) {
		fprintf ( stderr, "VALIDATE: rec no. %ui, read from line %ui, input '%s'.\n",
				slot, ds->records[slot].line, input );
	}

	/* compute number of fields expected in a valid record */
	if ( parser->tag_mode != PARSER_TAG_MODE_MIN ) {
		num_fields_required = ds->num_fields;
	} else {
		/* In tag mode "min", there may be reduced lines:
		   Determine how many fields a reduced line must have! */
		num_fields_required = 0;
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( parser->fields[i]->persistent == TRUE )
				/* coordinate fields also count: they are persistent by default! */
				num_fields_required ++;
		}
	}

	/* check if we have the required number of fields */
	if ( parser->tag_mode != PARSER_TAG_MODE_MIN ) {
		if (  num_fields_read < num_fields_required ) {
			err_show ( ERR_NOTE, "" );
			err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nFound only %i out of %i expected fields.\nRecord skipped."),
					ds->records[slot].line, ds->input,
					num_fields_read,
					num_fields_required );
			valid = FALSE;
		}
	} else {
		if (  num_fields_read < ds->num_fields && (num_fields_read != num_fields_required) ) {
			err_show ( ERR_NOTE, "" );
			err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nFound %i instead of either %i or %i fields.\nRecord skipped."),
					ds->records[slot].line, ds->input,
					num_fields_read,
					ds->num_fields,
					num_fields_required );
			valid = FALSE;
		}
	}

	/* mode "Min": check that reduced records have valid structure */
	if ( parser->tag_mode == PARSER_TAG_MODE_MIN ) {
		if (  num_fields_read == num_fields_required ) {
			for ( i = 0; i < ds->num_fields; i ++ ) {
				BOOLEAN is_coordinate_field = FALSE;
				if (	( parser->coor_x != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_x ) ) ||
						( parser->coor_y != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_y ) ) ||
						( parser->coor_z != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_z ) ) )
				{
					is_coordinate_field = TRUE;
				}
				if ( parser->fields[i]->value != NULL || ( parser->fields[i]->persistent == FALSE && is_coordinate_field == FALSE) ) {
					/* none of these fields should have an associated value */
					if ( ds->records[slot].contents[i] != NULL ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nThis is neither a valid reduced nor full record.\nRecord skipped."),
								ds->records[slot].line, ds->input );
						valid = FALSE;
						break;
					}
				} else {
					/* all of these fields should have an associated value */
					if ( ds->records[slot].contents[i] == NULL ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nThis is neither a valid reduced nor full record.\nRecord skipped."),
								ds->records[slot].line, ds->input );
						valid = FALSE;
						break;
					}
				}
			}
		}
	}

	/* check if empty fields are allowed to be empty */
	for ( i = 0; i < ds->num_fields; i ++ ) {
		if ( valid == TRUE && parser->fields[i]->empty_allowed == FALSE &&
				ds->records[slot].contents[i] == NULL &&
				ds->records[slot].skip == FALSE ) {
			err_show ( ERR_NOTE, "" );
			err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nField '%s' must not be empty.\nRecord skipped."),
					ds->records[slot].line, ds->input,
					parser->fields[i]->name );
			valid = FALSE;
		}
	}

	/* check for valid coordinate fields and store coordinates plus offset in record data structure */
	for ( i = 0; i < ds->num_fields; i ++ ) {
		/* X coordinate */
		if ( valid == TRUE && !strcasecmp ( parser->coor_x, parser->fields[i]->name ) ) {
			/* got X coordinate field */
			dvalue = t_str_to_dbl ( ds->records[slot].contents[i], opts->decimal_point[0], opts->decimal_group[0],
					&error, &overflow );
			if ( error == TRUE && overflow == FALSE) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nInvalid X coordinate ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( overflow == TRUE ) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nX coordinate overflow ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( valid == TRUE ) {
				ds->records[slot].x = dvalue + opts->offset_x;
			}
		}
		/* Y coordinate */
		if ( valid == TRUE && !strcasecmp ( parser->coor_y, parser->fields[i]->name ) ) {
			/* got Y coordinate field */
			dvalue = t_str_to_dbl ( ds->records[slot].contents[i], opts->decimal_point[0], opts->decimal_group[0],
					&error, &overflow );
			if ( error == TRUE && overflow == FALSE) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nInvalid Y coordinate ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( overflow == TRUE ) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nY coordinate overflow ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( valid == TRUE ) {
				ds->records[slot].y = dvalue + opts->offset_y;
				/* DEBUG */
				/* fprintf (stderr,"STORED Y = %.3f\n", ds->records[slot].y); */
			}
		}
		/* Z coordinate */
		if ( valid == TRUE && parser->coor_z != NULL && !strcasecmp ( parser->coor_z, parser->fields[i]->name ) ) {
			/* got Z coordinate field */
			dvalue = t_str_to_dbl ( ds->records[slot].contents[i], opts->decimal_point[0], opts->decimal_group[0],
					&error, &overflow );
			if ( error == TRUE && overflow == FALSE) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nInvalid Z coordinate ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( overflow == TRUE ) {
				err_show ( ERR_NOTE, "" );
				err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nZ coordinate overflow ('%s').\nRecord skipped."),
						ds->records[slot].line, ds->input,
						ds->records[slot].contents[i] );
				valid = FALSE;
			}
			if ( valid == TRUE ) {
				ds->records[slot].z = dvalue + opts->offset_z;
			}
		}
	}
	/* set Z to constant "0" + Z offset if we have 2D coordinates */
	if ( valid == TRUE && parser->coor_z == NULL ) {
		ds->records[slot].z = opts->offset_z;
	}

	if ( DEBUG == TRUE && valid == TRUE ) {
		fprintf ( stderr, "VALIDATE: coordinates = {%.4f, %.4f, %.4f}\n",
				ds->records[slot].x,
				ds->records[slot].y,
				ds->records[slot].z);
	}

	/* check for correct data types (try conversion) */
	if ( valid == TRUE ) {
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( ds->records[slot].skip[i] == FALSE ) {
				if ( ds->records[slot].contents[i] == NULL ) {
					err_show ( ERR_NOTE, "" );
					err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nValue of field is 'null'.\nRecord skipped."),
							ds->records[slot].line, ds->input,
							parser->fields[i]->name,
							ds->records[slot].contents[i] );
					valid = FALSE;
					continue;
				}
				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_DOUBLE ) {
					dvalue = parser_str_to_dbl_field ( parser, parser->fields[i]->name, ds->records[slot].contents[i], opts->decimal_point[0], opts->decimal_group[0],
							&error, &overflow );
					if ( error == TRUE && overflow == FALSE) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nValue of field '%s' is not a number ('%s').\nRecord skipped."),
								ds->records[slot].line, ds->input,
								parser->fields[i]->name,
								ds->records[slot].contents[i] );
						valid = FALSE;
					}
					if ( overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nOverflow in numeric value for field '%s'.\nRecord skipped."),
								ds->records[slot].line, ds->input,
								parser->fields[i]->name );
						valid = FALSE;
					}
				}
				if ( parser->fields[i]->type == PARSER_FIELD_TYPE_INT ) {
					int ivalue = 0;
					ivalue = parser_str_to_int_field ( parser, parser->fields[i]->name, ds->records[slot].contents[i], &error, &overflow );
					if ( ivalue == 0) ivalue = 0; /* this is just so that the compiler won't complain */
					if ( error == TRUE && overflow == FALSE) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nValue of field '%s' is not an integer ('%s').\nRecord skipped."),
								ds->records[slot].line, ds->input,
								parser->fields[i]->name,
								ds->records[slot].contents[i] );
						valid = FALSE;
					}
					if ( overflow == TRUE ) {
						err_show ( ERR_NOTE, "" );
						err_show ( ERR_WARN, _("\nInvalid record on line %i, read from \"%s\":\nOverflow in numeric value for field '%s'.\nRecord skipped."),
								ds->records[slot].line, ds->input,
								parser->fields[i]->name );
						valid = FALSE;
					}
				}
			}
		}
	}

	/* check for geometry tag and store if given */
	if ( valid == TRUE && parser->tag_field != NULL ) {
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( ds->records[slot].skip[i] == FALSE ) {
				if ( !strcasecmp ( parser->tag_field, parser->fields[i]->name ) ) {
					const char* tag = NULL;
					/* Point, line or polygon tag? Try all! */
					tag = parser_find_tag ( parser, GEOM_TYPE_POINT, (const char*) ds->records[slot].contents[i] );
					if ( tag == NULL ) { /* check for line tag next */
						tag = parser_find_tag ( parser, GEOM_TYPE_LINE, (const char*) ds->records[slot].contents[i] );
					}
					if ( tag == NULL ) { /* check for polygon tag next */
						tag = parser_find_tag ( parser, GEOM_TYPE_POLY, (const char*) ds->records[slot].contents[i] );
					}
					if ( tag != NULL ) { /* non-null at this point: store */
						ds->records[slot].tag = strdup ( tag );
					}
					// DELETE ME
					/*
					if ( 	parser->geom_tag_point != NULL &&
							strlen ( parser->geom_tag_point ) > 0 &&
							strstr ( ds->records[slot].contents[i], parser->geom_tag_point ) != NULL ) {
						ds->records[slot].tag = strdup ( parser->geom_tag_point );
					}
					tag = parser_find_tag ( parser, GEOM_TYPE_LINE, (const char*) ds->records[slot].contents[i] );
					if ( tag != NULL ) {
						ds->records[slot].tag = strdup ( tag );
					}
					if ( 	parser->geom_tag_line != NULL &&
							strlen ( parser->geom_tag_line ) > 0 &&
							strstr ( ds->records[slot].contents[i], parser->geom_tag_line ) != NULL ) {
						ds->records[slot].tag = strdup ( parser->geom_tag_line );
					}
					if ( 	parser->geom_tag_poly != NULL &&
							strlen ( parser->geom_tag_poly ) > 0 &&
							strstr ( ds->records[slot].contents[i], parser->geom_tag_poly ) != NULL ) {
						ds->records[slot].tag = strdup ( parser->geom_tag_poly );
					}
					*/
				}
			}
		}
	}

	if ( DEBUG == TRUE && valid == TRUE )
		if ( ds->records[slot].tag != NULL )
			fprintf ( stderr, "VALIDATE: tag = '%s'\n", ds->records[slot].tag );

	/* link key field for quick access */
	if ( valid == TRUE && parser->key_field != NULL ) {
		for ( i = 0; i < ds->num_fields; i ++ ) {
			if ( !strcasecmp ( parser->key_field, parser->fields[i]->name ) ) {
				ds->records[slot].key = ds->records[slot].contents[i];
			}
		}
	}

	ds->records[slot].is_valid = valid;

	free ( input );
	return ( 0 );
}


/*
 * "Fuses" geometries by combining records with the same
 * primary key and the same geometry type into one, multi-part object.
 * Must be run AFTER the geometries have been multiplexed and BEFORE
 * checking for unique key values.
 *
 * Returns number of "fused" (multi-part) geometries.
 */
unsigned int parser_ds_fuse ( parser_data_store **storage, options *opts, parser_desc *parser )
{
	int key;
	unsigned int i,j,k,l;
	unsigned int num_fused;
	unsigned int old_geom_id_1 = 0;
	unsigned int old_geom_id_2 = 0;


	/* we can only fuse, if the key is a unique primary key */
	if ( parser->key_unique == FALSE ) {
		return ( 0 );
	}

	/* get index of primary key field */
	key = 0;
	while ( parser->fields[key] != NULL ) {
		if ( !strcasecmp ( parser->fields[key]->name, parser->key_field ) ) {
			break;
		}
		key ++;
	}

	/* Now we go through all records, from all input files, and identify
	 * the ones that have the same primary keys and are of the same geometry
	 * type. */
	num_fused = 0;
	for ( i = 0 ; i < opts->num_input; i ++ ) {
		int part = 0;
		for ( j = 0; j < storage[i]->num_records; j ++ ) {
			if ( 	storage[i]->records[j].is_empty == FALSE &&
					storage[i]->records[j].is_valid == TRUE )
			{
				/* we got a valid record: check if there are any more with the
				 * same key and geom type */
				for ( k = 0; k < opts->num_input; k ++ ) {
					for ( l = 0 ; l < storage[k]->num_records ; l ++ ) {
						if ( 	( l != j || k != i  ) &&
								storage[k]->records[l].is_empty == FALSE &&
								storage[k]->records[l].geom_type != GEOM_TYPE_POINT &&
								storage[k]->records[l].geom_type != GEOM_TYPE_NONE &&
								storage[i]->records[j].geom_type == storage[k]->records[l].geom_type &&
								storage[k]->records[l].geom_id != storage[i]->records[j].geom_id &&
								!strcmp ( 	storage[i]->records[j].contents[key],
										storage[k]->records[l].contents[key]  ) )
						{
							/* found one: fuse it! */
							if ( old_geom_id_1 != storage[i]->records[j].geom_id ) {
								old_geom_id_1 = storage[i]->records[j].geom_id;
								part = 0;
							}
							if ( old_geom_id_2 != storage[k]->records[l].geom_id ) {
								old_geom_id_2 = storage[k]->records[l].geom_id;
								part ++;
								err_show ( ERR_NOTE, _("\n\nMerging geometry #'%s' (read from '%s', line %i+) with\ngeometry #'%s' (read from '%s', line %i+),\nas part %i"),
										storage[i]->records[j].contents[key], opts->input[k], storage[k]->records[l].line,
										storage[k]->records[l].contents[key], opts->input[i], storage[i]->records[j].line,
										part);
								num_fused ++;
							}
							storage[k]->records[l].geom_id = storage[i]->records[j].geom_id;
							storage[k]->records[l].part_id = part;
						}
					}
				}
			}
		}
	}
	return ( num_fused );
}


/*
 * Validates all records for which the "unique" option has
 * been set to "Yes", across all input files.
 * Must be run AFTER the geometries have been multiplexed.
 *
 * Returns number of detected duplicates.
 */
unsigned int parser_ds_validate_unique ( parser_data_store **storage, options *opts, parser_desc *parser )
{
	unsigned int i,j,k,l,m;
	unsigned int num_duplicates;


	num_duplicates = 0;

	/* check "unique" option across all data stores */
	i = 0;
	while ( parser->fields[i] != NULL ) {
		if ( parser->fields[i]->unique == TRUE ) {
			for ( j = 0 ; j < opts->num_input; j ++ ) {
				for ( k = 0; k < storage[j]->num_records; k ++ ) {
					if ( 	storage[j]->records[k].is_empty == FALSE &&
							storage[j]->records[k].is_valid == TRUE )
					{
						for ( l = 0; l < opts->num_input; l ++ ) {
							for ( m = 0 ; m < storage[l]->num_records ; m ++ ) {
								if ( 	( m != k || l != j  ) &&
										storage[l]->records[m].is_empty == FALSE &&
										storage[l]->records[m].is_valid == TRUE &&
										storage[l]->records[m].geom_id != storage[j]->records[k].geom_id &&
										!strcmp ( 	storage[j]->records[k].contents[i],
												storage[l]->records[m].contents[i]  ) )
								{
									/* found a duplicate: report it */
									err_show ( ERR_NOTE, "" );
									err_show ( ERR_WARN, _("\nValue of field '%s', read from '%s', line %i:\nThis is a duplicate of value read from '%s', line %i."),
											parser->fields[i]->name, opts->input[l], storage[l]->records[m].line,
											opts->input[j], storage[j]->records[k].line
									);
									num_duplicates ++;
								}
							}
						}
					}
				}
			}
		}
		i++;
	}

	return ( num_duplicates );
}


parser_desc *parser_desc_create ()
{
	int i;

	parser_desc *new_parser = malloc ( sizeof (parser_desc ));
	new_parser->name = NULL;
	new_parser->info = NULL;
	new_parser->comment_marks = malloc ( sizeof (char*) * PARSER_MAX_COMMENTS );
	for ( i = 0; i < PARSER_MAX_COMMENTS; i++ ) {
		new_parser->comment_marks[i] = NULL;
	}
	new_parser->fields = malloc ( sizeof (parser_field*) * PRG_MAX_FIELDS);
	for ( i = 0; i < PRG_MAX_FIELDS; i++ ) {
		new_parser->fields[i] = NULL;
	}
	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i++ ) {
		new_parser->tags_geom_point[i] = NULL;
		new_parser->tags_geom_line[i] = NULL;
		new_parser->tags_geom_poly[i] = NULL;
	}
	new_parser->tag_field = NULL;
	new_parser->key_field = NULL;
	new_parser->key_unique = FALSE;
	new_parser->key_unique_set = FALSE;
	new_parser->tag_mode = PARSER_TAG_MODE_MIN;
	new_parser->tag_mode_set = FALSE;
	new_parser->tag_strict = FALSE;
	new_parser->tag_strict_set = FALSE;
	new_parser->empty_val = 0;
	new_parser->empty_val_set = FALSE;
	new_parser->empty = TRUE;
	new_parser->coor_x = NULL;
	new_parser->coor_y = NULL;
	new_parser->coor_z = NULL;

	return ( new_parser );
}


/*
 * Destroys a parser, releasing all memory associated
 * with it. This also destroys all field descriptions
 * associated with the parser!
 */
void parser_desc_destroy ( parser_desc *parser )
{

	if ( parser != NULL ) {
		if ( parser->name != NULL )
			free ( parser->name );

		if ( parser->info != NULL )
			free ( parser->info );

		if ( parser->tag_field != NULL )
			free ( parser->tag_field );

		if ( parser->key_field != NULL )
			free ( parser->key_field );

		if ( parser->comment_marks != NULL  ) {
			int i;
			for ( i = 0; i < PARSER_MAX_COMMENTS; i++ ) {
				if ( parser->comment_marks[i] != NULL )
					free ( parser->comment_marks[i] );
			}
			free ( parser->comment_marks );
		}
		if ( parser->fields != NULL ) {
			int i;
			for ( i = 0; i < PRG_MAX_FIELDS; i++ ) {
				if ( parser->fields[i] != NULL ) {
					parser_field_destroy ( parser->fields[i] );
					free ( parser->fields[i] );
				}
			}
			free ( parser->fields );
		}
		{
			int i;
			for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i++ ) {
					t_free (parser->tags_geom_point[i]);
					t_free (parser->tags_geom_line[i]);
					t_free (parser->tags_geom_poly[i]);
			}
		}
		if ( parser->coor_x != NULL )
			free ( parser->coor_x);
		if ( parser->coor_y != NULL )
			free ( parser->coor_y);
		if ( parser->coor_z != NULL )
			free ( parser->coor_z);
		free ( parser );
	}
}


/*
 * Returns a new and empty field description.
 */
parser_field *parser_field_create ()
{
	int i;

	parser_field *new_field=malloc (sizeof (parser_field));
	new_field->definition = 0;
	new_field->name = NULL;
	new_field->info = NULL;
	new_field->type = PARSER_FIELD_TYPE_UNDEFINED;
	new_field->empty_allowed = TRUE;
	new_field->empty_allowed_set = FALSE;
	new_field->unique = FALSE;
	new_field->unique_set = FALSE;
	new_field->persistent = FALSE;
	new_field->persistent_set = FALSE;
	new_field->skip = FALSE;
	new_field->skip_set = FALSE;
	new_field->conversion_mode = PARSER_FIELD_CONVERT_NONE;
	new_field->conversion_mode_set = FALSE;
	new_field->separators = malloc ( sizeof (char*) * PARSER_MAX_SEPARATORS);
	for ( i = 0; i < PARSER_MAX_SEPARATORS; i ++ ) {
		new_field->separators[i] = NULL;
	}
	new_field->merge_separators = FALSE;
	new_field->merge_separators_set = FALSE;
	new_field->quote = 0;
	new_field->value = NULL;
	new_field->has_lookup = FALSE;
	for ( i = 0; i < PARSER_LOOKUP_MAX; i ++ ) {
		new_field->lookup_old[i] = NULL;
		new_field->lookup_new[i] = NULL;
	}
	new_field->empty = TRUE;

	return ( new_field );
}


/*
 * Destroys a parser field, releasing all memory
 * associated with it.
 */
void parser_field_destroy ( parser_field *field )
{

	if ( field != NULL ) {
		if ( field->name != NULL )
			free ( field->name );

		if ( field->info != NULL )
			free ( field->info );

		if ( field->separators != NULL ) {
			int i;
			for ( i = 0; i < PARSER_MAX_SEPARATORS ; i ++ ) {
				if ( field->separators[i] != NULL )
					free ( field->separators[i] );
			}
			free ( field->separators );
		}
		if ( field->value != NULL ) {
			free ( field->value );
		}
		int i;
		for ( i = 0; i < PARSER_LOOKUP_MAX; i ++ ) {
			if ( field->lookup_old[i] != NULL )
				free ( field->lookup_old[i] );
			if ( field->lookup_new[i] != NULL )
				free ( field->lookup_new[i] );
		}
	}
}


/*
 * Adds a new geometry tag to the list of known tags.
 *
 * Possible values for 'type' (parser.h):
 *
 * PARSER_GEOM_TAG_POINT	0
 * PARSER_GEOM_TAG_LINE 	1
 * PARSER_GEOM_TAG_POLY 	2
 *
 * Returns index number of tags array slot where tag was stored
 * or '-1' on error.
 *
 * This method is quite robust:
 * - Passing invalid or NULL parameter values will return '-1'.
 * - A tag with length = 0 will not be stored.
 * - Attempting to store a tag that is already stored will return
 *   the index number of the existing tag.
 * - Attempting to store a tag that is already stored for a
 *   different geometry type will return '-1'.
 * - Attempting to store a tag that is a substring of another
 *   tag, or contains another tag as a substring, will return '-1'.
 */
int parser_tags_add ( parser_desc *parser, int type, const char *tag )
{
	int i;
	char **store = NULL;

	if ( tag == NULL || parser == NULL ) {
		return (-1);
	}

	if ( strlen(tag) < 1 ) {
		return (-1);
	}

	/* check that the tag does not already exist for another geometry */
	if ( type == PARSER_GEOM_TAG_POINT || type == PARSER_GEOM_TAG_LINE || type == PARSER_GEOM_TAG_POLY ) {
		for ( i = PARSER_GEOM_TAG_POINT; i <= PARSER_GEOM_TAG_POLY; i++ ) {
			if ( i != type ) {
				if ( parser_is_tag ( parser, i, tag ) == TRUE ) {
					return (-1);
				}
			}
		}
	}

	if ( parser_tags_overlap ( parser, tag, FALSE ) ) {
		/* parser tag overlap (PARTIAL EQUALITY ONLY) is an error */
		return(-1);
	}

	/* store the tag */
	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		if ( type == PARSER_GEOM_TAG_POINT ) {
			store = parser->tags_geom_point;
		}
		else if ( type == PARSER_GEOM_TAG_LINE ) {
			store = parser->tags_geom_line;
		}
		else if ( type == PARSER_GEOM_TAG_POLY ) {
			store = parser->tags_geom_poly;
		}
		if ( store[i] == NULL ) {
			/* store new tag */
			store[i] = strdup (tag);
			return(i);
		} else {
			/* if tag already exist: skip & job done */
			if ( !strcmp((const char*)store[i], tag) ) {
				return(i);
			}
		}
		i ++;
	}

	return (-1);
}


/*
 * Removes all geometry tags for given geometry type from
 * parser definition. This function does _not_ free all
 * memory allocated for storing parser tags. It simply
 * sets all tag slots for the given geometry to NULL.
 * The purpose of this function is to clear all parser
 * tags for a given geometry, so that parser_get_num_tags()
 * will return '0' for that geometry. Releasing all memory
 * for the involved data structure is exclusively done
 * in parser_data_store_destroy().
 *
 * Possible values for 'type' (parser.h):
 *
 * PARSER_GEOM_TAG_POINT	0
 * PARSER_GEOM_TAG_LINE 	1
 * PARSER_GEOM_TAG_POLY 	2
 *
 */
void parser_tags_clear ( parser_desc *parser, int type )
{
	int t;

	if ( parser == NULL ) {
		return;
	}

	if ( type == PARSER_GEOM_TAG_POINT ) {
		for ( t = 0; t < PARSER_MAX_GEOM_TAGS; t++ ) {
			t_free (parser->tags_geom_point[t]);
		}
	}

	if ( type == PARSER_GEOM_TAG_LINE ) {
		for ( t = 0; t < PARSER_MAX_GEOM_TAGS; t++ ) {
			t_free (parser->tags_geom_line[t]);
		}
	}

	if ( type == PARSER_GEOM_TAG_POLY ) {
		for ( t = 0; t < PARSER_MAX_GEOM_TAGS; t++ ) {
			t_free (parser->tags_geom_poly[t]);
		}
	}

}


/* Checks whether 'str' is a geometry tag for geometry 'type'.
 * Testing is case-sensitive. It is safe to use this method,
 * even if no geometry tag for the given type has been defined
 * as part of the parser description.
 *
 * Possible values for 'type' (parser.h):
 *
 * PARSER_GEOM_TAG_POINT	0
 * PARSER_GEOM_TAG_LINE 	1
 * PARSER_GEOM_TAG_POLY 	2
 *
 */
BOOLEAN parser_is_tag ( parser_desc *parser, int type, const char *str )
{
	int i;
	char **store = NULL;

	if ( str == NULL || parser == NULL ) {
		return (FALSE);
	}

	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		if ( type == PARSER_GEOM_TAG_POINT ) {
			store = parser->tags_geom_point;
		}
		else if ( type == PARSER_GEOM_TAG_LINE ) {
			store = parser->tags_geom_line;
		}
		else if ( type == PARSER_GEOM_TAG_POLY ) {
			store = parser->tags_geom_poly;
		}
		if ( store[i] != NULL ) {
			if ( 0 == strcmp (str,(const char*)store[i]) ) {
				return (TRUE);
			}
		}
		i ++;
	}
	return (FALSE);
}


/*
 * Checks if 'str' overlaps with any of the tags already defined
 * for any geometry type.
 *
 * Overlaps means:
 *  1 matches an existing tag
 *  2 is a substring of an existing tag
 *  3 contains an existing string as substring
 *
 *  Condition 1 is only checked if 'equal' is true.
 *
 *  All tests are case-sensitive.
 */
BOOLEAN parser_tags_overlap ( parser_desc *parser, const char *str, BOOLEAN equal )
{
	int i;
	char **store = NULL;

	if ( parser == NULL || str == NULL ) {
		return ( FALSE );
	}

	/* test for equality depends on 'equal' */
	if ( equal == TRUE ) {
		/* check if 'str' exactly matches a tag registered for any geometry type */
		if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POINT, str ) ) {
			return ( TRUE );
		}
		if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, str ) ) {
			return ( TRUE );
		}
		if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, str ) ) {
			return ( TRUE );
		}
	}

	/* tests for partial equality are always performed */
	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		store = parser->tags_geom_point;
		if ( store[i] != NULL ) {
			if ( strstr( store[i],str ) != NULL  || strstr( str,store[i] ) != NULL ) {
				if ( strcmp (store[i],str) != 0 ) { /* we don't handle complete equality here */
					return (TRUE);
				}
			}
		}
		store = parser->tags_geom_line;
		if ( store[i] != NULL ) {
			if ( strstr( store[i],str ) != NULL  || strstr( str,store[i] ) != NULL ) {
				if ( strcmp (store[i],str) != 0 ) { /* we don't handle complete equality here */
					return (TRUE);
				}
			}
		}
		store = parser->tags_geom_poly;
		if ( store[i] != NULL ) {
			if ( strstr( store[i],str ) != NULL  || strstr( str,store[i] ) != NULL ) {
				if ( strcmp (store[i],str) != 0 ) { /* we don't handle complete equality here */
					return (TRUE);
				}
			}
		}
		i ++;
	}

	return (FALSE);
}


/*
 * Returns number of tag definitions for geometry 'type' in parser.
 */
int parser_get_num_tags ( parser_desc *parser, int type )
{
	int i;
	int count=0;
	char **store = NULL;

	if ( parser == NULL ) {
		return (FALSE);
	}

	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		if ( type == PARSER_GEOM_TAG_POINT ) {
			store = parser->tags_geom_point;
		}
		else if ( type == PARSER_GEOM_TAG_LINE ) {
			store = parser->tags_geom_line;
		}
		else if ( type == PARSER_GEOM_TAG_POLY ) {
			store = parser->tags_geom_poly;
		}
		if ( store[i] != NULL ) {
			count ++;
		}
		i ++;
	}

	return (count);
}


/*
 * Returns the tag definition for 'type' at 'pos' in the
 * array of tag definitions for the given parser.
 *
 * Returns NULL if there is no tag definition or the given
 * parameter values are out-of-range.
 *
 */
const char *parser_get_tag ( parser_desc *parser, int type, int pos )
{
	int i;
	char **store = NULL;

	if ( pos < 0 || pos > PARSER_MAX_GEOM_TAGS ) {
		return NULL;
	}

	if ( parser == NULL ) {
		return (FALSE);
	}

	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		if ( type == PARSER_GEOM_TAG_POINT ) {
			store = parser->tags_geom_point;
		}
		else if ( type == PARSER_GEOM_TAG_LINE ) {
			store = parser->tags_geom_line;
		}
		else if ( type == PARSER_GEOM_TAG_POLY ) {
			store = parser->tags_geom_poly;
		}
		if ( store[i] != NULL ) {
			return (const char*)store[i];
		}
		i ++;
	}

	return NULL;
}


/*
 * Searches string 'str' for any of the geometry tags for 'type' defined
 * in parser.
 *
 * Return values:
 * - The tag (char*) itself, if found in 'str' or
 * - NULL if the tag is not found 'str'.
 *
 * An non-NULL result will be a pointer into the parser's internal
 * array of tag strings and must not be altered or free'd by the caller!
 */
const char *parser_find_tag ( parser_desc *parser, int type, const char* str )
{
	int i;
	char **store = NULL;

	if ( parser == NULL || str == NULL ) {
		return (NULL);
	}

	for ( i = 0; i < PARSER_MAX_GEOM_TAGS; i ++ ) {
		if ( type == PARSER_GEOM_TAG_POINT ) {
			store = parser->tags_geom_point;
		}
		else if ( type == PARSER_GEOM_TAG_LINE ) {
			store = parser->tags_geom_line;
		}
		else if ( type == PARSER_GEOM_TAG_POLY ) {
			store = parser->tags_geom_poly;
		}
		if ( store[i] != NULL ) {
			if ( strstr(str, (const char*) store[i]) != NULL ) {
				return ((const char*)store[i]);
			}
		}
		i ++;
	}
	return NULL;
}


/*
 * Helper function for parser_desc_set.
 * Returns TRUE if "str" is a valid field name,
 * FALSE otherwise.
 */
BOOLEAN parser_is_valid_field_name ( const char *str )
{
	char *p;
	char *tmp;


	if ( str == NULL )
		return ( FALSE );

	if ( strlen ( str ) < 1 || strlen ( str ) > PRG_MAX_FIELD_LEN )
		return ( FALSE );

	tmp = strdup ( PRG_FIELD_NAME_CHARS );

	p = (char*) str;
	while ( *p != '\0' ) {
		char *p2 = tmp;
		BOOLEAN valid = FALSE;
		while ( *p2 != '\0' ) {
			if ( *p == *p2 ) {
				valid = TRUE;
				break;
			}
			p2 += sizeof (char);
		}
		if ( valid == FALSE ) {
			free ( tmp );
			return ( FALSE );
		}
		p += sizeof (char);
	}

	free ( tmp );
	return ( TRUE );
}


/*
 * Helper function for parser_process_option()
 * Returns TRUE, if "token" represents an enabled option value.
 * FALSE, otherwise.
 */
int parser_is_enabled_option ( const char *token, const char *file, int line_no, const char *name ) {
	if ( !strcasecmp (token, "Y") )
		return (TRUE);
	if ( !strcasecmp (token, "Yes") )
		return (TRUE);
	if ( !strcasecmp (token, "On") )
		return (TRUE);
	if ( !strcasecmp (token, "1") )
		return (TRUE);
	if ( !strcasecmp (token, "Enable") )
		return (TRUE);
	if ( !strcasecmp (token, "True") )
		return (TRUE);

	if ( !strcasecmp (token, "N") )
		return (FALSE);
	if ( !strcasecmp (token, "No") )
		return (FALSE);
	if ( !strcasecmp (token, "Off") )
		return (FALSE);
	if ( !strcasecmp (token, "0") )
		return (FALSE);
	if ( !strcasecmp (token, "Disable") )
		return (FALSE);
	if ( !strcasecmp (token, "False") )
		return (FALSE);

	err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" cannot be set to \"%s\"."),
			file, line_no, name, token );

	/* to avoid the compiler complaining */
	return (FALSE);
}


/*
 * Helper function for parser_process_option()
 * Returns the conversion option code for a text field.
 * Aborts on syntax error.
 */
int parser_conversion_option ( const char *token, const char *file, int line_no, const char *name )
{
	int i;


	i = 0;
	while ( strcmp ( PARSER_FIELD_CONVERSIONS[i],"" ) != 0  ) {
		if ( !strcasecmp ( token, PARSER_FIELD_CONVERSIONS[i] ) ) {
			return ( i );
		}
		i ++;
	}

	err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" cannot be set to \"%s\"."),
			file, line_no, name, token );

	/* to avoid the compiler complaining */
	return (-1);
}


/*
 * Helper function for parser_desc_set().
 * Takes an option name and value as strings and tries to store
 * the matching setting in the "opts" object.
 * Returns TRUE on success, FALSE on failure.
 */
BOOLEAN parser_process_option ( const char *option_name, const char *option_val,
		int section_type, unsigned int line_no, int field_num,
		parser_desc *parser, options *opts )
{
	BOOLEAN valid;
	BOOLEAN error;
	char *name, *value;

	name = NULL;
	value = NULL;

	if ( 	option_name == NULL || option_val == NULL ||
			strlen ( option_name ) < 1 || strlen ( option_val ) < 1 ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Empty option name or value."),
				opts->schema_file, line_no );
		t_free (name); t_free (value); return (FALSE);
	}

	valid = FALSE;
	name = t_str_pack (option_name);
	value = t_str_pack (option_val);

	if ( strlen ( value ) < 1 ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Option \"%s\" has an empty value."),
				opts->schema_file, line_no, name );
		t_free (name); t_free (value); return (FALSE);
	}

	if ( strlen ( value ) > PRG_MAX_STR_LEN ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Option values cannot be longer than %i characters."),
				opts->schema_file, line_no, PRG_MAX_STR_LEN );
		t_free (name); t_free (value); return (FALSE);
	}

	if ( section_type == PARSER_SECTION_PARSER ) {
		if ( !strcasecmp (name, "name")) {
			if ( parser->name != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->name = strdup ( value );
			valid = TRUE;
		}
		if ( !strcasecmp (name, "info")) {
			if ( parser->info != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->info = strdup ( value );
			valid = TRUE;
		}
		if ( !strcasecmp (name, "tagging_mode")) {
			if ( parser->tag_mode_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			int i = 0;
			while ( strcmp (PARSER_MODE_NAMES[i],"") != 0 ) {
				if (  !strcasecmp (PARSER_MODE_NAMES[i], value) ) {
					parser->tag_mode = i;
					parser->tag_mode_set = TRUE;
					valid = TRUE;
					break;
				}
				i ++;
			}
			if ( valid == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid geometry tagging mode."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
		}
		if ( !strcasecmp (name, "comment_mark")) {
			/* maximum number of distinct separators is PARSER_MAX_COMMENTS:
			   search all existing separators from 0 .. PARSER_MAX_COMMENTS-1
			   if one is NULL: that slot is available. If not: throw error! */
			int i = 0;
			while ( i < PARSER_MAX_COMMENTS ) {
				if ( parser->comment_marks[i] == NULL ) {
					parser->comment_marks[i] = strdup (value);
					valid = TRUE;
					break;
				}
				i ++;
			}
			if ( i == PARSER_MAX_COMMENTS ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Limit of %i distinct comment marks exceeded."),
						opts->schema_file, line_no, PARSER_MAX_COMMENTS );
				t_free (name); t_free (value); return (FALSE);
			}
		}
		if ( !strcasecmp (name, "coor_x")) {
			if ( parser->coor_x != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			if ( parser_is_valid_field_name (value) == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid field name."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->coor_x = strdup (value);
			valid = TRUE;
		}
		if ( !strcasecmp (name, "coor_y")) {
			if ( parser->coor_y != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			if ( parser_is_valid_field_name (value) == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid field name."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->coor_y = strdup (value);
			valid = TRUE;
		}
		if ( !strcasecmp (name, "coor_z")) {
			if ( parser->coor_z != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			if ( parser_is_valid_field_name (value) == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid field name."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->coor_z = strdup (value);
			valid = TRUE;
		}
		if ( !strcasecmp (name, "tag_field")) {
			if ( parser->tag_field != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->tag_field = strdup (value);
			valid = TRUE;
		}
		if ( !strcasecmp (name, "key_field")) {
			if ( parser->key_field != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->key_field = strdup (value);
			valid = TRUE;
		}
		if ( !strcasecmp (name, "tag_strict")) {
			if ( parser->tag_strict_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->tag_strict = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->tag_strict_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "key_unique")) {
			if ( parser->key_unique_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->key_unique = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->key_unique_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "no_data")) {
			if ( parser->empty_val_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->empty_val = t_str_to_int ( value, &error, NULL );
			if ( error == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Value for \"%s\" is not a valid integer number."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->empty_val_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "geom_tag_point")) {
			//
			/*
			if ( parser->geom_tag_point != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				return;
			}
			parser->geom_tag_point = strdup (value);
			*/
			if ( parser_tags_add ( parser, PARSER_GEOM_TAG_POINT, strdup(value) ) < 0 ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Tag \"%s\" is non-unique or exceeds storage."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			valid = TRUE;
		}
		if ( !strcasecmp (name, "geom_tag_line")) {
			//
			/*
			if ( parser->geom_tag_line != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->geom_tag_line = strdup (value);
			*/
			if ( parser_tags_add ( parser, PARSER_GEOM_TAG_LINE, strdup(value) ) < 0 ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Tag \"%s\" is non-unique or exceeds storage."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			valid = TRUE;
		}
		if ( !strcasecmp (name, "geom_tag_poly")) {
			//
			/*
			if ( parser->geom_tag_poly != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->geom_tag_poly = strdup (value);
			*/
			if ( parser_tags_add ( parser, PARSER_GEOM_TAG_POLY, strdup(value) ) < 0 ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Tag \"%s\" is non-unique or exceeds storage."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			valid = TRUE;
		}
	}

	/*
	 * FIELD DEFINITIONS
	 */
	if ( section_type == PARSER_SECTION_FIELD ) {
		int i;

		/* the index of the current field is given by field_num */
		if ( !strcasecmp (name, "name")) {
			/* Field names can only contain "a-z,A-Z,0-9,_" characters. They are NOT case-sensitive. */
			if ( parser->fields[field_num]->name != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			if ( parser_is_valid_field_name (value) == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid field name."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
			i = 0;
			while ( parser->fields[i] != NULL ) {
				if ( parser->fields[i]->name != NULL )
					if ( !strcasecmp ( parser->fields[i]->name, value ) && i != field_num ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: A field with name \"%s\" was already defined."),
								opts->schema_file, line_no, value );
						t_free (name); t_free (value); return (FALSE);
					}
				i++;
			}
			/* check for reserved field names */
			i = 0;
			while ( strcmp (PRG_RESERVED_FIELD_NAMES[i],"" ) != 0 ) {
				if ( !strcasecmp ( PRG_RESERVED_FIELD_NAMES[i], value ) ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is a reserved field name."),
							opts->schema_file, line_no, value );
					t_free (name); t_free (value); return (FALSE);
				}
				i ++;
			}
			/* parser->fields[field_num]->name = strdup ( value ); */
			/* convert field name to all lower case */
			parser->fields[field_num]->name = t_str_to_lower ( value );
			valid = TRUE;
		}
		if ( !strcasecmp (name, "value")) {
			/* we have a pseudo field that does not actually exist in the input data */
			if ( value == NULL ) {
				if ( parser->fields[field_num]->value != NULL ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
							opts->schema_file, line_no, value );
					t_free (name); t_free (value); return (FALSE);
				}
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: field value not defined."),
						opts->schema_file, line_no );
				t_free (name); t_free (value); return (FALSE);
			}
			/* we will do a type compatibility check for the value later */
			parser->fields[field_num]->value = strdup ( value );
			valid = TRUE;
		}
		if ( !strcasecmp (name, "info")) {
			if ( parser->fields[field_num]->info != NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->info = strdup ( value );
			valid = TRUE;
		}
		if ( !strcasecmp (name, "type")) {
			if ( parser->fields[field_num]->type != PARSER_FIELD_TYPE_UNDEFINED ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			i = 0;
			while ( strcmp ( PARSER_FIELD_TYPE_NAMES[i], "" ) != 0 ) {
				if ( !strcasecmp ( PARSER_FIELD_TYPE_NAMES[i], value ) ) {
					parser->fields[field_num]->type = i;
					valid = TRUE;
					break;
				}
				i++;
			}
			if ( valid == FALSE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid field type."),
						opts->schema_file, line_no, value );
				t_free (name); t_free (value); return (FALSE);
			}
		}
		if ( !strcasecmp (name, "empty_allowed")) {
			if ( parser->fields[field_num]->empty_allowed_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->empty_allowed = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->empty_allowed_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "unique")) {
			if ( parser->fields[field_num]->unique_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->unique = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->unique_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "persistent")) {
			if ( parser->fields[field_num]->persistent_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->persistent = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->persistent_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "skip")) {
			if ( parser->fields[field_num]->skip_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->skip = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->skip_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "change_case")) {
			if ( parser->fields[field_num]->conversion_mode_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->conversion_mode = parser_conversion_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->conversion_mode_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "separator")) {
			/* maximum number of distinct separators is PARSER_MAX_SEPARATORS:
			   search all existing separators from 0 .. PARSER_MAX_SEPARATORS-1
			   if one is NULL: that slot is available. If not: throw error! */
			if ( !strcmp ( value, "\n") ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Line break is not a valid field separator."),
						opts->schema_file, line_no );
				t_free (name); t_free (value); return (FALSE);
			}
			i = 0;
			while ( i < PARSER_MAX_SEPARATORS ) {
				if ( parser->fields[field_num]->separators[i] == NULL ) {
					parser->fields[field_num]->separators[i] = strdup (value);
					/* "space" and "tab" have to be stored as their respective ASCII values */
					if ( !strcasecmp ( value, "space") ) {
						free ( parser->fields[field_num]->separators[i] );
						parser->fields[field_num]->separators[i] = strdup (" ");
					}
					if ( !strcasecmp ( value, "tab") ) {
						free ( parser->fields[field_num]->separators[i] );
						parser->fields[field_num]->separators[i] = strdup ("\t");
					}
					valid = TRUE;
					break;
				}
				i ++;
			}
			if ( i == PARSER_MAX_SEPARATORS ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Limit of %i distinct field separators exceeded."),
						opts->schema_file, line_no, PARSER_MAX_SEPARATORS );
				t_free (name); t_free (value); return (FALSE);
			}
		}
		if ( !strcasecmp (name, "merge_separators")) {
			if ( parser->fields[field_num]->merge_separators_set == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->merge_separators = parser_is_enabled_option ( value, opts->schema_file, line_no, name );
			parser->fields[field_num]->merge_separators_set = TRUE;
			valid = TRUE;
		}
		if ( !strcasecmp (name, "quotation")) {
			if ( parser->fields[field_num]->quote != 0 ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" has already been set in this context."),
						opts->schema_file, line_no, name );
				t_free (name); t_free (value); return (FALSE);
			}
			if ( strlen (value) != 1 ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Quotation mark must be a single character."),
						opts->schema_file, line_no );
				t_free (name); t_free (value); return (FALSE);
			}
			parser->fields[field_num]->quote = *value;
			valid = TRUE;
		}
		/* check for lookup pairs for string replacement */
		if ( name[0] == PARSER_LOOKUP_TAG ) {
			if ( value == NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: No string specified for content replacement."),
						opts->schema_file, line_no, PRG_MAX_STR_LEN );
				t_free (name); t_free (value); return (FALSE);
			}
			int j;
			for ( j = 0; j < PARSER_LOOKUP_MAX; j ++ ) {
				if ( parser->fields[field_num]->lookup_old[j] == NULL ) {
					char *p = name;
					p ++;
					if ( strlen (p) > PRG_MAX_STR_LEN ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: String to be replaced exceeds %i characters."),
								opts->schema_file, line_no, PRG_MAX_STR_LEN );
						t_free (name); t_free (value); return (FALSE);
					}
					if ( strlen (value) > PRG_MAX_STR_LEN ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: String to replace with exceeds %i characters."),
								opts->schema_file, line_no, PRG_MAX_STR_LEN );
						t_free (name); t_free (value); return (FALSE);
					}
					parser->fields[field_num]->lookup_old[j] = strdup (p);
					parser->fields[field_num]->lookup_new[j] = strdup (value);
					parser->fields[field_num]->has_lookup = TRUE;
					valid = TRUE;
					break;
				}
			}
			if ( j == PARSER_LOOKUP_MAX ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Maximum number of lookup pairs (%i) exceeded."),
						opts->schema_file, line_no, PARSER_LOOKUP_MAX );
				t_free (name); t_free (value); return (FALSE);
			}
		}
	}

	if ( valid == FALSE ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: \"%s\" is not a valid option in this context."),
				opts->schema_file, line_no, name );
		t_free (name); t_free (value); return (FALSE);
	}

	t_free (name);
	t_free (value);

	return (TRUE);
}


/* read parser description (incl. all field descriptions) from ASCII file */
void parser_desc_set ( parser_desc *parser, options *opts )
{
	FILE *fp;
	char* line = NULL;
	char *buffer = NULL;
	unsigned int line_no;
	BOOLEAN valid;
	char *start = NULL;
	char *end = NULL;
	char *tmp = NULL;
	char *option_name = NULL;
	char *option_val = NULL;
	int section_type;
	char *p;
	int field_num;

	if ( parser == NULL ) {
		err_show ( ERR_EXIT, _("No parser file given."));
		return;
	}

	if ( parser->empty == FALSE ) {
		err_show ( ERR_EXIT, _("Attempting to modify an existing parser."));
		return;
	}

	if ( opts->schema_file == NULL || strlen (opts->schema_file) < 2 ) {
		err_show ( ERR_EXIT, _("Parser schema file must be provided."));
		return;
	}

#ifdef MINGW
	fp = t_fopen_utf8 ( opts->schema_file, "rt" );
#else
	fp = t_fopen_utf8 ( opts->schema_file, "r" );
#endif
	if ( fp == NULL ) {
		err_show ( ERR_EXIT, _("Cannot open parser schema for reading ('%s').\nReason: %s"),
				opts->schema_file, strerror (errno));
		return;
	}

	field_num = -1; /* track index of current field */

	line = malloc ( sizeof (char) * PARSER_MAX_FILE_LINE_LENGTH);
	line_no = 1;
	section_type = PARSER_SECTION_NONE;

	while ( fgets ( line, PARSER_MAX_FILE_LINE_LENGTH, fp ) ) {

		/* check if line is too long to be processed */
		p = index ( line, '\n');
		if (p == NULL) {
			err_show(
					ERR_EXIT,
					_("Line too long in parser schema file (line no.: %i).\nThe maximum line length allowed is: %i characters."),
					line_no, PARSER_MAX_FILE_LINE_LENGTH);
			t_free ( line );
			fclose ( fp );
			return;
		}

		buffer = t_str_pack ( (const char*) line );
		/* skip totally empty lines (after packing) */

		/* DEBUG */
		/* fprintf (stderr, "PROC: PACKED %i: {%s}\n", line_no, buffer ); */

		p = buffer;
		if ( *p != '#' && strlen ( buffer ) > 0 && strlen ( buffer ) < 3 ) {
			/* garbage line */
			err_show ( ERR_WARN, _("Garbage encountered in parser schema (%s).\nSkipping line #%i"),
					opts->schema_file, line_no );
		}
		if ( strlen ( buffer ) >= 3  ) {
			/* line with content */
			if ( *p != '#' ) { /* skip comment lines */
				if ( *p == '[' ) { /* section opener found */
					end = rindex ( buffer, ']' );
					if (  end == NULL ) {
						err_show ( ERR_EXIT, _("Syntax error in parser schema (%s).\nLine #%i: Missing ']'."),
								opts->schema_file, line_no );
						t_free ( line );
						fclose ( fp );
						return;
					}
					/* we have a section: check if name is valid */
					valid = FALSE;
					*end = '\0';
					start = p;
					start += sizeof (char);

					tmp = t_str_pack ( start );
					if ( !strcasecmp ( tmp, "parser" )  ) {
						section_type = PARSER_SECTION_PARSER;
						valid = TRUE;
					}
					if ( !strcasecmp ( tmp, "field" )  ) {
						section_type = PARSER_SECTION_FIELD;
						/* create a new, empty parser field object */
						field_num ++;
						if ( field_num > PRG_MAX_FIELDS ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: Exceeds limit of %i field definitions.\n"),
									opts->schema_file, line_no, PRG_MAX_FIELDS );
						}
						parser->fields[field_num] = parser_field_create();
						parser->fields[field_num]->definition = line_no;
						parser->fields[field_num]->empty = FALSE;
						valid = TRUE;
					}
					if ( valid == FALSE ) {
						err_show ( ERR_EXIT, _("Syntax error in parser schema (%s).\nLine #%i: \"%s\" is not a valid section identifier.\n"),
								opts->schema_file, line_no, tmp );
						t_free ( line );
						t_free ( tmp );
						fclose ( fp );
						return;
					}
					t_free ( tmp );
				} else {
					/* assume that we have an "option = value" line */
					start = buffer;
					end = index ( buffer, '=' );
					if ( end == NULL ) {
						err_show ( ERR_EXIT, _("Syntax error in parser schema (%s).\nLine #%i: Missing '=' (expected 'option=value' line)."),
								opts->schema_file, line_no );
						t_free ( line );
						fclose ( fp );
						return;
					}
					if ( section_type == PARSER_SECTION_NONE ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine #%i: option/value out of context."),
								opts->schema_file, line_no );
						t_free ( line );
						fclose ( fp );
						return;
					}

					/* extract option name */
					*end = '\0';
					tmp = t_str_pack (start);
					option_name = t_str_del_quotes (tmp, '"');
					t_free (tmp);

					/* extract option value */
					*end = '=';
					start = index ( buffer, '=' );
					start += sizeof (char);
					tmp = t_str_pack (start);
					option_val = t_str_del_quotes (tmp, '"');
					t_free (tmp);

					/* process option */
					if ( parser_process_option ( option_name, option_val, section_type,
							line_no, field_num, parser, opts ) == FALSE ) {
						t_free (option_name);
						t_free (option_val);
						t_free ( buffer );
						t_free (line);
						fclose ( fp );
						return;
					}

					t_free (option_name);
					t_free (option_val);
				}
			}
		}
		t_free ( buffer );
		line_no ++;
	}

	t_free (line);
	fclose ( fp );

	parser->empty = FALSE;
}


/*
 * Dump full parser and field descriptions to screen.
 */
void parser_dump ( parser_desc *parser, options *opts )
{
	int i,j;


	if ( parser == NULL || parser->empty == TRUE ) {
		fprintf ( stderr, _("NO PARSER DESCRIPTION AVAILABLE.\n"));
	}

	fprintf ( stderr, _("\n* PARSER AND FIELD DEFINITIONS *\n\n") );
	fprintf ( stderr, _("SCHEMA FILE:\t%s\n"), opts->schema_file );
	if ( parser->name != NULL )
		fprintf ( stderr, _("NAME:\t\t%s\n"), parser->name );
	else
		fprintf ( stderr, _("NAME:\t\tNot specified.\n") );
	if ( parser->info != NULL )
		fprintf ( stderr, _("INFO:\t\t%s\n"), parser->info );
	else
		fprintf ( stderr, _("INFO:\t\tNone.\n"));
	fprintf ( stderr, _("TAG MODE:\t%i (\"%s\")\n"), parser->tag_mode, PARSER_MODE_NAMES[parser->tag_mode]);
	if ( parser->tag_strict == TRUE )
		fprintf ( stderr, _("TAG STRICT:\tYes.\n"));
	else
		fprintf ( stderr, _("TAG STRICT:\tNo.\n"));
	if ( parser->key_unique== TRUE )
		fprintf ( stderr, _("KEY UNIQUE:\tYes.\n"));
	else
		fprintf ( stderr, _("KEY UNIQUE:\tNo.\n"));
	i = 0;
	fprintf ( stderr, _("COMMENT MARKS:\t"));
	while ( i < PARSER_MAX_COMMENTS && parser->comment_marks[i] != NULL ) {
		fprintf ( stderr, "\"%s\"", parser->comment_marks[i] );
		if ( i+1 < PARSER_MAX_COMMENTS && parser->comment_marks[i+1] != NULL ) {
			fprintf ( stderr, ", " );
		}
		i ++;
	}
	if ( i == 0 ) {
		fprintf ( stderr, _("None."));
	}
	fprintf ( stderr, _("\n" ));
	fprintf ( stderr, _("TAG FIELD:\t%s\n"), parser->tag_field);
	fprintf ( stderr, _("KEY FIELD:\t%s\n"), parser->key_field);
	if ( parser->empty_val_set == TRUE )
		fprintf ( stderr, _("EMPTY FLD VAL:\t%i\n"), parser->empty_val );
	else
		fprintf ( stderr, _("EMPTY FLD VAL:\tNULL (default)\n"));

	// DELETE ME
	/*
	if ( parser->geom_tag_point != NULL )
		fprintf ( stderr, _("GEOM TAG POINT:\t%s\n"), parser->geom_tag_point );
	else
		fprintf ( stderr, _("GEOM TAG POINT:\tNone.\n"));
	if ( parser->geom_tag_line != NULL )
		fprintf ( stderr, _("GEOM TAG LINE:\t%s\n"), parser->geom_tag_line );
	else
		fprintf ( stderr, _("GEOM TAG LINE:\tNone.\n"));
	if ( parser->geom_tag_poly != NULL )
		fprintf ( stderr, _("GEOM TAG POLY:\t%s\n"), parser->geom_tag_poly );
	else
		fprintf ( stderr, _("GEOM TAG POLY:\tNone.\n"));
	*/
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ); i ++ ) {
			fprintf ( stderr, _("GEOM TAG POINT (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, i ) );
		}
	} else {
		fprintf ( stderr, _("GEOM TAG POINT:\tNone.\n"));
	}
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ); i ++ ) {
			fprintf ( stderr, _("GEOM TAG LINE (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, i ) );
		}
	} else {
		fprintf ( stderr, _("GEOM TAG LINE:\tNone.\n"));
	}
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ); i ++ ) {
			fprintf ( stderr, _("GEOM TAG POLY (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, i ) );
		}
	} else {
		fprintf ( stderr, _("GEOM TAG POLY:\tNone.\n"));
	}
	if ( parser->coor_x != NULL )
		fprintf ( stderr, _("X COORD FIELD:\t%s\n"), parser->coor_x );
	else
		fprintf ( stderr, _("X COORD FIELD:\tUndefined.\n") );
	if ( parser->coor_y != NULL )
		fprintf ( stderr, _("Y COORD FIELD:\t%s\n"), parser->coor_y );
	else
		fprintf ( stderr, _("Y COORD FIELD:\tUndefined.\n") );
	if ( parser->coor_z != NULL && ( strlen ( parser->coor_z ) > 0 ) )
		fprintf ( stderr, _("Z COORD FIELD:\t%s\n"), parser->coor_z );
	else
		fprintf ( stderr, _("Z COORD FIELD:\tUndefined.\n") );
	fprintf ( stderr, _("\nFIELD DEFINITIONS:\n"));
	/* dump field definitions */
	i = 0;
	while ( parser->fields[i]!=NULL ) {
		fprintf ( stderr, _("\n\tFIELD NO. %i:\t%s\n"), i+1, parser->fields[i]->name );
		if ( parser->fields[i]->info != NULL )
			fprintf ( stderr, _("\tINFO:\t\t%s\n"), parser->fields[i]->info );
		else
			fprintf ( stderr, _("\tINFO:\t\tNone.\n"));
		if ( parser->fields[i]->type < 0 )
			fprintf ( stderr, _("\tTYPE:\t\tUnknown.\n"));
		else
			fprintf ( stderr, _("\tTYPE:\t\t%i (\"%s\")\n"), parser->fields[i]->type,
					PARSER_FIELD_TYPE_NAMES[parser->fields[i]->type] );
		if ( parser->fields[i]->value != NULL ) {
			/* pseudo field: we have a much terser definition here */
			fprintf ( stderr, _("\tVALUE:\t\t\"%s\"\n"), parser->fields[i]->value);
			fprintf (stderr,"\n");
		} else {
			if ( parser->fields[i]->empty_allowed == TRUE )
				fprintf ( stderr, _("\tEMPTY ALLOWED:\tYes.\n"));
			else
				fprintf ( stderr, _("\tEMPTY ALLOWED:\tNo.\n"));
			if ( parser->fields[i]->unique == TRUE )
				fprintf ( stderr, _("\tUNIQUE:\t\tYes.\n"));
			else
				fprintf ( stderr, _("\tUNIQUE:\t\tNo.\n"));
			if ( parser->fields[i]->persistent == TRUE )
				fprintf ( stderr, _("\tPERSISTENT:\tYes.\n"));
			else
				fprintf ( stderr, _("\tPERSISTENT:\tNo.\n"));
			if ( parser->fields[i]->skip == TRUE )
				fprintf ( stderr, _("\tSKIP:\t\tYes.\n"));
			else
				fprintf ( stderr, _("\tSKIP:\t\tNo.\n"));
			if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
				fprintf ( stderr, _("\tCASE CHANGE:\t"));
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_NONE ) {
					fprintf ( stderr, _("None.\n"));
				}
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_UPPER ) {
					fprintf ( stderr, _("To upper case.\n"));
				}
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_LOWER ) {
					fprintf ( stderr, _("To lower case.\n"));
				}
			}
			fprintf ( stderr, _("\tSEPARATORS:\t"));
			j = 0;
			while ( j < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j] != NULL ) {
				fprintf ( stderr, "\"%s\"", parser->fields[i]->separators[j] );
				if ( j+1 < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j+1] != NULL ) {
					fprintf ( stderr, ", " );
				}
				j ++;
			}
			if ( j == 0 ) {
				fprintf ( stderr, _("None."));
			}
			fprintf ( stderr, "\n" );
			if ( parser->fields[i]->merge_separators == TRUE )
				fprintf ( stderr, _("\tMERGE SEPS:\tYes.\n"));
			else
				fprintf ( stderr, _("\tMERGE SEPS:\tNo.\n"));
			if ( parser->fields[i]->quote != 0 )
				fprintf ( stderr, _("\tQUOTATION:\t\"%c\"\n"), parser->fields[i]->quote );
			else
				fprintf ( stderr, _("\tQUOTATION:\tNone.\n"));
			/* any replacements specified? */
			if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
				fprintf (  stderr, _("\tREPLACEMENTS:"));
				if ( parser->fields[i]->has_lookup == TRUE ) {
					fprintf (  stderr, _("\n"));
					j = 0;
					while ( parser->fields[i]->lookup_old[j] != NULL ) {
						fprintf ( stderr, _("\t\t\t\"%s\"=\"%s\"\n"),
								parser->fields[i]->lookup_old[j],
								parser->fields[i]->lookup_new[j]);
						j ++;
					}
				} else {
					fprintf (  stderr, _("\tNone."));
				}
			}
			fprintf (stderr,"\n");
		}
		i ++;
	}
	if ( i == 0 ) {
		fprintf ( stderr, _("\tNone found.\n"));
	}
}


/*
 * Dump full parser and field descriptions to GUI message area.
 */
#ifdef GUI

/* Helper function: print to text view, scroll if necessary */
void print ( const char *format, ... )
{
	char buffer[ERR_MSG_LENGTH+1];
	va_list argp;
	GtkTextIter iter;
	GtkTextBuffer *tbuffer;
	GtkTextMark *mark;

	va_start(argp, format);
	vsnprintf ( buffer, ERR_MSG_LENGTH, format, argp );

	tbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW));
	mark = gtk_text_buffer_get_insert(tbuffer);

	gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);

	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, buffer, -1, "font_blue", NULL);

	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);

	va_end(argp);

}

void parser_dump_gui ( parser_desc *parser, options *opts )
{
	int i,j;


	if (parser == NULL || parser->empty == TRUE) {
		/* refuse to run unless minimal options are given */
		if (opts->window != NULL) {
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(opts->window),
					GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE, "Missing or invalid parser file.");
			gtk_message_dialog_format_secondary_text(
					GTK_MESSAGE_DIALOG(dialog),
					_("Please select a valid parser file."));
			gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
			gtk_widget_show_all(dialog);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		}
		return;
	}

	print ( _("\n* PARSER AND FIELD DEFINITIONS *\n\n") );
	print ( _("SCHEMA FILE:\t%s\n"), opts->schema_file );
	if ( parser->name != NULL )
		print ( _("NAME:\t\t%s\n"), parser->name );
	else
		print ( _("NAME:\t\tNot specified.\n") );
	if ( parser->info != NULL )
		print ( _("INFO:\t\t%s\n"), parser->info );
	else
		print ( _("INFO:\t\tNone.\n"));
	print ( _("TAG MODE:\t%i (\"%s\")\n"), parser->tag_mode, PARSER_MODE_NAMES[parser->tag_mode]);
	if ( parser->tag_strict == TRUE )
		print ( _("TAG STRICT:\tYes.\n"));
	else
		print ( _("TAG STRICT:\tNo.\n"));
	if ( parser->key_unique == TRUE )
		print ( _("KEY UNIQUE:\tYes.\n"));
	else
		print ( _("KEY UNIQUE:\tNo.\n"));
	i = 0;
	print ( _("COMMENT MARKS:\t"));
	while ( i < PARSER_MAX_COMMENTS && parser->comment_marks[i] != NULL ) {
		print ( "\"%s\"", parser->comment_marks[i] );
		if ( i+1 < PARSER_MAX_COMMENTS && parser->comment_marks[i+1] != NULL ) {
			print ( ", " );
		}
		i ++;
	}
	if ( i == 0 ) {
		print ( _("None."));
	}
	print ( _("\n" ));
	print ( _("TAG FIELD:\t%s\n"), parser->tag_field);
	print ( _("KEY FIELD:\t%s\n"), parser->key_field);
	if ( parser->empty_val_set == TRUE )
		print ( _("EMPTY FLD VAL:\t%i\n"), parser->empty_val );
	else
		print ( _("EMPTY FLD VAL:\tNULL (default)\n"));

	// DELETE ME
	/*
	if ( parser->geom_tag_point != NULL )
		print ( _("GEOM TAG POINT:\t%s\n"), parser->geom_tag_point );
	else
		print ( _("GEOM TAG POINT:\tNone.\n"));
	if ( parser->geom_tag_line != NULL )
		print ( _("GEOM TAG LINE:\t%s\n"), parser->geom_tag_line );
	else
		print( _("GEOM TAG LINE:\tNone.\n"));
	if ( parser->geom_tag_poly != NULL )
		print ( _("GEOM TAG POLY:\t%s\n"), parser->geom_tag_poly );
	else
		print ( _("GEOM TAG POLY:\tNone.\n"));
	*/
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ); i ++ ) {
			print ( _("GEOM TAG POINT (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, i ) );
		}
	} else {
		print ( _("GEOM TAG POINT:\tNone.\n"));
	}
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ); i ++ ) {
			print ( _("GEOM TAG LINE (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, i ) );
		}
	} else {
		print ( _("GEOM TAG LINE:\tNone.\n"));
	}
	if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ) > 0 ) {
		for ( i = 0; i < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ); i ++ ) {
			print ( _("GEOM TAG POLY (%i):\t%s\n"), (i+1), parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, i ) );
		}
	} else {
		print ( _("GEOM TAG POLY:\tNone.\n"));
	}

	if ( parser->coor_x != NULL )
		print ( _("X COORD FIELD:\t%s\n"), parser->coor_x );
	else
		print ( _("X COORD FIELD:\tUndefined.\n") );
	if ( parser->coor_y != NULL )
		print ( _("Y COORD FIELD:\t%s\n"), parser->coor_y );
	else
		print ( _("Y COORD FIELD:\tUndefined.\n") );
	if ( parser->coor_z != NULL && ( strlen ( parser->coor_z ) > 0 ) )
		print ( _("Z COORD FIELD:\t%s\n"), parser->coor_z );
	else
		print ( _("Z COORD FIELD:\tUndefined.\n") );
	print ( _("\nFIELD DEFINITIONS:\n"));
	/* dump field definitions */
	i = 0;
	while ( parser->fields[i]!=NULL ) {
		print ( _("\tFIELD NO. %i:\t%s\n"), i+1, parser->fields[i]->name );
		if ( parser->fields[i]->info != NULL )
			print ( _("\t\tINFO:\t\t%s\n"), parser->fields[i]->info );
		else
			print ( _("\t\tINFO:\t\tNone.\n"));
		if ( parser->fields[i]->type < 0 )
			print ( _("\t\tTYPE:\t\tUnknown.\n"));
		else
			print ( _("\t\tTYPE:\t\t%i (\"%s\")\n"), parser->fields[i]->type,
					PARSER_FIELD_TYPE_NAMES[parser->fields[i]->type] );
		if ( parser->fields[i]->value != NULL ) {
			/* pseudo field: we have a much terser definition here */
			print (_("\t\tVALUE:\t\t\"%s\"\n"), parser->fields[i]->value);
			print ("\n");
		} else {
			if ( parser->fields[i]->empty_allowed == TRUE )
				print ( _("\t\tEMPTY ALLOWED:\tYes.\n"));
			else
				print ( _("\t\tEMPTY ALLOWED:\tNo.\n"));
			if ( parser->fields[i]->unique == TRUE )
				print ( _("\t\tUNIQUE:\t\tYes.\n"));
			else
				print ( _("\t\tUNIQUE:\t\tNo.\n"));
			if ( parser->fields[i]->persistent == TRUE )
				print ( _("\t\tPERSISTENT:\tYes.\n"));
			else
				print ( _("\t\tPERSISTENT:\tNo.\n"));
			if ( parser->fields[i]->skip == TRUE )
				print ( _("\t\tSKIP:\t\tYes.\n"));
			else
				print ( _("\t\tSKIP:\t\tNo.\n"));
			if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
				print (_("\t\tCASE CHANGE:\t"));
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_NONE ) {
					print ( _("None.\n"));
				}
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_UPPER ) {
					print ( _("To upper case.\n"));
				}
				if ( parser->fields[i]->conversion_mode == PARSER_FIELD_CONVERT_LOWER ) {
					print ( _("To lower case.\n"));
				}
			}
			print ( _("\t\tSEPARATORS:\t"));
			j = 0;
			while ( j < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j] != NULL ) {
				print ( "\"%s\"", parser->fields[i]->separators[j] );
				if ( j+1 < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j+1] != NULL ) {
					print ( ", " );
				}
				j ++;
			}
			if ( j == 0 ) {
				print ( _("None."));
			}
			print ( "\n" );
			if ( parser->fields[i]->merge_separators == TRUE )
				print ( _("\t\tMERGE SEPS:\tYes.\n"));
			else
				print ( _("\t\tMERGE SEPS:\tNo.\n"));
			if ( parser->fields[i]->quote != 0 )
				print ( _("\t\tQUOTATION:\t\"%c\"\n"), parser->fields[i]->quote );
			else
				print ( _("\t\tQUOTATION:\tNone.\n"));
			/* any replacements specified? */
			if ( parser->fields[i]->type == PARSER_FIELD_TYPE_TEXT ) {
				print ( _("\t\tREPLACEMENTS:"));
				if ( parser->fields[i]->has_lookup == TRUE ) {
					print ( _("\n"));
					j = 0;
					while ( parser->fields[i]->lookup_old[j] != NULL ) {
						print ( _("\t\t\t\t\"%s\"=\"%s\"\n"),
								parser->fields[i]->lookup_old[j],
								parser->fields[i]->lookup_new[j]);
						j ++;
					}
				} else {
					print ( _("\tNone.\n"));
				}
			}
			print ( "\n");
		}
		i ++;
	}
	if ( i == 0 ) {
		print ( _("\tNone found.\n"));
	}

}
#endif


/*
 * Helper function for geometry tag field validation:
 * Returns TRUE" if "str" contains a number or +/- character.
 * If "is_double" is set to true, then the string is also
 * checked for the presence of the currently set decimal point
 * and thousands separator characters.
 */
BOOLEAN parser_check_numeric ( const char *str, BOOLEAN is_double, options *opts )
{
	int i;

	if ( str == NULL ) return FALSE;

	for ( i=0; i < strlen(str); i ++) {
		if ( str[i] == '0' ) return TRUE;
		if ( str[i] == '1' ) return TRUE;
		if ( str[i] == '2' ) return TRUE;
		if ( str[i] == '3' ) return TRUE;
		if ( str[i] == '4' ) return TRUE;
		if ( str[i] == '5' ) return TRUE;
		if ( str[i] == '6' ) return TRUE;
		if ( str[i] == '7' ) return TRUE;
		if ( str[i] == '8' ) return TRUE;
		if ( str[i] == '9' ) return TRUE;
		if ( str[i] == '+' ) return TRUE;
		if ( str[i] == '-' ) return TRUE;
		if ( is_double == TRUE ) {
			/* check for decimal point/group char: this can be set via an option or i18n default */
			if ( opts != NULL ) {
				if ( strlen (opts->decimal_point) > 0 ) {
					/* compare with option setting */
					if ( str[i] == opts->decimal_point[0] ) return TRUE;
				} else {
					/* compare with i18n default */
					char *check = i18n_get_decimal_point ();
					if ( check != NULL ) {
						if ( str[i] == check[0] ) {
							t_free ( check );
							return TRUE;
						}
						t_free ( check );
					}
				}
				if ( strlen (opts->decimal_group) > 0 ) {
					if ( str[i] == opts->decimal_group[0] ) return TRUE;
				} else {
					char *check = i18n_get_thousands_separator ();
					if ( check != NULL ) {
						if ( str[i] == check[0] ) {
							t_free ( check );
							return TRUE;
						}
						t_free ( check );
					}
				}
			}
		}
	}

	return FALSE;
}


/*
 * Validates current parser settings.
 * Aborts in CLI mode, if an error was found.
 * In GUI mode, check the return value. Anything other than "0" is an error.
 */
int parser_desc_validate ( parser_desc *parser, options *opts )
{
	parser_field *field;
	int i,j,k;
	int num_fields;
	char *p1,*p2;
	BOOLEAN found_x, found_y, found_z;
	BOOLEAN found;


	/* check for at least one field definition */
	num_fields = 0;
	while ( parser->fields[num_fields] != NULL ) {
		num_fields ++;
	}
	if ( num_fields == 0 ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nThere are no field definitions."),
				opts->schema_file);
		return (1);
	}

	/* check for validity of field descriptions */
	i = 0;
	while ( parser->fields[i]!=NULL ) {
		field = parser->fields[i];
		if ( field->name == NULL ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField defined after line %i has no name."),
					opts->schema_file, field->definition);
			return (1);
		}
		if ( field->type == PARSER_FIELD_TYPE_UNDEFINED ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField defined after line %i has no type."),
					opts->schema_file, field->definition);
			return (1);
		}
		if ( field->value != NULL ) {
			/* This is a pseudo field with constant value.
			 * It requires only minimal checking.*/
			/* 1: check if any unneeded options are given */
			if ( field->conversion_mode_set == TRUE
					|| field->empty_allowed_set == TRUE
					|| field->merge_separators_set == TRUE
					|| field->persistent_set == TRUE
					|| field->skip_set == TRUE
					|| field->unique_set == TRUE
			) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField defined after line %i has too many options. This special field only accepts \"info\", \"name\", \"type\" and \"value\"."),
						opts->schema_file, field->definition);
				return (1);
			}
			/* 2: check for type compatibility */
			if ( field->type == PARSER_FIELD_TYPE_DOUBLE ) {
				BOOLEAN error;
				BOOLEAN overflow;
				parser_str_to_dbl_field ( parser, field->name, field->value, opts->decimal_point[0], opts->decimal_group[0],
						&error, &overflow );
				if ( error == TRUE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nValue of field \"%s\" is not a valid number."),
							opts->schema_file, field->name);
					return (1);
				}
				if ( overflow == TRUE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nOverflow detected for value of field \"%s\"."),
							opts->schema_file, field->name);
					return (1);
				}
				if ( field->lookup_new[0] != NULL ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nString replacement not allowed for field \"%s\" (wrong type) ."),
							opts->schema_file, field->name);
					return (1);
				}
			}
			if ( field->type == PARSER_FIELD_TYPE_INT ) {
				BOOLEAN error;
				BOOLEAN overflow;
				parser_str_to_int_field ( parser, field->name, field->value, &error, &overflow );
				if ( error == TRUE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nValue of field \"%s\" is not a valid integer number."),
							opts->schema_file, field->name);
					return (1);
				}
				if ( overflow == TRUE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nOverflow detected for value of field \"%s\"."),
							opts->schema_file, field->name);
					return (1);
				}
				if ( field->lookup_new[0] != NULL ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nString replacement not allowed for field \"%s\" (wrong type) ."),
							opts->schema_file, field->name);
					return (1);
				}
			}
			/* no further checks required */
			break;
		}
		if ( field->conversion_mode_set == TRUE ) {
			if ( field->type != PARSER_FIELD_TYPE_TEXT ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField defined after line %i is no a text field.\nTherefore, \"change_case\" is not a valid option."),
						opts->schema_file, field->definition);
				return (1);
			}
		}
		j = 0;
		while ( field->separators != NULL && j < PARSER_MAX_SEPARATORS ) {
			j++;
		}
		if ( j == 0 ) {
			if ( i != (num_fields-1) ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" needs a separator (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			} 
		}
		i++;
	}

	/* Comment marks, quotation marks and field separators may not have characters in common */
	for ( i=0; i < num_fields; i ++ ) {
		field = parser->fields[i];
		/* check for quotation mark (single char) vs. separators */
		j = 0;
		while ( j < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j] != NULL ) {
			if ( parser->fields[i]->quote != 0 ) {
				p1 = parser->fields[i]->separators[j];
				while ( *p1 != '\0' ) {
					if ( *p1 == parser->fields[i]->quote ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": quoting character matches separator (after line %i)."),
								opts->schema_file, field->name, field->definition );
						return (1);
					}
					p1 += sizeof(char);
				}
			}
			j ++;
		}
		/* check for comment chars vs. separator chars */
		j = 0;
		while ( j < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j] != NULL ) {
			k = 0;
			while ( k < PARSER_MAX_COMMENTS && parser->comment_marks[k] != NULL ) {
				p1 = parser->fields[i]->separators[j];
				while ( *p1 != '\0' ) {
					p2 = parser->comment_marks[k];
					while ( *p2!= '\0' ) {
						if ( *p2 == *p1 ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": separator matches comment character (after line %i)."),
									opts->schema_file, field->name, field->definition );
							return (1);
						}
						p2 += sizeof (char);
					}
					p1 += sizeof (char);
				}
				k ++;
			}
			j ++;
		}
		/* check for quoting char vs. comment chars */
		j = 0;
		while ( j < PARSER_MAX_COMMENTS && parser->comment_marks[j] != NULL ) {
			if ( parser->fields[i]->quote != 0 ) {
				p1 = parser->comment_marks[j];
				while ( *p1 != '\0' ) {
					if ( *p1 == parser->fields[i]->quote ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": quoting character matches comment char (after line %i)."),
								opts->schema_file, field->name, field->definition );
						return (1);
					}
					p1 += sizeof(char);
				}
			}
			j ++;
		}
	}

	/* "merge_separators" and "empty_allowed" cannot be used together */
	for ( i=0; i < num_fields; i ++ ) {
		field = parser->fields[i];
		if ( field->empty_allowed == TRUE && field->merge_separators == TRUE ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": \"empty_allowed\" and \"merge_seperators\" are mutually exclusive (after line %i)."),
					opts->schema_file, field->name, field->definition );
			return (1);
		}
	}

	/* Fields must have at least one separator, except for last field. */
	for ( i=0; i < num_fields; i ++ ) {
		field = parser->fields[i];
		if ( i < (num_fields-1) ) {
			if ( field->separators[0] == NULL && field->value == NULL ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" has no separator(s) (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			}
		}
	}
	field = parser->fields[num_fields-1];
	if ( field->separators[0] != NULL ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": Last field must not have a separator (after line %i)."),
				opts->schema_file, field->name, field->definition );
		return (1);
	}

	/* check that fields with "empty_allowed" do not use whitespace as field separators */
	for ( i = 0; i < num_fields; i ++ ) {
		field = parser->fields[i];
		if ( field->empty_allowed == TRUE ) {
			j = 0;
			while ( j < PARSER_MAX_SEPARATORS && field->separators[j] != NULL ) {
				if ( !strcmp (field->separators[j]," ") || !strcmp (field->separators[j],"\t") ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": Must not combine whitespace separators and \"empty_allowed\" (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
				j ++;
			}
		}
	}

	/* Exactly one X and Y field must exist. Exactly on Z field may exist.
	   The coordinate fields cannot be set to the same field name. */
	found_x = FALSE;
	found_y = FALSE;
	found_z = FALSE;
	if ( parser->coor_x == NULL ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo X coordinate field defined."),
				opts->schema_file );
		return (1);
	}
	if ( parser->coor_y == NULL ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo Y coordinate field defined."),
				opts->schema_file );
		return (1);
	}
	if ( parser->coor_z == NULL ) {
		err_show ( ERR_NOTE, _("No Z field defined in parser schema (%s).\nZ assumed to be constant 0."),
				opts->schema_file );
		parser->coor_z = strdup (""); /* set Z field name to empty string */
		found_z = TRUE;
	}
	if ( 	!strcasecmp (parser->coor_x, parser->coor_y ) ||
			!strcasecmp (parser->coor_x, parser->coor_z ) ||
			!strcasecmp (parser->coor_y, parser->coor_z ) ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nCoordinate fields are not unique."),
				opts->schema_file );
		return (1);
	}

	/* Coordinate fields must exist and must be of type double.
	   They also must not overlap with tag and key fields and cannot be empty. */
	for ( i=0; i < num_fields; i ++ ) {
		field = parser->fields[i];
		if ( !strcasecmp ( parser->coor_x, field->name ) ) {
			found_x = TRUE;
			if ( field->type != PARSER_FIELD_TYPE_DOUBLE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is X coordinate field, but not of type 'double' (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			}
			if ( field->empty_allowed == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is X coordinate field, but \"empty_allowed\" was set to \"Yes\" (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			}
			if ( parser->key_field!= NULL ) {
				if ( !strcasecmp ( parser->key_field, field->name ) ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is X coordinate field, and cannot be used as key field (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
			}
			if ( parser->tag_field!= NULL ) {
				if ( !strcasecmp ( parser->tag_field, field->name ) ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is X coordinate field, and cannot be used as tag field (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
			}
		}
		if ( !strcasecmp ( parser->coor_y, field->name ) ) {
			found_y = TRUE;
			if ( field->type != PARSER_FIELD_TYPE_DOUBLE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Y coordinate field, but not of type 'double' (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			}
			if ( field->empty_allowed == TRUE ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Y coordinate field, but \"missing_allowed\" was set to \"Yes\" (after line %i)."),
						opts->schema_file, field->name, field->definition );
				return (1);
			}
			if ( parser->key_field!= NULL ) {
				if ( !strcasecmp ( parser->key_field, field->name ) ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Y coordinate field, and cannot be used as key field (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
			}
			if ( parser->tag_field!= NULL ) {
				if ( !strcasecmp ( parser->tag_field, field->name ) ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Y coordinate field, and cannot be used as tag field (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
			}
		}
		if ( strlen ( parser->coor_z ) != 0 ) {
			if ( !strcasecmp ( parser->coor_z, field->name ) ) {
				found_z = TRUE;
				if ( field->type != PARSER_FIELD_TYPE_DOUBLE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Z coordinate field, but not of type 'double' (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
				if ( field->empty_allowed == TRUE ) {
					err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Z coordinate field, but \"missing_allowed\" was set to \"Yes\" (after line %i)."),
							opts->schema_file, field->name, field->definition );
					return (1);
				}
				if ( parser->key_field!= NULL ) {
					if ( !strcasecmp ( parser->key_field, field->name ) ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Z coordinate field, and cannot be used as key field (after line %i)."),
								opts->schema_file, field->name, field->definition );
						return (1);
					}
				}
				if ( parser->tag_field!= NULL ) {
					if ( !strcasecmp ( parser->tag_field, field->name ) ) {
						err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\" is Z coordinate field, and cannot be used as tag field (after line %i)."),
								opts->schema_file, field->name, field->definition );
						return (1);
					}
				}
			}
		}
	}
	if ( found_x == FALSE ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nX coordinate field does not exist."),
				opts->schema_file );
		return (1);
	}
	if ( found_y == FALSE ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nY coordinate field does not exist."),
				opts->schema_file );
		return (1);
	}
	if ( found_z == FALSE ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nZ coordinate field does not exist."),
				opts->schema_file );
		return (1);
	}

	/* Tag field can only be NULL if mode is "none" */
	if ( parser->tag_field == NULL && parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo geometry tag field provided."),
				opts->schema_file );
		return (1);
	}

	/* in most tag modes, tags for lines and polygons are required */
	if ( parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ) < 1 ) {
			// if ( parser->geom_tag_point == NULL ) { DELETE_ME
			if ( parser->tag_strict == TRUE || parser->tag_mode == PARSER_TAG_MODE_MAX ) {
				err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo tag string for point type geometries given."),
						opts->schema_file );
				return (1);
			}
		}
		if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ) < 1 ) {
			//if ( parser->geom_tag_line == NULL ) { DELETE ME
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo tag string for line type geometries given."),
					opts->schema_file );
			return (1);
		}
		if ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ) < 1 ) {
			// if ( parser->geom_tag_poly == NULL ) { DELETE_ME
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo tag string for polygon type geometries given."),
					opts->schema_file );
			return (1);
		}
	}

	/* clear point tags if the corresponding parser option is not set */
	if ( ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ) < 1 ) && parser->tag_mode != PARSER_TAG_MODE_NONE ) {
	// if ( parser->geom_tag_point == NULL && parser->tag_mode != PARSER_TAG_MODE_NONE ) { DELETE ME
		// parser->geom_tag_point = strdup (""); /* empty point tag means that it is not defined */ DELETE ME
		parser_tags_clear ( parser, PARSER_GEOM_TAG_POINT );
	}

	if ( parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		// if ( parser->geom_tag_line == NULL || parser->geom_tag_poly == NULL ) { DELETE ME
		if ( ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ) < 1 ) || ( parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ) < 1 ) ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nGeometry tags must be provided for both lines and polygons.\nSet both \"geom_tag_line\" and \"geom_tag_poly\""),
					opts->schema_file );
			return (1);
		}
	}

	// DELETE ME: This is now checked after a call to parser_tags_add().
	/* Tagging strings for points, lines and polygons must be unique,
	    and they cannot be the same as any quote, comment or separator char for the same field.	*/
	/*
	if ( parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		if (	!strcasecmp (parser->geom_tag_point, parser->geom_tag_line ) ||
				!strcasecmp (parser->geom_tag_point, parser->geom_tag_poly ) ||
				!strcasecmp (parser->geom_tag_line, parser->geom_tag_poly ) ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nGeometry tags are not unique."),
					opts->schema_file );
			return (1);
		}
	}
	*/

	/* Check for geom tags vs. separators:
	 * No geometry tag or character that is part of one(!) is allowed to match a separator char!
	 */
	if ( parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		for ( i=0; i < num_fields; i ++ ) {
			field = parser->fields[i];
			j = 0;
			while ( j < PARSER_MAX_SEPARATORS && parser->fields[i]->separators[j] != NULL ) {
				p1 = parser->fields[i]->separators[j];
				while ( *p1 != '\0' ) {
					int t;
					/* points */
					for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ); t ++ ) {
						// p2 = parser->geom_tag_point; DELETE ME
						p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, t );
						while ( *p2 != '\0' ) {
							if ( *p2 == *p1 ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": separator matches point geom tag (after line %i)."),
										opts->schema_file, field->name, field->definition );
								return (1);
							}
							p2 += sizeof (char);
						}
					}
					/* lines */
					for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ); t ++ ) {
						// p2 = parser->geom_tag_line; DELETE ME
						p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, t );
						while ( *p2!= '\0' ) {
							if ( *p2 == *p1 ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": separator matches line geom tag (after line %i)."),
										opts->schema_file, field->name, field->definition );
								return (1);
							}
							p2 += sizeof (char);
						}
					}
					/* polygons */
					for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ); t ++ ) {
						// p2 = parser->geom_tag_poly; DELETE ME
						p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, t );
						while ( *p2!= '\0' ) {
							if ( *p2 == *p1 ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": separator matches poly geom tag (after line %i)."),
										opts->schema_file, field->name, field->definition );
								return (1);
							}
							p2 += sizeof (char);
						}
					}
					p1 += sizeof (char);
				}
				j ++;
			}
		}
		/* check for geom tags vs. comment marks */
		j = 0;
		while ( j < PARSER_MAX_COMMENTS  && parser->comment_marks[j] != NULL ) {
			p1 = parser->comment_marks[j];
			while ( *p1 != '\0' ) {
				int t;
				/* points */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ); t ++ ) {
					// p2 = parser->geom_tag_point; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == *p1 ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nComment mark matches point geom tag."),
									opts->schema_file );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
				/* lines */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ); t ++ ) {
					// p2 = parser->geom_tag_line; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == *p1 ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nComment mark matches line geom tag."),
									opts->schema_file );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
				/* polygons */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ); t ++ ) {
					// p2 = parser->geom_tag_poly; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == *p1 ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nComment mark matches poly geom tag."),
									opts->schema_file );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
				p1 += sizeof (char);
			}
			j ++;
		}
		/* check for geom tags vs. quotation char (defined PER FIELD!) */
		for ( i=0; i < num_fields; i ++ ) {
			field = parser->fields[i]; /* need to check every field's definition */
			if ( parser->fields[i]->quote != 0 ) { /* 'quote' can only be a single character */
				int t;
				/* points */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT ); t ++ ) {
					// p2 = parser->geom_tag_point; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == parser->fields[i]->quote ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": quoting char matches point geom tag (after line %i)."),
									opts->schema_file, field->name, field->definition );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
				/* lines */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE ); t ++ ) {
					// p2 = parser->geom_tag_line; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == parser->fields[i]->quote ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": quoting char matches line geom tag (after line %i)."),
									opts->schema_file, field->name, field->definition );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
				/* polygons */
				for ( t=0; t < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY ); t ++ ) {
					// p2 = parser->geom_tag_poly; DELETE ME
					p2 = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, t );
					while ( *p2!= '\0' ) {
						if ( *p2 == parser->fields[i]->quote ) {
							err_show ( ERR_EXIT, _("Error in parser schema (%s).\nField \"%s\": quoting char matches poly geom tag (after line %i)."),
									opts->schema_file, field->name, field->definition );
							return (1);
						}
						p2 += sizeof (char);
					}
				}
			}
		}
	}

	/* if the tag is stored in a numeric field (not recommended) then we need some extra strictness */
	if ( parser->tag_mode != PARSER_TAG_MODE_NONE ) {
		int i;
		for ( i=0; i < PRG_MAX_FIELDS; i ++ ) {
			if ( parser->fields[i] != NULL ) {
				if ( !strcasecmp ( parser->tag_field, parser->fields[i]->name ) ) {
					int ftype = parser->fields[i]->type;
					/* apply some extra checks only if we have a numeric field */
					if ( ( ftype == PARSER_FIELD_TYPE_INT ) || ( ftype == PARSER_FIELD_TYPE_DOUBLE) ) {
						int j;
						BOOLEAN is_double = ( parser->fields[i]->type == PARSER_FIELD_TYPE_DOUBLE ); /* even more checks needed for double type fields */
						/* POINT TAGS */
						for ( j=0; j < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT); j++ ) {
							if ( parser_check_numeric ( parser_get_tag( parser, PARSER_GEOM_TAG_POINT, j), is_double, opts ) == TRUE ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nPoint geometry tag ('%s') with numeric characters cannot be stored in numeric field."),
													opts->schema_file, parser_get_tag( parser, PARSER_GEOM_TAG_POINT, j) );
								return (1);
							}
						}
						/* LINE TAGS */
						for ( j=0; j < parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE); j++ ) {
							if ( parser_check_numeric ( parser_get_tag( parser, PARSER_GEOM_TAG_LINE, j), is_double, opts ) == TRUE ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nLine geometry tag ('%s') with numeric characters cannot be stored in numeric field."),
										opts->schema_file, parser_get_tag( parser, PARSER_GEOM_TAG_LINE, j) );
								return (1);
							}
						}
						/* POLYGON TAGS */
						for ( j=0; j < parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY); j++ ) {
							if ( parser_check_numeric ( parser_get_tag( parser, PARSER_GEOM_TAG_POLY, j), is_double, opts ) == TRUE ) {
								err_show ( ERR_EXIT, _("Error in parser schema (%s).\nPolygon geometry tag ('%s') with numeric characters cannot be stored in numeric field."),
										opts->schema_file, parser_get_tag( parser, PARSER_GEOM_TAG_POLY, j) );
								return (1);
							}
						}
					}
				}
			}
		}
	}

	/* Key field MUST be specified for PARSER_TAG_MODE_END and PARSER_TAG_MODE_MAX,
	 * and it cannot be a coordinate field (all others are OK). */
	if ( parser->key_field == NULL ) {
		if ( 	parser->tag_mode == PARSER_TAG_MODE_END ||
				parser->tag_mode == PARSER_TAG_MODE_MAX )
		{
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo key field specified."),
					opts->schema_file );
			return (1);
		}
	}

	/* Key field has no function in PARSER_MODE_MIN and PARSER_MODE_NONE. */
	if ( parser->key_field != NULL ) {
		if ( 	parser->tag_mode == PARSER_TAG_MODE_MIN ||
				parser->tag_mode == PARSER_TAG_MODE_NONE )
		{
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for key field will be ignored."),
					opts->schema_file );
		}
	}

	/* Tag field must be specified in mode PARSER_MODE_MIN, PARSER_MODE_MAX, and PARSER_MODE_END. */
	if ( parser->tag_field == NULL ) {
		if ( 	parser->tag_mode == PARSER_TAG_MODE_MIN ||
				parser->tag_mode == PARSER_TAG_MODE_MAX ||
				parser->tag_mode == PARSER_TAG_MODE_END )
		{
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNo tag field specified."),
					opts->schema_file );
			return (1);
		}
	}

	/* In PARSER_MODE_MIN, we make sure that all coordinate fields are automatically set to "persistent=yes". */
	if ( parser->tag_mode == PARSER_TAG_MODE_MIN ) {
		for ( i=0; i < num_fields; i ++ ) {
			if ( ( parser->coor_x != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_x ) ) ||
					( parser->coor_y != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_y ) ) ||
					( parser->coor_z != NULL && !strcasecmp ( parser->fields[i]->name, parser->coor_z ) ) )
			{
				parser->fields[i]->persistent = TRUE;
			}
		}
	}

	/* Tag field has no function in PARSER_MODE_NONE. */
	if ( parser->key_field != NULL ) {
		if ( parser->tag_mode == PARSER_TAG_MODE_NONE )
		{
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for tag field will be ignored."),
					opts->schema_file );
		}
	}

	/* In PARSER_MODE_MAX, tag_field and key_field must be different fields */
	if ( parser->tag_mode == PARSER_TAG_MODE_MAX ) {
		if ( !strcasecmp ( parser->key_field, parser->tag_field ) ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nTag field and key field must not be identical."),
					opts->schema_file );
			return (1);
		}
	}

	/* check that tag field and key field point to valid field definitions */
	if ( parser->tag_field != NULL  ) {
		found = FALSE;
		for ( i=0; i < num_fields; i ++ ) {
			field = parser->fields[i];
			if ( !strcasecmp ( field->name, parser->tag_field ) )
				found = TRUE;
		}
		if ( found == FALSE ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nTag field is not set to the name of a valid field."),
					opts->schema_file );
			return (1);
		}
	}
	if ( parser->key_field != NULL  ) {
		found = FALSE;
		for ( i=0; i < num_fields; i ++ ) {
			field = parser->fields[i];
			if ( !strcasecmp ( field->name, parser->key_field ) )
				found = TRUE;
		}
		if ( found == FALSE ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nKey field is not set to the name of a valid field."),
					opts->schema_file );
			return (1);
		}
	}

	/* mode "Min": reduced lines must contain less fields than complete lines */
	if ( parser->tag_mode == PARSER_TAG_MODE_MIN ) {
		int num_fields_reduced;
		/* determine number of fields in reduced records in mode "min" */
		num_fields_reduced = 2; /* at least X and Y */
		/* do we also have a Z field and/or "persistent" fields? */
		if ( parser->coor_z != NULL ) {
			num_fields_reduced ++;
		}
		for ( i = 0; i < num_fields; i ++ ) {
			field = parser->fields[i];
			if ( field->persistent ) {
				num_fields_reduced ++;
			}
		}
		if ( num_fields_reduced >= num_fields ) {
			err_show ( ERR_EXIT, _("Error in parser schema (%s).\nNumber of fields in reduced records must be smaller\nthan in full records."),
					opts->schema_file );
			return (1);
		}
	}

	/* mode "None": many parser definitions may be superfluous */
	if ( parser->tag_mode == PARSER_TAG_MODE_NONE ) {
		if ( parser->key_field != NULL ) {
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for key field will be ignored."),
					opts->schema_file );
		}
		if ( parser->tag_field != NULL ) {
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for key field will be ignored."),
					opts->schema_file );
		}
		if ( parser->tag_strict_set == TRUE ) {
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for \"tag_strict\" will be ignored."),
					opts->schema_file );
		}
		if ( parser->key_unique_set == TRUE ) {
			err_show ( ERR_WARN, _("Unneeded setting in parser schema (%s).\nSetting for \"key_unique\" will be ignored."),
					opts->schema_file );
		}
		// if ( parser->geom_tag_point != NULL || parser->geom_tag_line != NULL || parser->geom_tag_poly != NULL ) { DELETE ME
		if ( parser_get_num_tags(parser,PARSER_GEOM_TAG_POINT) + parser_get_num_tags(parser,PARSER_GEOM_TAG_LINE) +
						parser_get_num_tags(parser,PARSER_GEOM_TAG_POLY) > 0 ) {
			err_show ( ERR_WARN, _("Unneeded setting(s) in parser schema (%s).\nSetting(s) for geometry tags will be ignored."),
					opts->schema_file );
		}
	}

	return (0);
}


/*
 * Main function that reads all input data and parses it for both
 * geometries and attribute values.
 */
void parser_consume_input ( parser_desc *parser, options *opts, parser_data_store **storage )
{
	int num_fields;
	int error;
	int i, j, k, len, len_reduced;
	parser_field *field, *field_reduced;
	int current_field, current_field_reduced;
	char **contents;
	FILE *in;
	char *buffer, *buffer_reduced, *tmp, *p, *p_reduced, *start, *start_reduced;
	BOOLEAN is_comment;
	/* BOOLEAN DEBUG = FALSE;*/

	/* determine number of declared fields */
	num_fields = 0;
	while ( parser->fields[num_fields] != NULL ) {
		num_fields ++;
	}

	/* storage array for field contents */
	contents = malloc ( sizeof (char*) * num_fields );

	/* Loop through all input files */
	for ( i=0; i < opts->num_input ; i ++ ) {

		/* file name "-" means "read from stdin" */
		if ( !strcmp ( opts->input[i], "-" ) ) {
			in = stdin;
		} else {
			/* attempt to open current input file */
#ifdef MINGW
			in = t_fopen_utf8 ( opts->input[i], "rt" );
#else
			in = t_fopen_utf8 ( opts->input[i], "r" );
#endif			
			if ( in == NULL ) {
				err_show ( ERR_EXIT, _("Cannot open input file for reading ('%s').\nReason: %s"),
						opts->input[i], strerror (errno));
				free ( contents );
				return;
			}
		}

		char *line = malloc ( sizeof (char) * PARSER_MAX_FILE_LINE_LENGTH);
		unsigned int line_no = 1;

		unsigned int valid_line_no = 0; /* the first valid line is one that has the same number of fields as specified by parser description */

		/* parse lines in current file */
		while ( fgets ( line, PARSER_MAX_FILE_LINE_LENGTH, in ) ) {

			for ( j = 0; j < num_fields; j ++ )
				contents[j] = NULL;

			/* check if line is too long to be processed */
			p = index ( line, '\n');
			if ( p == NULL ) {
				if ( strlen ( line ) > (PARSER_MAX_FILE_LINE_LENGTH-1) ) {
					if ( in != stdin ) {
						err_show ( ERR_EXIT, _("Line too long in input file '%s' (line no.: %i).\nThe maximum line length allowed is: %i characters."),
								opts->input[i], line_no, PARSER_MAX_FILE_LINE_LENGTH);
						free ( line );
						free ( contents );
						return;
					} else {
						err_show ( ERR_EXIT, _("Input line too long.\nThe maximum line length allowed is: %i characters."),
								PARSER_MAX_FILE_LINE_LENGTH);
						free ( line );
						free ( contents );
						return;
					}
				} else {
					/* add a line terminator */
					line[strlen(line)] = '\n';
				}
			}

			buffer = t_str_pack ( (const char*) line );
			buffer_reduced = strdup (buffer);
			/* skip totally empty lines (after packing) */

			/* check whether the first token is a comment mark */
			is_comment = FALSE;
			j = 0;
			while ( parser->comment_marks[j] != NULL ) {
				tmp = t_str_ndup ( buffer, strlen (parser->comment_marks[j]) );
				if ( !strcmp ( tmp, parser->comment_marks[j] ) ) {
					is_comment = TRUE;
					free ( tmp );
					break;
				}
				j ++;
				free ( tmp );
			}

			/* process line only if it actually contains data */
			if ( ( strlen ( buffer ) > 0 ) && ( is_comment == FALSE ) ) {

				current_field = 0; /* point to first field */
				field = parser->fields[current_field];

				p = buffer; /* pointer to current line of input */
				start = p; /* point to start of current token */
				len = 0; /* keeps track of length of current token */

				/* process current input line */
				while ( *p != '\0' ) { /* read until EOL is reached */
					if ( field->value != NULL ) {
						/* field is a pseudo field: assign constant value */
						contents[current_field] = strdup ( field->value );
						current_field ++;
						field = parser->fields[current_field];
					} else {
						if ( field->separators[0] != NULL ) {
							/* go through all field separators */
							j = 0;
							while ( field->separators[j] != NULL ) {
								tmp = t_str_ndup ( p, strlen (field->separators[j]) );
								if ( !strcmp ( tmp, field->separators[j] ) ) {
									/* got a separator */
									p += sizeof ( char ) * strlen (field->separators[j]);
									if ( len > 0 ) {
										contents[current_field] = t_str_ndup ( start, len );
										current_field ++;
									}
									j = 0;
									start = p;
									len = 0;
								} else {
									/* expand length of current field */
									field = parser->fields[current_field];
									len ++;
									p += sizeof ( char );
								}
								free ( tmp );
								j ++;
							}
						} else {
							/* expand length of current field */
							len ++;
							p += sizeof ( char );
						}
					}
				}
				/* get contents of last field (unless it's a pseudo field) */
				if ( field->value == NULL ) {
					contents[current_field] = strdup ( start );
				}
				/* get any trailing pseudo fields that may still exist (not in
				 * the actual data, but as definitions with constant values) */
				if ( num_fields-current_field < num_fields ) {
					/* we are short at least one field */
					for ( k=num_fields-1; k > 0; k-- ) {
						/* Move backwards through field definitions and add
						  any pseudo field values. */
						field = parser->fields[k];
						if ( field->value != NULL ) {
							contents[k] = strdup ( field->value );
							current_field ++; /* add this point, current_field is just a dumb counter */
						}
					}
				}

				if ( (current_field+1) == num_fields ) {
					/* we have the number of fields required from a full record */
					valid_line_no ++;
				}

				if ( 	parser->tag_mode == PARSER_TAG_MODE_MIN
						&& (current_field+1) < num_fields
						&& valid_line_no > 0) {
					/* In tag mode "min", we will encounter many lines with reduced field
					   numbers. In such case, we  read the line a second time,
					   shifting the field separators for those fields that also exist in reduced
					   records (i.e. coordinate fields and fields set to be "persistent".
					   Then we keep that instead of the result of the first parsing:
					   If the line was invalid in the first place, then it doesn't matter, anyway.
					 */

					/* clear old contents */
					for ( j=0; j < num_fields; j++ ) {
						if ( contents[j] != NULL ) {
							free (contents[j]);
						}
						contents[j] = NULL;
					}

					current_field_reduced = 0; /* point to first field */
					field_reduced = parser->fields[current_field_reduced];

					p_reduced = buffer_reduced; /* pointer to current line of input */
					start_reduced = p_reduced; /* point to start of current token */
					len_reduced = 0; /* keeps track of length of current token */

					/* process current input line */
					while ( *p_reduced != '\0' && current_field_reduced < num_fields-1 ) { /* read until EOL is reached */
						BOOLEAN is_coordinate_field = FALSE;
						if (( parser->coor_x != NULL && !strcasecmp ( field_reduced->name, parser->coor_x ) ) ||
								( parser->coor_y != NULL && !strcasecmp ( field_reduced->name, parser->coor_y ) ) ||
								( parser->coor_z != NULL && !strcasecmp ( field_reduced->name, parser->coor_z ) ))
						{
							is_coordinate_field = TRUE;
						}
						if ( field_reduced->value != NULL ||
								(field_reduced->persistent == FALSE && is_coordinate_field == FALSE) )
						{
							/* Just skip over pseudo-fields and fields that are not
							   marked "persistent". Note: Coordinate fields are
							   always assumed to be persistent.
							 */
							current_field_reduced ++;
							field_reduced = parser->fields[current_field_reduced];

						} else {
							if ( field_reduced->separators[0] != NULL ) {
								/* go through all field separators */
								j = 0;
								while ( field_reduced->separators[j] != NULL ) {
									tmp = t_str_ndup ( p_reduced, strlen (field_reduced->separators[j]) );
									if ( !strcmp ( tmp, field_reduced->separators[j] ) ) {
										/* got a separator */
										p_reduced += sizeof ( char ) * strlen (field_reduced->separators[j]);
										if ( len_reduced > 0 ) {
											contents[current_field_reduced] = t_str_ndup ( start_reduced, len_reduced );
											current_field_reduced ++;
											if ( is_coordinate_field == FALSE ) {
												current_field ++;
											}
										}
										j = 0;
										start_reduced = p_reduced;
										len_reduced = 0;
									} else {
										/* expand length of current field */
										field_reduced = parser->fields[current_field_reduced];
										len_reduced ++;
										p_reduced += sizeof ( char );
									}
									free ( tmp );
									j ++;
								}
							} else {
								/* expand length of current field */
								len_reduced ++;
								p_reduced += sizeof ( char );
							}
						}
					}
					/* get contents of last field (unless it's a pseudo field) */
					if ( field_reduced->value == NULL ) {
						contents[current_field_reduced] = strdup ( start_reduced );
					}
				}

				/* store record */
				error = parser_record_store ( (const char**) contents, current_field+1, line_no, storage[i], parser, opts );
				if ( error != 0 ) {
					if ( in != stdin ) {
						err_show ( ERR_EXIT, _("Error storing data from file '%s' (line no.: %i):\n%s"),
								opts->input[i], line_no, err_msg );
						free ( line );
						free ( contents );
						return;
					} else {
						err_show ( ERR_EXIT, _("Error storing data (line no.: %i):\n%s"),
								line_no, err_msg );
						free ( line );
						free ( contents );
						return;
					}
				}

				/* validate record and store coordinates */
				error = parser_record_validate_store_coords ( storage[i]->slot-1, current_field+1, storage[i], parser, opts );
				if ( error != 0 ) {
					if ( in != stdin ) {
						err_show ( ERR_EXIT, _("Error validating data from file '%s' (line no.: %i):\n%s"),
								opts->input[i], line_no, err_msg );
						free ( line );
						free ( contents );
						return;
					} else {
						err_show ( ERR_EXIT, _("Error validating data (line no.: %i):\n%s"),
								line_no, err_msg );
						free ( line );
						free ( contents );
						return;
					}
				}

			}

			free ( buffer );
			free ( buffer_reduced );
			/* free mem for field values */
			for ( j = 0; j < num_fields; j ++ ) {
				if ( contents[j] != NULL )
					free ( contents[j] );
			}
			line_no ++;

		} /* END (parse lines in current file) */

		/* done with this file */
		free ( line );
		if ( in != stdin )
			fclose ( in );
	} /* END (loop through all input files) */
	free ( contents );
}


/* Converts a string stored in a parser field to an integer if possible.
 * USE THIS FUNCTION WHENEVER AN INT VALUE MUST BE READ FROM A FIELD'S
 * CONTENTS!
 * This function works just like 't_str_to_int()', except for one
 * critical detail: characters that represent geometry tags are removed from
 * the input string before trying to convert it into a number.
 * This function is meant to cover the rare case where a numeric field
 * also serves as a geometry tag field. In a valid input dataset, geometry
 * tags may precede or succeed the numeric data in the field content.
 * This function requires a parser definition and a field name as
 * arguments, in addition to the arguments that must be passed to
 * 't_str_to_int()'.
 */
int parser_str_to_int_field ( parser_desc *parser, const char *field, const char *str, BOOLEAN *error, BOOLEAN *overflow )
{
	char* copy;
	char* cleaned;
	int result;

	/* reset error states */
	if ( error != NULL ) *error = FALSE;
	if ( overflow != NULL ) *overflow = FALSE;
	errno = 0;

	/* basic error handling */
	if ( parser == NULL || field == NULL || strlen (field ) < 1 ||
			str == NULL || strlen ( str ) < 1 )
	{
		if ( error != NULL ) *error = TRUE;
		return ( 0 );
	}

	/* check if field is a tagging field and needs special treatment */
	if ( parser->tag_field == NULL ) {
		/* no tags defined in parser */
		return ( t_str_to_int (str, error, overflow) );
	}
	if ( strcasecmp ( parser->tag_field, field ) != 0 ) {
		/* not a tag field */
		return ( t_str_to_int (str, error, overflow) );
	}

	/* duplicate input string */
	copy = strdup (str);

	/* clean input string from any geometry tag (replace with spaces and trim result) */
	{
		int num;
		int i;
		char *tag = NULL;
		char *token = NULL;
		/* POINTS */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
		/* LINES */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
		/* POLYGONS */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
	}
	/* pack string and release mem for copy */
	cleaned = t_str_pack(copy);
	t_free ( copy );

	/* basic error handling */
	if ( cleaned == NULL || strlen ( cleaned ) < 1 ) {
		if ( error != NULL ) *error = TRUE;
		t_free ( cleaned );
		return ( 0 );
	}

	/* call t_str_to_int to get conversion result */
	result = t_str_to_int ( (const char*) cleaned, error, overflow );

	/* clean up and return result */
	t_free ( cleaned );

	return ( result );
}

/* Converts a string stored in a parser field to a double if possible.
 * USE THIS FUNCTION WHENEVER A DOUBLE VALUE MUST BE READ FROM A FIELD'S
 * CONTENTS!
 * This function works just like 't_str_to_dbl()', except for one
 * critical detail: characters that represent geometry tags are removed from
 * the input string before trying to convert it into a number.
 * This function is meant to cover the rare case where a numeric field
 * also serves as a geometry tag field. In a valid input dataset, geometry
 * tags may precede or succeed the numeric data in the field content.
 * This function requires a parser definition and a field name as
 * arguments, in addition to the arguments that must be passed to
 * 't_str_to_dbl()'.
 */
double parser_str_to_dbl_field ( parser_desc *parser, const char *field, const char *str,
		const char decp, const char tsep,
		BOOLEAN *error, BOOLEAN *overflow )
{
	char* copy;
	char* cleaned;
	int result;

	/* reset error states */
	if ( error != NULL ) *error = FALSE;
	if ( overflow != NULL ) *overflow = FALSE;
	errno = 0;

	/* basic error handling */
	if ( parser == NULL || field == NULL || strlen (field ) < 1 ||
			str == NULL || strlen ( str ) < 1 )
	{
		if ( error != NULL ) *error = TRUE;
		return ( 0 );
	}

	/* check if field is a tagging field and needs special treatment */
	if ( parser->tag_field == NULL ) {
		/* no tags defined in parser */
		return ( t_str_to_dbl (str, decp, tsep, error, overflow) );
	}
	if ( strcasecmp ( parser->tag_field, field ) != 0 ) {
		/* not a tag field */
		return ( t_str_to_dbl (str, decp, tsep, error, overflow) );
	}

	/* duplicate input string */
	copy = strdup (str);

	/* clean input string from any geometry tag (replace with spaces and trim result) */
	{
		int num;
		int i;
		char *tag = NULL;
		char *token = NULL;
		/* POINTS */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_POINT );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POINT, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
		/* LINES */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_LINE );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_LINE, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
		/* POLYGONS */
		num = parser_get_num_tags ( parser, PARSER_GEOM_TAG_POLY );
		for ( i=0; i < num; i ++ ) {
			tag = (char*) parser_get_tag ( parser, PARSER_GEOM_TAG_POLY, i );
			token = strstr ( (const char*) copy, (const char*) tag );
			if ( token != NULL ) {
				int j = 0;
				while ( j < strlen (tag) ) {
					*token = ' ';
					token++;
					j++;
				}
			}
		}
	}
	/* pack string and release mem for copy */
	cleaned = t_str_pack(copy);
	t_free ( copy );

	/* basic error handling */
	if ( cleaned == NULL || strlen ( cleaned ) < 1 ) {
		if ( error != NULL ) *error = TRUE;
		t_free ( cleaned );
		return ( 0 );
	}

	/* call t_str_to_double to get conversion result */
	result = t_str_to_dbl ( (const char*) cleaned, decp, tsep, error, overflow );

	/* clean up and return result */
	t_free ( cleaned );

	return ( result );
}
