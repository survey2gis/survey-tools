/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	geom.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Functions for handling different types of survey geometries.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ****************************************************************************/

/* //TODO: We miss a function that is capable of resolving cases where polygon
 * A completely crosses Polygon B, effectively deviding B in two. In such cases,
 * Polygon A should be split into two parts automatically.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "errors.h"
#include "geom.h"
#include "i18n.h"
#include "options.h"
#include "parser.h"
#include "selections.h"
#include "tools.h"

#include "multclip/polyarea.h"


void geom_tools_part_destroy ( geom_part* part );
geom_part* geom_tools_part_add_vertex ( geom_part* part, int position, double x, double y, double z );
unsigned int geom_topology_intersections_2D_add ( geom_store *gs, options *opts );


/* We use a global geometry ID, because it needs to be
 tracked across all input files. */
static unsigned int GEOM_ID = 1;


/* Increases the local geom ID and guards against int overflow.
 * Returns the new geom ID or -1 on error.
 */
int inc_geom_id (parser_data_store *storage, int i) {
	GEOM_ID++;
	if ( GEOM_ID == UINT_MAX ) {
		err_show (ERR_NOTE,"");
		if (strcmp(storage->input, "-") != 0) {
			err_show (ERR_WARN,
					_("\nAborted after record read from line %i of input file '%s':\nInput too large (integer overflow)."),
					storage->records[i].line,
					storage->input);
		} else {
			err_show (ERR_WARN,
					_("\nAborted after record read from line %i of console input stream:\nInput too large (integer overflow)."),
					storage->records[i].line);
		}
		return ( -1 );
	}
	return (GEOM_ID);
}


/* Merges (multiplexes) data within one storage object into geometry sets,
 * using mode "end". In this mode, every measurement of a line or polygon
 * must be concluded with a geometry marker. The marker must be part of the
 * final vertex measurement.
 *
 * This function does nothing much except to assign the same
 * geom_id to all records that form one geometry. Multi-part
 * geometries must be build later, in a separate step.
 *
 * This function is never called directly, but via "geom_multiplex()".
 *
 * Return value is the number of multiplexed geometries or -1
 * on error.
 */
int geom_multiplex_mode_end (parser_data_store *storage, parser_desc *parser) {
	BOOLEAN DEBUG = FALSE;
	int i, j;
	unsigned int vertices;
	char *p;
	BOOLEAN abort;
	unsigned int num_multiplexed = 0;


	for (i = 0; i < storage->slot; i++) {
		/* multiplex valid geometries only */
		if (storage->records[i].is_valid == TRUE) {
			/* look for geometry tag */
			if (storage->records[i].tag != NULL) {
				if (DEBUG == TRUE) {
					fprintf(
							stderr,
							"GEOM M'PLEX: found tagged record (line %i)\n",
							i + 1);
					fprintf(stderr, "[%s]\n", storage->records[i].tag);
				}
				/* point? */
				if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POINT, (const char*)storage->records[i].tag )) {
				//if (parser->geom_tag_point != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_point)) { DELETE ME
					if (DEBUG == TRUE)
						fprintf(stderr, "GEOM M'PLEX: found POINT tag \n");
					/* make this a single-point geometry */
					storage->records[i].geom_id = GEOM_ID;
					storage->records[i].geom_type = GEOM_TYPE_POINT;
					storage->num_points++;
					num_multiplexed ++;
					if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
				}
				/* line? */
				if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
				//if (parser->geom_tag_line != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_line)) { DELETE ME
					/* find the other records that belong to this one */
					if (DEBUG == TRUE)
						fprintf(stderr, "GEOM M'PLEX: found LINE tag \n");
					vertices = 0;
					/* guard against corrupted key field */
					if ( i > 0 && storage->records[i].key != NULL && storage->records[i-1].key != NULL ) {
						if (i > 0 && strlen(storage->records[i - 1].key)
						<= strlen(storage->records[i].key)) {
							/* crop key field of tagged record if necessary */
							storage->records[i].key[strlen(storage->records[i
																			- 1].key)] = '\0';
							p = storage->records[i].key;
							if (DEBUG == TRUE)
								fprintf(stderr,
										"GEOM M'PLEX: comparing on '%s'\n", p);
							j = i;
							abort = FALSE;

							while (abort == FALSE && j >= 0
									&& storage->records[j].is_valid == TRUE
									&& !strcmp(p, storage->records[j].key)) {
								if (j < i && storage->records[j].tag != NULL) {
									/* abort if another tag was found */
									abort = TRUE;
								} else {
									vertices++;
									storage->records[j].geom_id = GEOM_ID;
									if ( j == 0 ) /* guard against unsigned int overflow! */
										abort = TRUE;
									j--;
								}
							}
						}
					}
					/* line must have at least two vertices */
					if (vertices >= 2) {
						for (j = 0; j < vertices; j++) {
							storage->records[i - j].geom_type
							= GEOM_TYPE_LINE;
						}
						if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
						storage->num_lines++;
						num_multiplexed ++;
					} else {
						err_show(ERR_NOTE, "");
						if (strcmp(storage->input, "-") != 0) {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of input file '%s':\nLine with less than two vertices found. Skipping."),
									storage->records[i].line,
									storage->input);
						} else {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of console input stream:\nLine with less than two vertices found. Skipping."),
									storage->records[i].line);
						}
						/* invalidate records */
						storage->records[i].is_valid = FALSE;
						for (j = 1; j < vertices; j++) {
							storage->records[i - j].is_valid = FALSE;
						}
					}
				}
				/* polygon? */
				if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)storage->records[i].tag )) {
				//if (parser->geom_tag_poly != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_poly)) { DELETE ME
					/* find the other records that belong to this one */
					if (DEBUG == TRUE)
						fprintf(stderr, "GEOM M'PLEX: found POLY tag \n");
					vertices = 0;
					/* guard against corrupted key field */
					if ( i>0 && storage->records[i].key != NULL && storage->records[i-1].key != NULL ) {
						if (i > 0 && strlen(storage->records[i - 1].key)
						<= strlen(storage->records[i].key)) {
							/* crop key field of tagged record if necessary */
							storage->records[i].key[strlen(storage->records[i
																			- 1].key)] = '\0';
							p = storage->records[i].key;
							if (DEBUG == TRUE)
								fprintf(stderr,
										"GEOM M'PLEX: comparing on '%s'\n", p);
							j = i;
							abort = FALSE;
							while (abort == FALSE && j >= 0
									&& storage->records[j].is_valid == TRUE
									&& !strcmp(p, storage->records[j].key)) {
								if (j < i && storage->records[j].tag != NULL) {
									/* abort if another tag was found */
									abort = TRUE;
								} else {
									vertices++;
									storage->records[j].geom_id = GEOM_ID;
									if ( j == 0 ) /* guard against unsigned int overflow! */
										abort = TRUE;
									j--;
								}
							}
						}
					}
					/* polygon must have at least three vertices */
					if (vertices >= 3) {
						for (j = 0; j < vertices; j++) {
							storage->records[i - j].geom_type
							= GEOM_TYPE_POLY;
						}
						if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
						storage->num_polygons++;
						num_multiplexed ++;
					} else {
						err_show(ERR_NOTE, "");
						if (strcmp(storage->input, "-") != 0) {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of input file '%s':\nPolygon with less than three vertices found. Skipping."),
									storage->records[i].line,
									storage->input);
						} else {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of console input stream:\nPolygon with less than three vertices found. Skipping."),
									storage->records[i].line);
						}
						/* invalidate records */
						storage->records[i].is_valid = FALSE;
						for (j = 1; j < vertices; j++) {
							storage->records[i - j].is_valid = FALSE;
						}
					}
				}
			}
		}
	}

	/* PASS 2: collect all untagged records that have not yet been assigned to a geometry */
	for (i = 0; i < storage->slot; i++) {
		/* multiplex valid geometries only */
		if (storage->records[i].is_valid == TRUE) {
			/* look for geometry tag */
			if (storage->records[i].tag == NULL) {
				if (storage->records[i].geom_type == GEOM_TYPE_NONE) {
					/* action depends on whether we are running in strict mode or not */
					if (parser->tag_strict == FALSE) {
						storage->records[i].geom_id = GEOM_ID;
						storage->records[i].geom_type = GEOM_TYPE_POINT;
						storage->num_points++;
						num_multiplexed ++;
						if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
						if (DEBUG == TRUE)
							fprintf(stderr,
									"GEOM M'PLEX: assuming POINT type.\n");
					} else {
						err_show(ERR_NOTE, "");
						if (strcmp(storage->input, "-") != 0) {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of input file '%s':\nNo geometry tag found. Skipping."),
									storage->records[i].line,
									storage->input);
						} else {
							err_show(
									ERR_WARN,
									_("\nRecord read from line %i of console input stream:\nNo geometry tag found. Skipping."),
									storage->records[i].line);
						}
					}
				}
			}
		}
	}

	return (num_multiplexed);
}


/* Helper function for geom_multiplex_mode_min (see below).
 * This function "closes" the current geometry by either invalidating
 * all of its associated records or leaving them untouched (except
 * for assigning an identical geometry type and geom_id to all vertices).
 *
 * Returns number of multiplexed geometries (0 or 1).
 *
 * Returns -1 on _fatal_ error (i.e. an error that forces aborting
 * the program, not one that merely means we need to invalidate the
 * current geometry!).
 */
int close_geometry ( 	BOOLEAN is_point, BOOLEAN is_line, BOOLEAN is_poly,
		int i, int num_vertices, int skip,
		parser_data_store *storage, parser_desc *parser)
{
	/* BOOLEAN DEBUG = FALSE; */
	int j;
	int num_multiplexed = 0;


	/*
	if ( DEBUG == TRUE ) {
		fprintf ( stderr ,"[%i] CLOSED (SKIPPED %i): ", storage->records[i].line, skip );
	}
	 */

	if ( !is_point && !is_line && !is_poly ) {
		if (parser->tag_strict == FALSE) {
			if (num_vertices == 1) {
				is_point = TRUE; /* assume point */
			} else {
				/* CORRUPT GEOMETRY */
				err_show(ERR_NOTE, "");
				if (strcmp(storage->input, "-") != 0) {
					err_show(
							ERR_WARN,
							_("\nRecord read from line %i of input file '%s':\nNo geometry tag found for geometry with %i vertices. Skipping."),
							storage->records[i].line,
							storage->input,
							num_vertices);
				} else {
					err_show(
							ERR_WARN,
							_("\nRecord read from line %i of console input stream:\nNo geometry tag found for geometry with %i vertices. Skipping."),
							storage->records[i].line,
							num_vertices);
				}
				/* invalidate the entire geometry */
				for (j = 0; j < num_vertices+skip; j++) {
					storage->records[i - j].is_valid = FALSE;
					storage->records[i - j].geom_id = -1;
				}
			}
		} else {
			/* CORRUPT GEOMETRY */
			err_show(ERR_NOTE, "");
			if (strcmp(storage->input, "-") != 0) {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of input file '%s':\nNo geometry tag found. Skipping."),
						storage->records[i].line,
						storage->input);
			} else {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of console input stream:\nNo geometry tag found. Skipping."),
						storage->records[i].line);
			}
			/* invalidate the entire geometry */
			for (j = 0; j < num_vertices+skip; j++) {
				storage->records[i - j].is_valid = FALSE;
				storage->records[i - j].geom_id = -1;
			}
		}
	}

	/* POINT ? */
	if ( is_point ) {
		/* make this a single-point geometry */
		for (j = 0; j < num_vertices+skip; j++) {
			/* Yes: a point should have only one vertex.
			 * HOWEVER: there may be corrupt lines between
			 * valid point lines that confuse the parser.
			 * We keep those in and let the geometry builder
			 * get rid of them afterwards. */
			if ( storage->records[i - j].is_valid == TRUE ) { /* guard against rubbish lines */
				storage->records[i - j].geom_type = GEOM_TYPE_POINT;
				storage->records[i - j].geom_id = GEOM_ID;
			} else {
				storage->records[i - j].geom_type = GEOM_TYPE_NONE;
				storage->records[i - j].geom_id = -1;
			}
		}
		storage->records[i].geom_type = GEOM_TYPE_POINT;
		storage->num_points++;
		num_multiplexed ++;
		if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
	}

	/* LINE ? */
	if ( is_line ) {
		if ( num_vertices < 2 ) {
			/* CORRUPT GEOMETRY */
			err_show(ERR_NOTE, "");
			if (strcmp(storage->input, "-") != 0) {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of input file '%s':\nLine with less than two vertices found. Skipping."),
						storage->records[i].line,
						storage->input);
			} else {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of console input stream:\nLine with less than two vertices found. Skipping."),
						storage->records[i].line);
			}
			/* invalidate the entire geometry */
			for (j = 0; j < num_vertices+skip; j++) {
				storage->records[i - j].is_valid = FALSE;
				storage->records[i - j].geom_id = -1;
			}
		} else {
			for (j = 0; j < num_vertices+skip; j++) {
				if ( storage->records[i - j].is_valid == TRUE ) { /* guard against rubbish lines */
					storage->records[i - j].geom_type = GEOM_TYPE_LINE;
					storage->records[i - j].geom_id = GEOM_ID;
				} else {
					storage->records[i - j].geom_type = GEOM_TYPE_NONE;
					storage->records[i - j].geom_id = -1;
				}
			}
			storage->num_lines++;
			num_multiplexed ++;
			if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
		}
	}

	/* POLY ? */
	if ( is_poly ) {
		if ( num_vertices < 3 ) {
			err_show(ERR_NOTE, "");
			if (strcmp(storage->input, "-") != 0) {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of input file '%s':\nPolygon with less than three vertices found. Skipping."),
						storage->records[i].line,
						storage->input);
			} else {
				err_show(
						ERR_WARN,
						_("\nRecord read from line %i of console input stream:\nPolygon with less than three vertices found. Skipping."),
						storage->records[i].line);
			}
			/* invalidate the entire geometry */
			for (j = 0; j < num_vertices+skip; j++) {
				storage->records[i - j].is_valid = FALSE;
				storage->records[i - j].geom_id = -1;
			}
		} else {
			for (j = 0; j < num_vertices+skip; j++) {
				if ( storage->records[i - j].is_valid == TRUE ) { /* guard against rubbish lines */
					storage->records[i - j].geom_type = GEOM_TYPE_POLY;
					storage->records[i - j].geom_id = GEOM_ID;
				} else {
					storage->records[i - j].geom_type = GEOM_TYPE_NONE;
					storage->records[i - j].geom_id = -1;
				}
			}
			storage->num_polygons++;
			num_multiplexed ++;
			if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
		}
	}

	/*
	if ( DEBUG ) {
		if ( is_point ) {
			fprintf ( stderr, " POINT\n" );
		}
		if ( is_line ) {
			fprintf ( stderr, " LINE\n" );
		}
		if ( is_poly ) {
			fprintf ( stderr, " POLY\n" );
		}
	}
	 */

	return (num_multiplexed);
}


/* Merges (multiplexes) data within one storage object into geometry sets,
 * using mode "min". In this mode, the first measurement of a line or
 * polygon must be tagged with the appropriate geometry marker.
 *
 * This function does nothing much except to assign the same
 * geom_id to all records that form one geometry. Multi-part
 * geometries must be build later, in a separate step.
 *
 * This function is never called directly, but via "geom_multiplex()".
 *
 * Return value is the number of multiplexed geometries or -1
 * on error.
 */
int geom_multiplex_mode_min (parser_data_store *storage, parser_desc *parser) {
	int i, j;
	int skip;
	int current_full;
	int num_vertices = 0;
	BOOLEAN is_first = FALSE;
	BOOLEAN is_poly = FALSE;
	BOOLEAN is_line = FALSE;
	BOOLEAN is_point = FALSE;
	BOOLEAN open = FALSE;
	unsigned int num_multiplexed = 0;
	/* BOOLEAN DEBUG = FALSE; */
	int retval;


	is_first = TRUE; /* it's our first scan for a _complete_ line */
	current_full = -1;
	skip = 0;

	for (i = 0; i < storage->slot; i++) {
		if (storage->records[i].is_valid == TRUE) {
			BOOLEAN is_complete = FALSE;
			BOOLEAN is_reduced = FALSE;
			/* Multiplex only valid geometries.
			 * Valid records in this mode are:
			 * (a) Complete lines, where all fields have valid values.
			 * (b) Reduced lines, where all persistent and coor fields have valid values.
			 * Validity has been checked at this point, but we still need to differentiate
			 * between the two cases.
			 */
			int num_values = 0;
			for ( j = 0; j < storage->num_fields; j ++ ) {
				if ( storage->records[i].contents[j] != NULL ) {
					num_values ++;
				}
			}
			if ( num_values == storage->num_fields ) {
				is_complete = TRUE;
			} else {
				is_reduced = TRUE; /* let's start on this assumption */
				for ( j=0; j < storage->num_fields; j ++) {
					/* Now we check that every field value that should be present
					 * in a reduced record is actually present. */
					if ( parser->fields[j]->value == NULL ) { /* we don't check pseudo fields */
						if ( parser->fields[j]->persistent && storage->records[i].contents[j] == NULL ) {
							is_reduced = FALSE;
						}
						if (  parser->coor_x != NULL && !strcasecmp(parser->fields[j]->name,parser->coor_x) ) {
							if ( storage->records[i].contents[j] == NULL ) {
								is_reduced = FALSE;
							}
						}
						if (  parser->coor_y != NULL && !strcasecmp(parser->fields[j]->name,parser->coor_y) ) {
							if ( storage->records[i].contents[j] == NULL ) {
								is_reduced = FALSE;
							}
						}
						if (  parser->coor_z != NULL && !strcasecmp(parser->fields[j]->name,parser->coor_z) ) {
							if ( storage->records[i].contents[j] == NULL ) {
								is_reduced = FALSE;
							}
						}
					}
				}
				/* at this point, "is_reduced" is only TRUE if there is a good match */
			}

			if ( is_reduced == FALSE && is_complete == FALSE ) {
				/* Invalid line!? Skip! */
				skip ++;
				storage->records[i].is_valid = FALSE;
				err_show (ERR_NOTE,"");
				if (strcmp(storage->input, "-") != 0) {
					err_show (ERR_WARN,
							_("\nAborted after record read from line %i of input file '%s':\nInvalid line. Skipping."),
							storage->records[i].line,
							storage->input);
				} else {
					err_show (ERR_WARN,
							_("\nAborted after record read from line %i of console input stream:\nInvalid line. Skipping."),
							storage->records[i].line);
				}
				/*
				if ( DEBUG ) {
					fprintf ( stderr ,"[%i] INVALID LINE (UNMARKED).\n", storage->records[i].line );
				}
				 */
				continue;
			}

			/* now parse! */
			if ( is_reduced ) {
				/* REDUCED RECORD */
				if ( is_first  == TRUE ) {
					/* error: cannot have a reduced line as first valid line! */
					err_show (ERR_NOTE,"");
					if (strcmp(storage->input, "-") != 0) {
						err_show( ERR_WARN,
								_("\nRecord read from line %i of input file '%s':\nSkipping incomplete record."),
								storage->records[i].line,
								storage->input);
					} else {
						err_show (ERR_WARN,
								_("\nRecord read from line %i of console input stream:\nSkipping incomplete record."),
								storage->records[i].line);
					}
					open = FALSE;
					num_vertices = 0;
				} else {
					if ( open ) {
						/* add one vertex to currently open geometry */
						storage->records[i].geom_id = GEOM_ID;
						num_vertices ++;
						/*
						if ( DEBUG ) {
							fprintf ( stderr ,"\t[%i] ADD VERTEX %i", storage->records[i].line, num_vertices );
						}
						 */
						if ( current_full > -1 && current_full != i ) {
							/* copy the complete attribute set from the last full record (lines & polys only) */
							/*
							if ( DEBUG ) {
								fprintf ( stderr, " & COPY ATTS FROM LINE %i.\n", storage->records[current_full].line);
							}
							 */
							for ( j = 0; j < storage->num_fields; j ++ ) {
								if ( storage->records[current_full].contents[j] != NULL ) {
									if ( storage->records[i].contents[j] == NULL ) {
										storage->records[i].contents[j] = strdup (storage->records[current_full].contents[j]);
									}
								}
							}
						} else {
							/*
							if ( DEBUG ) {
								fprintf ( stderr ,".\n" );
							}
							 */
						}
					}
				}
			} else if (is_complete) {
				/* FULL RECORD */
				current_full = i; /* handle to last full record */
				if ( open == TRUE ) {
					/* we close the current geometry... */
					retval = close_geometry (is_point, is_line, is_poly, i-1, num_vertices, skip, storage, parser);
					if ( retval < 0 ) {
						return ( retval );
					} else {
						num_multiplexed += retval;
					}
					/* .. and open the next one */
					skip = 0;
					/*
					if ( DEBUG ) {
						fprintf ( stderr ,"[%i] OPENED: ", storage->records[i].line );
					}
					 */
					num_vertices = 1;
					is_line = FALSE;
					is_poly = FALSE;
					is_point = FALSE;
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POINT, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_point != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_point)) { DELETE ME
						is_point = TRUE;
						is_line = FALSE;
						is_poly = FALSE;
					}
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_line != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_line)) { DELETE ME
						is_point = FALSE;
						is_line = TRUE;
						is_poly = FALSE;
					}
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_poly != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_poly)) { DELETE ME
						is_point = FALSE;
						is_line = FALSE;
						is_poly = TRUE;
					}
					/*
					if ( DEBUG ) {
						if ( is_point ) {
							fprintf ( stderr, " POINT\n" );
						}
						if ( is_line ) {
							fprintf ( stderr, " LINE\n" );
						}
						if ( is_poly ) {
							fprintf ( stderr, " POLY\n" );
						}
					}
					 */
				} else {
					/* we open a new geometry (the first one) */
					/*
					if ( DEBUG ) {
						fprintf ( stderr ,"[%i] OPENED: ", storage->records[i].line );
					}
					 */
					open = TRUE;
					num_vertices = 1;
					is_point = FALSE;
					is_line = FALSE;
					is_poly = FALSE;
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POINT, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_point != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_point)) { DELETE ME
						is_point = TRUE;
						is_line = FALSE;
						is_poly = FALSE;
					}
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_line != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_line)) { DELETE ME
						is_point = FALSE;
						is_line = TRUE;
						is_poly = FALSE;
					}
					if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)storage->records[i].tag )) {
					//if (parser->geom_tag_poly != NULL && !strcmp(storage->records[i].tag, parser->geom_tag_poly)) { DELETE ME
						is_point = FALSE;
						is_line = FALSE;
						is_poly = TRUE;
					}
					/*
					if ( DEBUG ) {
						if ( is_point ) {
							fprintf ( stderr, " POINT\n" );
						}
						if ( is_line ) {
							fprintf ( stderr, " LINE\n" );
						}
						if ( is_poly ) {
							fprintf ( stderr, " POLY\n" );
						}
					}
					 */
				}
			}
		} else {
			skip ++; /* number of invalid records that we might have to skip */
			/*
			if ( DEBUG ) {
				fprintf ( stderr ,"[%i] INVALID LINE (MARKED).\n", storage->records[i].line );
			}
			 */
		}
		is_first = FALSE;
	}
	/* perhaps we need to close the final geometry? */
	retval = close_geometry (is_point, is_line, is_poly, i-1, num_vertices, skip, storage, parser);
	if ( retval < 0 ) {
		return ( retval );
	} else {
		num_multiplexed += retval;
	}

	return ( num_multiplexed );
}


/* Merges (multiplexes) data within one storage object into geometry sets,
 * using mode "max". In this mode, each measurement must be tagged with a
 * geometry marker.
 *
 * This function does nothing much except to assign the same
 * geom_id to all records that form one geometry. Multi-part
 * geometries must be built later, in a separate step.
 *
 * This function is never called directly, but via "geom_multiplex()".
 *
 * Return value is the number of multiplexed geometries or -1
 * on error.
 */
int geom_multiplex_mode_max (parser_data_store *storage, parser_desc *parser) {

	/* BOOLEAN DEBUG = FALSE;
	   unsigned int num_vertices = 0; */
	unsigned int i;
	char *last_key = NULL;
	char *last_tag = NULL;
	unsigned int num_multiplexed = 0;
	BOOLEAN open = FALSE;

	/* loop over all (potentially) allocated data slots in the current geometry store */
	for (i = 0; i < storage->slot; i++) {
		/* multiplex valid geometries only */
		if (storage->records[i].is_valid == TRUE) {

			/* DEBUG */
			/*
			if ( DEBUG == TRUE ) {
				fprintf ( stderr, "DEBUG: [%d] {%f, %f, %f} tag='%s' key='%s' valid=%i empty=%i\n",
						i, storage->records[i].x, storage->records[i].y, storage->records[i].z,
						storage->records[i].tag, storage->records[i].key,
						storage->records[i].is_valid, storage->records[i].is_empty);
			}
			 */

			/* look for geometry tag */
			if (storage->records[i].tag == NULL) {
				err_show (ERR_NOTE,"");
				if (strcmp(storage->input, "-") != 0) {
					err_show( ERR_WARN,
							_("\nRecord read from line %i of input file '%s':\nSkipping untagged record."),
							storage->records[i].line,
							storage->input);
				} else {
					err_show (ERR_WARN,
							_("\nRecord read from line %i of console input stream:\nSkipping untagged record."),
							storage->records[i].line);
				}
			} else {
				/* point? */
				if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POINT, (const char*)storage->records[i].tag )) {
				//if (parser->geom_tag_point != NULL && !strcmp( storage->records[i].tag, parser->geom_tag_point)) { DELETE ME
					/* make this a single-point geometry */
					if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
					/*
					if (DEBUG == TRUE) {
						fprintf ( stderr ,"[%i] IS POINT.\n", storage->records[i].line );
						fprintf ( stderr, "GEOM ID = %d\n", GEOM_ID);
					}
					 */
					storage->records[i].geom_id = GEOM_ID;
					storage->records[i].geom_type = GEOM_TYPE_POINT;
					storage->num_points++;
					num_multiplexed ++;
					/*
					if ( DEBUG ) {
						num_vertices = 0;
					}
					*/
					continue;
				}
				/* line or polygon? */
				if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag ) ||
						parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)storage->records[i].tag ) ) {
				/*
				if (	parser->geom_tag_line != NULL
						&&
						( !strcmp(storage->records[i].tag, parser->geom_tag_line)
								||
								!strcmp(storage->records[i].tag, parser->geom_tag_poly) ) )
				{*/
					if ( storage->records[i].key == NULL ) {
						err_show (ERR_NOTE,"");
						if (strcmp(storage->input, "-") != 0) {
							err_show( ERR_WARN,
									_("\nRecord read from line %i of input file '%s':\nSkipping record with missing key field value."),
									storage->records[i].line,
									storage->input);
						} else {
							err_show (ERR_WARN,
									_("\nRecord read from line %i of console input stream:\nSkipping record with missing key field value."),
									storage->records[i].line);
						}
					} else {
						if ( last_key == NULL ) {
							/* this is the first vertex of the first complex geometry */
							last_key = storage->records[i].key;
							last_tag = storage->records[i].tag;
							storage->records[i].geom_id = GEOM_ID;
							/*
							if ( DEBUG ) {
								num_vertices ++;
							}
							*/
							/*
							if ( DEBUG ) {
								fprintf ( stderr ,"\t[%i] ADDED VERTEX #1 OF FIRST LINE OR POLY.\n", storage->records[i].line );
								fprintf ( stderr, "\tGEOM ID = %d\n", GEOM_ID);
							}
							 */
							if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
							//if ( !strcmp(storage->records[i].tag, parser->geom_tag_line) ) { DELETE ME
								storage->records[i].geom_type = GEOM_TYPE_LINE;
							} else {
								storage->records[i].geom_type = GEOM_TYPE_POLY;
							}
							open = TRUE;
						} else {
							/* if ( !strcmp ( last_key, storage->records[i].key ) &&				DELETE ME
									( !strcmp(storage->records[i].tag, parser->geom_tag_line) ||	DELETE ME
									  !strcmp(storage->records[i].tag, parser->geom_tag_poly) )		DELETE ME
								) { DELETE ME */
							if ( !strcmp ( last_key, storage->records[i].key ) &&
									(parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag ) ||
									parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)storage->records[i].tag ))
								) {
								last_tag = storage->records[i].tag;
								storage->records[i].geom_id = GEOM_ID;
								/*
								if ( DEBUG ) {
									num_vertices ++;
								}
								*/
								/* we are still parsing the same geometry */
								/*
								if ( DEBUG ) {
									fprintf ( stderr ,"\t[%i] ADDED VERTEX #%i.\n", storage->records[i].line, num_vertices );
									fprintf ( stderr, "\tGEOM ID = %d\n", GEOM_ID);
								}
								 */
								if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
								//if ( !strcmp(storage->records[i].tag, parser->geom_tag_line) ) { DELETE ME
									storage->records[i].geom_type = GEOM_TYPE_LINE;
								} else {
									storage->records[i].geom_type = GEOM_TYPE_POLY;
								}
								open = TRUE;
							} else {
								/* we are switching to the next geometry */
								last_tag = storage->records[i].tag;
								last_key = storage->records[i].key;
								/* build geometry! */
								if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)storage->records[i].tag )) {
								//if ( !strcmp(storage->records[i].tag, parser->geom_tag_line) ) { DELETE ME
									/* build line geometry */
									/*
									if ( DEBUG ) {
										fprintf ( stderr ,"[%i] BUILT LINE WITH %i VERTICES.\n", storage->records[i].line, num_vertices );
									}
									 */
									/* done */
									storage->num_lines++;
									num_multiplexed ++;
									/*
									if ( DEBUG ) {
										num_vertices = 0;
									}
									*/
									i--;
									if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
									open = FALSE;
									continue;
								} else {
									/* build polygon geometry */
									/*
									if ( DEBUG ) {
										fprintf ( stderr ,"[%i] BUILT POLY WITH %i VERTICES.\n", storage->records[i].line, num_vertices );
									}
									 */
									/* done */
									storage->num_polygons++;
									num_multiplexed ++;
									i--;
									if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
									open = FALSE;
									continue;
								}
							}
						}
					}
				}
			}
		}
	}

	/* if there is still an open line or poly geometry: close it now */
	if ( open == TRUE && last_tag != NULL ) {
		if ( parser_is_tag ( parser, PARSER_GEOM_TAG_LINE, (const char*)last_tag )) {
		//if ( ( !strcmp(last_tag, parser->geom_tag_line) ) ) { DELETE ME
			storage->num_lines++;
			num_multiplexed ++;
		}
		if ( parser_is_tag ( parser, PARSER_GEOM_TAG_POLY, (const char*)last_tag )) {
		//if ( ( !strcmp(last_tag, parser->geom_tag_poly) ) ) { DELETE ME
			storage->num_polygons++;
			num_multiplexed ++;
		}
	}

	return ( num_multiplexed );
}


/* Processes1 data within one storage object into _point_ geometries,
 * using mode "none". In this mode, there will be no marking of geometries.
 *
 * This function does nothing much except to assign the same
 * geom_id to each record that represents a point. There is no
 * support for multi-part geometries.
 *
 * This function is never called directly, but via "geom_multiplex()".
 *
 * Return value is the number of multiplexed geometries or -1
 * on error.
 */
int geom_multiplex_mode_none (parser_data_store *storage, parser_desc *parser) {
	int i;
	unsigned int num_multiplexed = 0;
	/* BOOLEAN DEBUG = FALSE; */


	for (i = 0; i < storage->slot; i++) {
		/* multiplex valid geometries only */
		if (storage->records[i].is_valid == TRUE) {
			/*
			if ( DEBUG ) {
				fprintf (stderr,"[%i] POINT.\n", storage->records[i].line);
			}
			 */
			/* make this a single-point geometry */
			storage->records[i].geom_id = GEOM_ID;
			storage->records[i].geom_type = GEOM_TYPE_POINT;
			storage->num_points++;
			num_multiplexed ++;
			if ( inc_geom_id (storage,i) < 0 ) return ( -1 );
		} else {
			/*
			if ( DEBUG ) {
				fprintf (stderr,"[%i] INVALID.\n", storage->records[i].line);
			}
			 */
		}
	}

	return ( num_multiplexed );
}


/*
 * Merges the data within one data storage into geometry sets
 * (point, lines, polygons).
 *
 * The precise mode of operation depends on the tagging mode
 * specified in the parser description. The actual work is done
 * by another "geom_multiplex_MODE()" helper function.
 *
 * Return value is the number of multiplexed geometries or -1
 * on error.
 */
int geom_multiplex(parser_data_store *storage, parser_desc *parser) {
	/* BOOLEAN DEBUG = FALSE;
	unsigned int i; */

	if (storage == NULL || parser == NULL)
		return (-1);

	if (parser->tag_mode == PARSER_TAG_MODE_END) {
		return (geom_multiplex_mode_end(storage, parser));
	}

	if (parser->tag_mode == PARSER_TAG_MODE_MIN) {
		return (geom_multiplex_mode_min(storage, parser));
	}

	if (parser->tag_mode == PARSER_TAG_MODE_MAX) {
		return (geom_multiplex_mode_max(storage, parser));
	}

	if (parser->tag_mode == PARSER_TAG_MODE_NONE) {
		return (geom_multiplex_mode_none(storage, parser));
	}

	/*
	if (DEBUG == TRUE) {
		for (i = 0; i < storage->num_records; i++) {
			if (storage->records[i].is_empty == FALSE) {
				if (storage->records[i].is_valid == FALSE) {
					fprintf(stderr, "[%i, %s] = INVALID\n", i + 1,
							storage->records[i].contents[0]);
				} else {
					if (storage->records[i].geom_type == GEOM_TYPE_NONE) {
						fprintf(stderr, "[%i, %s] = UNASSIGNED\n", i + 1,
								storage->records[i].contents[0]);
					} else {
						fprintf(stderr, "[%i, %s] = %s [%i]\n", i + 1,
								storage->records[i].contents[0],
								GEOM_TYPE_NAMES[storage->records[i].geom_type],
								storage->records[i].geom_id);
					}
				}
			}
		}
	}
	 */

	return ( -1 );
}


/*
 * Helper function: Allocate memory for intersections
 * lists (lines and polygons) and fill them with default
 * values.
 */
void geom_store_init_intersections ( geom_store *gs )
{
	int i = 0;

	/* LINE INTERSECTIONS */
	gs->lines_intersections = malloc ( sizeof ( geom_store_intersection ) );
	gs->lines_intersections->num_intersections = 0;
	gs->lines_intersections->capacity = GEOM_STORE_CHUNK_BIG;
	gs->lines_intersections->geom_id = malloc ( sizeof ( unsigned int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->part_id = malloc ( sizeof ( unsigned int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->X = malloc ( sizeof (  double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->Y = malloc ( sizeof (  double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->Z = malloc ( sizeof (  double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->v = malloc ( sizeof (  int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines_intersections->added = malloc ( sizeof ( BOOLEAN ) * (GEOM_STORE_CHUNK_BIG) );
	for ( i = 0; i < GEOM_STORE_CHUNK_BIG; i++ ) {
		gs->lines_intersections->geom_id[i] = -1;
		gs->lines_intersections->part_id[i] = -1;
		gs->lines_intersections->X[i] = 0.0;
		gs->lines_intersections->Y[i] = 0.0;
		gs->lines_intersections->Z[i] = 0.0;
		gs->lines_intersections->v[i] = -1;
		gs->lines_intersections->added[i] = FALSE;
	}

	/* POLYGON INTERSECTIONS */
	gs->polygons_intersections = malloc ( sizeof ( geom_store_intersection ) );
	gs->polygons_intersections->num_intersections = 0;
	gs->polygons_intersections->capacity = GEOM_STORE_CHUNK_BIG;
	gs->polygons_intersections->geom_id = malloc ( sizeof ( unsigned int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->part_id = malloc ( sizeof ( unsigned int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->X = malloc ( sizeof ( double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->Y = malloc ( sizeof ( double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->Z = malloc ( sizeof ( double ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->v = malloc ( sizeof ( int ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons_intersections->added = malloc ( sizeof ( BOOLEAN ) * (GEOM_STORE_CHUNK_BIG) );
	for ( i = 0; i < GEOM_STORE_CHUNK_BIG; i++ ) {
		gs->polygons_intersections->geom_id[i] = -1;
		gs->polygons_intersections->part_id[i] = -1;
		gs->polygons_intersections->X[i] = 0.0;
		gs->polygons_intersections->Y[i] = 0.0;
		gs->polygons_intersections->Z[i] = 0.0;
		gs->polygons_intersections->v[i] = -1;
		gs->polygons_intersections->added[i] = FALSE;
	}
}


/*
 * Creates a new, empty geometry store.
 *
 * A geometry store contains a strongly structured, hierarchical
 * representation of the geometries, converted from the less
 * structured parser data store(s) by "geom_store_build".
 * Line and polygon geometries in a "geom_store" can be complex
 * multi-part objects or simple ones that consist of just one part.
 * See "geom.h" for a commented schema.
 *
 * In this form, the geometry data is much easier to parse for
 * high-level topological functions and export functions.
 *
 * However, we do lose the 1:1 association between a record and
 * the input file that it came from, which basically means that
 * error and warning messages will be less precise.
 *
 * Use geom_store_destroy() to release all memory associated with
 * as "geom_store" object.
 *
 */
geom_store *geom_store_new ()
{
	geom_store *gs;
	unsigned int i;
	unsigned int j;

	gs = malloc ( sizeof ( geom_store ) );

	gs->num_points = 0;
	gs->num_points_raw = 0;
	gs->num_lines = 0;
	gs->num_polygons = 0;
	gs->points = malloc ( sizeof ( geom_store_point ) * (GEOM_STORE_CHUNK_BIG) );
	gs->points_raw = malloc ( sizeof ( geom_store_point ) * (GEOM_STORE_CHUNK_BIG) );
	gs->lines = malloc ( sizeof ( geom_store_line ) * (GEOM_STORE_CHUNK_BIG) );
	gs->polygons = malloc ( sizeof ( geom_store_polygon ) * (GEOM_STORE_CHUNK_BIG) );
	gs->free_points = GEOM_STORE_CHUNK_BIG;
	gs->free_points_raw = GEOM_STORE_CHUNK_BIG;
	gs->free_lines = GEOM_STORE_CHUNK_BIG;
	gs->free_polygons = GEOM_STORE_CHUNK_BIG;
	for ( i = 0; i < GEOM_STORE_CHUNK_BIG; i++ ) {
		gs->points[i].geom_id = -1;
		gs->points[i].is_empty = TRUE;
		gs->points[i].is_selected = FALSE;
		gs->points[i].source = NULL;
		for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
			gs->points[i].atts[j] = NULL;
		}
		gs->points_raw[i].geom_id = -1;
		gs->points_raw[i].is_empty = TRUE;
		gs->points_raw[i].is_selected = FALSE;
		gs->points_raw[i].source = NULL;
		for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
			gs->points_raw[i].atts[j] = NULL;
		}
		gs->lines[i].geom_id = -1;
		gs->lines[i].is_empty = TRUE;
		gs->lines[i].num_parts = 0;
		gs->lines[i].parts = NULL;
		gs->lines[i].bbox_x1 = 0.0;
		gs->lines[i].bbox_y1 = 0.0;
		gs->lines[i].bbox_z1 = 0.0;
		gs->lines[i].bbox_x2 = 0.0;
		gs->lines[i].bbox_y2 = 0.0;
		gs->lines[i].bbox_z2 = 0.0;
		gs->lines[i].is_selected = FALSE;
		gs->lines[i].source = NULL;
		for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
			gs->lines[i].atts[j] = NULL;
		}
		gs->polygons[i].geom_id = -1;
		gs->polygons[i].is_empty = TRUE;
		gs->polygons[i].num_parts = 0;
		gs->polygons[i].parts = NULL;
		gs->polygons[i].bbox_x1 = 0.0;
		gs->polygons[i].bbox_y1 = 0.0;
		gs->polygons[i].bbox_z1 = 0.0;
		gs->polygons[i].bbox_x2 = 0.0;
		gs->polygons[i].bbox_y2 = 0.0;
		gs->polygons[i].bbox_z2 = 0.0;
		gs->polygons[i].is_selected = FALSE;
		gs->polygons[i].source = NULL;
		for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
			gs->polygons[i].atts[j] = NULL;
		}
	}

	geom_store_init_intersections ( gs );

	gs->path_points = NULL;
	gs->path_points_atts = NULL;
	gs->path_points_raw = NULL;
	gs->path_points_raw_atts = NULL;
	gs->path_lines = NULL;
	gs->path_lines_atts = NULL;
	gs->path_polys = NULL;
	gs->path_polys_atts = NULL;
	gs->path_all = NULL;
	gs->path_all_atts = NULL;
	gs->path_labels = NULL;
	gs->path_labels_atts = NULL;
	gs->path_labels_gva = NULL;

	gs->min_x_set = FALSE;
	gs->min_y_set = FALSE;
	gs->min_z_set = FALSE;
	gs->max_x_set = FALSE;
	gs->max_y_set = FALSE;
	gs->max_z_set = FALSE;
	gs->min_x = 0.0;
	gs->min_y = 0.0;
	gs->min_z = 0.0;
	gs->max_x = 0.0;
	gs->max_y = 0.0;
	gs->max_z = 0.0;

	gs->is_empty = TRUE;

	return ( gs );
}


/*
 * Release memory used for storing output paths.
 */
void geom_store_free_paths ( geom_store *gs )
{
	if ( gs->path_points != NULL ) {
		free ( gs->path_points );
		gs->path_points = NULL;
	}

	if ( gs->path_points_atts != NULL ) {
		free ( gs->path_points_atts );
		gs->path_points_atts = NULL;
	}

	if ( gs->path_points_raw != NULL ) {
		free ( gs->path_points_raw );
		gs->path_points_raw = NULL;
	}

	if ( gs->path_points_raw_atts != NULL ) {
		free ( gs->path_points_raw_atts );
		gs->path_points_raw_atts = NULL;
	}

	if ( gs->path_lines != NULL ) {
		free ( gs->path_lines );
		gs->path_lines = NULL;
	}

	if ( gs->path_lines_atts != NULL ) {
		free ( gs->path_lines_atts );
		gs->path_lines_atts = NULL;
	}

	if ( gs->path_polys != NULL ) {
		free ( gs->path_polys );
		gs->path_polys = NULL;
	}

	if ( gs->path_polys_atts != NULL ) {
		free ( gs->path_polys_atts );
		gs->path_polys_atts = NULL;
	}

	if ( gs->path_all != NULL ) {
		free ( gs->path_all );
		gs->path_all = NULL;
	}

	if ( gs->path_all_atts != NULL ) {
		free ( gs->path_all_atts );
		gs->path_all_atts = NULL;
	}

	if ( gs->path_labels != NULL ) {
		free ( gs->path_labels );
		gs->path_labels = NULL;
	}

	if ( gs->path_labels_atts != NULL ) {
		free ( gs->path_labels_atts );
		gs->path_labels_atts = NULL;
	}

	if ( gs->path_labels_gva != NULL ) {
		free ( gs->path_labels_gva );
		gs->path_labels_gva = NULL;
	}
}


/*
 * Create output paths for Shapefile format output.
 *
 * Returns "0" if OK, "-1" on error.
 * In addition, if error is non-NULL, then a precise
 * description of the error will be stored in the _pre-allocated_
 * memory pointed to by "error".
 */
int geom_store_make_paths_shp ( geom_store *gs, options *opts, char *error )
{
	char fs[3];
	int len;
	char *check_path;

	/* create output paths */
	fs[0] = PRG_FILE_SEPARATOR;
	fs[1] = '\0';
	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_POINT])  + strlen (".shp") + 1;
	gs->path_points = malloc ( sizeof ( char ) * len);
	gs->path_points_atts = malloc ( sizeof ( char ) * len );
	gs->path_points[0] = '\0';
	gs->path_points_atts[0] = '\0';
	strcat ( gs->path_points, opts->output );
	strcat ( gs->path_points, fs );
	strcat ( gs->path_points, opts->base );
	strcat ( gs->path_points, "_" );
	strcat ( gs->path_points, GEOM_TYPE_NAMES[GEOM_TYPE_POINT] );
	strcat ( gs->path_points_atts, gs->path_points );
	strcat ( gs->path_points, ".shp" );
	strcat ( gs->path_points_atts, ".dbf" );

	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_POINT_RAW])  + strlen (".shp") + 1;
	gs->path_points_raw = malloc ( sizeof ( char ) * len );
	gs->path_points_raw_atts = malloc ( sizeof ( char ) * len );
	gs->path_points_raw[0] = '\0';
	gs->path_points_raw_atts[0] = '\0';
	strcat ( gs->path_points_raw, opts->output );
	strcat ( gs->path_points_raw, fs );
	strcat ( gs->path_points_raw, opts->base );
	strcat ( gs->path_points_raw, "_" );
	strcat ( gs->path_points_raw, GEOM_TYPE_NAMES[GEOM_TYPE_POINT_RAW] );
	strcat ( gs->path_points_raw_atts, gs->path_points_raw );
	strcat ( gs->path_points_raw, ".shp" );
	strcat ( gs->path_points_raw_atts, ".dbf" );

	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_LINE])  + strlen (".shp") + 1;
	gs->path_lines = malloc ( sizeof ( char ) * len );
	gs->path_lines_atts = malloc ( sizeof ( char ) * len );
	gs->path_lines[0] = '\0';
	gs->path_lines_atts[0] = '\0';
	strcat ( gs->path_lines, opts->output );
	strcat ( gs->path_lines, fs );
	strcat ( gs->path_lines, opts->base );
	strcat ( gs->path_lines, "_" );
	strcat ( gs->path_lines, GEOM_TYPE_NAMES[GEOM_TYPE_LINE] );
	strcat ( gs->path_lines_atts, gs->path_lines );
	strcat ( gs->path_lines, ".shp" );
	strcat ( gs->path_lines_atts, ".dbf" );

	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_POLY])  + strlen (".shp") + 1;
	gs->path_polys = malloc ( sizeof ( char ) * len );
	gs->path_polys_atts = malloc ( sizeof ( char ) * len );
	gs->path_polys[0] = '\0';
	gs->path_polys_atts[0] = '\0';
	strcat ( gs->path_polys, opts->output );
	strcat ( gs->path_polys, fs );
	strcat ( gs->path_polys, opts->base );
	strcat ( gs->path_polys, "_" );
	strcat ( gs->path_polys, GEOM_TYPE_NAMES[GEOM_TYPE_POLY] );
	strcat ( gs->path_polys_atts, gs->path_polys );
	strcat ( gs->path_polys, ".shp" );
	strcat ( gs->path_polys_atts, ".dbf" );

	/* need path for labels, as well? */
	if ( opts->label_field != NULL ) {
		len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
														+ strlen (GEOM_LABELS_SUFFIX)  + strlen (".shp") + 1;
		gs->path_labels = malloc ( sizeof ( char ) * len );
		gs->path_labels_atts = malloc ( sizeof ( char ) * len );
		gs->path_labels_gva = malloc ( sizeof ( char ) * len );
		gs->path_labels[0] = '\0';
		gs->path_labels_atts[0] = '\0';
		gs->path_labels_gva[0] = '\0';
		strcat ( gs->path_labels, opts->output );
		strcat ( gs->path_labels, fs );
		strcat ( gs->path_labels, opts->base );
		strcat ( gs->path_labels, "_" );
		strcat ( gs->path_labels, GEOM_LABELS_SUFFIX );
		strcat ( gs->path_labels_atts, gs->path_labels );
		strcat ( gs->path_labels_gva, gs->path_labels );
		strcat ( gs->path_labels, ".shp" );
		strcat ( gs->path_labels_atts, ".dbf" );
		strcat ( gs->path_labels_gva, ".gva" );
	}

	/* check if output path is writeable */
	check_path = NULL;
	if ( selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ) {
		check_path = gs->path_points_atts;
	}
	if ( selections_get_num_selected ( GEOM_TYPE_POINT_RAW, gs ) > 0 ) {
		check_path = gs->path_points_raw_atts;
	}
	if ( selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ) {
		check_path = gs->path_lines_atts;
	}
	if ( selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0 ) {
		check_path = gs->path_polys_atts;
	}
	if ( opts->label_field != NULL ) {
		check_path = gs->path_labels_atts;
	}

	/* This is assuming that we have at least one output file! */
	if ( check_path != NULL ) {
		errno = 0;
		FILE *check = t_fopen_utf8 ( check_path, "w" );
		if ( check == NULL ) {
			err_show (ERR_NOTE,"");
			if ( errno != 0 ) {
				if ( error != NULL ) {
					snprintf ( error, PRG_MAX_STR_LEN, "%s (%s).", strerror (errno), opts->output );
				}
				geom_store_free_paths (gs);
				return (-1);
			}
			else {
				if ( error != NULL ) {
					snprintf ( error, PRG_MAX_STR_LEN, _("Cannot write output into '%s'.\nCheck that the directory exists and is writable."),
							opts->output );
				}
				geom_store_free_paths (gs);
				return (-1);
			}
		}
		fclose ( check );
		return ( 0 );
	}

	return ( -1 );
}


/*
 * Create output paths for DXF format output.
 *
 * Returns "0" if OK, "-1" on error.
 * In addition, if error is non-NULL, then a precise
 * description of the error will be stored in the _pre-allocated_
 * memory pointed to by "error".
 */
int geom_store_make_paths_dxf ( geom_store *gs, options *opts, char *error )
{
	char fs[3];
	int len;
	char *check_path;
	FILE *check;


	/* create output paths */
	fs[0] = PRG_FILE_SEPARATOR;
	fs[1] = '\0';
	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_ALL])  + strlen (".dxf") + 1;
	gs->path_all = malloc ( sizeof ( char ) * len);
	gs->path_all_atts = malloc ( sizeof ( char ) * len );
	gs->path_all[0] = '\0';
	gs->path_all_atts[0] = '\0';
	strcat ( gs->path_all, opts->output );
	strcat ( gs->path_all, fs );
	strcat ( gs->path_all, opts->base );
	strcat ( gs->path_all, "_" );
	strcat ( gs->path_all, GEOM_TYPE_NAMES[GEOM_TYPE_ALL] );
	strcat ( gs->path_all_atts, gs->path_all );
	strcat ( gs->path_all, ".dxf" );
	strcat ( gs->path_all_atts, ".txt" );

	/* check if output path is writeable */
	check_path = gs->path_all_atts;

	errno = 0;
	check = t_fopen_utf8 ( check_path, "w" );
	if ( check == NULL ) {
		err_show (ERR_NOTE,"");
		if ( errno != 0 ) {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, "%s (%s).", strerror (errno), opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
		else {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, _("Cannot write output into '%s'.\nCheck that the directory exists and is writable."),
						opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
	}
	fclose ( check );

	return ( 0 );
}


/*
 * Create output paths for GeoJSON format output.
 *
 * Returns "0" if OK, "-1" on error.
 * In addition, if error is non-NULL, then a precise
 * description of the error will be stored in the _pre-allocated_
 * memory pointed to by "error".
 */
int geom_store_make_paths_geojson ( geom_store *gs, options *opts, char *error )
{
	char fs[3];
	int len;
	char *check_path;
	FILE *check;


	/* create output paths */
	fs[0] = PRG_FILE_SEPARATOR;
	fs[1] = '\0';
	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_ALL])  + strlen (".geojson") + 1;
	gs->path_all = malloc ( sizeof ( char ) * len);
	gs->path_all[0] = '\0';
	strcat ( gs->path_all, opts->output );
	strcat ( gs->path_all, fs );
	strcat ( gs->path_all, opts->base );
	strcat ( gs->path_all, "_" );
	strcat ( gs->path_all, GEOM_TYPE_NAMES[GEOM_TYPE_ALL] );
	strcat ( gs->path_all, ".geojson" );

	/* check if output path is writeable */
	check_path = gs->path_all;

	errno = 0;
	check = t_fopen_utf8 ( check_path, "w" );
	if ( check == NULL ) {
		err_show (ERR_NOTE,"");
		if ( errno != 0 ) {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, "%s (%s).", strerror (errno), opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
		else {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, _("Cannot write output into '%s'.\nCheck that the directory exists and is writable."),
						opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
	}
	fclose ( check );

	return ( 0 );
}


/*
 * Create output paths for KML format output.
 *
 * Returns "0" if OK, "-1" on error.
 * In addition, if error is non-NULL, then a precise
 * description of the error will be stored in the _pre-allocated_
 * memory pointed to by "error".
 */
int geom_store_make_paths_kml ( geom_store *gs, options *opts, char *error )
{
	char fs[3];
	int len;
	char *check_path;
	FILE *check;


	/* create output paths */
	fs[0] = PRG_FILE_SEPARATOR;
	fs[1] = '\0';
	len = strlen ( opts->output ) + strlen ( fs ) + strlen ( opts->base ) + strlen ( "_" )
										+ strlen (GEOM_TYPE_NAMES[GEOM_TYPE_ALL])  + strlen (".kml") + 1;
	gs->path_all = malloc ( sizeof ( char ) * len);
	gs->path_all[0] = '\0';
	strcat ( gs->path_all, opts->output );
	strcat ( gs->path_all, fs );
	strcat ( gs->path_all, opts->base );
	strcat ( gs->path_all, "_" );
	strcat ( gs->path_all, GEOM_TYPE_NAMES[GEOM_TYPE_ALL] );
	strcat ( gs->path_all, ".kml" );

	/* check if output path is writeable */
	check_path = gs->path_all;

	errno = 0;
	check = t_fopen_utf8 ( check_path, "w" );
	if ( check == NULL ) {
		err_show (ERR_NOTE,"");
		if ( errno != 0 ) {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, "%s (%s).", strerror (errno), opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
		else {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, _("Cannot write output into '%s'.\nCheck that the directory exists and is writable."),
						opts->output );
			}
			geom_store_free_paths (gs);
			return (-1);
		}
	}
	fclose ( check );

	return ( 0 );
}


/*
 * Create output file names for geometries and attributes,
 * based on output file format.
 *
 * Returns -1 on error, 0 otherwise.
 * In addition, if "error" is non-NULL, then it is taken to point to
 * a _pre-allocated_ char[] of length PRG_MAX_STR_LEN and a more
 * descriptive error message will be stored there.
 */
int geom_store_make_paths ( geom_store *gs, options *opts, char *error )
{
	int result = -1;

	if ( !strcasecmp (PRG_OUTPUT_DESC[opts->format], PRG_OUTPUT_DESC[PRG_OUTPUT_SHP]) ) {
		result = geom_store_make_paths_shp ( gs, opts, error );
	}

	if ( !strcasecmp (PRG_OUTPUT_DESC[opts->format], PRG_OUTPUT_DESC[PRG_OUTPUT_DXF]) ) {
		result = geom_store_make_paths_dxf ( gs, opts, error );
	}

	if ( !strcasecmp (PRG_OUTPUT_DESC[opts->format], PRG_OUTPUT_DESC[PRG_OUTPUT_GEOJSON]) ) {
		result = geom_store_make_paths_geojson ( gs, opts, error );
	}

	if ( !strcasecmp (PRG_OUTPUT_DESC[opts->format], PRG_OUTPUT_DESC[PRG_OUTPUT_KML]) ) {
		result = geom_store_make_paths_kml ( gs, opts, error );
	}

	return ( result );
}


/*
 * Tests if a point lies within a polygon part.
 * This is a 2D test. Z data is ignored.
 *
 * Input is a pointer to a polygon geometry and a part ID.
 * The part ID goes from 0 to polygon->num_parts.
 * Use "0" for single-part, simple polygons.
 *
 * Returns TRUE if point lies within the part, FALSE otherwise.
 */
BOOLEAN geom_tools_point_in_part_2D ( double X, double Y, geom_store_polygon *polygon, unsigned int part )
{
	geom_part *P;
	unsigned int i, j;
	BOOLEAN odd = FALSE;


	P = &polygon->parts[part];

	j = P->num_vertices - 1;

	for ( i = 0; i < P->num_vertices; i++ ) {
		if ( ( P->Y[i] < Y && P->Y[j] >= Y ) || ( P->Y[j] < Y && P->Y[i] >= Y ) ) {
			if ( P->X[i] + ( Y - P->Y[i] ) / ( P->Y[j] - P->Y[i] ) * ( P->X[j] - P->X[i]) < X) {
				odd =! odd;
			}
		}
		j = i;
	}

	return ( odd );
}


/*
 * Tests if the bounding boxes, represented by two sets of 2D corner coordinates, overlap.
 * Bounding boxes include the entire polygons, i.e. all of their parts.
 */
BOOLEAN geom_tools_bb_xy_overlap_2D ( double A_x1, double A_x2, double A_y1, double A_y2,
		double B_x1, double B_x2, double B_y1, double B_y2 )
{
	/* Normal case: Partial overlap. */
	if ( A_y1 >= B_y1 && A_y1 <= B_y2 ) {
		if ( A_x1 >= B_x1 && A_x1 <= B_x2 )
			return ( TRUE );
		if ( A_x2 >= B_x1 && A_x2 <= B_x2 )
			return ( TRUE );
	}
	if ( A_y2 >= B_y1 && A_y2 <= B_y2 ) {
		if ( A_x1 >= B_x1 && A_x1 <= B_x2 )
			return ( TRUE );
		if ( A_x2 >= B_x1 && A_x2 <= B_x2 )
			return ( TRUE );
	}
	if ( B_y1 >= A_y1 && B_y1 <= A_y2 ) {
		if ( B_x1 >= A_x1 && B_x1 <= A_x2 )
			return ( TRUE );
		if ( B_x2 >= A_x1 && B_x2 <= A_x2 )
			return ( TRUE );
	}
	if ( B_y2 >= A_y1 && B_y2 <= A_y2 ) {
		if ( B_x1 >= A_x1 && B_x1 <= A_x2 )
			return ( TRUE );
		if ( B_x2 >= A_x1 && B_x2 <= A_x2 )
			return ( TRUE );
	}

	/* Extreme case 1: All corner points outside overlap area. */
	if ( A_x1 >= B_x1 && A_x2 <= B_x2 ) {
		if ( A_y1 <= B_y1 && A_y2 >= B_y2 ) {
			/* north and south edges of bbox A are outside of bbox B, but
			 * body of bbox A overlaps bbox B */
			return ( TRUE );
		}
	}
	if ( A_y1 >= B_y1 && A_y2 <= B_y2 ) {
		if ( A_x1 <= B_x1 && A_x2 >= B_x2 ) {
			/* east and west edges of bbox A are outside of bbox B, but
			 * body of bbox A overlaps bbox B */
			return ( TRUE );
		}
	}

	/* Extreme case 2: Equal bboxes */
	if ( A_x1 == B_x1 && A_x2 == B_x2 &&
			A_y1 == B_y1 && A_y2 == B_y2 ) {
		return ( TRUE );
	}

	/* No overlap */
	return ( FALSE );
}


/*
 * Tests if the bounding boxes of A and B overlap.
 * Bounding boxes include the entire polygons, i.e. all of their parts.
 */
BOOLEAN geom_tools_bb_overlap_2D ( geom_store_polygon *A, geom_store_polygon *B )
{
	return ( geom_tools_bb_xy_overlap_2D (A->bbox_x1, A->bbox_x2, A->bbox_y1, A->bbox_y2,
				B->bbox_x1, B->bbox_x2, B->bbox_y1, B->bbox_y2) );
}


/*
 * Tests if the bounding boxes of A and B overlap. This version compares a
 * regular geom_store polygon 'A' with a simplified 'multclip' polygon 'B'.
 * Such simplified polygons consist of only one part.
 */
BOOLEAN geom_tools_mpoly_bb_overlap_2D ( geom_store_polygon *A, geom_multclip_poly *B )
{
	return ( geom_tools_bb_xy_overlap_2D (A->bbox_x1, A->bbox_x2, A->bbox_y1, A->bbox_y2,
				B->bbox_x1, B->bbox_x2, B->bbox_y1, B->bbox_y2) );
}


//DELETE ME: outdated version of this method.
BOOLEAN geom_tools_part_in_part_2D ( geom_part *A, geom_part *B )
{
	BOOLEAN odd = FALSE;

	if ( A == NULL || B == NULL ) {
		return ( FALSE );
	}

	if ( A != B ) { /* do not check part against itself */
		/* test all vertices of A */
		int v;
		for ( v = 0; v < A->num_vertices; v ++ ) {
			odd = FALSE;
			double X = A->X[v];
			double Y = A->Y[v];
			int i,j;
			j = B->num_vertices - 1;
			for ( i = 0; i < B->num_vertices; i++ ) {
				if ( ( B->Y[i] < Y && B->Y[j] >= Y ) || ( B->Y[j] < Y && B->Y[i] >= Y ) ) {
					if ( B->X[i] + ( Y - B->Y[i] ) / ( B->Y[j] - B->Y[i] ) * ( B->X[j] - B->X[i]) < X) {
						odd =! odd;
					}
				}
				j = i;
			}
		}
	} else {
		return ( FALSE );
	}

	return ( odd );
}


/**
 * Tests if part "A" of a polygon lies COMPLETELY (i.e.
 * with all its vertices AND edges) within part "B"
 * of a polygon (including the case that "B" is a hole).
 *
 * Returns TRUE if A lies completely in B; FALSE otherwise.
 *
 * This test does not differentiate between parts that are marked
 * "hole" and those that are not. Therefore, this test is
 * appropriate at the stage of topological processing where the
 * presence of a hole in a polygon is checked for the first time.
 *
 * Note that equal bounding boxes do not necessarily mean equal
 * geometries. Note also that this test counts vertices of B that
 * lie exactly on edges or vertices of A as "within A".
 *
 * If A == B (same memory location or same set of 2D coordinates)
 * then this test returns FALSE.
 *
 * If A or B is NULL then this test returns FALSE.
 *
 * If A or B has less than 3 vertices then this test returns FALSE.
 *
 * CAVEATS: Will return FALSE if the two parts have the same set
 * of 2D vertices. In this case, geometric equality is assumed,
 * even the Z coordinates differ.
 */
BOOLEAN geom_tools_part_in_part_2D_NEW ( geom_part *A, geom_part *B )
{
	if ( A == NULL || B == NULL ) {
		return ( FALSE );
	}

	if ( A->num_vertices < 3 || B->num_vertices < 3 ) {
		return ( FALSE );
	}

	if ( A == B ) {
		return ( FALSE );
	}

	/* Check for special case A == B (coordinates). */
	/* Test if A and B share the same set of vertices. */
	if ( A->num_vertices == B->num_vertices ) {
		BOOLEAN diff = FALSE;
		int i = 0;
		for ( i = 0; i < A->num_vertices; i++ ) {
			int j = 0;
			double x = A->X[i];
			double y = A->Y[i];
			for ( j = 0; j < B->num_vertices; j++ ) {
				if (( x != B->X[j]) || ( y != B->Y[j])) {
					diff = TRUE;
					break;
				}
			}
			if ( diff == TRUE ) {
				break;
			}
		}
		if ( diff == FALSE ) {
			/* Did not find a coordinate pair that A and do NOT have in common! */
			return ( FALSE );
		}
	}

	/* Compute bounding boxes for A and B. */
	double min_x_A = 0.0;
	double min_y_A = 0.0;
	double max_x_A = 0.0;
	double max_y_A = 0.0;
	double min_x_B = 0.0;
	double min_y_B = 0.0;
	double max_x_B = 0.0;
	double max_y_B = 0.0;
	{
		/* Bounding box of A */
		BOOLEAN min_set = FALSE;
		BOOLEAN max_set = FALSE;

		int i = 0;
		for ( i = 0; i < A->num_vertices; i ++ ) {
			if ( min_set == FALSE ) {
				min_x_A = A->X[i];
				min_y_A = A->Y[i];
				min_set = TRUE;
			}
			if ( max_set == FALSE ) {
				max_x_A = A->X[i];
				max_y_A = A->Y[i];
				max_set = TRUE;
			}
			if ( A->X[i] < min_x_A ) {
				min_x_A = A->X[i];
			}
			if ( A->Y[i] < min_y_A ) {
				min_y_A = A->Y[i];
			}
			if ( A->X[i] > max_x_A ) {
				max_x_A = A->X[i];
			}
			if ( A->Y[i] > max_y_A ) {
				max_y_A = A->Y[i];
			}
		}

		/* Bounding box of B */
		min_set = FALSE;
		max_set = FALSE;
		for ( i = 0; i < B->num_vertices; i ++ ) {
			if ( min_set == FALSE ) {
				min_x_B = B->X[i];
				min_y_B = B->Y[i];
				min_set = TRUE;
			}
			if ( max_set == FALSE ) {
				max_x_B = B->X[i];
				max_y_B = B->Y[i];
				max_set = TRUE;
			}
			if ( B->X[i] < min_x_B ) {
				min_x_B = B->X[i];
			}
			if ( B->Y[i] < min_y_B ) {
				min_y_B = B->Y[i];
			}
			if ( B->X[i] > max_x_B ) {
				max_x_B = B->X[i];
			}
			if ( B->Y[i] > max_y_B ) {
				max_y_B = B->Y[i];
			}
		}

	}

	/* Check if the bounding box of A lies completely within
	 * the bounding box of B. This is NOT enough to tell whether
	 * A is indeed completely contained within B (since some edges
	 * of A might cross concave sections of B), but it is the first
	 * condition that must be fulfilled for A to lie completely
	 * (with all edges and vertices) within B! */
	BOOLEAN bbox_A_within_bbox_B = FALSE;
	if ( ( min_x_A >= min_x_B ) &&
		( max_x_A <= max_x_B ) &&
		( min_y_A >= min_y_B ) &&
		( max_y_A <= max_x_B ) ) {
		bbox_A_within_bbox_B = TRUE;
	}
	if ( bbox_A_within_bbox_B == FALSE ) {
		/* We have at least one vertex of A outside the bounding box of B.
		 * By definition, this means that A cannot lie completely within B! */
		return ( FALSE );
	}

	/* If we got this far, then all vertices of A lie within the bounding
	 * box of B. If, in addition, there are no intersection between the
	 * edges of A and B, either, then we can be sure that A lies completely
	 * within B! So we now do an exhaustive edge intersection test.
	 */
	int intersects = geom_tools_parts_intersection_2D ( A, B, NULL, -1, -1, -1, TRUE );

	if ( intersects < 1 ) {
		/* A completely within B */
		return ( TRUE );
	}

	/* Intersection detected: A not completely within B */
	return ( FALSE );
}


/*
 * Tests if part "A" of a polygon lies COMPLETELY (i.e. with
 * all vertices AND edges) within _any_ part of polygon "B"
 * (i.e. any part of B that is NOT a hole!).
 *
 * Returns TRUE if so; FALSE otherwise.
 *
 * This test is appropriate if e.g. a new polygon (part)
 * is checked to see whether it is a hole that needs to be
 * added to an existing polygon.
 *
 * This function is called by geom_store_build() each
 * time a new part is added to an existing polygon object,
 * to mark all internal rings as holes.
 *
 */
//TODO: This test is currently insufficient: derive new version from geom_tools_part_in_part_2D()!
//- MUST BE COMPLETELY WITHIN OUTER BOUNDARY
//- MUST _NOT_ BE COMPLETELY WITHIN ANY EXISTING INNER BOUNDARY (HOLE)
//- REPHRASE DESCRIPTION OF FUNCTION

BOOLEAN geom_tools_part_in_poly_2D ( geom_part *A, geom_store_polygon *B )
{
	int i, j;
	/* step through all parts in B */
	for ( i=0; i < B->num_parts; i ++ ) {
		if ( A != &B->parts[i] ) { /* do not check part against itself */
			/* test all vertices of A */
			BOOLEAN inside = TRUE;
			for ( j=0; j < A->num_vertices; j ++ ) {
				/* test every single vertex in A against the current part in B */
				/* TODO: This test is insufficient if B is concave (edge crossings)!
				 * Use geom_tools_part_in_part_2D() instead!
				 */
				inside = geom_tools_point_in_part_2D ( A->X[j], A->Y[j], B, i );
				if ( inside == FALSE )
					break;
			}
			if ( inside == TRUE ) {
				/* if inside is still TRUE, then all vertices are within the current part of B */
				if ( B->parts[i].is_hole == TRUE ) /* cannot have a hole inside another hole */
					return ( FALSE );
				else
					return ( TRUE ); /* prevent test from "falling through" to other polygon parts */
			}
		}
	}

	return ( FALSE );
}


/*
 * Releases memory for all vertices in *part (but not
 * the memory for the entire struct!)
 *
 * Does nothing if *part is NULL.
 */
void geom_tools_part_destroy ( geom_part* part )
{
	if ( part != NULL ) {
		if ( part->X != NULL ) {
			free ( part->X );
		}
		if ( part->Y != NULL ) {
			free ( part->Y );
		}
		if ( part->Z != NULL ) {
			free ( part->Z );
		}
	}
}


/*
 * Returns a deep copy of "part".
 * New memory for returned result is allocated and must be free'd by caller
 *
 * Returns NULL if the argument is NULL.
 */
struct geom_part* geom_tools_part_duplicate ( geom_part* part )
{
	geom_part* result = NULL;

	if ( part != NULL ) {
		result = malloc ( sizeof ( geom_part ) );
		result->num_vertices = part->num_vertices;
		result->X = malloc ( sizeof (double) * part->num_vertices );
		result->Y = malloc ( sizeof (double) * part->num_vertices );
		result->Z = malloc ( sizeof (double) * part->num_vertices );
		int i;
		for ( i = 0; i < part->num_vertices; i ++ ) {
			result->X[i] = part->X[i];
			result->Y[i] = part->Y[i];
			result->Z[i] = part->Z[i];
		}
		result->has_label = part->has_label;
		result->label_x = part->label_x;
		result->label_y = part->label_y;
		result->is_hole = part->is_hole;
		result->is_undershoot_first = part->is_undershoot_first;
		result->dist_undershoot_first = part->dist_undershoot_first;
		result->x_undershoot_first = part->x_undershoot_first;
		result->y_undershoot_first = part->y_undershoot_first;
		result->is_undershoot_last = part->is_undershoot_last;
		result->dist_undershoot_last = part->dist_undershoot_last;
		result->x_undershoot_last = part->x_undershoot_last;
		result->y_undershoot_last = part->y_undershoot_last;
		result->err_self_intersects = part->err_self_intersects;
		result->is_empty = part->is_empty;
	}

	return ( result );
}


/*
 * Returns TRUE if the two line segments defined by the 2D points "p0" and "p1",
 * and "p2" and "p3", respectively, intersect, FALSE otherwise.
 * The 2D intersection point is returned in "i_x" and "i_y" if each is a non-NULL
 * memory location.
 *
 * An intersection point that lies exactly at the coordinates of one of the
 * given vertices constitutes an extreme case and is not regarded a valid
 * intersection (i.e. "FALSE" is returned in such case). This makes sure that
 * this method will also work properly when checking for self-intersections
 * between connected line/boundary segments.
 *
 * Note: 'i_x' and 'i_y' must point to properly allocated variables to store
 * intersection coordinates.
 */
BOOLEAN geom_tools_line_intersection_2D (double p0_x, double p0_y, double p1_x, double p1_y,
		double p2_x, double p2_y, double p3_x, double p3_y, double *i_x, double *i_y)
{
    double s1_x, s1_y, s2_x, s2_y;
    double s, t;


    s1_x = p1_x - p0_x;
    s1_y = p1_y - p0_y;
    s2_x = p3_x - p2_x;
    s2_y = p3_y - p2_y;

    s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
    t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
        /* intersection detected */
        if (i_x != NULL)
            *i_x = p0_x + (t * s1_x);
        if (i_y != NULL)
            *i_y = p0_y + (t * s1_y);
        return TRUE;
    }

    return FALSE; /* No intersection or parallel lines */
}


/*
 * Basically the same as geom_tools_line_intersection_2D(), but with an
 * added guard against self-intersections. This function is only called
 * by geom_store_add().
 */
BOOLEAN geom_tools_line_self_intersection_2D (double p0_x, double p0_y, double p1_x, double p1_y,
		double p2_x, double p2_y, double p3_x, double p3_y, double *i_x, double *i_y)
{
	double s1_x, s1_y, s2_x, s2_y;
	double s, t;


	s1_x = p1_x - p0_x;
	s1_y = p1_y - p0_y;
	s2_x = p3_x - p2_x;
	s2_y = p3_y - p2_y;

	s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
	t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

	if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
		/* intersection detected */
		if (i_x != NULL)
			*i_x = p0_x + (t * s1_x);
		if (i_y != NULL)
			*i_y = p0_y + (t * s1_y);
		/* guard for extreme cases: intersection point exactly at existing line vertex */
		if ( (p0_x != *i_x && p0_y != *i_y) &&
				(p1_x != *i_x && p1_y != *i_y) &&
				(p2_x != *i_x && p2_y != *i_y) &&
				(p3_x != *i_x && p3_y != *i_y)) {
			return TRUE; /* passed: valid intersection point */
		}
	}

	return FALSE; /* No intersection or parallel lines */
}



/*
 * Checks whether the vertex described by x/y/z was added to the
 * geometry part as an intersection vertex. This is important for
 * deciding whether a very short first or last line segment is
 * possibly a dangle.
 */
BOOLEAN geom_tools_is_intersection_vertex ( double x, double y, double z,
		geom_store *gs, int geom_type, unsigned int geom_id, unsigned int part_idx )
{
	geom_store_intersection *gsi = NULL;
	int i = 0;


	if ( ( geom_type != GEOM_TYPE_LINE ) && ( geom_type != GEOM_TYPE_POLY ) ) {
		return ( FALSE );
	}

	if ( geom_type == GEOM_TYPE_LINE ) {
		gsi = gs->lines_intersections;
	} else {
		gsi = gs->polygons_intersections;
	}

	if ( gsi == NULL ) {
		return ( FALSE );
	}

	/* check if this vertex is on the list of intersections for its geom type */
	for ( i = 0; i < gsi->num_intersections; i ++ ) {
		if ( gsi->added[i] == TRUE ) { /* check only intersections that were actually added */
			if ( gsi->geom_id[i] == geom_id && gsi->part_id[i] == part_idx ) {
				if ( gsi->X[i] == x && gsi->Y[i] == y && gsi->Z[i] == z ) {
					return ( TRUE ); /* yes: return TRUE */
				}
			}
		}
	}

	return ( FALSE ); /* not an intersection node */
}


/*
 * Removes the first vertex from the passed line part. This will
 * only work if the line has at least three vertices.
 *
 * Returns a pointer to a newly allocated part struct that is a copy
 * of the input part with the first vertex removed.
 *
 * Returns NULL on error.
 */
geom_part *geom_tools_line_part_remove_first_vertex ( geom_part *part )
{
	geom_part *new_part = NULL;
	int i = 0;


	if ( part == NULL ) {
		return ( NULL );
	}

	if ( part->num_vertices < 3 ) {
		return ( NULL );
	}

	new_part = geom_tools_part_duplicate ( part );
	if ( new_part == NULL ) {
		return ( NULL );
	}

	/* release memory for copied vertices */
	geom_tools_part_destroy ( new_part );

	/* subtract one vertex from count */
	new_part->num_vertices --;

	/* allocate memory for new vertices */
	new_part->X = malloc ( sizeof (double) * new_part->num_vertices );
	new_part->Y = malloc ( sizeof (double) * new_part->num_vertices );
	new_part->Z = malloc ( sizeof (double) * new_part->num_vertices );

	/* copy remaining vertices */
	for ( i = 0; i < new_part->num_vertices; i ++ ) {
		new_part->X[i] = part->X[i+1];
		new_part->Y[i] = part->Y[i+1];
		new_part->Z[i] = part->Z[i+1];
	}

	return ( new_part );
}


/*
 * Removes the last vertex from the passed line part. This will
 * only work if the line has at least three vertices.
 *
 * Returns a pointer to a newly allocated part struct that is a copy
 * of the input part with the last vertex removed.
 *
 * Returns NULL on error.
 */
geom_part *geom_tools_line_part_remove_last_vertex ( geom_part *part )
{
	geom_part *new_part = NULL;
	int i = 0;


	if ( part == NULL ) {
		return ( NULL );
	}

	if ( part->num_vertices < 3 ) {
		return ( NULL );
	}

	new_part = geom_tools_part_duplicate ( part );
	if ( new_part == NULL ) {
		return ( NULL );
	}

	/* release memory for copied vertices */
	geom_tools_part_destroy ( new_part );

	/* subtract one vertex from count */
	new_part->num_vertices --;

	/* allocate memory for new vertices */
	new_part->X = malloc ( sizeof (double) * new_part->num_vertices );
	new_part->Y = malloc ( sizeof (double) * new_part->num_vertices );
	new_part->Z = malloc ( sizeof (double) * new_part->num_vertices );

	/* copy remaining vertices */
	for ( i = 0; i < new_part->num_vertices; i ++ ) {
		new_part->X[i] = part->X[i];
		new_part->Y[i] = part->Y[i];
		new_part->Z[i] = part->Z[i];
	}

	return ( new_part );
}


/*
 * Registers a new intersection point in 'gs'.
 * Does NOT add a new vertex to a geometry (this is done by
 * geom_topology_intersections_2D_add).
 * Intersection coordinates, node position, geometry store,
 * geometry ID and part index no. must all be provided.
 *
 * Returns TRUE if intersection was added, FALSE otherwise.
 */
BOOLEAN geom_tools_new_intersection ( double x, double y, double z, int position,
		geom_store *gs, int geom_type, unsigned int geom_id, unsigned int part_idx )
{
	BOOLEAN DEBUG = FALSE;
	geom_store_intersection *gsi = NULL;
	void *new_mem = NULL;
	int i = 0;


	if ( gs == NULL ) {
		return FALSE;
	}

	if ( ( geom_type != GEOM_TYPE_LINE ) && ( geom_type != GEOM_TYPE_POLY ) ) {
		return FALSE;
	}

	/* Select intersections list in which to store this intersection
	 * and allocate more memory if needed. */
	if ( geom_type == GEOM_TYPE_LINE ) {
		gsi = gs->lines_intersections;
		if ( gsi->capacity < 1 ) {
			if ( DEBUG == TRUE ) {
				fprintf (stderr, "\n*** increase capacity of lines intersection list ***\n");
				fprintf (stderr, "store intersection #%u\n", gsi->num_intersections);
				fprintf (stderr, "increase to: %u\n\n", gs->lines_intersections->num_intersections + GEOM_STORE_CHUNK_BIG);
			}
			/* need more memory */
			int length = gs->lines_intersections->num_intersections + GEOM_STORE_CHUNK_BIG;
			int size = 0;
			/* mem alloc: geom_id */
			size = length * sizeof ( unsigned int );
			new_mem = realloc ( (void*) gs->lines_intersections->geom_id, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->geom_id = (unsigned int*) new_mem;
			/* mem alloc: part_id */
			size = length * sizeof ( unsigned int );
			new_mem = realloc ( (void*) gs->lines_intersections->part_id, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->part_id = (unsigned int*) new_mem;
			/* mem alloc: X */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->lines_intersections->X, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->X = (double*) new_mem;
			/* mem alloc: Y */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->lines_intersections->Y, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->Y = (double*) new_mem;
			/* mem alloc: Z */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->lines_intersections->Z, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->Z = (double*) new_mem;
			/* mem alloc: v */
			size = length * sizeof ( int );
			new_mem = realloc ( (void*) gs->lines_intersections->v, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->v = (int*) new_mem;
			/* mem alloc: added */
			size = length * sizeof ( int );
			new_mem = realloc ( (void*) gs->lines_intersections->added, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->lines_intersections->added = (BOOLEAN*) new_mem;			

			/* store new capacity */
			gs->lines_intersections->capacity = GEOM_STORE_CHUNK_BIG;
		}
	} else {
		gsi = gs->polygons_intersections;
		if ( gsi->capacity < 1 ) {
			if ( DEBUG == TRUE ) {
				fprintf (stderr, "\n*** increase capacity of polygons intersection list ***\n");
				fprintf (stderr, "store intersection #%u\n", gsi->num_intersections);
				fprintf (stderr, "increase to: %u\n\n", gs->polygons_intersections->num_intersections + GEOM_STORE_CHUNK_BIG);
			}
			/* need more memory */
			int length = gs->polygons_intersections->num_intersections + GEOM_STORE_CHUNK_BIG;
			int size = 0;
			/* mem alloc: geom_id */
			size = length * sizeof ( unsigned int );
			new_mem = realloc ( (void*) gs->polygons_intersections->geom_id, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->geom_id = (unsigned int*) new_mem;
			/* mem alloc: part_id */
			size = length * sizeof ( unsigned int );
			new_mem = realloc ( (void*) gs->polygons_intersections->part_id, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->part_id = (unsigned int*) new_mem;
			/* mem alloc: X */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->polygons_intersections->X, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->X = (double*) new_mem;
			/* mem alloc: Y */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->polygons_intersections->Y, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->Y = (double*) new_mem;
			/* mem alloc: Z */
			size = length * sizeof ( double );
			new_mem = realloc ( (void*) gs->polygons_intersections->Z, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->Z = (double*) new_mem;
			/* mem alloc: v */
			size = length * sizeof ( int );
			new_mem = realloc ( (void*) gs->polygons_intersections->v, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->v = (int*) new_mem;
			/* mem alloc: added */
			size = length * sizeof ( int );
			new_mem = realloc ( (void*) gs->polygons_intersections->added, size );
			if ( new_mem == NULL ) {
				return ( FALSE );
			}
			gs->polygons_intersections->added = (BOOLEAN*) new_mem;

			/* store new capacity */
			gs->polygons_intersections->capacity = GEOM_STORE_CHUNK_BIG;
		}
	}

	/* make sure we have a valid geometry */
	if ( gsi == NULL || geom_id < 0 ) { // if ( gsi == NULL ) {
		return ( FALSE );
	}

	/* make sure that this intersection is not already on the list */
	for ( i = 0; i < gsi->num_intersections; i ++ ) {
		if ( geom_id == gsi->geom_id[i] && gsi->part_id[i] == part_idx ) {
			/* intersection is in same geometry and part? */
			if ( x == gsi->X[i] && y == gsi->Y[i] && z == gsi->Z[i] ) {
				/* got equal coordinates, as well? */
				return ( FALSE ); /* this one is already registered: abort! */
			}
		}
	}

	/* add intersection point */
	gsi->geom_id[gsi->num_intersections] = geom_id;
	gsi->part_id[gsi->num_intersections] = part_idx;
	gsi->X[gsi->num_intersections] = x;
	gsi->Y[gsi->num_intersections] = y;
	gsi->Z[gsi->num_intersections] = z;
	gsi->v[gsi->num_intersections] = position;
	gsi->added[gsi->num_intersections] = FALSE;

	if ( DEBUG == TRUE ) {
		fprintf (stderr, "\n*** add intersection point to list ***\n");
		fprintf (stderr, "store intersection #%u\n", gsi->num_intersections);
		if ( geom_type == GEOM_TYPE_LINE ) {
			fprintf (stderr, "geometry type: LINE\n");
		} else {
			fprintf (stderr, "geometry type: POLYGON\n");
		}
		fprintf (stderr, "geom id: %u\n", gsi->geom_id[gsi->num_intersections]);
		fprintf (stderr, "part no: %u\n", gsi->part_id[gsi->num_intersections]);
		fprintf (stderr, "coordinates: %.12f/%.12f/%.12f\n", gsi->X[gsi->num_intersections], gsi->Y[gsi->num_intersections], gsi->Z[gsi->num_intersections]);
		fprintf (stderr, "vertex: %i\n\n", gsi->v[gsi->num_intersections]);
	}

	/* increase intersections counter */
	gsi->num_intersections ++;

	/* decrease capacity */
	gsi->capacity --;

	return ( TRUE );
}

/*
 * Helper function: Returns the number of intersection vertices registered for a
 * given geom and part ID of a given geometry type that have _not_ yet been added
 * as vertices to the given geometry part.
 */
int geom_tools_get_num_open_intersects ( unsigned int geom_id, unsigned int part_id, int geom_type, geom_store *gs ) {
	int result = 0;

	if ( (gs != NULL) && ( ( geom_type = GEOM_TYPE_LINE ) || ( geom_type = GEOM_TYPE_POLY ) ) ) {
		geom_store_intersection* lists = gs->polygons_intersections; /* default: check poly intersections */
		if ( geom_type == GEOM_TYPE_LINE ) {
			lists = gs->lines_intersections; /* otherwise: check line intersections */
		}
		unsigned int i;
		for ( i=0; i < lists->num_intersections; i ++ ) {
			if ( ( lists->geom_id[i] == geom_id ) && ( lists->part_id[i] == part_id ) ) {
				if ( lists->added[i] == FALSE ) {
					result ++;
				}
			}
		}
	}

	return (result);
}


/*
 * Checks whether geom part A is intersected by B and, if so, registers the intersection
 * point(s) in the geometry store 'gs'. Since the geometry store keeps separate intersection
 * lists for lines and polygons, the geometry type for A must also be provided. Also, the
 * geometry ID and part index no. of A must be provided.
 * This mode of operation is set by passing "check_only" as "FALSE". It requires "gs" to be
 * a properly allocated geom_store struct.
 *
 * Alternatively, this function can perform only the intersection test, without registering
 * any points. This mode of operation is set by passing "check_only" as "TRUE". In that
 * case, "gs" can also be passed as "NULL", and "geom_type", "geom_id" and "part_idx" can be
 * passed as any value: they will be ignored.
 *
 * Returns number of intersections detected (0 or more) or -1 on error.
 */
 int geom_tools_parts_intersection_2D ( geom_part* A, geom_part* B, geom_store *gs,
		int geom_type, unsigned int geom_id, unsigned int part_idx, BOOLEAN check_only )
{
	int result = 0;
	double p0_x = 0.0;
	double p0_y = 0.0;
	double p0_z = 0.0;
	double p1_x = 0.0;
	double p1_y = 0.0;
	double p1_z = 0.0;
	double p2_x = 0.0;
	double p2_y = 0.0;
	double p3_x = 0.0;
	double p3_y = 0.0;
	double i_x = 0.0;
	double i_y = 0.0;
	double v_x = 0.0;
	double v_y = 0.0;
	double v_z = 0.0;
	double dist_ab_2d  = 0.0;
	int i, j, k = 0;
	int offset = 0;
	BOOLEAN inserted_node = FALSE;
	BOOLEAN found = FALSE;
	BOOLEAN DEBUG = FALSE;


	if ( A == NULL || B == NULL ) {
		return (-1);
	}
	if ( A->is_empty == TRUE || B->is_empty == TRUE ) {
		return (-1);
	}
	if ( A->num_vertices < 2 || B->num_vertices < 2 ) {
		return (-1);
	}

	/* We need an allocated geom_store if we want to store intersection points. */
	if ( check_only == FALSE ) {
		if ( gs == NULL ) {
			return ( -1 );
		}
	}

	/* We need to know the geometry type if we want to store intersection points. */
	if ( check_only == FALSE ) {
		if ( ( geom_type != GEOM_TYPE_LINE ) && ( geom_type != GEOM_TYPE_POLY ) ) {
			return (-1);
		}
	}

	/* Main loop: Repeat, until we complete one run without inserting a now node at
	   an intersection point. */
	do {
		/* Outer loop: step through all "line segments", i.e. pairs of immediately
		   consecutive vertices in geometry A (i.e. the one into which we might need
		   to insert new intersection vertices. */		
		for ( i = 0; i < A->num_vertices-1; i ++ ) {
			/* */
			inserted_node = FALSE; /* Presume that we won't have to insert a node. */
			/* these are to store a simplified representation of the current segment */
			p0_x = A->X[i]; /* first vertex of current segment */
			p0_y = A->Y[i];
			p0_z = A->Z[i];
			p1_x = A->X[i+1]; /* second vertex of current segment */
			p1_y = A->Y[i+1];
			p1_z = A->Z[i+1];
			if ( DEBUG == TRUE ) {
				fprintf (stderr,"Segment A: (%i/%i)%.10f/%.10f -- (%i/%i)%.10f/%.10f\n", i, i, p0_x, p0_y, (i+1), (i+1), p1_x, p1_y );
			}
			/* Inner loop: step through all segments of geometry B.*/
			for ( j = 0; j < B->num_vertices-1; j ++ ) {
				p2_x = B->X[j];
				p2_y = B->Y[j];
				p3_x = B->X[j+1];
				p3_y = B->Y[j+1];				
				if ( DEBUG == TRUE ) {
					fprintf (stderr,"Segment B: (%i/%i)%.10f/%.10f -- (%i/%i)%.10f/%.10f\n", j, j, p2_x, p2_y, (j+1), (j+1), p3_x, p3_y );
				}
				/* By definition, a line segment cannot intersect with another line segment more than once! */
				if ( geom_tools_line_intersection_2D ( p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, &i_x, &i_y) == TRUE ) {
					/* We have an intersection:
					   add, but only if intersection point is _not_ an already existing node in A!
					   Note that we round coordinate when checking for identical points. This is necessary,
					   as limited geometrical precision can result in "phantom points" on sloping segments. */					   
					found = FALSE;
					for ( k = 0; k < A->num_vertices-1; k ++ ) {
						if ( ( fabs ( A->X[k] - i_x ) < 0.000000001 ) && ( ( fabs ( A->Y[k] - i_y ) < 0.000000001 ) ) ) {							
							found = TRUE;
							break;
						}
					}
					if ( found == FALSE ) {
						/* Ok: It's a new intersection point: build new vertex at intersection */
						v_x = i_x;
						v_y = i_y;
						/* get vertex distances */
						dist_ab_2d  = sqrt ( pow ( ( p1_x - p0_x ), 2 ) + pow ( ( p1_y - p0_y ), 2 ) );
						if ( dist_ab_2d > 0.0 ) {
							/* interpolate Z as weighted (by distance from existing vertices) arithmetic mean */
							v_z = geom_tools_interpolate_z ( p0_x, p0_y, p0_z, p1_x, p1_y, p1_z, v_x, v_y );
							/* register new intersection vertex */
							if ( check_only == FALSE ) {								
								if ( DEBUG == TRUE ) {
									fprintf (stderr,"\tINTERSECTION: ADD %i/%i betw. %i-%i: %.10f/%.10f\n", 
											i+1+offset, A->num_vertices, i, (i+1), v_x, v_y);									
									//fprintf (stderr,"\tP0: %.10f/%.10f\n", p0_x, p0_y);
									//fprintf (stderr,"\tP1: %.10f/%.10f\n", p1_x, p1_y);									
								}
								geom_tools_new_intersection ( v_x, v_y, v_z, i+1+offset, gs, geom_type, geom_id, part_idx );
								/* Rewrite this part of geometry A */
								//TODO: What to do with return value? Should be = 1 if operation was successful!
								geom_topology_intersections_2D_add ( gs, NULL ); //TODO: this function does not actually use *opts!
								//num_vertices_added = geom_topology_intersections_2D_add ( gs, opts );								
								inserted_node = TRUE; /* This means we have to restart at first segment of A! */
								/* update count */								
								result ++;
							}
						}
						break; /* Important: we need to break here and restart, as the geometry has changed!
								  The caller must make sure that the process for this part is restartet,
								  until no more intersections are detected for it! */						
					}
				}
			}			
		}
	} while ( inserted_node == TRUE ); /* We only do one more round of a node was inserted. */

	if ( DEBUG == TRUE ) {
		fprintf (stderr,"PART DONE.\n");
	}

	return result;
}


/*
 * Interpolates Z coordinate for a new vertex (defined by 'v_x' and 'v_y') that
 * lies on a straight line between the existing vertices 'p0' and 'p1' (both of
 * the latter are defined by 3D coordinates). This is a simple linear interpolation.
 *
 * Returns the new Z coordinate.
 *
 * Note: For best results, 'v_x' and 'v_y' should describe a point that lies exactly
 * on the straight 2D line between the X/Y locations of p0 and p1. If this is
 * not the case then the result will be less accurate/realistic!
 */
double geom_tools_interpolate_z ( double p0_x, double p0_y, double p0_z,
								double p1_x, double p1_y, double p1_z,
								double v_x, double v_y )
{
	double dist_ab_2d;
	double dist_a_new_2d;
	double dist_b_new_2d;
	double weight_a;
	double weight_b;
	double v_z = 0.0;

	dist_a_new_2d = sqrt ( pow ( ( p0_x - v_x ), 2 ) + pow ( ( p0_y - v_y ), 2 ) );
	dist_b_new_2d = sqrt ( pow ( ( p1_x - v_x ), 2 ) + pow ( ( p1_y - v_y ), 2 ) );
	dist_ab_2d  = dist_a_new_2d + dist_b_new_2d;
	/* Note: if the new vertex lies on the straight line between the two given ones,
	 * then dist_ab_2d = sqrt ( pow ( ( p1_x - p0_x ), 2 ) + pow ( ( p1_y - p0_y ), 2 ) );
	 *                 = dist_a_new_2d + dist_b_new_2d;
	 */
	if ( dist_ab_2d > 0.0 ) {
		/* interpolate Z as weighted (by distance from existing vertices) arithmetic mean */
		weight_a = ( dist_a_new_2d / dist_ab_2d );
		weight_b = ( dist_b_new_2d / dist_ab_2d );
		v_z = ( weight_a * p0_z ) + ( weight_b * p1_z );
	} else {
		/* vertices are at identical X/Y location */
		v_z = ( p0_z + p1_z ) / 2.0;
	}

	return ( v_z );
}


/*
 * Adds a new vertex, defined by its X, Y and Z coordinates, to an existing geometry part.
 * The vertex is added _at_ the specified "position", which must be an integer
 * value between "0" (inclusive) and part->num_vertices (exclusive).
 *
 * Returns a new geometry part with the added vertex on success, NULL on error.
 *
 * If a vertex already exists (at any index position) with the exact same X/Y/Z coordinates
 * then the returned results will be an exact copy (i.e. with the same number of vertices)
 * of the part passed to this function.
 */
geom_part* geom_tools_part_add_vertex ( geom_part* part, int position, double x, double y, double z )
{
	geom_part *result = NULL;
	int insert = 0;
	int length = 0;
	int i, j;
	BOOLEAN DEBUG = FALSE;


	insert = position;

	if ( DEBUG == TRUE ) {
		fprintf ( stderr, "\tAdding vertex at position: %i\n", insert );
	}

	/* check for problems with the input part */
	if ( part == NULL || insert < 0 || insert > part->num_vertices ) {
		return NULL;
	}
	if ( part->X == NULL || part->Y == NULL || part->Z == NULL ) {
		return NULL;
	}

	/* check whether vertex does already exist */
	for ( i = 0; i < part->num_vertices; i ++ ) {
		if ( part->X[i] == x && part->Y[i] == y && part->Z[i] == z )
		{
			// return an exact duplicate of the part
			return (geom_tools_part_duplicate ( part ));
		}
	}

	/* we work on a copy of the original part */
	result = geom_tools_part_duplicate ( part );

	/* remove copied vertices of result part */
	free ( result->X );
	free ( result->Y );
	free ( result->Z );

	/* allocate memory for old vertices + one new vertex */
	length = part->num_vertices + 1;
	result->X = malloc ( sizeof(double) * length );
	result->Y = malloc ( sizeof(double) * length );
	result->Z = malloc ( sizeof(double) * length );
	result->num_vertices = length;

	/* copy old vertices, insert new vertex while copying */
	j = 0;
	for ( i = 0; i < length; i ++ ) {
		if ( i == insert ) {
			result->X[i] = x;
			result->Y[i] = y;
			result->Z[i] = z;
		} else {
			result->X[i] = part->X[j];
			result->Y[i] = part->Y[j];
			result->Z[i] = part->Z[j];
			j ++;
		}
	}

	/* done */
	return (result);
}


/*
 * Extends a line part by extruding the first and/or last segment(s) by "amount".
 * This operation is 3D: the line is extended along its X/Y/Z vectors.
 *
 * Arguments:
 * 	geometry part: (must be type "line") to extend
 * 	amount: by which to extend line part
 * 	first: whether to extend first segment
 * 	last: whether to extend last segment
 *
 * Returns a new geometry part that represents the extended input line part.
 * The first and last vertices of the output line part will have new coordinates.
 * Memory for the result is allocated by this function and must
 * be released by the caller.
 *
 * Returns NULL on error.
 */
struct geom_part* geom_tools_line_part_extend_3D ( geom_part* part, double amount, BOOLEAN first, BOOLEAN last )
{
	int A, B; /* vertex pair representing first/last segment */
	double x1, y1, z1;
	double x2, y2, z2;
	double x3, y3, z3; /* adjusted vertex coordinates */
	double dist;
	geom_part *result = NULL;


	if ( amount < 0.0 || part == NULL || part->num_vertices < 2 || part->is_empty == TRUE ) {
		return NULL;
	}

	/* we work on a copy of the original part */
	result = geom_tools_part_duplicate ( part );

	/* 1: EXTEND LAST SEGMENT */

	/* get coordinates of last vertex and its predecessor */
	A = part->num_vertices - 2; /* predecessor */
	B = part->num_vertices - 1; /* last vertex */
	x1 = part->X[A];
	y1 = part->Y[A];
	z1 = part->Z[A];
	x2 = part->X[B];
	y2 = part->Y[B];
	z2 = part->Z[B];
	/* add offsets */
	dist = sqrt(pow((x2 - x1),2) + pow((y2 - y1),2) + pow((z2 - z1),2));
	/* guard against devision by zero */
	if ( dist == 0.0 ) {
		geom_tools_part_destroy ( result );
		return NULL;
	}
	/* translate vertex */
	x3 = x2 + ( ( x2 - x1 ) / dist ) * amount ;
	y3 = y2 + ( ( y2 - y1 ) / dist ) * amount;
	z3 = z2 + ( ( z2 - z1 ) / dist ) * amount;
	result->X[B] = x3;
	result->Y[B] = y3;
	result->Z[B] = z3;

	/* 2: EXTEND FIRST SEGMENT */

	/* get coordinates of first vertex and its successor */
	A = 1; /* successor */
	B = 0; /* first vertex */
	x1 = part->X[A];
	y1 = part->Y[A];
	z1 = part->Z[A];
	x2 = part->X[B];
	y2 = part->Y[B];
	z2 = part->Z[B];
	/* add offsets */
	dist = sqrt(pow((x2 - x1),2) + pow((y2 - y1),2) + pow((z2 - z1),2));
	/* guard against devision by zero */
	if ( dist == 0.0 ) {
		geom_tools_part_destroy ( result );
		return NULL;
	}
	/* translate vertex */
	x3 = x2 + ( ( x2 - x1 ) / dist ) * amount ;
	y3 = y2 + ( ( y2 - y1 ) / dist ) * amount ;
	z3 = z2 + ( ( z2 - z1 ) / dist ) * amount ;
	/* assign new coordinates to first vertex */
	result->X[B] = x3;
	result->Y[B] = y3;
	result->Z[B] = z3;

	/* done */
	return ( result );
}




/*
 * Add a point, line or polygon to geometry store "gs".
 * Points are straight forward, with X, Y, and Z being "arrays" of length 1.
 *
 * For lines and polygons, the situation is more complex:
 *
 * If a geometry with "geom_id" already exists, then the geom. is added
 * as a new part of a multi-part line string or polygon.
 * Otherwise, it is added as a simple new geometry.
 * The vertices must be given in the arrays X, Y, Z.
 * It is mandatory that all three are of the same length and that
 * the precise length is passed as "num_vertices".
 *
 * This function will automatically allocated additional memory
 * in the geometry store if it should run low.
 *
 * On error, this function will return FALSE (otherwise TRUE).
 * In addition, if "error" is non-NULL, then it is taken to point to
 * a _pre-allocated_ char[] of length PRG_MAX_STR_LEN and a more
 * descriptive error message will be stored there.
 */
BOOLEAN geom_store_add 	( 	geom_store *gs, parser_desc *parser,
		unsigned int geom_id, unsigned int num_vertices, int GEOM_TYPE,
		double *X, double *Y, double *Z,
		unsigned int part_id,
		char **atts, char *source, unsigned int line,
		BOOLEAN is_3D, char *error, options *opts )
{
	unsigned int part = 0;
	unsigned int cur_geom = 0;

	/* update min/max extents of data in this store */
	if ( num_vertices > 0 ) {
		if ( gs->min_x_set == FALSE ) {
			gs->min_x = X[0];
			gs->min_x_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( X[i] < gs->min_x ) {
					gs->min_x = X[i];
				}
			}
		}
		if ( gs->min_y_set == FALSE ) {
			gs->min_y = Y[0];
			gs->min_y_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( Y[i] < gs->min_y ) {
					gs->min_y = Y[i];
				}
			}
		}
		if ( gs->min_z_set == FALSE ) {
			gs->min_z = Z[0];
			gs->min_z_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( Z[i] < gs->min_z ) {
					gs->min_z = Z[i];
				}
			}
		}
		if ( gs->max_x_set == FALSE ) {
			gs->max_x = X[0];
			gs->max_x_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( X[i] > gs->max_x ) {
					gs->max_x = X[i];
				}
			}
		}
		if ( gs->max_y_set == FALSE ) {
			gs->max_y = Y[0];
			gs->max_y_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( Y[i] > gs->max_y ) {
					gs->max_y = Y[i];
				}
			}
		}
		if ( gs->max_z_set == FALSE ) {
			gs->max_z = Z[0];
			gs->max_z_set = TRUE;
		} else {
			int i;
			for ( i = 0; i < num_vertices; i ++ ) {
				if ( Z[i] > gs->max_z ) {
					gs->max_z = Z[i];
				}
			}
		}
	}

	/* find index of current geometry */
	if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
		cur_geom = gs->num_lines;
		int i;
		for ( i=0; i < gs->num_lines; i ++) {
			if ( gs->lines[i].geom_id == geom_id ) {
				cur_geom = i;
				break;
			}
		}
	} else {
		cur_geom = gs->num_polygons;
		int i;
		for ( i=0; i < gs->num_polygons; i ++) {			
			if ( gs->polygons[i].geom_id == geom_id ) {
				cur_geom = i;
				break;
			}
		}
	}

	/* first deal with points as the most simple case */
	if ( GEOM_TYPE == GEOM_TYPE_POINT || GEOM_TYPE == GEOM_TYPE_POINT_RAW ) {
		geom_store_point *points = NULL;
		unsigned int num_points = 0;
		unsigned int free_points = 0;
		if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
			points = gs->points;
			num_points = gs->num_points;
			free_points = gs->free_points;
		} else {
			points = gs->points_raw;
			num_points = gs->num_points_raw;
			free_points = gs->free_points_raw;
		}
		if ( free_points < 1 ) {
			void *new_mem;
			if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
				new_mem = realloc ( (void*) gs->points, sizeof (geom_store_point) * (gs->num_points + GEOM_STORE_CHUNK_BIG) );
				if ( new_mem == NULL ) {
					if ( error != NULL ) {
						snprintf ( error, PRG_MAX_STR_LEN, _("Out of memory.") );
					}
					return ( FALSE );
				}
				gs->points = (geom_store_point*) new_mem;
				/* initialize with default values */
				int i;
				for ( i = gs->num_points; i < (gs->num_points+GEOM_STORE_CHUNK_BIG); i++ ) {
					gs->points[i].geom_id = -1;
					gs->points[i].is_empty = TRUE;
					gs->points[i].is_selected = FALSE;
					gs->points[i].source = NULL;
					int j;
					for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
						gs->points[i].atts[j] = NULL;
					}
				}
				points = gs->points;
			} else {
				new_mem = realloc ( (void*) gs->points_raw, sizeof (geom_store_point) * (gs->num_points_raw + GEOM_STORE_CHUNK_BIG) );
				if ( new_mem == NULL ) {
					if ( error != NULL ) {
						snprintf ( error, PRG_MAX_STR_LEN, _("Out of memory.") );
					}
					return ( FALSE );
				}
				gs->points_raw = (geom_store_point*) new_mem;
				/* initialize with default values */
				int i;
				for ( i = gs->num_points_raw; i < (gs->num_points_raw+GEOM_STORE_CHUNK_BIG); i++ ) {
					gs->points_raw[i].geom_id = -1;
					gs->points_raw[i].is_empty = TRUE;
					gs->points_raw[i].is_selected = FALSE;
					gs->points_raw[i].source = NULL;
					int j;
					for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
						gs->points_raw[i].atts[j] = NULL;
					}
				}
				points = gs->points_raw;
			}
			if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
				gs->free_points = GEOM_STORE_CHUNK_BIG;
			} else {
				gs->free_points_raw = GEOM_STORE_CHUNK_BIG;
			}
		}
		if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
			gs->free_points --;
		} else {
			gs->free_points_raw --;
		}

		/* store new point */
		points[num_points].geom_id = geom_id;
		points[num_points].X = X[0];
		points[num_points].Y = Y[0];
		if ( is_3D == TRUE ) {
			points[num_points].Z = Z[0];
		} else {
			points[num_points].Z = 0.0;
		}
		points[num_points].is_empty = FALSE;
		points[num_points].has_errors = FALSE;
		points[num_points].is_selected = TRUE;
		points[num_points].is_3D = is_3D;
		points[num_points].has_label = FALSE;
		points[num_points].label_x = 0.0;
		points[num_points].label_y = 0.0;
		/* copy attribute fields */
		int j = 0;
		while ( parser->fields[j] != NULL ) {
			if ( atts[j] == NULL ) {
				points[num_points].atts[j] = strdup ( "" );
			} else {
				points[num_points].atts[j] = strdup ( atts[j] );
			}
			j++;
		}
		points[num_points].atts[j] = NULL;

		/* copy source information */
		if ( source != NULL ) {
			points[num_points].source = strdup ( source );
		} else {
			points[num_points].source = NULL;
		}
		points[num_points].line = line;

		/* add label to point? */
		if ( opts != NULL && opts->label_field != NULL ) {
			/* Label mode is ignored for points! */
			points[num_points].label_x = points[num_points].X;
			points[num_points].label_y = points[num_points].Y;
			points[num_points].has_label = TRUE;
		}

		/* increase point counter */
		if ( GEOM_TYPE == GEOM_TYPE_POINT ) {
			gs->num_points ++;
		} else {
			gs->num_points_raw ++;
		}

		gs->is_empty = FALSE;

		/* DONE: Get out of here! */
		return ( TRUE );
	}

	/* now deal with more complex, potentially multi-part, geometries */
	if ( GEOM_TYPE != GEOM_TYPE_LINE && GEOM_TYPE != GEOM_TYPE_POLY ) {
		if ( error != NULL ) {
			snprintf ( error, PRG_MAX_STR_LEN, _("Wrong geometry type (must be point, line or polygon).") );
		}
		return ( FALSE );
	}

	if ( 	( ( num_vertices < 2 ) && ( GEOM_TYPE == GEOM_TYPE_LINE ) ) ||
			( ( num_vertices < 3 ) && ( GEOM_TYPE == GEOM_TYPE_POLY ) ) ) {
		if ( error != NULL ) {
			if ( GEOM_TYPE == GEOM_TYPE_LINE )
				snprintf ( error, PRG_MAX_STR_LEN, _("Not enough vertices given (need at least 2).") );
			else
				snprintf ( error, PRG_MAX_STR_LEN, _("Not enough vertices given (need at least 3).") );
		}
		return ( FALSE );
	}

	/* we have a valid line or poly geometry: store it! */
	if ( part_id == 0 ) {
		/* this geometry does not yet exist: create a new line or poly type geom */
		unsigned int cur_free_geoms;
		if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
			cur_free_geoms = gs->free_lines;
		} else {
			cur_free_geoms = gs->free_polygons;
		}
		if ( cur_free_geoms < 1 ) {
			BOOLEAN out_of_mem = FALSE;
			if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
				/* get fresh memory if needed */
				void *new_mem = realloc ( (void*) gs->lines, sizeof (geom_store_line) * (gs->num_lines + GEOM_STORE_CHUNK_BIG) );
				if ( new_mem == NULL ) {
					out_of_mem = TRUE;
				} else {
					gs->lines = (geom_store_line*) new_mem;
					/* initialize with default values */
					int i;
					for ( i = gs->num_lines; i < (gs->num_lines+GEOM_STORE_CHUNK_BIG); i++ ) {
						gs->lines[i].geom_id = -1;
						gs->lines[i].is_empty = TRUE;
						gs->lines[i].has_errors = FALSE;
						gs->lines[i].is_3D = TRUE;
						gs->lines[i].num_parts = 0;
						gs->lines[i].parts = NULL;
						gs->lines[i].bbox_x1 = 0.0;
						gs->lines[i].bbox_y1 = 0.0;
						gs->lines[i].bbox_z1 = 0.0;
						gs->lines[i].bbox_x2 = 0.0;
						gs->lines[i].bbox_y2 = 0.0;
						gs->lines[i].bbox_z2 = 0.0;
						gs->lines[i].is_selected = FALSE;
						gs->lines[i].source = NULL;
						int j;
						for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
							gs->lines[i].atts[j] = NULL;
						}
					}
					gs->free_lines = GEOM_STORE_CHUNK_BIG;
				}
			} else {
				void *new_mem = realloc ( (void*) gs->polygons, sizeof (geom_store_polygon) * (gs->num_polygons + GEOM_STORE_CHUNK_BIG) );
				if ( new_mem == NULL ) {
					out_of_mem = TRUE;
				} else {
					gs->polygons = (geom_store_polygon*) new_mem;
					/* initialize with default values */
					int i;
					for ( i = gs->num_polygons; i < (gs->num_polygons+GEOM_STORE_CHUNK_BIG); i++ ) {
						gs->polygons[i].geom_id = -1;
						gs->polygons[i].is_empty = TRUE;
						gs->polygons[i].has_errors = FALSE;
						gs->polygons[i].is_3D = TRUE;
						gs->polygons[i].num_parts = 0;
						gs->polygons[i].parts = NULL;
						gs->polygons[i].bbox_x1 = 0.0;
						gs->polygons[i].bbox_y1 = 0.0;
						gs->polygons[i].bbox_z1 = 0.0;
						gs->polygons[i].bbox_x2 = 0.0;
						gs->polygons[i].bbox_y2 = 0.0;
						gs->polygons[i].bbox_z2 = 0.0;
						gs->polygons[i].is_selected = FALSE;
						gs->polygons[i].source = NULL;
						int j;
						for ( j = 0; j < PRG_MAX_FIELDS; j ++ ) {
							gs->polygons[i].atts[j] = NULL;
						}
					}					
					gs->free_polygons = GEOM_STORE_CHUNK_BIG;
				}
			}
			if ( out_of_mem == TRUE ) {
				if ( error != NULL ) {
					snprintf ( error, PRG_MAX_STR_LEN, _("Out of memory.") );
				}
				return ( FALSE );
			}
		}
		gs->is_empty = FALSE;
		if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
			gs->lines[cur_geom].parts = malloc ( sizeof (geom_part) * GEOM_STORE_CHUNK_SMALL);
			gs->lines[cur_geom].free_parts = GEOM_STORE_CHUNK_SMALL;
			gs->lines[cur_geom].num_parts = 1;
			gs->lines[cur_geom].geom_id = geom_id;
			gs->lines[cur_geom].length = 0.0;
			gs->free_lines --;
			gs->num_lines ++;
			/* copy attribute fields */
			int j = 0;
			while ( parser->fields[j] != NULL ) {
				if ( atts[j] == NULL ) {
					gs->lines[cur_geom].atts[j] = strdup ( "" );
				} else {
					gs->lines[cur_geom].atts[j] = strdup ( atts[j] );
				}
				j++;
			}
			gs->lines[cur_geom].atts[j] = NULL;
			/* copy source information */
			if ( source != NULL )
				gs->lines[cur_geom].source = strdup ( source );
			else
				gs->lines[cur_geom].source = NULL;
			gs->lines[cur_geom].line = line;
			/* make pseudo bounding box */
			gs->lines[cur_geom].bbox_x1 = X[0];
			gs->lines[cur_geom].bbox_x2 = X[0];
			gs->lines[cur_geom].bbox_y1 = Y[0];
			gs->lines[cur_geom].bbox_y2 = Y[0];
			if ( is_3D == TRUE ) {
				gs->lines[cur_geom].bbox_z1 = Z[0];
				gs->lines[cur_geom].bbox_z2 = Z[0];
			} else {
				gs->lines[cur_geom].bbox_z1 = 0.0;
				gs->lines[cur_geom].bbox_z2 = 0.0;
			}
		} else {
			gs->polygons[cur_geom].parts = malloc ( sizeof (geom_part) * GEOM_STORE_CHUNK_SMALL);
			gs->polygons[cur_geom].free_parts = GEOM_STORE_CHUNK_SMALL;
			gs->polygons[cur_geom].num_parts = 1;
			gs->polygons[cur_geom].geom_id = geom_id;
			gs->polygons[cur_geom].length = 0.0;
			gs->free_polygons --;
			gs->num_polygons ++;						
			/* copy attribute fields */
			int j = 0;
			while ( parser->fields[j] != NULL ) {
				if ( atts[j] == NULL ) {
					gs->polygons[cur_geom].atts[j] = strdup ( "" );
				} else {
					gs->polygons[cur_geom].atts[j] = strdup ( atts[j] );
				}
				j++;
			}
			gs->polygons[cur_geom].atts[j] = NULL;
			/* copy source information */
			if ( source != NULL )
				gs->polygons[cur_geom].source = strdup ( source );
			else
				gs->polygons[cur_geom].source = NULL;
			gs->polygons[cur_geom].line = line;
			/* make pseudo bounding box */
			gs->polygons[cur_geom].bbox_x1 = X[0];
			gs->polygons[cur_geom].bbox_x2 = X[0];
			gs->polygons[cur_geom].bbox_y1 = Y[0];
			gs->polygons[cur_geom].bbox_y2 = Y[0];
			if ( is_3D == TRUE ) {
				gs->polygons[cur_geom].bbox_z1 = Z[0];
				gs->polygons[cur_geom].bbox_z2 = Z[0];
			} else {
				gs->polygons[cur_geom].bbox_z1 = 0.0;
				gs->polygons[cur_geom].bbox_z2 = 0.0;
			}
		}
	} else { /* part_id > 0 */
		/* a geometry with this geom ID exists already: add as new part */
		/* get fresh memory if needed */
		BOOLEAN out_of_mem = FALSE;
		void *new_mem;
		if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
			if ( gs->lines[cur_geom].free_parts <= part_id ) {
				new_mem = realloc ( (void*) gs->lines[cur_geom].parts,
						sizeof (geom_part) * (GEOM_STORE_CHUNK_SMALL+gs->lines[cur_geom].num_parts) );
				if ( new_mem == NULL ) {
					out_of_mem = TRUE;
				} else {
					gs->lines[cur_geom].parts = (geom_part*) new_mem;
					gs->lines[cur_geom].free_parts = GEOM_STORE_CHUNK_SMALL;
				}
			}
			if ( out_of_mem == FALSE ) {
				part = part_id;
				gs->lines[cur_geom].num_parts ++;
			}
		} else {
			if ( gs->polygons[cur_geom].free_parts <= part_id ) {
				new_mem = realloc ( (void*) gs->polygons[cur_geom].parts,
						sizeof (geom_part) * (GEOM_STORE_CHUNK_SMALL+gs->polygons[cur_geom].num_parts) );
				if ( new_mem == NULL ) {
					out_of_mem = TRUE;
				} else {
					gs->polygons[cur_geom].parts = (geom_part*) new_mem;
					gs->polygons[cur_geom].free_parts = GEOM_STORE_CHUNK_SMALL;
				}
			}
			if ( out_of_mem == FALSE ) {
				part = part_id;
				gs->polygons[cur_geom].num_parts++;
			}
		}
		if ( out_of_mem == TRUE ) {
			if ( error != NULL ) {
				snprintf ( error, PRG_MAX_STR_LEN, _("Out of memory.") );
			}
			return ( FALSE );
		}
	}

	/* set common fields */
	if ( GEOM_TYPE == GEOM_TYPE_LINE ) {
		gs->lines[cur_geom].parts[part].num_vertices = num_vertices;
		gs->lines[cur_geom].parts[part].X = malloc ( sizeof (double) * num_vertices );
		gs->lines[cur_geom].parts[part].Y = malloc ( sizeof (double) * num_vertices );
		gs->lines[cur_geom].parts[part].Z = malloc ( sizeof (double) * num_vertices );
		/* copy vertices to store them in assigned part */
		int i;
		for ( i = 0; i < num_vertices; i ++ ) {
			gs->lines[cur_geom].parts[part].X[i] = X[i];
			gs->lines[cur_geom].parts[part].Y[i] = Y[i];
			if ( is_3D == TRUE )
				gs->lines[cur_geom].parts[part].Z[i] = Z[i];
			else
				gs->lines[cur_geom].parts[part].Z[i] = 0.0;
		}
		if ( part == 0 ) { /* only need to set this once, for the first part */
			gs->lines[cur_geom].is_3D = is_3D;
			gs->lines[cur_geom].is_empty = FALSE;
			gs->lines[cur_geom].is_selected = TRUE;
		}
		gs->lines[cur_geom].parts[part].has_label = FALSE;
		gs->lines[cur_geom].parts[part].label_x = 0.0;
		gs->lines[cur_geom].parts[part].label_y = 0.0;
		gs->lines[cur_geom].parts[part].is_hole = FALSE;
		gs->lines[cur_geom].parts[part].is_undershoot_first = FALSE;
		gs->lines[cur_geom].parts[part].dist_undershoot_first = -1.0;
		gs->lines[cur_geom].parts[part].x_undershoot_first = 0.0;
		gs->lines[cur_geom].parts[part].y_undershoot_first = 0.0;
		gs->lines[cur_geom].parts[part].is_undershoot_last = FALSE;
		gs->lines[cur_geom].parts[part].dist_undershoot_last = -1.0;
		gs->lines[cur_geom].parts[part].x_undershoot_last = 0.0;
		gs->lines[cur_geom].parts[part].y_undershoot_last = 0.0;
		gs->lines[cur_geom].parts[part].err_self_intersects = 0;
		gs->lines[cur_geom].parts[part].is_empty = FALSE;
		gs->lines[cur_geom].free_parts --;
		/* check all line segments for self-intersections */
		for ( i = 1; i < (num_vertices-1); i ++ ) {
			int j;
			BOOLEAN result = TRUE;
			while ( (result == TRUE) ) {
				for ( j = 0; j < (num_vertices-2); j ++ ) {
					if ( i != j ) { /* make sure that we only compare different segments */
						/* current segment to check with */
						double p0_x = gs->lines[cur_geom].parts[part].X[i];
						double p0_y = gs->lines[cur_geom].parts[part].Y[i];
						double p1_x = gs->lines[cur_geom].parts[part].X[i+1];
						double p1_y = gs->lines[cur_geom].parts[part].Y[i+1];
						/* preceding segment to check against */
						double p2_x = gs->lines[cur_geom].parts[part].X[j];
						double p2_y = gs->lines[cur_geom].parts[part].Y[j];
						double p3_x = gs->lines[cur_geom].parts[part].X[j+1];
						double p3_y = gs->lines[cur_geom].parts[part].Y[j+1];
						double i_x;
						double i_y;
						result = geom_tools_line_self_intersection_2D (p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, &i_x, &i_y);
						if (result == TRUE ) {
							err_show (ERR_WARN, _("\nSelf-intersection in line geometry at %.3f/%.3f (geometry #%i, part #%i).\n"),
									i_x, i_y, cur_geom, part);
							/* mark errors in geometry */
							gs->lines[cur_geom].has_errors = TRUE;
							gs->lines[cur_geom].parts[part].err_self_intersects ++;
							/* interpolate Z on checked segment at intersection point */
							double p0_z = gs->lines[cur_geom].parts[part].Z[i];
							double p1_z = gs->lines[cur_geom].parts[part].Z[i+1];
							double z = geom_tools_interpolate_z(p0_x, p0_y, p0_z, p1_x, p1_y, p1_z, i_x, i_y);
							/* add intersection point to geometry part as new vertex */
							int pos = i+1;
							double x = i_x;
							double y = i_y;
							geom_part* new_part = geom_tools_part_add_vertex ( &gs->lines[cur_geom].parts[part], pos, x, y, z );
							if ( new_part != NULL ) {
								/* swap old part for new part with added self-intersection vertex */
								gs->lines[cur_geom].parts[part].num_vertices = new_part->num_vertices;
								gs->lines[cur_geom].parts[part].X = malloc ( sizeof (double ) * new_part->num_vertices );
								gs->lines[cur_geom].parts[part].Y = malloc ( sizeof (double ) * new_part->num_vertices );
								gs->lines[cur_geom].parts[part].Z = malloc ( sizeof (double ) * new_part->num_vertices );
								for ( j = 0; j < new_part->num_vertices; j ++ ) {
									gs->lines[cur_geom].parts[part].X[j] = new_part->X[j];
									gs->lines[cur_geom].parts[part].Y[j] = new_part->Y[j];
									gs->lines[cur_geom].parts[part].Z[j] = new_part->Z[j];
								}
								geom_tools_part_destroy ( new_part );
								free ( new_part );
							}
						}
					}
				}
			}
		}
		/* compute real bounding box from given data (excl. any self-intersection points) */
		for ( i=0; i < num_vertices; i++ ) {
			if ( X[i] < gs->lines[cur_geom].bbox_x1 )
				gs->lines[cur_geom].bbox_x1 = X[i];
			if ( X[i] > gs->lines[cur_geom].bbox_x2 )
				gs->lines[cur_geom].bbox_x2 = X[i];
			if ( Y[i] < gs->lines[cur_geom].bbox_y1 )
				gs->lines[cur_geom].bbox_y1 = Y[i];
			if ( Y[i] > gs->lines[cur_geom].bbox_y2 )
				gs->lines[cur_geom].bbox_y2 = Y[i];
			if ( is_3D == TRUE ) {
				if ( Z[i] < gs->lines[cur_geom].bbox_z1 )
					gs->lines[cur_geom].bbox_z1 = Z[i];
				if ( Z[i] > gs->lines[cur_geom].bbox_z2 )
					gs->lines[cur_geom].bbox_z2 = Z[i];
			}
		}
	} else {
		gs->polygons[cur_geom].parts[part].num_vertices = num_vertices;
		gs->polygons[cur_geom].parts[part].X = malloc ( sizeof (double) * num_vertices );
		gs->polygons[cur_geom].parts[part].Y = malloc ( sizeof (double) * num_vertices );
		gs->polygons[cur_geom].parts[part].Z = malloc ( sizeof (double) * num_vertices );
		/* copy vertices to store them in assigned part */
		int i;
		for ( i = 0; i < num_vertices; i ++ ) {
			gs->polygons[cur_geom].parts[part].X[i] = X[i];
			gs->polygons[cur_geom].parts[part].Y[i] = Y[i];
			if ( is_3D == TRUE )
				gs->polygons[cur_geom].parts[part].Z[i] = Z[i];
			else
				gs->polygons[cur_geom].parts[part].Z[i] = 0.0;
		}
		if ( part == 0 ) { /* only need to set this once, for the first part */
			gs->polygons[cur_geom].is_3D = is_3D;
			gs->polygons[cur_geom].is_empty = FALSE;
			gs->polygons[cur_geom].is_selected = TRUE;
		}
		gs->polygons[cur_geom].parts[part].has_label = FALSE;
		gs->polygons[cur_geom].parts[part].label_x = 0.0;
		gs->polygons[cur_geom].parts[part].label_y = 0.0;
		gs->polygons[cur_geom].parts[part].is_hole =
				geom_tools_part_in_poly_2D ( &gs->polygons[cur_geom].parts[part],
						&gs->polygons[cur_geom] );
		gs->polygons[cur_geom].parts[part].err_self_intersects = 0;
		gs->polygons[cur_geom].parts[part].is_empty = FALSE;
		gs->polygons[cur_geom].free_parts --;
		/* check all boundary segments for self-intersections */
		for ( i = 1; i < (num_vertices-1); i ++ ) {
			int j;
			BOOLEAN result = TRUE;
			while ( (result == TRUE) ) {
				for ( j = 0; j < (num_vertices-2); j ++ ) {
					if ( i != j ) { /* make sure that we only compare different segments */
						/* current segment to check with */
						double p0_x = gs->polygons[cur_geom].parts[part].X[i];
						double p0_y = gs->polygons[cur_geom].parts[part].Y[i];
						double p1_x = gs->polygons[cur_geom].parts[part].X[i+1];
						double p1_y = gs->polygons[cur_geom].parts[part].Y[i+1];
						/* preceding segment to check against */
						double p2_x = gs->polygons[cur_geom].parts[part].X[j];
						double p2_y = gs->polygons[cur_geom].parts[part].Y[j];
						double p3_x = gs->polygons[cur_geom].parts[part].X[j+1];
						double p3_y = gs->polygons[cur_geom].parts[part].Y[j+1];
						double i_x;
						double i_y;
						result = geom_tools_line_self_intersection_2D (p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, &i_x, &i_y);
						if ( result == TRUE ) {
							err_show (ERR_WARN, _("\nSelf-intersection in polygon geometry at %.3f/%.3f (geometry #%i, part #%i).\n"),
									i_x, i_y, cur_geom, part);
							/* mark errors in geometry */
							gs->polygons[cur_geom].has_errors = TRUE;
							gs->polygons[cur_geom].parts[part].err_self_intersects ++;
							/* interpolate Z on checked segment at intersection point */
							double p0_z = gs->polygons[cur_geom].parts[part].Z[i];
							double p1_z = gs->polygons[cur_geom].parts[part].Z[i+1];
							double z = geom_tools_interpolate_z(p0_x, p0_y, p0_z, p1_x, p1_y, p1_z, i_x, i_y);
							/* add intersection point to geometry part as new vertex */
							int pos = i+1;
							double x = i_x;
							double y = i_y;
							geom_part* new_part = geom_tools_part_add_vertex ( &gs->polygons[cur_geom].parts[part], pos, x, y, z );
							if ( new_part != NULL ) {
								/* swap old part for new part with added self-intersection vertex */
								gs->polygons[cur_geom].parts[part].num_vertices = new_part->num_vertices;
								gs->polygons[cur_geom].parts[part].X = malloc ( sizeof (double ) * new_part->num_vertices );
								gs->polygons[cur_geom].parts[part].Y = malloc ( sizeof (double ) * new_part->num_vertices );
								gs->polygons[cur_geom].parts[part].Z = malloc ( sizeof (double ) * new_part->num_vertices );
								for ( j = 0; j < new_part->num_vertices; j ++ ) {
									gs->polygons[cur_geom].parts[part].X[j] = new_part->X[j];
									gs->polygons[cur_geom].parts[part].Y[j] = new_part->Y[j];
									gs->polygons[cur_geom].parts[part].Z[j] = new_part->Z[j];
								}
								geom_tools_part_destroy ( new_part );
								free ( new_part );
							}
						}
					}
				}
			}
		}

		/* compute real bounding box from given data (excl. any self-intersection points) */
		int j;
		for ( j=0; j < num_vertices; j++ ) {
			if ( X[j] < gs->polygons[cur_geom].bbox_x1 )
				gs->polygons[cur_geom].bbox_x1 = X[j];
			if ( X[j] > gs->polygons[cur_geom].bbox_x2 )
				gs->polygons[cur_geom].bbox_x2 = X[j];
			if ( Y[j] < gs->polygons[cur_geom].bbox_y1 )
				gs->polygons[cur_geom].bbox_y1 = Y[j];
			if ( Y[j] > gs->polygons[cur_geom].bbox_y2 )
				gs->polygons[cur_geom].bbox_y2 = Y[j];
			if ( is_3D == TRUE ) {
				if ( Z[j] < gs->polygons[cur_geom].bbox_z1 )
					gs->polygons[cur_geom].bbox_z1 = Z[j];
				if ( Z[j] > gs->polygons[cur_geom].bbox_z2 )
					gs->polygons[cur_geom].bbox_z2 = Z[j];
			}
		}
	}

	//TODO: CONTINUE: SKIP (SOME) TOPOLOGY TESTS FOR GEOMETRIES THAT HAVE SELF-INTERSECTION ERRORS!

	/* Add label point? */
	if ( opts != NULL && opts->label_field != NULL ) {
		/* Place (2D) label according to geometry type */
		/* LINES */
		if ( GEOM_TYPE == GEOM_TYPE_LINE && gs->lines[cur_geom].is_empty == FALSE ) {
			if ( opts->label_mode_line == OPTIONS_LABEL_MODE_CENTER ) {
				/* Center = Place one label per (non-empty) part, at half distance between first
				 * and last vertex of that part. */
				int p;
				for ( p = 0; p < gs->lines[cur_geom].num_parts; p ++ ) {
					if ( gs->lines[cur_geom].parts[p].is_empty == FALSE ) {
						/* Get total length of this polyline part. */
						double total_length = 0.0;
						double x1 = 0.0;
						double x2 = 0.0;
						double y1 = 0.0;
						double y2 = 0.0;
						int v;
						for ( v = 0; v < gs->lines[cur_geom].parts[p].num_vertices-1; v ++ ) {
							x1 = gs->lines[cur_geom].parts[p].X[v];
							y1 = gs->lines[cur_geom].parts[p].Y[v];
							x2 = gs->lines[cur_geom].parts[p].X[v+1];
							y2 = gs->lines[cur_geom].parts[p].Y[v+1];
							double dist = sqrt ( pow ( ( x2 - x1 ), 2 ) + pow ( ( y2 - y1 ), 2 ) );
							total_length += dist;
						}
						if ( total_length > 0.0 ) {
							/* find the last vertex that is <= midpoint distance from first vertex */
							double cur_length = 0.0;
							double mid_distance = total_length / 2.0;
							int p1 = 0; /* last vertex before geometric midpoint */
							int p2 = 1; /* first vertex after geometric midpoint */
							/* iterate until we have a vertex that is further than mean distance from first vertex */
							for ( v = 0; v < gs->lines[cur_geom].parts[p].num_vertices-1 && cur_length < mid_distance; v ++ ) {
								x1 = gs->lines[cur_geom].parts[p].X[v];
								y1 = gs->lines[cur_geom].parts[p].Y[v];
								x2 = gs->lines[cur_geom].parts[p].X[v+1];
								y2 = gs->lines[cur_geom].parts[p].Y[v+1];
								p1 = v; /* track vertices that enclose midpoint */
								p2 = v+1;
								double dist = sqrt ( pow ( ( x2 - x1 ), 2 ) + pow ( ( y2 - y1 ), 2 ) );
								cur_length += dist;
							}
							if ( cur_length > 0.0 ) {
								if ( cur_length == mid_distance ) {
									/* Extreme case: vertex lies exactly at midpoint distance. */
									gs->lines[cur_geom].parts[p].label_x = x2;
									gs->lines[cur_geom].parts[p].label_y = y2;
									gs->lines[cur_geom].parts[p].has_label = TRUE;
								} else {
									/* Common case: We find the true midpoint of the polyline between the two vertices */
									/* 1. Find distance from first vertex to last vertex lying before midpoint */
									double dist1 = 0.0;
									for ( v = 0; v < p1; v ++ ) {
										x1 = gs->lines[cur_geom].parts[p].X[v];
										y1 = gs->lines[cur_geom].parts[p].Y[v];
										x2 = gs->lines[cur_geom].parts[p].X[v+1];
										y2 = gs->lines[cur_geom].parts[p].Y[v+1];
										double dist = sqrt ( pow ( ( x2 - x1 ), 2 ) + pow ( ( y2 - y1 ), 2 ) );
										dist1 += dist;
									}
									/* 2. Find distance from first vertex to first vertex lying after midpoint */
									double dist2 = 0.0;
									for ( v = 0; v < p2; v ++ ) {
										x1 = gs->lines[cur_geom].parts[p].X[v];
										y1 = gs->lines[cur_geom].parts[p].Y[v];
										x2 = gs->lines[cur_geom].parts[p].X[v+1];
										y2 = gs->lines[cur_geom].parts[p].Y[v+1];
										double dist = sqrt ( pow ( ( x2 - x1 ), 2 ) + pow ( ( y2 - y1 ), 2 ) );
										dist2 += dist;
									}
									/* 3. Calculate difference between each enclosing vertex and mid distance */
									double diff1 = fabs(mid_distance-dist1);
									double diff2 = fabs(mid_distance-dist2);
									/* 4. Normalize differences and use as weight */
									double diff_total = diff1 + diff2;
									double t = diff1/diff_total;
									/* Calculate weighted average distance between enclosing vertices. */
									gs->lines[cur_geom].parts[p].label_x = ((1.0-t)*x1)+(t*x2);
									gs->lines[cur_geom].parts[p].label_y = ((1.0-t)*y1)+(t*y2);
									gs->lines[cur_geom].parts[p].has_label = TRUE;
								}
							}
						}
					}
				}
			}
			if ( opts->label_mode_line == OPTIONS_LABEL_MODE_FIRST ) {
				/* First = Place label on first vertex of every (non-empty) part. */
				int p;
				for ( p = 0; p < gs->lines[cur_geom].num_parts; p ++ ) {
					if ( gs->lines[cur_geom].parts[p].is_empty == FALSE ) {
						gs->lines[cur_geom].parts[p].label_x = gs->lines[cur_geom].parts[p].X[0];
						gs->lines[cur_geom].parts[p].label_y = gs->lines[cur_geom].parts[p].Y[0];
						gs->lines[cur_geom].parts[p].has_label = TRUE;
					}
				}
			}
			if ( opts->label_mode_line == OPTIONS_LABEL_MODE_LAST ) {
				/* Last = Place label on last vertex of every (non-empty) part. */
				int p;
				for ( p = 0; p < gs->lines[cur_geom].num_parts; p ++ ) {
					if ( gs->lines[cur_geom].parts[p].is_empty == FALSE ) {
						int tgt_vertex = gs->lines[cur_geom].parts[p].num_vertices - 1; /* last vertex of this part */
						gs->lines[cur_geom].parts[p].label_x = gs->lines[cur_geom].parts[p].X[tgt_vertex];
						gs->lines[cur_geom].parts[p].label_y = gs->lines[cur_geom].parts[p].Y[tgt_vertex];
						gs->lines[cur_geom].parts[p].has_label = TRUE;
					}
				}
			}
		}
		/* POLYGONS */
		if ( GEOM_TYPE == GEOM_TYPE_POLY && gs->polygons[cur_geom].is_empty == FALSE ) {
			if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_CENTER ) {
				/* Center = Place label at geometric center of every part that is not a hole. */
				int p;
				for ( p = 0; p < gs->polygons[cur_geom].num_parts; p ++ ) {
					if ( 	gs->polygons[cur_geom].parts[p].is_empty == FALSE &&
							gs->polygons[cur_geom].parts[p].is_hole == FALSE )
					{
						double tgt_x = 0.0;
						double tgt_y = 0.0;
						/* Note: Last vertex is not included, since it is the same as the first! */
						int v;
						for ( v = 0; v < gs->polygons[cur_geom].parts[p].num_vertices-1; v ++ ) {
							tgt_x += gs->polygons[cur_geom].parts[p].X[v];
							tgt_y += gs->polygons[cur_geom].parts[p].Y[v];
						}
						if ( tgt_x > 0.0 && tgt_y > 0.0 && gs->polygons[cur_geom].parts[p].num_vertices > 0 ) {
							tgt_x = tgt_x / (double) (gs->polygons[cur_geom].parts[p].num_vertices-1);
							tgt_y = tgt_y / (double) (gs->polygons[cur_geom].parts[p].num_vertices-1);
							gs->polygons[cur_geom].parts[p].label_x = tgt_x;
							gs->polygons[cur_geom].parts[p].label_y = tgt_y;
							gs->polygons[cur_geom].parts[p].has_label = TRUE;
						}
					}
				}
			}
			if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_FIRST ) {
				/* First = Place label on first vertex of every (non-empty) part that is not a hole. */
				int p;
				for ( p = 0; p < gs->polygons[cur_geom].num_parts; p ++ ) {
					if ( 	gs->polygons[cur_geom].parts[p].is_empty == FALSE &&
							gs->polygons[cur_geom].parts[p].is_hole == FALSE )
					{
						gs->polygons[cur_geom].parts[p].label_x = gs->polygons[cur_geom].parts[p].X[0];
						gs->polygons[cur_geom].parts[p].label_y = gs->polygons[cur_geom].parts[p].Y[0];
						gs->polygons[cur_geom].parts[p].has_label = TRUE;
					}
				}
			}
			if ( opts->label_mode_poly == OPTIONS_LABEL_MODE_LAST ) {
				/* First = Place label on first vertex of every (non-empty) part that is not a hole. */
				int p;
				for ( p = 0; p < gs->polygons[cur_geom].num_parts; p ++ ) {
					if ( 	gs->polygons[cur_geom].parts[p].is_empty == FALSE &&
							gs->polygons[cur_geom].parts[p].is_hole == FALSE )
					{
						int tgt_vertex = gs->polygons[cur_geom].parts[p].num_vertices-1; /* last vertex of this part */
						gs->polygons[cur_geom].parts[p].label_x = gs->polygons[cur_geom].parts[p].X[tgt_vertex];
						gs->polygons[cur_geom].parts[p].label_y = gs->polygons[cur_geom].parts[p].Y[tgt_vertex];
						gs->polygons[cur_geom].parts[p].has_label = TRUE;
					}
				}
			}
		}
	}

	/* compute updated length/circumference of geometry, including all parts */
	double dist = 0.0;
	int i;
	for ( i = 1; i < num_vertices; i ++ ) {
		if ( is_3D == TRUE )
			dist = (sqrt(pow((X[i-1] - X[i]), 2) + pow((Y[i-1] - Y[i]), 2) + pow((Z[i-1] - Z[i]), 2)));
		else
			dist = (sqrt(pow((X[i-1] - X[i]), 2) + pow((Y[i-1] - Y[i]), 2)));
		if ( GEOM_TYPE == GEOM_TYPE_LINE )
			gs->lines[cur_geom].length += dist;
		else {
			gs->polygons[cur_geom].length += dist;
		}
	}

	return ( TRUE );
}




/*
 * Adds a point to a geometry store.
 */
BOOLEAN geom_store_add_point ( 	geom_store *gs, parser_desc *parser,
		unsigned int geom_id,
		double X, double Y, double Z,
		unsigned int part_id,
		char **atts, char *source, unsigned int line,
		BOOLEAN is_3D, char *error, options *opts )
{
	return ( geom_store_add (gs, parser, geom_id, 1, GEOM_TYPE_POINT, &X, &Y, &Z, part_id, atts, source, line, is_3D, error, opts) );
}




/*
 * Adds a "raw" (i.e. a vertex of any geometry) as a point to a geometry store.
 */
BOOLEAN geom_store_add_point_raw ( 	geom_store *gs, parser_desc *parser,
		unsigned int geom_id,
		double X, double Y, double Z,
		unsigned int part_id,
		char **atts, char *source, unsigned int line,
		BOOLEAN is_3D, char *error, options *opts )
{

	return ( geom_store_add (gs, parser, geom_id, 1, GEOM_TYPE_POINT_RAW, &X, &Y, &Z, part_id, atts, source, line, is_3D, error, opts) );
}




/*
 * Adds a (multi-)line (part) to a geometry store.
 */
BOOLEAN geom_store_add_line ( 	geom_store *gs, parser_desc *parser,
		unsigned int geom_id, unsigned int num_vertices,
		double *X, double *Y, double *Z,
		unsigned int part_id,
		char **atts, char *source, unsigned int line,
		BOOLEAN is_3D, char *error, options *opts )
{
	return ( geom_store_add (gs, parser, geom_id, num_vertices, GEOM_TYPE_LINE, X, Y, Z, part_id, atts, source, line, is_3D, error, opts) );
}




/*
 * Adds a (multi-)polygon (part) to a geometry store.
 */
BOOLEAN geom_store_add_poly ( 	geom_store *gs, parser_desc *parser,
		unsigned int geom_id, unsigned int num_vertices,
		double *X, double *Y, double *Z,
		unsigned int part_id,
		char **atts, char *source, unsigned int line,
		BOOLEAN is_3D, char *error, options *opts )
{
	return ( geom_store_add (gs, parser, geom_id, num_vertices, GEOM_TYPE_POLY, X, Y, Z, part_id, atts, source, line, is_3D, error, opts) );
}




/*
 * DEBUG:
 * Dump contents of geometry store to the screen.
 */
void geom_store_print 	( geom_store *gs, BOOLEAN print_points )
{
	unsigned int i,j,k;

	if ( gs->is_empty == TRUE ) {
		fprintf ( stderr, "* GEOM STORE IS EMPTY. *\n" );
		return;
	}

	fprintf ( stderr, "* GEOM STORE CONTENTS BELOW *\n" );

	fprintf ( stderr, "\tPOINTS: %u\n", gs->num_points );
	if ( print_points == TRUE ) {
		for ( i=0; i < gs->num_points; i++ ) {
			fprintf (stderr, "\t\t\t%u: {%.3f|%.3f|%.3f}\n", i+1,
					gs->points[i].X,
					gs->points[i].Y,
					gs->points[i].Z);
		}

	}

	fprintf ( stderr, "\tPOINTS (RAW): %u\n", gs->num_points_raw );
	if ( print_points == TRUE ) {

		for ( i=0; i < gs->num_points_raw; i++ ) {
			fprintf (stderr, "\t\t\t%u: {%.3f|%.3f|%.3f}\n", i+1,
					gs->points_raw[i].X,
					gs->points_raw[i].Y,
					gs->points_raw[i].Z);
		}
	}

	fprintf ( stderr, "\tLINES: %u\n", gs->num_lines );
	for ( i=0; i < gs->num_lines; i++ ) {
		fprintf ( stderr, "\n\t\tLine with geom ID %u has length %.3f and %u part(s).\n",
				gs->lines[i].geom_id, gs->lines[i].length, gs->lines[i].num_parts );
		for ( j=0; j < gs->lines[i].num_parts; j++ ) {
			fprintf (stderr, "\t\tPart %u has %u vertices.\n", j, gs->lines[i].parts[j].num_vertices);
			for ( k=0; k<gs->lines[i].parts[j].num_vertices; k ++) {
				fprintf (stderr, "\t\t\t%u: {%.3f|%.3f|%.3f}\n", k+1,
						gs->lines[i].parts[j].X[k],
						gs->lines[i].parts[j].Y[k],
						gs->lines[i].parts[j].Z[k]);
			}
		}
		fprintf (stderr, "\t\tBounding box: {%.3f|%.3f|%.3f} to {%.3f|%.3f|%.3f}.\n",
				gs->lines[i].bbox_x1, gs->lines[i].bbox_y1,
				gs->lines[i].bbox_z1, gs->lines[i].bbox_x2,
				gs->lines[i].bbox_y2, gs->lines[i].bbox_z2);
	}

	fprintf ( stderr, "\tPOLYGONS: %u\n", gs->num_polygons );
	for ( i=0; i < gs->num_polygons; i++ ) {
		fprintf ( stderr, "\n\t\tPolygon with geom ID %u has circumf. %.3f and %u part(s).\n",
				gs->polygons[i].geom_id, gs->polygons[i].length, gs->polygons[i].num_parts );
		for ( j=0; j < gs->polygons[i].num_parts; j++ ) {
			fprintf (stderr, "\t\tPart %u has %u vertices.\n", j, gs->polygons[i].parts[j].num_vertices);
			if ( gs->polygons[i].parts[j].is_hole == TRUE )
				fprintf (stderr, "\t\tThis part is a HOLE (inner ring).\n" );
			for ( k=0; k<gs->polygons[i].parts[j].num_vertices; k ++) {
				fprintf (stderr, "\t\t\t%u: {%.3f|%.3f|%.3f}\n", k+1,
						gs->polygons[i].parts[j].X[k],
						gs->polygons[i].parts[j].Y[k],
						gs->polygons[i].parts[j].Z[k]);
			}
		}
		fprintf (stderr, "\t\tBounding box: {%.3f|%.3f|%.3f} to {%.3f|%.3f|%.3f}.\n",
				gs->polygons[i].bbox_x1, gs->polygons[i].bbox_y1,
				gs->polygons[i].bbox_z1, gs->polygons[i].bbox_x2,
				gs->polygons[i].bbox_y2, gs->polygons[i].bbox_z2);
	}

}




/*
 * Builds points, lines and polygons from the records in the
 * "raw" data store(s) "ds". Saves the result in the geometry store "gs".
 *
 * Returns number of geometry build errors.
 */
int geom_store_build ( 	geom_store *gs, parser_data_store **ds,
		parser_desc *parser, options *opts)
{
	double *X,*Y,*Z;
	parser_record *rec, *vertex, *first;
	unsigned int m, i, j, k;
	unsigned int num_errors;
	char error[PRG_MAX_STR_LEN];


	/* write raw vertices to separate layer? */
	BOOLEAN dump_raw = opts->dump_raw;

	/* go trough all data stores and extract all valid geometries */
	num_errors = 0;
	/* go through all input files and convert associated data store contents to geometries */
	for ( m = 0; m < opts->num_input; m ++ ) {
		/* check if there are any geometries */
		if ( ds[m]->num_points + ds[m]->num_lines + ds[m]->num_polygons < 1 )  {
			err_show (ERR_NOTE,"");
			err_show (ERR_WARN, _("\nNo valid geometries found in '%s'. No associated output produced."), ds[m]->input );
		}
		else {
			/* go through records in data store and write them to output */
			for ( i = 0; i < ds[m]->num_records; i++) {
				if ( 	ds[m]->records[i].is_empty == FALSE && ds[m]->records[i].is_valid == TRUE &&
						ds[m]->records[i].geom_type != GEOM_TYPE_NONE && ds[m]->records[i].written_out == FALSE )
				{
					rec = &ds[m]->records[i];

					if ( dump_raw == TRUE ) {
						/* If raw point data is to be dumped, then we take any kind
						 * of vertex and write it out as a simple point.
						 */
						/* RAW POINTS */
						if ( rec->geom_type == GEOM_TYPE_POINT ) {
							if ( geom_store_add_point_raw ( gs, parser, rec->geom_id, rec->x, rec->y, rec->z,
									rec->part_id,
									rec->contents, ds[m]->input, rec->line,
									TRUE, error, opts )
									== FALSE ) {
								err_show (ERR_NOTE,"");
								err_show (ERR_WARN, _("\nCould not store point from '%s', line %i.\nReason: %s"),
										ds[m]->input, rec->line, error );
								num_errors ++;
							} else {
								gs->is_empty = FALSE;
							}
						}
						/* RAW LINE VERTICES (AS POINTS) */
						if ( rec->geom_type == GEOM_TYPE_LINE ) {
							j = i;
							vertex = rec;
							while ( j < ds[m]->num_records &&
									vertex->geom_id == rec->geom_id )
							{
								if ( geom_store_add_point_raw ( gs, parser, rec->geom_id, vertex->x, vertex->y, vertex->z,
										vertex->part_id,
										vertex->contents, ds[m]->input, vertex->line,
										TRUE, error, opts )
										== FALSE ) {
									err_show (ERR_NOTE,"");
									err_show (ERR_WARN, _("\nCould not store line vertex from '%s', line %i.\nReason: %s"),
											ds[m]->input, vertex->line, error );
									num_errors ++;
								} else {
									gs->is_empty = FALSE;
								}
								j ++;
								vertex ++;
							}
						}
						/* RAW POLYGON VERTICES (AS POINTS) */
						if ( rec->geom_type == GEOM_TYPE_POLY ) {
							j = i;
							vertex = rec;
							while ( j < ds[m]->num_records &&
									vertex->geom_id == rec->geom_id )
							{
								if ( geom_store_add_point_raw ( gs, parser, rec->geom_id, vertex->x, vertex->y, vertex->z,
										vertex->part_id,
										vertex->contents, ds[m]->input, vertex->line,
										TRUE, error, opts )
										== FALSE ) {
									err_show (ERR_NOTE,"");
									err_show (ERR_WARN, _("\nCould not store polygon vertex from '%s', line %i.\nReason: %s"),
											ds[m]->input, vertex->line, error );
									num_errors ++;
								} else {
									gs->is_empty = FALSE;
								}
								j ++;
								vertex ++;
							}
						}
					}

					/* POINTS */
					if ( rec->geom_type == GEOM_TYPE_POINT ) {
						/* write geometry data */
						if ( geom_store_add_point ( gs, parser, rec->geom_id, rec->x, rec->y, rec->z,
								rec->part_id,
								rec->contents, ds[m]->input, rec->line,
								TRUE, error, opts )
								== FALSE ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nCould not store point geometry from '%s', line %i.\nReason: %s"),
									ds[m]->input, rec->line, error );
							num_errors ++;
						} else {
							gs->is_empty = FALSE;
						}
						rec->written_out = TRUE;
					}

					/* LINES */
					if ( rec->geom_type == GEOM_TYPE_LINE ) {
						/* pass 1: get number of vertices */
						j = i;
						k = 0;
						vertex = rec;
						while ( j < ds[m]->num_records &&
								vertex->geom_id == rec->geom_id &&
								vertex->part_id == rec->part_id )
						{
							if ( vertex->is_valid == TRUE )
								k ++;
							j ++;
							vertex ++;
						}
						/* pass 2: convert vertices to array of doubles */
						X = malloc ( sizeof (double) * k ); Y = malloc ( sizeof (double) * k ); Z = malloc ( sizeof (double) * k );
						j = i;
						k = 0;
						vertex = rec;
						while ( j < ds[m]->num_records &&
								vertex->geom_id == rec->geom_id &&
								vertex->part_id == rec->part_id )
						{
							if ( vertex->is_valid == TRUE ) {
								X[k] = vertex->x; Y[k] = vertex->y; Z[k] = vertex->z;
								vertex->written_out = TRUE;
								k ++;
							}
							j ++;
							vertex ++;
						}
						if ( geom_store_add_line ( gs, parser, rec->geom_id, k, X, Y, Z,
								rec->part_id,
								rec->contents, ds[m]->input, rec->line,
								TRUE, error, opts )
								== FALSE ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nCould not store line geometry from '%s', line %i.\nReason: %s"),
									ds[m]->input, rec->line, error );
							num_errors ++;
						} else {
							gs->is_empty = FALSE;
						}
						free ( X ); free ( Y ); free ( Z );
					}

					/* POLYGONS */
					if ( rec->geom_type == GEOM_TYPE_POLY ) {
						/* pass 1: get number of vertices */
						j = i;
						k = 0;
						vertex = rec;
						while ( j < ds[m]->num_records &&
								vertex->geom_id == rec->geom_id &&
								vertex->part_id == rec->part_id )
						{
							if ( vertex->is_valid == TRUE )
								k ++;
							j ++;
							vertex ++;
						}
						/* pass 2: convert vertices to array of doubles */
						X = malloc ( sizeof (double) * (k+1) ); Y = malloc ( sizeof (double) * (k+1) ); Z = malloc ( sizeof (double) * (k+1) );
						j = i;
						k = 0;
						vertex = rec;
						first = NULL;
						while ( j < ds[m]->num_records &&
								vertex->geom_id == rec->geom_id &&
								vertex->part_id == rec->part_id )
						{
							if ( first == NULL )
								first = vertex; /* remember first vertex */
							if ( vertex->is_valid == TRUE ) {
								X[k] = vertex->x; Y[k] = vertex->y; Z[k] = vertex->z;
								vertex->written_out = TRUE;
								k ++;
							}
							j ++;
							vertex ++;
						}
						/* original Shapefile specs demand that last vertex = first vertex */
						X[k] = first->x; Y[k] = first->y; Z[k] = first->z;
						if ( geom_store_add_poly ( gs, parser, rec->geom_id, k+1, X, Y, Z,
								rec->part_id,
								rec->contents, ds[m]->input, rec->line,
								TRUE, error, opts )
								== FALSE ) {
							err_show (ERR_NOTE,"");
							err_show (ERR_WARN, _("\nCould not store polygon geometry from '%s', line %i.\nReason: %s"),
									ds[m]->input, rec->line, error );
							num_errors ++;
						} else {
							gs->is_empty = FALSE;
						}
						free ( X ); free ( Y ); free ( Z );
					}
				}

			} /* END (build point, line and polygon geometries */

		}
	}

	return ( num_errors );
}


void geom_store_destroy ( geom_store *gs )
{
	unsigned int i,j;


	geom_store_free_paths (gs);

	/* free points */
	for ( i=0; i < gs->num_points; i++ ) {
		j=0;
		while ( gs->points[i].atts[j] != NULL ) {
			free ( gs->points[i].atts[j] );
			j++;
		}
		if ( gs->points[i].source != NULL ) {
			free ( gs->points[i].source );
		}
	}
	free ( gs->points );

	/* free raw vertices */
	for ( i=0; i < gs->num_points_raw; i++ ) {
		j=0;
		while ( gs->points_raw[i].atts[j] != NULL ) {
			free ( gs->points_raw[i].atts[j] );
			j++;
		}
		if ( gs->points_raw[i].source != NULL ) {
			free ( gs->points_raw[i].source );
		}
	}
	free ( gs->points_raw );

	/* free lines */
	for ( i=0; i < gs->num_lines; i++ ) {
		j=0;
		while ( gs->lines[i].atts[j] != NULL ) {
			free ( gs->lines[i].atts[j] );
			j++;
		}
		if ( gs->lines[i].source != NULL ) {
			free ( gs->lines[i].source );
		}
		if ( gs->lines[i].parts != NULL ) {
			for ( j=0; j < gs->lines[i].num_parts; j ++ ) {
				geom_tools_part_destroy ( &gs->lines[i].parts[j] );
			}
			free ( gs->lines[i].parts );
		}
	}
	free ( gs->lines );

	/* free polygons */
	for ( i=0; i < gs->num_polygons; i++ ) {
		j=0;
		while ( gs->polygons[i].atts[j] != NULL ) {
			free ( gs->polygons[i].atts[j] );
			j++;
		}
		if ( gs->polygons[i].source != NULL ) {
			free ( gs->polygons[i].source );
		}
		if ( gs->polygons[i].parts != NULL ) {
			for ( j=0; j < gs->polygons[i].num_parts; j ++ ) {
				geom_tools_part_destroy ( &gs->polygons[i].parts[j] );
			}
			free ( gs->polygons[i].parts );
		}
	}
	free ( gs->polygons );

	/* free intersections lists */
	if ( gs->lines_intersections != NULL ) {
		if ( gs->lines_intersections->geom_id != NULL ) {
			free ( gs->lines_intersections->geom_id );
		}
		if ( gs->lines_intersections->part_id != NULL ) {
			free ( gs->lines_intersections->part_id );
		}
		if ( gs->lines_intersections->X != NULL ) {
			free ( gs->lines_intersections->X );
		}
		if ( gs->lines_intersections->Y != NULL ) {
			free ( gs->lines_intersections->Y );
		}
		if ( gs->lines_intersections->Z != NULL ) {
			free ( gs->lines_intersections->Z );
		}
		if ( gs->lines_intersections->v != NULL ) {
			free ( gs->lines_intersections->v );
		}
		if ( gs->lines_intersections->added != NULL ) {
			free ( gs->lines_intersections->added );
		}
		free ( gs->lines_intersections );
	}
	if ( gs->polygons_intersections != NULL ) {
		if ( gs->polygons_intersections->geom_id != NULL ) {
			free ( gs->polygons_intersections->geom_id );
		}
		if ( gs->polygons_intersections->part_id != NULL ) {
			free ( gs->polygons_intersections->part_id );
		}
		if ( gs->polygons_intersections->X != NULL ) {
			free ( gs->polygons_intersections->X );
		}
		if ( gs->polygons_intersections->Y != NULL ) {
			free ( gs->polygons_intersections->Y );
		}
		if ( gs->polygons_intersections->Z != NULL ) {
			free ( gs->polygons_intersections->Z );
		}
		if ( gs->polygons_intersections->v != NULL ) {
			free ( gs->polygons_intersections->v );
		}
		if ( gs->polygons_intersections->added != NULL ) {
			free ( gs->polygons_intersections->added );
		}
		free ( gs->polygons_intersections );
	}

	/* free entire geometry store */
	free ( gs );
}


/*
 * Invalidates current topological data structures.
 * Must be called after a topological cleaning and before rebuilding
 * with geom_multiplex().
 */
void geom_topology_invalidate(parser_data_store *ds, options *opts) {
	unsigned int i;

	if (ds == NULL || opts == 0)
		return;

	for (i = 0; i < ds->num_records; i++) {
		ds->records[i].geom_id = 0;
		ds->records[i].geom_type = GEOM_TYPE_NONE;
		ds->num_points = 0;
		ds->num_lines = 0;
		ds->num_polygons = 0;
	}
}




/*
 * Remove duplicate vertices within one geometry.
 * Two vertices are considered duplicates if their
 * distance is equal or smaller than a user-defined
 * threshold. Points are checked against all other
 * points in the dataset.
 *
 * A critical decision is which vertex to remove.
 * We assume that the first measured vertex is always
 * the one that should be preserved. Therefore, the
 * list of measurements is processed from the last
 * to the first vertex. Obviously, this assumes that
 * the data is in the exact order as it has been originally
 * measured in (first measurement in first record)!
 *
 * Returns number of duplicate vertices that were removed.
 */
int geom_topology_remove_duplicates ( parser_data_store *ds, options *opts, BOOLEAN in3D ) {
	unsigned int count;
	double d;
	char *input;

	if (ds == NULL || opts == NULL)
		return (0);

	if ( opts->tolerance < 0.0 ) {
		return ( 0 );
	}

	count = 0;

	/* replace "-" with proper name */
	if (!strcmp("-", ds->input)) {
		input = strdup("<console input stream>");
	} else {
		input = strdup(ds->input);
	}

	/* go backwards through all valid geometries and remove duplicate vertices */
	int i;
	for (i = ds->num_records - 1; i >= 0; i--) {
		parser_record *rec = &ds->records[i];
		if (rec->is_empty == FALSE && rec->is_valid == TRUE && rec->geom_type != GEOM_TYPE_NONE)
		{
			/* points */
			if (rec->geom_type == GEOM_TYPE_POINT) {
				/* go through all other point geometries and make sure there are no duplicates */
				int j;
				for (j = 0; j < ds->num_records; j++) {
					parser_record *comp = &ds->records[j];
					if (comp->is_empty == FALSE && comp->is_valid == TRUE
							&& comp->geom_type == GEOM_TYPE_POINT && comp
							!= rec) {
						/* invalidate this point if it is a duplicate (always use 3D distance for points) */
						d = (sqrt(pow((rec->x - comp->x), 2) + pow((rec->y - comp->y), 2) + pow((rec->z - comp->z), 2)));
						if (d <= opts->tolerance) {
							err_show(ERR_NOTE, "");
							if ( d == 0.0 ) {
								err_show(
										ERR_WARN,
										_("\nPoint read from '%s' (line %i) failed topology check:\nCoordinates are identical with line %i (tolerance=%f).\nPoint deleted."),
										input, comp->line, rec->line, opts->tolerance);
							} else {
								err_show(
										ERR_WARN,
										_("\nPoint read from '%s' (line %i) failed topology check:\nCoordinates too close to line %i (tolerance=%f).\nPoint deleted."),
										input, comp->line, rec->line, opts->tolerance);
							}
							comp->is_valid = FALSE;
							count++;
						}
					}
				}
			}
			/* lines */
			if (rec->geom_type == GEOM_TYPE_LINE) {
				int j;
				for (j = 0; j < ds->num_records; j++) {
					/* check all vertices in the same line string */
					parser_record *comp = &ds->records[j];
					if (comp->is_empty == FALSE && comp->is_valid == TRUE
							&& comp->geom_type == GEOM_TYPE_LINE
							&& comp->geom_id == rec->geom_id && comp != rec ) {
						/* invalidate this point if it is a duplicate */
						if ( in3D == TRUE )
							d = (sqrt(pow((rec->x - comp->x), 2) + pow((rec->y
									- comp->y), 2) + pow((rec->z - comp->z), 2)));
						else
							d = (sqrt(pow((rec->x - comp->x), 2) + pow((rec->y
									- comp->y), 2) ));
						if (d <= opts->tolerance) {
							err_show(ERR_NOTE, "");
							if ( d == 0.0 ) {
								err_show(
										ERR_WARN,
										_("\nLine vertex read from '%s' (line %i) failed topology check:\nCoordinates are identical with line %i (tolerance=%f).\nVertex deleted."),
										input, comp->line, rec->line,
										opts->tolerance);
							} else {
								err_show(
										ERR_WARN,
										_("\nLine vertex read from '%s' (line %i) failed topology check:\nCoordinates too close to line %i (tolerance=%f).\nVertex deleted."),
										input, comp->line, rec->line,
										opts->tolerance);
							}
							comp->is_valid = FALSE;
							count++;
						}
					}
				}
			}
			/* polygons */
			if (rec->geom_type == GEOM_TYPE_POLY) {
				int j;
				for (j = 0; j < ds->num_records; j++) {
					parser_record *comp = &ds->records[j];
					/* check all vertices in the same polygon */
					if (comp->is_empty == FALSE && comp->is_valid == TRUE
							&& comp->geom_type == GEOM_TYPE_POLY
							&& comp->geom_id == rec->geom_id && comp != rec ) {
						/* invalidate this point if it is a duplicate */
						if ( in3D == TRUE )
							d = (sqrt(pow((rec->x - comp->x), 2) + pow((rec->y
									- comp->y), 2) + pow((rec->z - comp->z), 2)));
						else
							d = (sqrt(pow((rec->x - comp->x), 2) + pow((rec->y
									- comp->y), 2) ));
						if (d <= opts->tolerance) {
							err_show(ERR_NOTE, "");
							if ( d == 0.0 ) {
								err_show(
										ERR_WARN,
										_("\nPolygon vertex read from '%s' (line %i) failed topology check:\nCoordinates are identical with line %i (tolerance=%f).\nVertex deleted."),
										input, comp->line, rec->line,
										opts->tolerance);
							} else {
								err_show(
										ERR_WARN,
										_("\nPolygon vertex read from '%s' (line %i) failed topology check:\nCoordinates too close to line %i (tolerance=%f).\nVertex deleted."),
										input, comp->line, rec->line,
										opts->tolerance);
							}
							comp->is_valid = FALSE;
							count++;
						}
					}
				}
			}
		}
	}

	free(input);
	return (count);
}




/*
 * Removes geometries that have less than "min" vertices.
 * Returns number of geometries removed.
 */
int geom_topology_remove_splinters (unsigned int min, int geom_type,
		parser_data_store *ds, options *opts) {
	unsigned int count, vertices;
	char *input;

	if (ds == NULL || opts == NULL)
		return (0);

	count = 0;

	/* replace "-" with proper name */
	if (!strcmp("-", ds->input)) {
		input = strdup("<console input stream>");
	} else {
		input = strdup(ds->input);
	}

	/* go through all geometries and find the type we need */
	unsigned int i;
	for (i = 0; i < ds->num_records; i++) {
		parser_record *rec = &ds->records[i];
		if (rec->is_empty == FALSE && rec->is_valid == TRUE && rec->geom_type
				!= GEOM_TYPE_NONE) {
			if (rec->geom_type == geom_type) {
				vertices = 0;
				unsigned int j;
				for (j = 0; j < ds->num_records; j++) {
					if (ds->records[j].geom_id == rec->geom_id
							&& ds->records[j].is_valid == TRUE) {
						vertices++;
					}
				}
				/* not enough vertices? */
				if (vertices < min) {
					/* eliminate all records */
					err_show(ERR_NOTE, "");
					err_show(
							ERR_WARN,
							_("\nLine or polygon read from '%s' (up to line %i) failed topology check:\nNot enough vertices. Geometry deleted."),
							input, rec->line, rec->line);
					unsigned int j;
					for (j = 0; j < ds->num_records; j++) {
						if (ds->records[j].geom_id == rec->geom_id) {
							ds->records[j].is_valid = FALSE;
						}
					}
					count++;
				}
			}
		}
	}

	free(input);
	return (count);
}



/*
 * Removes lines that have less than two vertices.
 * Returns number of lines removed.
 */
int geom_topology_remove_splinters_lines(parser_data_store *ds,
		options *opts) {
	unsigned int count;

	count = geom_topology_remove_splinters(2, GEOM_TYPE_LINE, ds, opts);

	ds->num_lines -= count;

	return (count);
}



/*
 * Removes poylgons that have less than three vertices.
 * Returns number of lines removed.
 */
int geom_topology_remove_splinters_polygons (parser_data_store *ds,
		options *opts) {
	unsigned int count;

	count = geom_topology_remove_splinters (3, GEOM_TYPE_POLY, ds, opts);

	ds->num_polygons -= count;

	return (count);
}




/*
 * Tests if any part of A (that is not a hole) lies completely within
 * any part of B (that is also not a hole).
 *
 * If so, then a new inner ring (hole) is added to B in the shape
 * of the overlaid part from A ("punch hole").
 *
 * Returns number of overlays detected.
 */
unsigned int geom_topology_poly_overlay_2D ( geom_store *gs, parser_desc *parser )
{
	unsigned int i, j, k;
	unsigned int overlaps;
	geom_store_polygon *A;
	geom_store_polygon *B;


	if ( gs->num_polygons < 2 ) {
		return ( 0 );
	}

	/* Go through all polygons in the geom store,
	 * check each poly against each other. */
	overlaps = 0;
	for ( i = 0; i < gs->num_polygons; i++ ) {
		if ( ( gs->polygons[i].is_selected == TRUE ) && ( gs->polygons[i].is_empty == FALSE ) ) {
			for ( j = 0; j < gs->num_polygons-1; j++ ) {
				if ( ( i != j ) && ( gs->polygons[j].is_selected == TRUE ) && ( gs->polygons[j].is_empty == FALSE ) ) {
					A = &gs->polygons[i];
					B = &gs->polygons[j];
					/* need to compare only if bounding boxes overlap */
					if ( geom_tools_bb_overlap_2D ( A, B ) == TRUE ) {
						for ( k = 0; k< A->num_parts; k ++ ) {
							if ( A->parts[k].is_hole == FALSE ) {
								if ( geom_tools_part_in_poly_2D ( &A->parts[k], B ) == TRUE ) {
									/* punch a hole into the overlapped part in B */
									geom_store_add_poly ( 	gs, parser, B->geom_id, A->parts[k].num_vertices,
											A->parts[k].X, A->parts[k].Y, A->parts[k].Z,
											B->num_parts,
											B->atts, B->source, B->line, B->is_3D, NULL, NULL );
									overlaps ++;
								}
							}
						}
					}
				}
			}
		}
	}
	return ( overlaps );
}


/**
 * Updates bounding boxes for all lines and polygons in given geometry store.
 */
void geom_tools_update_bboxes ( geom_store *gs )
{
	unsigned int i, j, v;
	geom_store_polygon *P;
	geom_store_line *L;

	if ( gs == NULL ) {
		return;
	}

	/* POLYGONS */
	for ( i = 0; i < gs->num_polygons; i++ ) {
		P = &gs->polygons[i];
		if ( P->is_empty == FALSE ) {
			for ( j = 0; j < P->num_parts; j ++ ) {
				geom_part *p = &P->parts[j];
				for ( v = 0; v < p->num_vertices; v++ ) {
					if ( p->X[v] < P->bbox_x1 ) { P->bbox_x1 = p->X[v]; }
					if ( p->X[v] > P->bbox_x2 ) { P->bbox_x2 = p->X[v]; }
					if ( p->Y[v] < P->bbox_y1 ) { P->bbox_y1 = p->Y[v]; }
					if ( p->Y[v] > P->bbox_y2 ) { P->bbox_y2 = p->Y[v]; }
					if ( P->is_3D == TRUE ) {
						if ( p->Z[v] < P->bbox_z1 ) { P->bbox_z1 = p->Z[v]; }
						if ( p->Z[v] > P->bbox_z2 ) { P->bbox_z2 = p->Z[v]; }
					}
				}
			}
		}
	}

	/* LINES */
	for ( i = 0; i < gs->num_lines; i++ ) {
		L = &gs->lines[i];
		if ( L->is_empty == FALSE ) {
			for ( j = 0; j < L->num_parts; j ++ ) {
				geom_part *p = &L->parts[j];
				for ( v = 0; v < p->num_vertices; v++ ) {
					if ( p->X[v] < L->bbox_x1 ) { L->bbox_x1 = p->X[v]; }
					if ( p->X[v] > L->bbox_x2 ) { L->bbox_x2 = p->X[v]; }
					if ( p->Y[v] < L->bbox_y1 ) { L->bbox_y1 = p->Y[v]; }
					if ( p->Y[v] > L->bbox_y2 ) { L->bbox_y2 = p->Y[v]; }
					if ( L->is_3D == TRUE ) {
						if ( p->Z[v] < L->bbox_z1 ) { L->bbox_z1 = p->Z[v]; }
						if ( p->Z[v] > L->bbox_z2 ) { L->bbox_z2 = p->Z[v]; }
					}
				}
			}
		}
	}
}


/* Retrieves all coordinate values stored in a ring of a 'multclip' polygon structure,
 * and their count. The parameter 'coord_idx' determine which coordinates are
 * retrieved:
 *
 * 0 = X
 * 1 = Y
 * 2 = Z
 *
 * For convenience, wrapper functions exist that do not require 'coord_idx':
 *
 * geom_tools_get_mpoly_X();
 * geom_tools_get_mpoly_Y();
 * geom_tools_get_mpoly_Z();
 *
 * Index of first/outer ring/contour in input 'mpoly' is '0'.
 *
 * This function will return NULL if an invalid ring index is given or
 * the polygon is empty, or 'count' is a NULL pointer, or there is not enough
 * memory for the result, or 'coord_idx' is invalid.
 *
 * Otherwise, a newly allocated array of Z coordinates is returned.
 * The first and last Z value will always be identical (as they represent the
 * coordinates of a closed polygon, where first vertex = last vertex).
 * Allocated memory for the result must be released by caller when no longer
 * needed.
 *
 * In addition, the number of vertices will be stored in 'count'.
 */
double *geom_tools_get_mpoly_coords ( POLYAREA *mpoly, int ring, int *count, int coord_idx)
{
	int num_vertices = 0;
	double *result = NULL;

	if ( count == NULL ) {
		return NULL;
	}

	if ( coord_idx < 0 || coord_idx > 2 ) {
		*count = 0;
		return NULL;
	}

	if ( mpoly == NULL ) {
		*count = 0;
		return NULL;
	}

	if ( ring < 0 ) {
		*count = 0;
		return NULL;
	}

	/* Get number of vertices in current ring. */
	int c = 0; /* index of ring/contour in 'multclip' representation */
	PLINE *cntr;
	for ( cntr = mpoly->contours; cntr != NULL; cntr = cntr->next ) {
		VNODE *cur;
		cur = &cntr->head;
		do {
			if ( c == ring ) {
				num_vertices ++;
			}
		} while ( ( cur = cur->next ) != &cntr->head );
		c ++;
	}

	if ( num_vertices < 1 ) {
		*count = 0;
		return NULL;
	}

	/* We have vertices: allocate new array and store them. */
	result = malloc ( sizeof(double) * (num_vertices+1)); /* add one slot for duplicate Z coord */
	if ( result == NULL ) {
		/* Unable to allocate memory. */
		if ( count != NULL ) {
			*count = 0;
		}
		return ( NULL );
	}

	c = 0; /* index of ring/contour in 'multclip' representation */
	int pos = 0;
	for ( cntr = mpoly->contours; cntr != NULL; cntr = cntr->next ) {
		VNODE *cur;
		cur = &cntr->head;
		do {
			if ( c == ring ) {
				result[pos] = cur->point[coord_idx];
				pos++;
			}
		} while ( ( cur = cur->next ) != &cntr->head );
		c ++;
	}

	/* add closing vertex' coordinate */
	result[pos] = result[0];

	/* return result data */
	*count = (num_vertices+1);
	return ( result );
}


/* Retrieves all X coordinate values stored in a ring of a 'multclip' polygon structure,
 * and their count.
 *
 * See geom_tools_get_mpoly_coords() for details.
 */
double *geom_tools_get_mpoly_X ( POLYAREA *mpoly, int ring, int *count ) {
	return (geom_tools_get_mpoly_coords (mpoly, ring, count, 0));
}


/* Retrieves all Y coordinate values stored in a ring of a 'multclip' polygon structure,
 * and their count.
 *
 * See geom_tools_get_mpoly_coords() for details.
 */
double *geom_tools_get_mpoly_Y ( POLYAREA *mpoly, int ring, int *count ) {
	return (geom_tools_get_mpoly_coords (mpoly, ring, count, 1));
}


/* Retrieves all Z coordinate values stored in a ring of a 'multclip' polygon structure,
 * and their count.
 *
 * See geom_tools_get_mpoly_coords() for details.
 */
double *geom_tools_get_mpoly_Z ( POLYAREA *mpoly, int ring, int *count ) {
	return (geom_tools_get_mpoly_coords (mpoly, ring, count, 2));
}


/* Retrieves the maximum Z value stored in a 'multclip' polygon structure. */
double geom_tools_get_max_z ( geom_multclip_poly *mpoly )
{
	int i, j;
	double max_Z;

	if ( mpoly == NULL ) {
		return ( 0.0 );
	}

	/* Outer ring. */
	for ( i = 0; i < mpoly->outer->num_vertices; i++) {
		/* Init/update max Z statistic. */
		if ( i == 0 ) {
			max_Z = mpoly->outer->Z[0];
		} else {
			if ( max_Z < mpoly->outer->Z[i] ) {
				max_Z = mpoly->outer->Z[i];
			}
		}
	}

	/* Inner ring(s). */
	for ( j = 0; j < mpoly->num_inner; j++) {
		for ( i = 0; i < mpoly->inner[j]->num_vertices; i++) {
			/* Update max Z statistic. */
			if ( max_Z < mpoly->inner[j]->Z[i] ) {
				max_Z = mpoly->inner[j]->Z[i];
			}
		}
	}

	return ( max_Z );
}


/**
 * Helper function for geom_topology_poly_intersect():
 * Compares an original (simplified) polygon 'org' with the result of
 * a 'multclip' operation stored in 'new'. If there are any new vertices
 * in 'new', then Z values will be interpolated for them ('multclip' produces
 * strictly 2D results, but it's data struct does have 3D vertices).
 *
 * When running in 2D mode, all Z coordinates of new vertices will be
 * set to '0.0'. Otherwise, for all vertices that exist in both 'org' and 'new',
 * Z values will be transferred from 'org' by linear interpolation.
 * Therefore, 'org' must have valid Z values at all of its vertices!
 * If the inerpolation failes, Z='0.0' will be stored and a warning issued.
 *
 * To allow useful error reporting, 'poly_id' and 'part_id' of currently processed
 * polygon must also be passed to this function.
 *
 * Return:
 * Newly allocated array with all new vertices (for which Z values were interpolated
 * or set to '0.0'). Memory for this must be released by caller!
 *
 * 'count' must be a pre-allocated integer and will store the number of interpolated vertices.
 *
 * Any interpolated Z values are also stored directly in the vertices of 'new'.
 *
 * (This function will _modify_ its parameter 'new'.)
 *
 */
/*
TODO: It would be possible to improve the accuracy of this function by using
      an IDW interpolation on all available Z coords from all polygons!
*/
geom_coords_3d *geom_topology_poly_check_z_interpolate ( geom_multclip_poly *org, POLYAREA *new, int poly_id, int part_id, options *opts, int *count )
{
	int num_new_z = 0;
	int i;
	int num_new_vertices = 0;
	BOOLEAN DEBUG = FALSE;
	geom_coords_3d *store = NULL;

	/* Basic validity checks */
	if ( count == NULL ) {
		return ( NULL );
	}

	if ( org == NULL || new == NULL ) {
		*count = 0;
		return ( NULL );
	}

	if ( org->outer == NULL ) {
		*count = 0;
		return ( NULL );
	}

	/* Retrieve maximum Z coordinate value from 'org'. */
	double max_Z = geom_tools_get_max_z ( org );

	/* PASS ONE: Transfer Z values from existing vertices in 'org' to
	 * corresponding vertices in 'new'. */

	/* Step through all vertices in all rings/contours of 'new'. */
	int c = 0; /* index of ring/contour in 'multclip' representation */
	PLINE *cntr;
	for ( cntr = new->contours; cntr != NULL; cntr = cntr->next ) {
		VNODE *cur;
		cur = &cntr->head;
		do
		{
			double p1_x = cur->point[0];
			double p1_y = cur->point[1];
			/* Check all vertices on outer ring of 'org' against all vertices in 'new'
			 * to find the vertices that are not part of the boundary of 'org'. */
			double p2_x;
			double p2_y;
			double p2_z;
			/* We abort the point search if a vertex with identical coordinates was found (2D)! */
			BOOLEAN found = FALSE;
			for ( i = 0; i < org->outer->num_vertices; i++) {
				/* Init/update max Z statistic. */
				if ( i == 0 ) {
					max_Z = org->outer->Z[0];
				} else {
					if ( max_Z < org->outer->Z[i] ) {
						max_Z = org->outer->Z[i];
					}
				}
				p2_x = org->outer->X[i];
				p2_y = org->outer->Y[i];
				p2_z = org->outer->Z[i];
				if ( (p1_x == p2_x) && (p1_y == p2_y) ) {
					found = TRUE;
					cur->point[2] = (double) p2_z; /* copy Z value from existing vertex */
					break;
				}
			}
			if ( found == FALSE ) {
				/* Now check all vertices on inner ring(s) of 'org' (if any). */
				int j;
				for ( j = 0; j < org->num_inner; j++) {
					if ( found == FALSE ) {
						p1_x = cur->point[0];
						p1_y = cur->point[1];
						p2_x = 0.0;
						p2_y = 0.0;
						p2_z = 0.0;
						for ( i = 0; i < org->inner[j]->num_vertices; i++) {
							/* Update max Z statistic. */
							if ( max_Z < org->inner[j]->Z[i] ) {
								max_Z = org->inner[j]->Z[i];
							}
							p2_x = org->inner[j]->X[i];
							p2_y = org->inner[j]->Y[i];
							p2_z = org->inner[j]->Z[i];
							if ( (p1_x == p2_x) && (p1_y == p2_y) ) {
								found = TRUE;
								cur->point[2] = (double) p2_z; /* copy Z value from existing vertex */
								break;
							}
						}
					}
				}
			}

			/* New point? Mark for interpolation! */
			if ( found == FALSE ) {
				/* Assigning this point with "Max Z + 1" marks it for
				 * interpolation in pass two.*/
				cur->point[2] = max_Z + 1.0;
			}

		} while ( ( cur = cur->next ) != &cntr->head );
		c ++;
	}

	/* Count number of new vertices and create storage space for them. */
	c = 0;
	for ( cntr = new->contours; cntr != NULL; cntr = cntr->next ) {
		VNODE *cur;
		cur = &cntr->head;
		do
		{
			if ( cur->point[2] > max_Z ) { /* Point marked for interpolation? */
				num_new_vertices ++;
			}
		} while ( ( cur = cur->next ) != &cntr->head );
		c ++; /* next ring */
	}
	store = malloc ( sizeof(geom_coords_3d) * num_new_vertices);

	/* PASS TWO: Interpolate missing Z values for those vertices in 'new'
	 * for which no Z coordinates could be transfered during 1st pass. */
	int pos = 0; /* index of storage array for new vertices */
    int cur_v = 0; /* index of current vertex on current ring */
	c = 0;
	for ( cntr = new->contours; cntr != NULL; cntr = cntr->next ) {
		VNODE *cur;
		cur = &cntr->head;
		do
		{
			double p1_x = cur->point[0];
			double p1_y = cur->point[1];
			double p1_z = cur->point[2];
			if ( p1_z > max_Z ) { /* Point marked for interpolation? */

				/* Running in 2D mode? Then we just store Z=0.0 and done. */
				if ( opts->force_2d == TRUE ) {
					cur->point[2] = 0.0;
					store[pos].X = p1_x;
					store[pos].Y = p1_y;
					store[pos].Z = 0.0;
					pos ++;
				} else {

					/* Interpolate new Z value for this vertex: */
					/* Since the vertices in 'new' are ordered, we can just
					 * retrieve the two nearest neighbours (on the current
					 * ring/contour) with previous Z coordinates, and perform a
					 * Z value interpolation between them. */

					//DEBUG
					if ( DEBUG ) {
						fprintf ( stderr, "\tINTERPOLATE Z FOR: %f\t%f\n", p1_x, p1_y);
					}
					/* retrieve array of all Z coordinates */
					int count;
					double *z_coords = geom_tools_get_mpoly_Z ( new, c, &count);

					if ( z_coords == NULL ) {
						err_show (ERR_NOTE,"");
						err_show ( ERR_WARN,_("\nZ interpolation failed (polygon %i, part %i, vertex %i). Z coordinate set to '0.0' instead."),
								poly_id, part_id, cur_v);
						cur->point[2] = 0.0;
						store[pos].X = p1_x;
						store[pos].Y = p1_y;
						store[pos].Z = 0.0;
						pos ++;
					} else {
						//DEBUG
						if ( DEBUG ) {
							int v;
							for ( v = 0; v < count; v ++ ) {
								fprintf ( stderr, "\t\t%i/%i has Z: %f\n", (v+1), count, z_coords[v]);
							}
						}
						/* get "left" neighbour */
						int ln = cur_v;
						double ln_z = z_coords[cur_v];
						while ( ln > 0 ) {
							ln --;
							if ( z_coords[ln] <= max_Z ) {
								ln_z = z_coords[ln];
								break;
							}
						}
						/* get "right" neighbour */
						int rn = cur_v;
						double rn_z = z_coords[cur_v];
						while ( rn < count ) {
							rn ++;
							if ( z_coords[rn] <= max_Z ) {
								rn_z = z_coords[rn];
								break;
							}
						}
						/* Did we get two valid Z coords? */
						if ( ( ln_z <= max_Z ) && ( rn_z <= max_Z ) ) {
							//DEBUG
							if ( DEBUG ) {
								fprintf ( stderr, "\t\tLN Z = %f\n", ln_z);
								fprintf ( stderr, "\t\tRN Z = %f\n", rn_z);
							}
							double *x_coords = geom_tools_get_mpoly_X ( new, c, &count);
							double *y_coords = geom_tools_get_mpoly_Y ( new, c, &count);
							if ( ( x_coords == NULL ) || ( y_coords == NULL ) ) {
								err_show (ERR_NOTE,"");
								err_show ( ERR_WARN,_("\nZ interpolation failed (polygon %i, part %i, vertex %i). Z coordinate set to '0.0' instead."),
										poly_id, part_id, cur_v);
								cur->point[2] = 0.0;
								store[pos].X = p1_x;
								store[pos].Y = p1_y;
								store[pos].Z = 0.0;
								pos ++;
							} else {
								/* Now interpolate Z, weighted by distance from each neighbour. */
								//DEBUG
								if ( DEBUG ) {
									fprintf ( stderr, "LEFT  NN: %f\t%f\t%f\n", x_coords[ln], y_coords[ln], z_coords[ln] );
									fprintf ( stderr, "RIGHT NN: %f\t%f\t%f\n", x_coords[rn], y_coords[rn], z_coords[rn] );
								}
								cur->point[2] = geom_tools_interpolate_z ( x_coords[ln], y_coords[ln], z_coords[ln],
										x_coords[rn], y_coords[rn], z_coords[rn], p1_x, p1_y );
								store[pos].X = p1_x;
								store[pos].Y = p1_y;
								store[pos].Z = cur->point[2];
								pos ++;
								//DEBUG
								if ( DEBUG ) {
									fprintf ( stderr, "NEW    Z: %f\n", cur->point[2] );
								}
							}
							/* release mem */
							t_free ( x_coords );
							t_free ( y_coords );
						} else {
							err_show (ERR_NOTE,"");
							err_show ( ERR_WARN,_("\nZ interpolation failed (polygon %i, part %i, vertex %i). Z coordinate set to '0.0' instead."),
									poly_id, part_id, cur_v);
							cur->point[2] = 0.0;
							store[pos].X = p1_x;
							store[pos].Y = p1_y;
							store[pos].Z = 0.0;
							pos ++;
						}
						/* release mem */
						t_free ( z_coords );
						/* keep count of interpolated Z coordinates */
						num_new_z ++;
					}
				}
			}
			cur_v ++; /* next vertex on current ring */
		} while ( ( cur = cur->next ) != &cntr->head );
		c ++; /* next ring */
		cur_v = 0; /* reset to first vertex */
	}
	*count = num_new_vertices;
	return (store);
}


/**
 * Helper function for geom_topology_poly_intersect():
 * Attempts to insert a new vertex (represented by x,y and z coords) into any one
 * of the boundaries of polygon A (a simplified 'multclip' representation), i.e.
 * the outer boundary or any of the inner boundaries/holes (if present).
 *
 * Insertion succeeds only if the new vertex (x/y/z) lies directly on a straight
 * line segment between two existing, consecutive vertices of A, and if it is
 * not an exact duplicate of an existing vertex. Since this is a
 * geometric test, an acceptable tolerance value for the location test must also
 * be provided: Insertion will occur if the difference of the sum of straight-line
 * distances (2D!) between the new vertex and the two existing neighbours and the
 * straight line distance between the two existing vertices is <= 'tolerance'.
 *
 * Returns: TRUE if the vertex was inserted, FALSE otherwise.
 *
 * This function modifies its argument 'A'!
 */
BOOLEAN geom_topology_mpoly_place_vertex ( geom_multclip_poly *A, double Nx, double Ny, double Nz, double tolerance )
{
	int i,r;
	BOOLEAN DEBUG = FALSE;

	if ( A == NULL ) {
		return ( FALSE );
	}

	/* CHECK IF VERTEX EXISTS ALREADY */

	int num_vertices = 0; /* while we are here, we might as well count the vertices... */

	/* check vertices on outer ring */
	for ( i = 0; i < A->outer->num_vertices; i++) {
		double Ax = A->outer->X[i];
		double Ay = A->outer->Y[i];
		if ( Ax == Nx && Ay == Ny ) {
			return (FALSE);
		}
		num_vertices ++;
	}
	/* loop through all inner rings */
	for ( r = 0; r < A->num_inner; r ++ ) {
		/* check vertices on this inner ring */
		for ( i = 0; i < A->inner[r]->num_vertices; i++) {
			double Ax = A->inner[r]->X[i];
			double Ay = A->inner[r]->Y[i];
			if ( Ax == Nx && Ay == Ny ) {
				return (FALSE);
			}
			num_vertices ++;
		}
	}

	/* CHECK IF VERTEX LIES BETWEEN TWO EXISTING ONES */

	/* Check between all consecutive vertices on outer ring: */
	for ( i = 0; i < A->outer->num_vertices-1; i++) {
		double Ax = A->outer->X[i];
		double Ay = A->outer->Y[i];
		double Bx = A->outer->X[i+1];
		double By = A->outer->Y[i+1];
		double dist_a_new_2d = sqrt ( pow ( ( Ax - Nx ), 2 ) + pow ( ( Ay - Ny ), 2 ) );
		double dist_b_new_2d = sqrt ( pow ( ( Bx - Nx ), 2 ) + pow ( ( By - Ny ), 2 ) );
		double dist_ab_2d    = sqrt ( pow ( ( Ax - Bx ), 2 ) + pow ( ( Ay - By ), 2 ) );
		if ( fabs ( (dist_a_new_2d + dist_b_new_2d) - dist_ab_2d ) <= tolerance ) {
			/* Found a place for this vertex! */
			//DEBUG
			if ( DEBUG ) {
				fprintf (stderr, "\t* PLACED VERTEX %f %f ON OUTER RING.\n", Nx, Ny);
			}
			/* Create new part with new vertex added to it at the correct position! */
			geom_part* new_part = geom_tools_part_add_vertex ( A->outer, i, Nx, Ny, Nz );
			if ( new_part == NULL ) {
				return ( FALSE );
			}
			geom_tools_part_destroy (A->outer);
			A->outer = new_part;
			return (TRUE);
		}
	}

	/* loop through all inner rings and do the same check */
	for ( r = 0; r < A->num_inner; r ++ ) {
		/* check vertices on this inner ring */
		for ( i = 0; i < A->inner[r]->num_vertices-1; i++) {
			double Ax = A->inner[r]->X[i];
			double Ay = A->inner[r]->Y[i];
			double Bx = A->inner[r]->X[i+1];
			double By = A->inner[r]->Y[i+1];
			double dist_a_new_2d = sqrt ( pow ( ( Ax - Nx ), 2 ) + pow ( ( Ay - Ny ), 2 ) );
			double dist_b_new_2d = sqrt ( pow ( ( Bx - Nx ), 2 ) + pow ( ( By - Ny ), 2 ) );
			double dist_ab_2d    = sqrt ( pow ( ( Ax - Bx ), 2 ) + pow ( ( Ay - By ), 2 ) );
			if ( fabs ( (dist_a_new_2d + dist_b_new_2d) - dist_ab_2d ) <= tolerance ) {
				/* Found a place for this vertex! */
				//DEBUG
				if ( DEBUG ) {
					fprintf (stderr, "\t* PLACED VERTEX %f %f ON INNER RING %i.\n", Nx, Ny, (r+1));
				}
				geom_part* new_part = geom_tools_part_add_vertex ( A->inner[r], i, Nx, Ny, Nz );
				if ( new_part == NULL ) {
					return ( FALSE );
				}
				geom_tools_part_destroy (A->inner[r]);
				A->inner[r] = new_part;
				return (TRUE);
			}
		}
	}

	return (FALSE);
}


/**
 * Helper function for geom_topology_poly_intersect():
 * Attempts to insert a new vertex (represented by x,y and z coords) into an
 * existing polygon part.
 *
 * Insertion succeeds only if the new vertex (x/y/z) lies directly on a straight
 * line segment between two existing, consecutive vertices of A, and if it is
 * not an exact duplicate of an existing vertex. Since this is a
 * geometric test, an acceptable tolerance value for the location test must also
 * be provided: Insertion will occur if the difference of the sum of straight-line
 * distances (2D!) between the new vertex and the two existing neighbours and the
 * straight line distance between the two existing vertices is <= 'tolerance'.
 *
 * Returns: A newly allocated geom_part (memory must be freed by caller) that
 * is a copy of 'part' plus the added vertex, or NULL if no vertex was added
 * or there was an error.
 *
 */
geom_part *geom_topology_part_place_vertex ( geom_part *part, double Nx, double Ny, double Nz, double tolerance )
{
	int i;
	BOOLEAN DEBUG = FALSE;

	if ( part == NULL ) {
		return ( NULL );
	}

	if ( part->is_empty == TRUE ) {
		return ( NULL );
	}

	/* CHECK IF VERTEX EXISTS ALREADY */
	for ( i = 0; i < part->num_vertices; i++) {
		double Ax = part->X[i];
		double Ay = part->Y[i];
		if ( Ax == Nx && Ay == Ny ) {
			return (NULL);
		}
	}

	/* CHECK IF VERTEX LIES BETWEEN TWO EXISTING ONES */
	for ( i = 0; i < part->num_vertices-1; i++) {
		double Ax = part->X[i];
		double Ay = part->Y[i];
		double Bx = part->X[i+1];
		double By = part->Y[i+1];
		double dist_a_new_2d = sqrt ( pow ( ( Ax - Nx ), 2 ) + pow ( ( Ay - Ny ), 2 ) );
		double dist_b_new_2d = sqrt ( pow ( ( Bx - Nx ), 2 ) + pow ( ( By - Ny ), 2 ) );
		double dist_ab_2d    = sqrt ( pow ( ( Ax - Bx ), 2 ) + pow ( ( Ay - By ), 2 ) );
		if ( fabs ( (dist_a_new_2d + dist_b_new_2d) - dist_ab_2d ) <= tolerance ) {
			/* Found a place for this vertex! */
			//DEBUG
			if ( DEBUG ) {
				fprintf (stderr, "\t* PLACED VERTEX %f %f ON PART (%i - %i).\n", Nx, Ny, i, i+1);
			}
			/* Create new part with new vertex added to it at the correct position! */
			geom_part* new_part = geom_tools_part_add_vertex ( part, i+1, Nx, Ny, Nz );
			return (new_part);
			//return (geom_tools_part_duplicate(part)); //DEBUG
		}
	}

	return (NULL);
}


/**
 * Helper function for geom_topoly_poly_remove_overlap_2D():
 * Uses 'multclip' 3rd party functions to check whether two
 * polygons (A&B) intersect, and cuts the intersecting polygon (B)
 * if required. Also, additional intersection vertices are added
 * to each affected boundary of 'A'.
 * The polygons must be passed in the special, simplified
 * representation and inverse vertex order required by 'multclip'.
 * The polygon and part IDs must be passed to allow this function to produce
 * meaningful error messages.
 *
 * Return values:
 * If there is an areal overlap (intersection), then 'true' is returned and
 * 'B' is _modified_ to represent the original polygon _minus_ the intersection
 * area. At the same time, 'A' is _modified_ by adding additional vertices at
 * the intersection points along its boundary.
 * Otherwise, 'false' is returned and 'A' and 'B' remain unmodified.
 *
 * This function modifies its arguments 'A' and 'B'!
 */
//DEBUG
BOOLEAN geom_topology_poly_intersect (geom_multclip_poly *A, geom_multclip_poly *B,
		int A_poly_id, int A_part_id, int B_poly_id, int B_part_id, geom_store *gs, options *opts )
{
	BOOLEAN result = FALSE;
	POLYAREA *pA = NULL;
	POLYAREA *pB = NULL;
	PLINE *res = NULL;
	Vector v;
	int i, r;
	BOOLEAN DEBUG = FALSE;
	BOOLEAN DEBUG_MORE = FALSE;


	/* Basic validity checks for input geometries. */
	if ( A == NULL || B == NULL ) {
		return ( FALSE );
	}

	if ( A == B ) {
		return ( FALSE );
	}

	if ( A->outer == NULL ) {
		return ( FALSE );
	}

	if ( B->outer == NULL ) {
		return ( FALSE );
	}

	/* Skip this test if one of the polygons lies entirely within the other! */
	/* TODO: This does not work as it returns false positives, leading to uncleaned
	      polygons. Disabled for the time being. It is also questionable whether
	      this test should be performed here at all. In the sequence of topological
	      cleaning ops, polygon boundary intersections are cleaned first, then
	      polygon overlays (holes, poly in poly) are detected in the next step;
	      so we should not be concerned about this here!
	if ( geom_tools_part_in_part_2D ( A->outer, B->outer ) == TRUE ) {
		return ( FALSE );
	}
	if ( geom_tools_part_in_part_2D ( B->outer, A->outer ) == TRUE ) {
		return ( FALSE );
	}
	*/

	/* BUILD ADHOC 'multclip' DATA STRUCTURES */

	if ( DEBUG ) {
		fprintf (stderr, "    BUILD A\n");
	}

	/* polygon A */
	pA = poly_Create(); /* a complete 'multclip' polygon */
	res = NULL;			/* one ring in a complete 'multclip' polygon */
	/* add this part's outer ring */
	for ( i = 0; i < A->outer->num_vertices-1; i++) { /* 'multclip' does not require 1st vertex = last vertex */
		v[0] = (double) A->outer->X[i];
		v[1] = (double) A->outer->Y[i];
		v[2] = (double) 0.0; /* default Z value: 'multclip' operates in 2D */
		if ( res == NULL ) {
			res = poly_NewContour(v); /* create new ring and add first vertex to it */
		} else {
			poly_InclVertex(res->head.prev, poly_CreateNode(v)); /* add another vertex to existing ring */
		}
		if ( DEBUG_MORE ) {
			fprintf(stderr,"ADD[%i]: %.3f/%.3f/%.3f\n", (i), v[0], v[1], v[2]);
			fprintf(stderr,"ORG[%i]: %.3f/%.3f/%.3f\n", (i), A->outer->X[i], A->outer->Y[i], A->outer->Z[i]);
		}
	}
	/* Add outer ring to 'mulclip' polygon 'A'. */
	poly_PreContour(res, TRUE); /* determines orientation(!) of ring and removes NULL vertices */
	BOOLEAN check = poly_InclContour(pA, res); /* include ring in polygon */
	if ( DEBUG ) {
		fprintf (stderr, "    INCL: %i\n",check);
		fprintf (stderr, "    VALID: %i\n",poly_Valid(pA));
	}
	res = NULL; /* now ref'd by polygon struct */
	if ( ( check == FALSE ) || (!poly_Valid(pA)) ) {
		err_show (ERR_NOTE,"");
		err_show ( ERR_WARN,_("\nInvalid polygon ring (outer) detected (ID %i, part %i). Overlap cleaning skipped."),
				A_poly_id, A_part_id);
		if ( pA != NULL ) {
			poly_Free (&pA);
		}
		return ( FALSE );
	}
	/* add all of this part's inner rings */
	for ( r = 0; r < A->num_inner; r ++ ) {
		res = NULL;
		for ( i = 0; i < A->inner[r]->num_vertices-1; i++) {
			v[0] = (double) A->inner[r]->X[i];
			v[1] = (double) A->inner[r]->Y[i];
			v[2] = (double) 0.0;
			if ( res == NULL ) {
				res = poly_NewContour(v);
			} else {
				poly_InclVertex(res->head.prev, poly_CreateNode(v));
			}
			if ( DEBUG_MORE ) {
				fprintf(stderr,"NEW[%i]: %.3f/%.3f/%.3f\n", (i), v[0], v[1], v[2]);
				fprintf(stderr,"ORG[%i]: %.3f/%.3f/%.3f\n", (i), A->inner[r]->X[i], A->inner[r]->Y[i], A->inner[r]->Z[i]);
			}
		}
		poly_PreContour(res, TRUE);
		BOOLEAN check = poly_InclContour(pA, res);
		if ( DEBUG ) {
			fprintf (stderr, "INCL: %i\n", check);
			fprintf (stderr, "VALID: %i\n", poly_Valid(pA));
		}
		res = NULL;
		if ( ( check == FALSE ) || (!poly_Valid(pA)) ) {
			err_show (ERR_NOTE,"");
			err_show ( ERR_WARN,_("\nInvalid polygon ring (inner) detected (ID %i, part %i). Overlap cleaning skipped."), A_poly_id, A_part_id);
			if ( pA != NULL ) {
				poly_Free (&pA);
			}
			return ( FALSE );
		}
	}

	/* polygon B */

	if ( DEBUG ) {
		fprintf (stderr, "    BUILD B\n");
	}

	pB = poly_Create();
	res = NULL;
	/* add outer ring */
	for ( i = 0; i < B->outer->num_vertices-1; i++) { /* 'multclip' does not require 1st vertex = last vertex */
		v[0] = (double) B->outer->X[i];
		v[1] = (double) B->outer->Y[i];
		v[2] = (double) 0.0;
		if ( res == NULL ) {
			res = poly_NewContour(v);
		} else {
			poly_InclVertex(res->head.prev, poly_CreateNode(v));
		}
		if ( DEBUG_MORE ) {
			fprintf(stderr,"NEW[%i]: %.3f/%.3f/%.3f\n", (i), v[0], v[1], v[2]);
			fprintf(stderr,"ORG[%i]: %.3f/%.3f/%.3f\n", (i), B->outer->X[i], B->outer->Y[i], B->outer->Z[i]);
		}
	}
	poly_PreContour(res, TRUE);
	check = poly_InclContour(pB, res);
	if ( DEBUG ) {
		fprintf (stderr, "    INCL: %i\n",check);
		fprintf (stderr, "    VALID: %i\n",poly_Valid(pB));
	}
	res = NULL; /* now ref'd by polygon struct */
	if ( ( check == FALSE ) || (!poly_Valid(pB)) ) {
		err_show (ERR_NOTE,"");
		err_show ( ERR_WARN,_("\nInvalid polygon ring (outer) detected (ID %i, part %i). Overlap cleaning skipped."),
				B_poly_id, B_part_id);
		if ( pA != NULL ) {
			poly_Free (&pA);
		}
		if ( pB != NULL ) {
			poly_Free (&pB);
		}
		return ( FALSE );
	}
	/* add all inner rings */
	for ( r = 0; r < B->num_inner; r ++ ) {
		res = NULL;
		for ( i = 0; i < B->inner[r]->num_vertices-1; i++) {
			v[0] = (double) B->inner[r]->X[i];
			v[1] = (double) B->inner[r]->Y[i];
			v[2] = (double) 0.0;
			if ( res == NULL ) {
				res = poly_NewContour(v);
			} else {
				poly_InclVertex(res->head.prev, poly_CreateNode(v));
			}
			if ( DEBUG_MORE ) {
				fprintf(stderr,"NEW[%i]: %.3f/%.3f/%.3f\n", (i), v[0], v[1], v[2]);
				fprintf(stderr,"ORG[%i]: %.3f/%.3f/%.3f\n", (i), B->inner[r]->X[i], B->inner[r]->Y[i], B->inner[r]->Z[i]);
			}
		}
		poly_PreContour(res, TRUE);
		BOOLEAN check = poly_InclContour(pB, res);
		res = NULL;
		if ( ( check == FALSE ) || (!poly_Valid(pB)) ) {
			err_show (ERR_NOTE,"");
			err_show ( ERR_WARN,_("\nInvalid polygon ring (inner) detected (ID %i, part %i). Overlap cleaning skipped."), B_poly_id, B_part_id);
			if ( pA != NULL ) {
				poly_Free (&pA);
			}
			if ( pB != NULL ) {
				poly_Free (&pB);
			}
			return ( FALSE );
		}
	}

	/* perform intersection test */
	POLYAREA *area_intersect = NULL;
	POLYAREA *area_subtract = NULL;
	int code;
	code = poly_Boolean(pB, pA, &area_intersect, PBO_ISECT);
	if ( code == err_ok && area_intersect == NULL ) {
		/* No error, but empty result: Check again with roles swapped. */
		code = poly_Boolean(pA, pB, &area_intersect, PBO_ISECT);
	}
	if ( code != err_ok ) {
		if ( DEBUG_MORE ) {
			fprintf(stderr,"*** ERR CODE = %i\n", code);
		}
		err_show (ERR_NOTE,"");
		err_show ( ERR_WARN,_("\nPolygon intersection test failed (ID %i, part %i & ID %i, part %i). Overlap cleaning skipped."), A_poly_id, A_part_id, B_poly_id, B_part_id);
	}
	if ( code == err_ok && area_intersect != NULL ) {
		/* Got a valid intersection area! */
		if ( DEBUG ) {
			fprintf(stderr,"INTERSECT!\n");
		}
		if ( DEBUG ) {
			fprintf(stderr,"AREA OF INTERSECT:\n");
			int c = 0;
			PLINE *cntr;
			for ( cntr = area_intersect->contours; cntr != NULL; cntr = cntr->next ) {
				fprintf(stderr,"  Contour %i:\n", c);
				VNODE *cur;
				cur = &cntr->head;
				do
				{
					fprintf(stderr, "\t%lf %lf\n", cur->point[0], cur->point[1] );
				} while ( ( cur = cur->next ) != &cntr->head );
				c ++;
			}
		}

		/* Subtract intersection area from polygon. */
		code = poly_Boolean(pB, area_intersect, &area_subtract, PBO_SUB);
		if ( code != err_ok ) {
			err_show ( ERR_NOTE,"");
			err_show ( ERR_WARN,_("\nPolygon intersection test failed (ID %i, part %i & ID %i, part %i). Overlap cleaning skipped."), A_poly_id, A_part_id, B_poly_id, B_part_id);
		} else {
			if ( area_subtract != NULL ) {
				err_show ( ERR_NOTE,"");
				err_show ( ERR_WARN,_("\nPolygon boundaries overlap (ID %i, part %i & ID %i, part %i):"),
						A_poly_id, A_part_id, B_poly_id, B_part_id);

				PLINE *cntr;
				int c = 0;

				/* copy/interpolate Z values at result polygon's vertices */
				int num_new_v = 0;
				geom_coords_3d *store = geom_topology_poly_check_z_interpolate ( B, area_subtract, B_poly_id, B_part_id, opts, &num_new_v );
				if ( num_new_v > 0 ) {
					err_show ( ERR_NOTE,_("Cleaned and added %d vertices at polygon boundary intersections."), num_new_v);
				} else {
					err_show ( ERR_NOTE,_("Could not clean automatically, please check."));
				}

				if ( DEBUG ) {
					fprintf(stderr,"NEW VERTICES:\n");
					int j;
					for ( j=0; j < num_new_v; j++) {
						fprintf(stderr,"\t%f %f %f\n", store[j].X, store[j].Y, store[j].Z);
					}
				}

				if ( DEBUG ) {
					/* This is the new polygon/part with new vertices and interpolated Z values. */
					fprintf(stderr,"AREA AFTER SUBTRACTION:\n");
					for ( cntr = area_subtract->contours; cntr != NULL; cntr = cntr->next ) {
						fprintf(stderr,"  Contour %i:\n", c);
						VNODE *cur;
						cur = &cntr->head;
						do
						{
							fprintf(stderr, "\t%lf %lf %lf\n", cur->point[0], cur->point[1], cur->point[2] );
						} while ( ( cur = cur->next ) != &cntr->head );
						c ++;
					}
				}

				/* Now we need to check that the number of rings in the result
				 * set is still the same as that of the unmodified original.
				 * If that is not the case, then we cannot associated the original
				 * rings with the modified ones 1:1 with complete certainty, and we
				 * have to abort this operation! */
				c = 0;
				for ( cntr = area_subtract->contours; cntr != NULL; cntr = cntr->next ) {
					c ++;
				}
				if ( DEBUG ) {
					fprintf(stderr,"NUM ORG CONTOURS: %i\n", (1 + B->num_inner));
					fprintf(stderr,"NUM NEW CONTOURS: %i\n", c);
				}

				if ( c != ( 1 + B->num_inner ) ) {
					err_show ( ERR_NOTE,"");
					err_show ( ERR_WARN,_("\nPolygon intersection test failed (ID %i, part %i & ID %i, part %i). Overlap cleaning skipped."),
							A_poly_id, A_part_id, B_poly_id, B_part_id);
				} else {
					/* Count number of vertices in result set for inner and outer rings. */
					int num_inner_rings = (c-1);
					int num_vertices_outer = 0;
					int *num_vertices_inner = NULL;
					num_vertices_inner = malloc ( sizeof (int) * (num_inner_rings) );
					c = 0;
					for ( cntr = area_subtract->contours; cntr != NULL; cntr = cntr->next ) {
						int v = 0;
						VNODE *cur;
						cur = &cntr->head;
						do {
							if ( c == 0 ) {
								num_vertices_outer ++;
							} else {
								num_vertices_inner[c-1] ++;
							}
							v++;
						} while ( ( cur = cur->next ) != &cntr->head );
						c ++;
					}
					if ( DEBUG ) {
						fprintf(stderr,"NUM VERTICES OUTER RING: %i\n", (num_vertices_outer));
						int i;
						for ( i = 0; i < num_inner_rings; i ++) {
							fprintf(stderr,"NUM VERTICES INNER RING %i: %i\n", i, (num_vertices_inner[i]));
						}
					}

					/* Release old vertex data and allocate space for new. */
					t_free (B->outer->X);
					t_free (B->outer->Y);
					t_free (B->outer->Z);
					B->outer->X = malloc (sizeof(double)*num_vertices_outer);
					B->outer->Y = malloc (sizeof(double)*num_vertices_outer);
					B->outer->Z = malloc (sizeof(double)*num_vertices_outer);
					B->outer->num_vertices = num_vertices_outer;
					for ( i = 0; i < num_inner_rings; i ++) {
						t_free ( B->inner[i]->X );
						t_free ( B->inner[i]->Y );
						t_free ( B->inner[i]->Z );
						B->inner[i]->X = malloc (sizeof(double)*num_vertices_inner[i]);
						B->inner[i]->Y = malloc (sizeof(double)*num_vertices_inner[i]);
						B->inner[i]->Z = malloc (sizeof(double)*num_vertices_inner[i]);
						B->inner[i]->num_vertices = num_vertices_inner[i];
					}

					/* Copy result vertices into polygon B. */
					c = 0;
					for ( cntr = area_subtract->contours; cntr != NULL; cntr = cntr->next ) {
						int v = 0;
						VNODE *cur;
						cur = &cntr->head;
						do {
							if ( c == 0 ) {
								B->outer->X[v] = cur->point[0];
								B->outer->Y[v] = cur->point[1];
								B->outer->Z[v] = cur->point[2];
							} else {
								B->inner[c-1]->X[v] = cur->point[0];
								B->inner[c-1]->Y[v] = cur->point[1];
								B->inner[c-1]->Z[v] = cur->point[2];
							}
							v++;
						} while ( ( cur = cur->next ) != &cntr->head );
						c ++;
					}

					if ( num_new_v > 0 ) {
						/* 	add new vertices to boundary of intersected polygon A, as well!
						It's not enough to try and add vertices to 'A': Since we might be dealing with
						Intersections of boundaries that are inner rings, we can have multiple boundary
						intersections! SOLUTION: Do a bbox test against ALL polygon parts.
						Since 'B' is already completely built, we can blindly check against every polygon
						part in the geom store (filtered by bbox): Duplicate vertices (of B) will
						just be skipped by geom_topology_poly_place_vertex().
						 */
						int num_added = 0;
						geom_store_polygon *candidate = gs->polygons;
						for ( i = 1; i < gs->num_polygons; i++ ) {
							if (( gs->polygons[i].is_selected == TRUE ) && ( gs->polygons[i].is_empty == FALSE ) && i != B_poly_id ) {
								if ( DEBUG ) {
									fprintf (stderr, "CHECK: %i.\n", candidate->geom_id);
								}
								/* Check if candidate has bounding box intersection with intersecting polygon. */
								if ( geom_tools_mpoly_bb_overlap_2D ( candidate, B ) == TRUE ) {
									if ( DEBUG ) {
										fprintf (stderr, "\tBBOX INTERSECT: %i and %i.\n", B_poly_id, candidate->geom_id);
									}
									int k;
									geom_part *old_part = candidate->parts;
									for ( k = 0; k < candidate->num_parts; k ++ ) {
										int v;
										for ( v=0; v < num_new_v; v++) {
											geom_part *new_part = geom_topology_part_place_vertex ( old_part, store[v].X, store[v].Y, store[v].Z, GEOM_PREC_PLACE_P_SEG );
											if ( new_part != NULL ) {
												if ( DEBUG ) {
													fprintf (stderr, "\t\tNUM VERTICES OLD: %i.\n", old_part->num_vertices);
													fprintf (stderr, "\t\tNUM VERTICES NEW: %i.\n", new_part->num_vertices);
												}
												/* replace part with new version with added vertices */
												/* free vertex memory */
												geom_tools_part_destroy (old_part);
												/* allocate memory for new vertices */
												old_part->num_vertices = new_part->num_vertices;
												old_part->X = malloc ( sizeof (double) * new_part->num_vertices );
												old_part->Y = malloc ( sizeof (double) * new_part->num_vertices );
												old_part->Z = malloc ( sizeof (double) * new_part->num_vertices );
												/* copy vertices from new to old */
												for ( i = 0; i < old_part->num_vertices; i ++ ) {
													old_part->X[i] = new_part->X[i];
													old_part->Y[i] = new_part->Y[i];
													old_part->Z[i] = new_part->Z[i];
												}
												/* free vertex memory */
												geom_tools_part_destroy (new_part);
												num_added ++;
											}
										}
										old_part++;
									}
								}
							}
							candidate++;
						}
						/* Warn if no additional vertices were placed. */
						if ( num_added < 1 ) {
							err_show ( ERR_NOTE,"");
							err_show ( ERR_WARN,_("\nFailed to add intersection vertices to polygon(s) intersected by polygon ID %i.\n"),
									B_poly_id );
						}
					}

					/* clean up */
					t_free (store);
					t_free (num_vertices_inner);
					result = TRUE;
				}
			}
		}

	}

	/* clean up */
	if ( area_intersect != NULL ) {
		poly_Free(&area_intersect);
	}
	if ( area_subtract != NULL ) {
		poly_Free(&area_subtract);
	}
	if ( pA != NULL ) {
		poly_Free (&pA);
	}
	if ( pB != NULL ) {
		poly_Free (&pB);
	}

	return ( result );
}


/**
 * Computes the 3D bounding box for polygon 'A' (simplified representation
 * for 'multclip', and stores the bbox's extreme coordinates in 'A'.
 *
 * This function _modifies_ its argument!
 */
void geom_topology_mpoly_compute_bbox_3d ( geom_multclip_poly *A )
{
	int i,r;

	if ( A == NULL ) {
		return;
	}
	
	if ( A->outer == NULL ) {
		return;
	}	

	/* check vertices on outer ring */
	for ( i = 0; i < A->outer->num_vertices; i++) {
		double Ax = A->outer->X[i];
		double Ay = A->outer->Y[i];
		double Az = A->outer->Z[i];
		if ( i == 0 ) {
			/* initial bbox */
			A->bbox_x1 = Ax;
			A->bbox_x2 = Ax;
			A->bbox_y1 = Ay;
			A->bbox_y2 = Ay;
			A->bbox_z1 = Az;
			A->bbox_z2 = Az;
		} else {
			/* update bbox */
			if ( Ax < A->bbox_x1 ) {
				A->bbox_x1 = Ax;
			}
			if ( Ax > A->bbox_x2 ) {
				A->bbox_x2 = Ax;
			}
			if ( Ay < A->bbox_y1 ) {
				A->bbox_y1 = Ay;
			}
			if ( Ay > A->bbox_y2 ) {
				A->bbox_y2 = Ay;
			}
			if ( Az < A->bbox_z1 ) {
				A->bbox_z1 = Az;
			}
			if ( Az > A->bbox_z2 ) {
				A->bbox_z2 = Az;
			}
		}
	}	

	for ( r = 0; r < A->num_inner; r ++ ) {
		/* check vertices on this inner ring */
		for ( i = 0; i < A->inner[r]->num_vertices; i++) {
			double Ax = A->inner[r]->X[i];
			double Ay = A->inner[r]->Y[i];
			double Az = A->inner[r]->Z[i];
			if ( Ax < A->bbox_x1 ) {
				A->bbox_x1 = Ax;
			}
			if ( Ax > A->bbox_x2 ) {
				A->bbox_x2 = Ax;
			}
			if ( Ay < A->bbox_y1 ) {
				A->bbox_y1 = Ay;
			}
			if ( Ay > A->bbox_y2 ) {
				A->bbox_y2 = Ay;
			}
			if ( Az < A->bbox_z1 ) {
				A->bbox_z1 = Az;
			}
			if ( Az > A->bbox_z2 ) {
				A->bbox_z2 = Az;
			}

		}
	}
}


/**
 * Performs a geometric AND operation between all polygon pairs in a geometry
 * store to detect and remove overlap areas.
 * Overlap detection and removal is done in the order in which the polygons
 * appear in the input data: Polygon A is checked against all other polygons B_i,
 * and if B_i intersects with A, then the intersection area is removed from B_i,
 * while A is _not_ modified.
 *
 * TODO: (2019-09-09, s2g 1.5.1)
 *       This function is currently run before geom_topology_poly_overlay_2D() in main.c
 *       What this means is that there are no inner rings/holes in the data at the time
 *       of checking/removing overlaps. In _theory_ this should work fine: First all
 *       overlapping boundaries (including those of potential holes) are adjusted, then
 *       any remaining overlays are converted to holes. However, THIS NEEDS TESTING with
 *       real data. The current sequence also means that we do not have to deal with
 *       complex "outer+inner rings" polygon structures while cleaning overlaps. In any
 *       case, this function (and the helper functions that it calls) are already prepared
 *       for more complex polygons with holes, but again: NEEDS TESTING!
 *
 * TODO: Test with '-2D' option!
 */
unsigned int geom_topology_poly_remove_overlap_2D ( geom_store *gs, parser_desc *parser, options *opts )
{
	unsigned int i, j, k, l, m, r;
	unsigned int overlaps_removed;
	geom_store_polygon *A;
	geom_store_polygon *B;
	BOOL DEBUG = FALSE;
	BOOL DEBUG_MORE = FALSE;

	if ( DEBUG ) {
		fprintf (stderr, "POLY INTERSECTION TEST...\n");
	}

	if ( gs->num_polygons < 2 ) {
		return ( 0 );
	}

	/* Go through all (selected and non-empty) polygons in the geom store, check each polygon against all successive ones. */
	overlaps_removed = 0; /* keep count of modified geometries */
	/* Loop through all polygons, excluding the very last one (which has no successor). */
	for ( i = 0; i < gs->num_polygons-1; i++ ) {
		if ( ( gs->polygons[i].is_selected == TRUE ) && ( gs->polygons[i].is_empty == FALSE ) ) {
			/* Loop through all successors of the current polygon. */
			for ( j = i+1; j < gs->num_polygons; j++ ) {
				if ( ( gs->polygons[j].is_selected == TRUE ) && ( gs->polygons[j].is_empty == FALSE ) ) {
					A = &gs->polygons[i]; /* New vertices may be added to this polygon if B overlaps with it. */
					B = &gs->polygons[j]; /* This polygon will be cut, if it overlaps with A. */
					/* exact overlap check is only required if bounding boxes overlap */
					if ( geom_tools_bb_overlap_2D ( A, B ) == TRUE ) {
						if ( DEBUG ) {
							fprintf (stderr,"\n  BBOX OVERLAP: %i & %i\n", (i+1), (j+1));
						}
						/* We have two polygons with _potential_ overlap! */
						int num_outer_A = 0; /* number of non-hole parts */
						int num_inner_A = 0; /* number of holes in polygon */
						/* 1ST PASS: Count number of outer and inner rings in A and B. */
						for ( k = 0; k < A->num_parts; k ++ ) {
							if ( A->parts[k].is_empty == FALSE ) {
								if ( A->parts[k].is_hole == FALSE ) {
									num_outer_A++; /* counts as outer ring (part) */
								} else {
									num_inner_A++; /* counts as inner ring (hole) */
								}
							}
						}
						int num_outer_B = 0;
						int num_inner_B = 0;
						for ( k = 0; k< B->num_parts; k ++ ) {
							if ( B->parts[k].is_empty == FALSE ) {
								if ( B->parts[k].is_hole == FALSE ) {
									num_outer_B++;
								} else {
									num_inner_B++;
								}
							}
						}
						/* 2ND PASS: Convert polygons to 'multclip' data format:
						 * 'multclip' functions support a slightly less capable geometry
						 * model. It can handle polygons with holes, but it cannot handle
						 * polygons with more than one outer ring (parts). This means that
						 * we have to run the check separately against every polygon part
						 * that is not a hole/inner ring! */
						for ( l = 0; l < num_outer_A; l ++ ) {
							geom_multclip_poly poly_A; /* simplified 'multclip' polygon structure */
							poly_A.inner = NULL;
							poly_A.org_inner = NULL;
							poly_A.outer = NULL;
							poly_A.num_inner = 0;
							/* get next outer ring */
							int ring_no = 0;
							for ( r = 0; r < A->num_parts; r ++ ) {
								if ( A->parts[r].is_empty == FALSE ) {
									if ( A->parts[r].is_hole == FALSE ) {
										if ( ring_no == l ) {
											/* we got the outer ring that we're looking for at this loop step */
											poly_A.outer = geom_tools_part_duplicate( &A->parts[r]);
											poly_A.org_outer = r; /* store original part ID */
											/* put vertices into order required by 3rd party 'multclip' */
											geom_tools_sort_part_reverse ( poly_A.outer );
										}
										ring_no ++;
									}
								}
							}
							/* now count all inner rings that lie in the current outer ring */
							for ( r = 0; r < A->num_parts; r ++ ) {
								if ( A->parts[r].is_empty == FALSE ) {
									if ( A->parts[r].is_hole == TRUE ) {
										if ( geom_tools_part_in_part_2D ( &A->parts[r], poly_A.outer ) == TRUE ) {
											poly_A.num_inner ++;
										}
									}
								}
							}
							/* add all inner rings to 'multclip' polygon representation */
							poly_A.inner = malloc (sizeof(geom_part*)*poly_A.num_inner);
							poly_A.org_inner = malloc (sizeof(unsigned int)*poly_A.num_inner);
							int cur = 0;
							for ( r = 0; r < A->num_parts; r ++ ) {
								if ( A->parts[r].is_empty == FALSE ) {
									if ( A->parts[r].is_hole == TRUE ) {
										if ( geom_tools_part_in_part_2D ( &A->parts[r], poly_A.outer ) == TRUE ) {
											poly_A.inner[cur] = geom_tools_part_duplicate( &A->parts[r]);
											poly_A.org_inner[cur] = r; /* store original part ID */
											geom_tools_sort_part_reverse ( poly_A.inner[cur] );
											cur++;
										}
									}
								}
							}

							/* DEBUG: Print vertex lists for A. */
							if ( DEBUG_MORE ) {
								/* dump A for debug purposes */
								fprintf (stderr,"    A (%i of %i):\n", (l+1), num_outer_A);
								fprintf (stderr,"      OUTER RING (CCW): %i\n", poly_A.outer->num_vertices);
								int v;
								int r;
								for ( v = 0; v < poly_A.outer->num_vertices; v ++ ) {
									fprintf (stderr, "[%.3f|%.3f] ", poly_A.outer->X[v], poly_A.outer->Y[v]);
								}
								fprintf (stderr, "\n");
								if ( poly_A.num_inner > 0 ) {
									for ( r = 0; r < poly_A.num_inner; r ++ ) {
										fprintf (stderr,"      INNER RING (CW): %i\n", poly_A.inner[r]->num_vertices);
										for ( v = 0; v < poly_A.inner[r]->num_vertices; v ++ ) {
											fprintf (stderr, "[%.3f|%.3f] ", poly_A.inner[r]->X[v], poly_A.inner[r]->Y[v]);
										}
										fprintf (stderr, "\n");
									}
								}
							}

							/* inner loop: through all outer rings of B */
							for ( m = 0; m < num_outer_B; m ++ ) {
								/* Build B using same pattern as for A. */
								geom_multclip_poly poly_B;
								poly_B.inner = NULL;
								poly_B.outer = NULL;
								poly_B.org_inner = NULL;
								poly_B.num_inner = 0;
								int ring_no = 0;
								for ( r = 0; r < B->num_parts; r ++ ) {
									if ( B->parts[r].is_empty == FALSE ) {
										if ( B->parts[r].is_hole == FALSE ) {
											if ( ring_no == m ) {
												poly_B.outer = geom_tools_part_duplicate( &B->parts[r]);
												poly_B.org_outer = r; /* store original part ID */
												geom_tools_sort_part_reverse ( poly_B.outer );
											}
											ring_no ++;
										}
									}
								}
								for ( r = 0; r < B->num_parts; r ++ ) {
									if ( B->parts[r].is_empty == FALSE ) {
										if ( B->parts[r].is_hole == TRUE ) {
											if ( geom_tools_part_in_part_2D ( &B->parts[r], poly_B.outer ) == TRUE ) {
												poly_B.num_inner ++;
											}
										}
									}
								}
								poly_B.inner = malloc (sizeof(geom_part*)*poly_B.num_inner);
								poly_B.org_inner = malloc (sizeof(unsigned int)*poly_B.num_inner);
								int cur = 0;
								for ( r = 0; r < B->num_parts; r ++ ) {
									if ( B->parts[r].is_empty == FALSE ) {
										if ( B->parts[r].is_hole == TRUE ) {
											if ( geom_tools_part_in_part_2D ( &B->parts[r], poly_B.outer ) == TRUE ) {
												poly_B.inner[cur] = geom_tools_part_duplicate( &B->parts[r]);
												poly_B.org_inner[cur] = r; /* store original part ID */
												geom_tools_sort_part_reverse ( poly_B.inner[cur] );
												cur++;
											}
										}
									}
								}

								/* Compute bounding boxes for simplified polygons. */
								geom_topology_mpoly_compute_bbox_3d ( &poly_A );
								geom_topology_mpoly_compute_bbox_3d ( &poly_B );

								/* DEBUG: Print vertex lists for B. */
								if ( DEBUG_MORE ) {
									/* dump A for debug purposes */
									fprintf (stderr,"    B (%i of %i):\n", (l+1), num_outer_B);
									fprintf (stderr,"      OUTER RING (CCW): %i\n", poly_B.outer->num_vertices);
									int v;
									int r;
									for ( v = 0; v < poly_B.outer->num_vertices; v ++ ) {
										fprintf (stderr, "[%.3f|%.3f] ", poly_B.outer->X[v], poly_B.outer->Y[v]);
									}
									fprintf (stderr, "\n");
									if ( poly_B.num_inner > 0 ) {
										for ( r = 0; r < poly_B.num_inner; r ++ ) {
											fprintf (stderr,"      INNER RING (CW): %i\n", poly_B.inner[r]->num_vertices);
											for ( v = 0; v < poly_B.inner[r]->num_vertices; v ++ ) {
												fprintf (stderr, "[%.3f|%.3f] ", poly_B.inner[r]->X[v], poly_B.inner[r]->Y[v]);
											}
											fprintf (stderr, "\n");
										}
									}
								}

								/* TEST A & B for PRECISE OVERLAP inside this loop HERE! */
								//DEBUG:
								if ( geom_topology_poly_intersect (&poly_A, &poly_B, i, l, j, m, gs, opts ) == TRUE )
								{
									if ( DEBUG ) {
											fprintf (stderr,"\n  OVERLAP (ACCURATE) DETECTED.\n");
									}

									/* Replace all parts in ORIGINAL B with parts from 'multclip' representation! */
									/* OUTER RING */
									geom_part* cur_part = &B->parts[poly_B.org_outer]; /* link with original outer ring in B */
									if ( DEBUG ) {
										fprintf(stderr,"ORIGINAL AREA (OUTER RING):\n");
										int v;
										for ( v = 0; v < cur_part->num_vertices; v ++ ) {
											fprintf(stderr, "\t%lf %lf %lf\n", cur_part->X[v], cur_part->Y[v], cur_part->Z[v] );
										}
									}
									//Reverse vertex order & set first = last vertex
									t_free (cur_part->X);
									t_free (cur_part->Y);
									t_free (cur_part->Z);
									cur_part->X = malloc ( sizeof(double)*(poly_B.outer->num_vertices+1));
									cur_part->Y = malloc ( sizeof(double)*(poly_B.outer->num_vertices+1));
									cur_part->Z = malloc ( sizeof(double)*(poly_B.outer->num_vertices+1));
									cur_part->num_vertices = (poly_B.outer->num_vertices+1);
									int v = 0;
									int w = 0;
									for ( v = poly_B.outer->num_vertices-1; v > -1; v -- ) {
										cur_part->X[w] = poly_B.outer->X[v];
										cur_part->Y[w] = poly_B.outer->Y[v];
										cur_part->Z[w] = poly_B.outer->Z[v];
										w++;
									}
									if ( DEBUG_MORE ) {
										fprintf(stderr,"LAST VERTEX = %i\n", w);
										fprintf(stderr,"NUM VERTICES = %i\n", cur_part->num_vertices);
									}
									cur_part->X[w] = cur_part->X[0];
									cur_part->Y[w] = cur_part->Y[0];
									cur_part->Z[w] = cur_part->Z[0];
									if ( DEBUG ) {
										fprintf(stderr,"REPLACED AREA (OUTER RING):\n");
										int v;
										for ( v = 0; v < cur_part->num_vertices; v ++ ) {
											fprintf(stderr, "\t%lf %lf %lf\n", cur_part->X[v], cur_part->Y[v], cur_part->Z[v] );
										}
									}
									/* INNER RING(S) */
									for ( r = 0; r < poly_B.num_inner; r ++ ) {
										geom_part* cur_part = &B->parts[poly_B.org_inner[r]]; /* link with original inner ring in B */
										if ( DEBUG ) {
											fprintf(stderr,"ORIGINAL AREA (INNER RING %i/%i):\n", (r+1), poly_B.num_inner);
											int v;
											for ( v = 0; v < cur_part->num_vertices; v ++ ) {
												fprintf(stderr, "\t%lf %lf %lf\n", cur_part->X[v], cur_part->Y[v], cur_part->Z[v] );
											}
										}
										//Reverse vertex order & set first = last vertex
										t_free (cur_part->X);
										t_free (cur_part->Y);
										t_free (cur_part->Z);
										cur_part->X = malloc ( sizeof(double)*(poly_B.inner[r]->num_vertices+1));
										cur_part->Y = malloc ( sizeof(double)*(poly_B.inner[r]->num_vertices+1));
										cur_part->Z = malloc ( sizeof(double)*(poly_B.inner[r]->num_vertices+1));
										cur_part->num_vertices = (poly_B.inner[r]->num_vertices+1);
										int v = 0;
										int w = 0;
										for ( v = poly_B.inner[r]->num_vertices-1; v > -1; v -- ) {
											cur_part->X[w] = poly_B.inner[r]->X[v];
											cur_part->Y[w] = poly_B.inner[r]->Y[v];
											cur_part->Z[w] = poly_B.inner[r]->Z[v];
											w++;
										}
										if ( DEBUG_MORE ) {
											fprintf(stderr,"LAST VERTEX = %i\n", w);
											fprintf(stderr,"NUM VERTICES = %i\n", cur_part->num_vertices);
										}
										cur_part->X[w] = cur_part->X[0];
										cur_part->Y[w] = cur_part->Y[0];
										cur_part->Z[w] = cur_part->Z[0];
										if ( DEBUG ) {
											fprintf(stderr,"REPLACED AREA (INNER RING %i/%i):\n", (r+1), poly_B.num_inner);
											int v;
											for ( v = 0; v < cur_part->num_vertices; v ++ ) {
												fprintf(stderr, "\t%lf %lf %lf\n", cur_part->X[v], cur_part->Y[v], cur_part->Z[v] );
											}
										}
									}
									/* Keep count of removed overlap areas. */
									overlaps_removed ++;
								} else {
									if ( DEBUG ) {
										fprintf (stderr,"* NO OVERLAP (ACCURATE) *\n");
									}
								}
								/* Clean up B! */
								geom_tools_part_destroy (poly_B.outer);
								for ( r = 0; r < poly_B.num_inner; r ++ ) {
									geom_tools_part_destroy (poly_B.inner[r]);
								}
								t_free (poly_B.inner);
								t_free (poly_B.org_inner);
							}
							/* Clean up A! */
							geom_tools_part_destroy (poly_A.outer);
							for ( r = 0; r < poly_A.num_inner; r ++ ) {
								geom_tools_part_destroy (poly_A.inner[r]);
							}
							t_free (poly_A.inner);
							t_free (poly_A.org_inner);
						} /* done with current polygon (part) of A */
					} /* Done checking A against B: on to next polygon pair! */
				}
			}
		}
	}
	return ( overlaps_removed );
}


/*
 * Snaps polygon boundary vertices by moving any vertex
 * of a polygon in "gs" that is not part of an inner ring (hole)
 * and that is closer than the user-defined snapping distance
 * to a vertex of another polygon in "gs" to that vertex on the other
 * polygon.
 *
 * If snapping distance is 0.0, then no snapping will be
 * performed.
 *
 * Returns number of snapped vertices;
 */
unsigned int geom_topology_snap_boundaries_2D ( geom_store *gs, options *opts )
{
	unsigned int snaps;
	unsigned int i, j, k, l, m, n;
	geom_store_polygon *A;
	geom_store_polygon *B;
	geom_part *VA, *VB;
	double dist;
	double closest;
	unsigned int candidate;


	if ( opts->snapping == 0.0 ) {
		return ( 0 );
	}

	snaps = 0;
	for ( i = 1; i < gs->num_polygons; i++ ) {
		if ( gs->polygons[i].is_selected == TRUE ) {
			for ( j = 0; j < gs->num_polygons-1; j++ ) {
				if ( i != j ) { /* Do not snap to vertices on the same polygon! */
					A = &gs->polygons[i];
					B = &gs->polygons[j];
					if ( ( A->parts != NULL ) && ( B->parts != NULL ) ) {
						for ( k = 0; k < A->num_parts; k ++ ) {
							if ( A->parts[k].is_hole == FALSE ) {
								VA = &A->parts[k];
								for ( l = 0; l < B->num_parts; l ++ ) {
									if ( B->parts[l].is_hole == FALSE ) {
										/* found two outer boundaries in A and B that need to be check for snapping */
										VB = &B->parts[l];
										/* fprintf (stderr, "*** TOUCH: %i and %i\n", A->geom_id, B->geom_id); */
										/* fprintf ( stderr, "\tVA = %i vertices; VB = %i vertices\n", VA->num_vertices, VB->num_vertices ); */
										for ( m = 0; m < A->parts[k].num_vertices; m ++ ) { //l
											candidate = 0;
											closest = -1.0;
											for ( n = 0; n < B->parts[l].num_vertices; n ++ ) { //k
												/* check distance between all vertices */
												dist = (sqrt(pow((VA->X[m] - VB->X[n]), 2) + pow((VA->Y[m] - VB->Y[n]), 2) ));
												/* fprintf ( stderr, "\tDIST = %.4f\n", dist ); */
												if ( dist <= opts->snapping ) {
													if ( closest < 0.0 ) {
														/* first candidate for snapping to */
														candidate = n;
														closest = dist;
													} else {
														/* closer candidate? */
														if ( dist < closest ) {
															candidate = n;
															closest = dist;
														}
													}
												}
											}
											if ( closest >= 0.0 ) {
												/* need to snap? */
												VA->X[m] = VB->X[candidate];
												VA->Y[m] = VB->Y[candidate];
												/* fprintf ( stderr,		 "\tSNAPPING {%.3f|{%.3f} to {%.3f|{%.3f}.\n",
															VA->X[m], VA->Y[m], VB->X[candidate], VB->Y[candidate] ); */
												snaps ++;												
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/* too large values for snapping distance can degenerate polygons */	
	for ( i = 1; i < gs->num_polygons; i++ ) {
		A = &gs->polygons[i];
		for ( j = 0; j< A->num_parts; j ++ ) {
			int duplicates = 0;				
			/* check for duplicate vertices in every part and warn */
			VA = &A->parts[j];
			//fprintf (stderr, "***\n");
			//fprintf (stderr, "\t %i\n", VA->num_vertices);
			for ( k = 1; k < VA->num_vertices; k ++ ) {
				//fprintf (stderr, "\t\t %.3f/%.3f\n", VA->X[k], VA->Y[k]);				
				int l;				
				for ( l = 1; l < VA->num_vertices; l ++ ) {
					if ( l != k ) {
						if ( ( VA->X[k] == VA->X[l] ) && ( VA->Y[k] == VA->Y[l] ) ) {
							duplicates ++;							
						}
					}
				}
			}
			if ( duplicates > 0 ) {
				err_show (ERR_NOTE, "");
				err_show (ERR_WARN, _("\nDuplicate vertices detected in polygon %i, part %i."), i, j);
				err_show (ERR_WARN, _("\nSnapping distance might be too large (%.f)."), opts->snapping);
				duplicates = 0;
			}
		}
	}
	
	/* TODO: Duplicate vertices are actually a topological error and should be resolved.
	 * Removing them can potentially "collapse" a polygon. If less than three different
	 * vertices remain after deduplication, the polygon is a splinter and should be
	 * removed entirely.
	 * NOTE: First and last vertex are always duplicate, so start at vertex 1, not 0!
	 * */

	return ( snaps );
}


/*
 * High-level function for topological cleaning of lines and polygon
 * boundaries.
 *
 * This function performs stage 1 of topological cleaning of
 * lines and polygon boundaries.
 *
 * It detects intersections between line/boundary segments and adds
 * the coordinates of those intersections to a list managed by the
 * geometry store 'gs'. It then calls the stage 2 function
 * geom_topology_intersections_2D_detect_add() to add all intersections
 * found in a part as new vertices to that part.
 *
 * The argument 'mode' determines which type of intersections to
 * detect:
 *
 *   GEOM_INTERSECT_LINE_LINE
 *   GEOM_INTERSECT_LINE_POLY
 *   GEOM_INTERSECT_POLY_POLY
 *
 * Returns number of intersections detected, "-1" on error.
 */
unsigned int geom_topology_intersections_2D_detect ( geom_store *gs, options *opts, int mode,
		unsigned int*num_added, unsigned int *topo_errors )
{
	unsigned int num_vertices_detected = 0;
	unsigned int num_vertices_added = 0;
	struct geom_part *A = NULL;
	struct geom_part *B = NULL;
	struct geom_part *A_extended_first = NULL;
	struct geom_part *A_extended_last = NULL;
	struct geom_part *B_extended = NULL;
	int i, j, k, l, m = 0;
	double p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y = 0.0;
	double v_x, v_y;
	double dist;
	BOOLEAN DEBUG = FALSE;


	if ( gs == NULL || opts == NULL ) {
		return ( -1 );
	}

	/* DEBUG */
	if ( DEBUG == TRUE ) {
		fprintf ( stderr, "\n*** TOPO CLEAN INTERSECT: ***\n");
		fprintf ( stderr, "\tDangle snapping distance = %.6f\n", opts->dangling);
	}

	/* 1. LINE-LINE INTERSECTIONS */
	if ( mode == GEOM_INTERSECT_LINE_LINE ) {
		/* DEBUG */
		if ( DEBUG == TRUE ) {
			fprintf ( stderr, "\n\t*****************************\n" );
			fprintf ( stderr, "\t*** Checking %u lines. ***\n", gs->num_lines );
			fprintf ( stderr, "\t*****************************\n" );
		}
		for ( i = 0; i < gs->num_lines; i++ ) {
			if ( gs->lines[i].is_selected == TRUE && gs->lines[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->num_lines; j++ ) {
					if ( gs->lines[j].is_selected == TRUE && gs->lines[j].is_empty == FALSE ) {
						/* DEBUG */
						if ( DEBUG == TRUE ) {
							/* fprintf ( stderr, "\n\t\t %i-%i\n", i, j); */
						}
						/* step through all parts of A and check against all parts of B */
						for ( k = 0; k < gs->lines[i].num_parts; k++ ) {
							A = &gs->lines[i].parts[k];
							if ( opts->dangling > 0.0 ) {
								A_extended_first = geom_tools_line_part_extend_3D ( A, opts->dangling, TRUE, FALSE );
								A_extended_last = geom_tools_line_part_extend_3D ( A, opts->dangling, FALSE, TRUE );
							}
							for ( l = 0; l < gs->lines[j].num_parts; l++ ) {
								B = &gs->lines[j].parts[l];

								if ( opts->dangling > 0.0 && A_extended_first != NULL && A_extended_last != NULL ) {
									/* Check for undershoots of line nodes against other line segments:
									 * We extend B to both sides, then we check for intersection
									 * with the first and last extended segments of A, respectively.
									 * If there are any, then we set the associated booleans in
									 * A (i.e. the unextended, original line part) to mark the
									 * undershoot(s). At this time, we do not add any vertices and
									 * we keep the original geometry intact. The function
									 * geom_topology_clean_dangles() will take care of re-extending
									 * segments with undershoots, adding intersection vertices and
									 * cutting off the dangling segments.
									 */
									/* TODO: Checking against an extended version of B allows us to
									  detect "double dangles", but then we have to deal with all
									  kinds of strange overshoot artefacts in the next stage!
									  B_extended = geom_tools_line_part_extend_3D ( B, opts->dangling, TRUE, TRUE ); */
									B_extended = geom_tools_part_duplicate (B);
									if ( B_extended != NULL ) {
										/* undershoots will not be found if two segments are in the same geometry part */
										if ( ( gs->lines[i].geom_id != gs->lines[j].geom_id) || ( k != l) ) {
											/* check for intersection between extended segments: first segment of A */
											p0_x = A_extended_first->X[0];
											p0_y = A_extended_first->Y[0];
											p1_x = A->X[0];
											p1_y = A->Y[0];
											for ( m = 0; m < B_extended->num_vertices - 1; m ++ ) {
												p2_x = B_extended->X[m];
												p2_y = B_extended->Y[m];
												p3_x = B_extended->X[m + 1];
												p3_y = B_extended->Y[m + 1];
												if ( geom_tools_line_intersection_2D (p0_x, p0_y, p1_x, p1_y,
														p2_x, p2_y, p3_x, p3_y, &v_x, &v_y ) == TRUE ) {
													/* We mark this as a _possible_ undershoot:
													 * It is not certain that it actually is one, because what
													 * is an undershoot in relation to one geometry part may be
													 * an overshoot in relation to another (closer) one.
													 * But it is a candidate and we will resolve this later.
													 */
													dist = sqrt(pow((p1_x - v_x), 2) + pow((p1_y - v_y), 2));
													if ( A->is_undershoot_first == FALSE || dist < A->dist_undershoot_first ) {
														/* Store only if there is no undershoot yet, or if this one is closer! */
														A->is_undershoot_first = TRUE;
														A->dist_undershoot_first = dist;
														A->x_undershoot_first = v_x;
														A->y_undershoot_first = v_y;
														if ( DEBUG == TRUE ) {
															fprintf ( stderr, "\n*** UNDERSHOOT FIRST (L-L): %i-%i ***\n", i, j );
															fprintf ( stderr, "\tfrom: %.4f/%.4f to: %.4f/%.4f\n", p1_x, p1_y, v_x, v_y );
															fprintf ( stderr, "\tdist: %.4f\n\n", A->dist_undershoot_first );
														}
													}
												}
											}
											/* check for intersection between extended segments: last segment of A */
											p0_x = A_extended_last->X[A_extended_last->num_vertices - 1];
											p0_y = A_extended_last->Y[A_extended_last->num_vertices - 1];
											p1_x = A->X[A->num_vertices - 1];
											p1_y = A->Y[A->num_vertices - 1];
											for ( m = 0; m < B_extended->num_vertices - 1; m ++ ) {
												p2_x = B_extended->X[m];
												p2_y = B_extended->Y[m];
												p3_x = B_extended->X[m + 1];
												p3_y = B_extended->Y[m + 1];
												if ( geom_tools_line_intersection_2D (p0_x, p0_y, p1_x, p1_y,
														p2_x, p2_y, p3_x, p3_y, &v_x, &v_y ) == TRUE ) {
													dist = sqrt(pow((p1_x - v_x), 2) + pow((p1_y - v_y), 2));
													if ( A->is_undershoot_last == FALSE || dist < A->dist_undershoot_last ) {
														A->is_undershoot_last = TRUE;
														A->dist_undershoot_last = dist;
														A->x_undershoot_last = v_x;
														A->y_undershoot_last = v_y;
														if ( DEBUG == TRUE ) {
															fprintf ( stderr, "\n*** UNDERSHOOT LAST (L-L): %i-%i ***\n", i, j );
															fprintf ( stderr, "\tfrom: %.4f/%.4f to: %.4f/%.4f\n", p1_x, p1_y, v_x, v_y );
															fprintf ( stderr, "\tdist: %.4f\n\n", A->dist_undershoot_last );
														}
													}
												}
											}
										}
										/* release mem for extended B */
										if ( B_extended != NULL ) {
											geom_tools_part_destroy ( B_extended );
											free ( B_extended );
											B_extended = NULL;
										}
									}
								}
								/* We need to run the intersection test as often as we need, until
								 * no more new intersections are detected. */
								do {						
									num_vertices_added = geom_tools_parts_intersection_2D ( A, B, gs,
											GEOM_TYPE_LINE, gs->lines[i].geom_id, k, FALSE );
									if ( num_vertices_added > 0 ) {
										num_vertices_detected += num_vertices_added;
										/* Keep total count of intersection vertices added. */
										if ( num_added != NULL ) {
											*num_added += num_vertices_added;
										}
									} else {
										num_vertices_added = 0;
									}
								} while ( num_vertices_added > 0 );	
										
							}
							if ( opts->dangling > 0.0 ) {
								/* release mem for extended A */
								if ( A_extended_first != NULL ) {
									geom_tools_part_destroy ( A_extended_first );
									free ( A_extended_first );
									A_extended_first = NULL;
								}
								if ( A_extended_last != NULL ) {
									geom_tools_part_destroy ( A_extended_last );
									free ( A_extended_last );
									A_extended_last = NULL;
								}
							}
						}
					}
				}
			}
		}
	}

	/* 2. LINE-POLYGON INTERSECTIONS */
	if ( mode == GEOM_INTERSECT_LINE_POLY ) {
		if ( DEBUG == TRUE ) {
			fprintf ( stderr, "\n\t*****************************\n" );
			fprintf ( stderr, "\t*** Checking %u lines & ***\n", gs->num_lines );
			fprintf ( stderr, "\t*** %u polygons         ***\n", gs->num_polygons );
			fprintf ( stderr, "\t*****************************\n" );
		}
		for ( i = 0; i < gs->num_lines; i++ ) {
			if ( gs->lines[i].is_selected == TRUE && gs->lines[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->num_polygons; j++ ) {
					if ( gs->polygons[j].is_selected == TRUE && gs->lines[j].is_empty == FALSE ) {
						/* DEBUG */
						if ( DEBUG == TRUE ) {
							/* fprintf ( stderr, "\n\t\t %i-%i\n", i, j); */
						}
						/* step through all parts of A and check against all parts of B */
						for ( k = 0; k < gs->lines[i].num_parts; k++ ) {
							A = &gs->lines[i].parts[k];
							if ( opts->dangling > 0.0 ) {
								A_extended_first = geom_tools_line_part_extend_3D ( A, opts->dangling, TRUE, FALSE );
								A_extended_last = geom_tools_line_part_extend_3D ( A, opts->dangling, FALSE, TRUE );
							}
							for ( l = 0; l < gs->polygons[j].num_parts; l++ ) {
								B = &gs->polygons[j].parts[l];
								if ( opts->dangling > 0.0 && A_extended_first != NULL && A_extended_last != NULL ) {
									/* check for undershoots of line nodes against other line segments */
									/* TODO: Checking against an extended version of B allows us to
									  detect "double dangles", but then we have to deal with all
									  kinds of strange overshoot artefacts in the next stage!
									  B_extended = geom_tools_line_part_extend_3D ( B, opts->dangling, TRUE, TRUE ); */
									B_extended = geom_tools_part_duplicate (B);
									if ( B_extended != NULL ) {
										/* undershoots will not be found if two segments are in the same geometry part */
										if ( ( gs->lines[i].geom_id != gs->lines[j].geom_id) || ( k != l) ) {
											/* check for intersection between extended segments: first segment of A */
											p0_x = A_extended_first->X[0];
											p0_y = A_extended_first->Y[0];
											p1_x = A->X[0];
											p1_y = A->Y[0];
											for ( m = 0; m < B_extended->num_vertices - 1; m ++ ) {
												p2_x = B_extended->X[m];
												p2_y = B_extended->Y[m];
												p3_x = B_extended->X[m + 1];
												p3_y = B_extended->Y[m + 1];
												if ( geom_tools_line_intersection_2D (p0_x, p0_y, p1_x, p1_y,
														p2_x, p2_y, p3_x, p3_y, &v_x, &v_y ) == TRUE ) {
													/* we mark this as a _possible_ undershoot */
													dist = sqrt(pow((p1_x - v_x), 2) + pow((p1_y - v_y), 2));
													if ( A->is_undershoot_first == FALSE || dist < A->dist_undershoot_first ) {
														/* Store only if there is no undershoot yet, or if this one is closer! */
														A->is_undershoot_first = TRUE;
														A->dist_undershoot_first = dist;
														A->x_undershoot_first = v_x;
														A->y_undershoot_first = v_y;
														if ( DEBUG == TRUE ) {
															fprintf ( stderr, "\n*** UNDERSHOOT FIRST (L-P): %i-%i ***\n", i, j );
															fprintf ( stderr, "\tfrom: %.4f/%.4f to: %.4f/%.4f\n", p1_x, p1_y, v_x, v_y );
															fprintf ( stderr, "\tdist: %.4f\n\n", A->dist_undershoot_first );
														}
													}
												}
											}
											/* check for intersection between extended segments: last segment of A */
											p0_x = A_extended_last->X[A_extended_last->num_vertices - 1];
											p0_y = A_extended_last->Y[A_extended_last->num_vertices - 1];
											p1_x = A->X[A->num_vertices - 1];
											p1_y = A->Y[A->num_vertices - 1];
											for ( m = 0; m < B_extended->num_vertices - 1; m ++ ) {
												p2_x = B_extended->X[m];
												p2_y = B_extended->Y[m];
												p3_x = B_extended->X[m + 1];
												p3_y = B_extended->Y[m + 1];
												if ( geom_tools_line_intersection_2D (p0_x, p0_y, p1_x, p1_y,
														p2_x, p2_y, p3_x, p3_y, &v_x, &v_y ) == TRUE ) {
													/* we mark this as a _possible_ undershoot */
													dist = sqrt(pow((p1_x - v_x), 2) + pow((p1_y - v_y), 2));
													if ( A->is_undershoot_last == FALSE || dist < A->dist_undershoot_last ) {
														A->is_undershoot_last = TRUE;
														A->dist_undershoot_last = dist;
														A->x_undershoot_last = v_x;
														A->y_undershoot_last = v_y;
														if ( DEBUG == TRUE ) {
															fprintf ( stderr, "\n*** UNDERSHOOT LAST (L-P): %i-%i ***\n", i, j );
															fprintf ( stderr, "\tfrom: %.4f/%.4f to: %.4f/%.4f\n", p1_x, p1_y, v_x, v_y );
															fprintf ( stderr, "\tdist: %.4f\n\n", A->dist_undershoot_last );
														}
													}
												}
											}
										}
										/* release mem for extended B */
										if ( B_extended != NULL ) {
											geom_tools_part_destroy ( B_extended );
											free ( B_extended );
											B_extended = NULL;
										}
									}
								}
								/* We need to run the intersection test as often as we need, until
								 * no more new intersections are detected. */
								do {						
									num_vertices_added = geom_tools_parts_intersection_2D ( A, B, gs,
											GEOM_TYPE_LINE, gs->lines[i].geom_id, k, FALSE );
									if ( num_vertices_added > 0 ) {
										num_vertices_detected += num_vertices_added;
										/* Keep total count of intersection vertices added. */
										if ( num_added != NULL ) {
											*num_added += num_vertices_added;
										}
									} else {
										num_vertices_added = 0;
									}
								} while ( num_vertices_added > 0 );
							}
							if ( opts->dangling > 0.0 ) {
								/* release mem for extended A */
								if ( A_extended_first != NULL ) {
									geom_tools_part_destroy ( A_extended_first );
									free ( A_extended_first );
									A_extended_first = NULL;
								}
								if ( A_extended_last != NULL ) {
									geom_tools_part_destroy ( A_extended_last );
									free ( A_extended_last );
									A_extended_last = NULL;
								}
							}
						}
					}
				}
			}
		}
	}

	/* 3. POLYGON-POLYGON INTERSECTIONS */
	if ( mode == GEOM_INTERSECT_POLY_POLY ) {
	//if ( mode == 10000 ) { //DEBUG: SKIP THIS
		if ( DEBUG == TRUE ) {
			fprintf ( stderr, "\n\t*****************************\n" );
			fprintf ( stderr, "\t*** Checking %u polygons. ***\n", gs->num_polygons );
			fprintf ( stderr, "\t*****************************\n" );
		}
		for ( i = 0; i < gs->num_polygons; i++ ) {
			if ( gs->polygons[i].is_selected == TRUE && gs->polygons[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->num_polygons; j++ ) {
					if ( gs->polygons[j].is_selected == TRUE && gs->polygons[j].is_empty == FALSE ) {
						if ( i != j ) { /* skip polygon intersection with itself */
							if ( DEBUG == TRUE ) {
								fprintf ( stderr, "\n\t\t %i-%i\n", i, j);
							}
							/* step through all parts of A and check against all parts of B */
							for ( k = 0; k < gs->polygons[i].num_parts; k++ ) {
								A = &gs->polygons[i].parts[k];
								for ( l = 0; l < gs->polygons[j].num_parts; l++ ) {
									B = &gs->polygons[j].parts[l];
									/* register intersections (if any) */
									unsigned int num_vertices_before = num_vertices_detected;
									/* We need to run the intersection test as often as we need, until
									 * no more new intersections are detected. */
									do {
										num_vertices_added = geom_tools_parts_intersection_2D ( A, B, gs,
												GEOM_TYPE_POLY, gs->polygons[i].geom_id, k, FALSE );
										if ( num_vertices_added > 0 ) {
											num_vertices_detected += num_vertices_added;
											/* Keep total count of intersection vertices added. */
											if ( num_added != NULL ) {
												*num_added += num_vertices_added;
											}
										} else {
											num_vertices_added = 0;
										}
									} while ( num_vertices_added > 0 );
									if ( num_vertices_detected > num_vertices_before ) {
										err_show (ERR_NOTE,"");
										err_show ( ERR_WARN,_("\nBoundary intersection detected in polygons (IDs %i & %i), part nos %i & %i."),
												gs->polygons[i].geom_id, gs->polygons[j].geom_id, k, l );
										if ( topo_errors != NULL ) {
											*topo_errors = *topo_errors + ( num_vertices_detected - num_vertices_before ) / 2;
										}
									}
								}								
							}
						} else {
							/* If there are still intersetions left, then we have a self-intersection. */
							if ( geom_tools_parts_intersection_2D ( A, B, gs, GEOM_TYPE_POLY, gs->polygons[i].geom_id, k, TRUE ) > 0 ) {
								err_show (ERR_NOTE,"");
								err_show ( ERR_WARN,_("\nSelf-intersection in polygon (ID %i), part no. %i."),
										gs->polygons[i].geom_id, k, gs->polygons[j].geom_id, l );
								if ( topo_errors != NULL ) {
									*topo_errors = *topo_errors + 1;
								}
							}
						}
					}
				}
			}
		}
	}

	return (num_vertices_detected);
}


/*
 * High-level function for topological cleaning of lines and polygon
 * boundaries.
 *
 * This function performs stage 2 of topological cleaning of
 * lines and polygon boundaries.
 *
 * It adds intersections that were previously detected by the stage 1
 * function to the geometries in 'gs', and put on the intersection list
 * by calls to 'geom_tools_new_intersection()'.
 *
 * When done, this function will delete all intersection points from the
 * intersection lists and reset both lists to their default capacities,
 * allocating new default sizes of memory.
 *
 * Returns number of intersections added, "-1" on error.
  */
//TODO: *opts not used in this function: REMOVE parameter
unsigned int geom_topology_intersections_2D_add ( geom_store *gs, options *opts )
{
	unsigned int num_vertices_added = 0;
	unsigned int geom_id = 0;
	unsigned int geom_idx = 0;
	unsigned int part_id = 0;
	double x, y, z = 0.0;
	int position = 0;
	int total_vertices = 0;
	geom_part* old_part = NULL;
	geom_part* new_part = NULL;
	int i, j = 0;


	/* 1. INTERSECTIONS ON LINES */
	/* guard against corrupt geometries (parallel segments) */
	total_vertices = 0;
	for ( i = 0; i < gs->num_lines; i ++ ) {
		for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
			total_vertices += gs->lines[i].parts[j].num_vertices;
		}
	}
	if ( gs->lines_intersections->num_intersections > total_vertices ) {
		return ( 0 );
	}
	for ( i = 0; i < gs->lines_intersections->num_intersections; i ++ ) {
		if ( gs->lines_intersections->added[i] == FALSE ) { /* make sure to add only once! */
			part_id = gs->lines_intersections->part_id[i];
			geom_id = gs->lines_intersections->geom_id[i];
			x = gs->lines_intersections->X[i];
			y = gs->lines_intersections->Y[i];
			z = gs->lines_intersections->Z[i];
			position = gs->lines_intersections->v[i];
			/* retrieve geometry part */
			for ( j = 0; j < gs->num_lines; j ++ ) {
				if ( gs->lines[j].geom_id == geom_id ) {
					if ( part_id < gs->lines[j].num_parts ) {
						old_part = &gs->lines[j].parts[part_id];
						geom_idx = j; /* store address of this geometry */
						break;
					}
				}
			}
			if ( old_part != NULL ) {
				new_part = geom_tools_part_add_vertex ( old_part, position, x, y, z );
				if ( new_part != NULL ) {
					/* swap old part for new part with added intersection vertices */
					geom_tools_part_destroy ( old_part );
					gs->lines[geom_idx].parts[part_id].num_vertices = new_part->num_vertices;
					gs->lines[geom_idx].parts[part_id].X = malloc ( sizeof (double ) * new_part->num_vertices );
					gs->lines[geom_idx].parts[part_id].Y = malloc ( sizeof (double ) * new_part->num_vertices );
					gs->lines[geom_idx].parts[part_id].Z = malloc ( sizeof (double ) * new_part->num_vertices );
					for ( j = 0; j < new_part->num_vertices; j ++ ) {
						gs->lines[geom_idx].parts[part_id].X[j] = new_part->X[j];
						gs->lines[geom_idx].parts[part_id].Y[j] = new_part->Y[j];
						gs->lines[geom_idx].parts[part_id].Z[j] = new_part->Z[j];
					}
					geom_tools_part_destroy ( new_part );
					free ( new_part );
					num_vertices_added ++;
					gs->lines_intersections->added[i] = TRUE; /* register this intersection as "added" */
				}
			}
		}
	}

	/* 2. INTERSECTIONS ON POLYGONS */	
	/* guard against corrupt geometries (parallel segments) */
	total_vertices = 0;
	for ( i = 0; i < gs->num_polygons; i ++ ) {
		for ( j = 0; j < gs->polygons[i].num_parts; j ++ ) {
			total_vertices += gs->polygons[i].parts[j].num_vertices;
		}
	}
	if ( gs->polygons_intersections->num_intersections > total_vertices ) {
		return ( 0 );
	}	
	for ( i = 0; i < gs->polygons_intersections->num_intersections; i ++ ) {
		if ( gs->polygons_intersections->added[i] == FALSE ) { /* make sure to add only once! */
			part_id = gs->polygons_intersections->part_id[i];
			geom_id = gs->polygons_intersections->geom_id[i];
			x = gs->polygons_intersections->X[i];
			y = gs->polygons_intersections->Y[i];
			z = gs->polygons_intersections->Z[i];
			position = gs->polygons_intersections->v[i];
			/* retrieve geometry part */
			for ( j = 0; j < gs->num_polygons; j ++ ) {
				if ( gs->polygons[j].geom_id == geom_id ) {
					if ( part_id < gs->polygons[j].num_parts ) {
						old_part = &gs->polygons[j].parts[part_id];
						geom_idx = j; /* store address of this geometry */
						break;
					}
				}
			}
			if ( old_part != NULL ) {
				new_part = geom_tools_part_add_vertex ( old_part, position, x, y, z );
				if ( new_part != NULL ) {
					/* swap old part for new part with added intersection vertices */
					geom_tools_part_destroy ( old_part );
					gs->polygons[geom_idx].parts[part_id].num_vertices = new_part->num_vertices;
					gs->polygons[geom_idx].parts[part_id].X = malloc ( sizeof (double ) * new_part->num_vertices );
					gs->polygons[geom_idx].parts[part_id].Y = malloc ( sizeof (double ) * new_part->num_vertices );
					gs->polygons[geom_idx].parts[part_id].Z = malloc ( sizeof (double ) * new_part->num_vertices );
					for ( j = 0; j < new_part->num_vertices; j ++ ) {
						gs->polygons[geom_idx].parts[part_id].X[j] = new_part->X[j];
						gs->polygons[geom_idx].parts[part_id].Y[j] = new_part->Y[j];
						gs->polygons[geom_idx].parts[part_id].Z[j] = new_part->Z[j];
					}
					geom_tools_part_destroy ( new_part );
					free ( new_part );
					num_vertices_added ++;
					gs->polygons_intersections->added[i] = TRUE; /* register this intersection as "added" */
				}
			}
		}
	}

	return ( num_vertices_added );
}


/*
 * Snaps/cleans overshoots and undershoots (=dangles) from line geometries. *
 * Returns number of dangles removed, -1 on error.
 */
unsigned int geom_topology_clean_dangles_2D ( geom_store *gs, options *opts, unsigned int *topo_errors,
		unsigned int *num_detected, unsigned int *num_added )
{
	unsigned int num_dangles_cleaned = 0;
	unsigned num_vertices_detected = 0;
	unsigned num_vertices_added = 0;
	double dist = 0.0;
	double p0_x, p0_y, p1_x, p1_y = 0.0;
	geom_part *new_part = NULL;
	geom_part *A, *B = NULL;
	int first, last = 0;
	int next, prev = 0;
	int i, j, k, l = 0;
	int min_vertices_required = 3;
	BOOLEAN remove_first = FALSE;
	BOOLEAN remove_last = FALSE;
	/* BOOLEAN DEBUG = FALSE; */


	if ( opts == NULL || gs == NULL ) {
		return ( -1 );
	}

	/* Clean dangles only if snapping distance > 0! */
	if ( opts->dangling <= 0.0 ) {
		return ( 0 );
	}

	/* 1. CLEAN OVERSHOOTS */
	for ( i = 0; i < gs->num_lines; i++ ) {
		if ( gs->lines[i].is_selected == TRUE && gs->lines[i].is_empty == FALSE ) {
			for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
				/*
				if ( DEBUG == TRUE ) {
					fprintf (stderr, "\nCLEAN OVERSHOOT: GEOM#%i, PART #%i\n", i, j);
					fprintf (stderr, "\tvertices before: %u\n", gs->lines[i].parts[j].num_vertices);
				}
				*/
				remove_first = FALSE;
				remove_last = FALSE;
				min_vertices_required = 3;
				/* by definition, there cannot be an overshoot if we have < 3 vertices */
				if ( gs->lines[i].parts[j].num_vertices >= min_vertices_required ) {
					/* an overshoot can only occur if the first or last vertex is preceded by an intersection vertex */
					/* check first vertex: */
					first = 0;
					next = first + 1;
					p0_x = gs->lines[i].parts[j].X[first];
					p0_y = gs->lines[i].parts[j].Y[first];
					p1_x = gs->lines[i].parts[j].X[next];
					p1_y = gs->lines[i].parts[j].Y[next];
					/* Check if succeeding vertex ('next') is an intersection vertex! */
					if (  geom_tools_is_intersection_vertex ( p1_x, p1_y, gs->lines[i].parts[j].Z[next],
							gs, GEOM_TYPE_LINE, gs->lines[i].geom_id, j ) ) {
						/* all clear: check if it's a dangle */
						dist = sqrt ( pow ( ( p1_x - p0_x ), 2 ) + pow ( ( p1_y - p0_y ), 2 ) );
						if ( dist < opts->dangling ) {
							/* check if we have an undershoot that is closer */
							if ( gs->lines[i].parts[j].is_undershoot_first == FALSE ||
									( gs->lines[i].parts[j].is_undershoot_first == TRUE &&
											gs->lines[i].parts[j].dist_undershoot_first > dist ) ) {
								/* overshoot: remove (delayed) */
								remove_first = TRUE;
								gs->lines[i].parts[j].is_undershoot_first = FALSE;
							}
						}
					}
				}
				/* check how many vertices are left */
				if ( remove_first == TRUE ) {
					min_vertices_required = 4;
				}
				if ( gs->lines[i].parts[j].num_vertices >= min_vertices_required ) {
					last = gs->lines[i].parts[j].num_vertices-1;
					prev = last - 1;
					/* check last vertex: */
					p0_x = gs->lines[i].parts[j].X[last];
					p0_y = gs->lines[i].parts[j].Y[last];
					p1_x = gs->lines[i].parts[j].X[prev];
					p1_y = gs->lines[i].parts[j].Y[prev];
					/* Check if preceding vertex ('prev') is an intersection vertex! */
					if (  geom_tools_is_intersection_vertex ( p1_x, p1_y, gs->lines[i].parts[j].Z[prev],
							gs, GEOM_TYPE_LINE, gs->lines[i].geom_id, j ) ) {
						/* all clear: check if it's a dangle */
						dist = sqrt ( pow ( ( p1_x - p0_x ), 2 ) + pow ( ( p1_y - p0_y ), 2 ) );
						if ( dist < opts->dangling ) {
							/* check if we have an undershoot that is closer */
							if ( gs->lines[i].parts[j].is_undershoot_last == FALSE ||
									( gs->lines[i].parts[j].is_undershoot_last == TRUE &&
											gs->lines[i].parts[j].dist_undershoot_last > dist ) ) {
								/* dangle: remove (delayed) */
								remove_last = TRUE;
								gs->lines[i].parts[j].is_undershoot_last = FALSE;
							}
						}
					}
				} else {
					/* this is an extreme case, we have two tiny segments: don't touch */
					remove_first = FALSE;
					remove_last = FALSE;
					err_show (ERR_NOTE,"");
					err_show ( ERR_WARN,_("\nOvershoot correction would delete entire line (ID %i, part no. %i). Please fix manually."),
							gs->lines[i].geom_id, j );
					if ( topo_errors != NULL ) {
						*topo_errors = *topo_errors + 1;
					}
				}
				/* do actual removal(s) now */
				if ( remove_first == TRUE ) {
					new_part = geom_tools_line_part_remove_first_vertex ( &gs->lines[i].parts[j] );
					if ( new_part != NULL ) {
						/* copy new vertices of part into geometry store */
						geom_tools_part_destroy ( &gs->lines[i].parts[j] );
						gs->lines[i].parts[j].num_vertices--;
						gs->lines[i].parts[j].X = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						gs->lines[i].parts[j].Y = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						gs->lines[i].parts[j].Z = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k ++ ) {
							gs->lines[i].parts[j].X[k] = new_part->X[k];
							gs->lines[i].parts[j].Y[k] = new_part->Y[k];
							gs->lines[i].parts[j].Z[k] = new_part->Z[k];
						}
						geom_tools_part_destroy ( new_part );
						free ( new_part );
						num_dangles_cleaned ++;

						/*
						if ( DEBUG == TRUE ) {
							fprintf (stderr, "\tvertices after del first: %i\n", gs->lines[i].parts[j].num_vertices);
						}
						*/
					}
				}
				if ( remove_last == TRUE ) {
					new_part = geom_tools_line_part_remove_last_vertex ( &gs->lines[i].parts[j] );
					if ( new_part != NULL ) {
						/* copy new vertices of part into geometry store */
						geom_tools_part_destroy ( &gs->lines[i].parts[j] );
						gs->lines[i].parts[j].num_vertices--;
						gs->lines[i].parts[j].X = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						gs->lines[i].parts[j].Y = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						gs->lines[i].parts[j].Z = malloc ( sizeof (double) * gs->lines[i].parts[j].num_vertices );
						for ( k = 0; k < gs->lines[i].parts[j].num_vertices; k ++ ) {
							gs->lines[i].parts[j].X[k] = new_part->X[k];
							gs->lines[i].parts[j].Y[k] = new_part->Y[k];
							gs->lines[i].parts[j].Z[k] = new_part->Z[k];
						}
						geom_tools_part_destroy ( new_part );
						free ( new_part );
						num_dangles_cleaned ++;
						/*
						if ( DEBUG == TRUE ) {
							fprintf (stderr, "\tvertices after del last: %i\n", gs->lines[i].parts[j].num_vertices);
						}
						*/
					}
				}
				/*
				if ( DEBUG == TRUE ) {
					fprintf (stderr, "DONE.\n\n");
				}
				*/
			}
		}
	}

	/* 2. CLEAN UNDERSHOOTS */
	for ( i = 0; i < gs->num_lines; i++ ) {
		if ( gs->lines[i].is_selected == TRUE && gs->lines[i].is_empty == FALSE ) {
			for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
				if ( gs->lines[i].parts[j].is_undershoot_first == TRUE ) {
					/*
					if ( DEBUG == TRUE ) {
						fprintf (stderr, "\nCLEAN UNDERSHOOT (FIRST): GEOM#%i, PART #%i\n", i, j);
						fprintf (stderr, "\tDistance: %.4f\n", gs->lines[i].parts[j].dist_undershoot_first);
					}
					*/
					new_part = geom_tools_line_part_extend_3D ( &gs->lines[i].parts[j],
							gs->lines[i].parts[j].dist_undershoot_first, TRUE, FALSE );
					/* we only need to replace the coordinates of the first vertex */
					if ( new_part != NULL ) {
						/* TODO: We take the 2D coordinates of the undershoot here,
						   which can lead to inaccurate 3D line geometry! */
						gs->lines[i].parts[j].X[0] = gs->lines[i].parts[j].x_undershoot_first;
						gs->lines[i].parts[j].Y[0] = gs->lines[i].parts[j].y_undershoot_first;
						/* gs->lines[i].parts[j].X[0] = new_part->X[0]; */
						/* gs->lines[i].parts[j].Y[0] = new_part->Y[0]; */
						gs->lines[i].parts[j].Z[0] = new_part->Z[0];
						geom_tools_part_destroy ( new_part );
						free ( new_part );
						num_dangles_cleaned ++;
					}
				}
				if ( gs->lines[i].parts[j].is_undershoot_last == TRUE ) {
					/*
					if ( DEBUG == TRUE ) {
						fprintf (stderr, "\nCLEAN UNDERSHOOT (LAST): GEOM#%i, PART #%i\n", i, j);
						fprintf (stderr, "\tDistance: %.4f\n", gs->lines[i].parts[j].dist_undershoot_last);
					}
					*/
					new_part = geom_tools_line_part_extend_3D ( &gs->lines[i].parts[j],
							gs->lines[i].parts[j].dist_undershoot_last, FALSE, TRUE );
					/* we only need to replace the coordinates of the first vertex */
					if ( new_part != NULL ) {
						/* TODO: We take the 2D coordinates of the undershoot here,
						   which can lead to inaccurate 3D line geometry! */
						gs->lines[i].parts[j].X[gs->lines[i].parts[j].num_vertices-1] = gs->lines[i].parts[j].x_undershoot_last;
						gs->lines[i].parts[j].Y[gs->lines[i].parts[j].num_vertices-1] = gs->lines[i].parts[j].y_undershoot_last;
						/* gs->lines[i].parts[j].X[gs->lines[i].parts[j].num_vertices-1] = new_part->X[new_part->num_vertices-1]; */
						/* gs->lines[i].parts[j].Y[gs->lines[i].parts[j].num_vertices-1] = new_part->Y[new_part->num_vertices-1]; */
						gs->lines[i].parts[j].Z[gs->lines[i].parts[j].num_vertices-1] = new_part->Z[new_part->num_vertices-1];
						geom_tools_part_destroy ( new_part );
						free ( new_part );
						num_dangles_cleaned ++;
					}
				}
			}
		}
	}

	/* 3. CHECK ALL LINES FOR INTERSECTIONS ONE MORE TIME */
	for ( i = 0; i < gs->num_lines; i++ ) {
		if ( gs->lines[i].is_selected == TRUE && gs->lines[i].is_empty == FALSE ) {
			for ( j = 0; j < gs->num_lines; j++ ) {
				if ( gs->lines[j].is_selected == TRUE && gs->lines[j].is_empty == FALSE ) {
					/* step through all parts of A and check against all parts of B */
					for ( k = 0; k < gs->lines[i].num_parts; k++ ) {
						A = &gs->lines[i].parts[k];
						for ( l = 0; l < gs->lines[j].num_parts; l++ ) {
							B = &gs->lines[j].parts[l];
							/* Register intersections (if any). */							
							/* We need to run the intersection test as often as we need, until
							 * no more new intersections are detected. */
							do {						
								num_vertices_added = geom_tools_parts_intersection_2D ( A, B, gs,
										GEOM_TYPE_LINE, gs->lines[i].geom_id, k, FALSE );
								if ( num_vertices_added > 0 ) {
									num_vertices_detected += num_vertices_added;
									/* Keep total count of intersection vertices detected. */
									if ( num_detected != NULL ) {
										*num_detected += num_vertices_detected;
									}
									/* Keep total count of intersection vertices added. */
									if ( num_added != NULL ) {
										*num_added += num_vertices_added;
									}
								} else {
									num_vertices_added = 0;
								}
							} while ( num_vertices_added > 0 );
						}
					}
				}
			}
		}
	}

	return ( num_dangles_cleaned );
}




/*
 * Helper function for geom_topology_sort_vertices().
 *
 * Determines whether the vertices of a polygon part are in
 * clockwise or counter clockwise order. This is a robust
 * method that will even work for self-intersecting polygons
 * (in which case it returns the order for the majority
 * of vertices).
 *
 * Returns:
 * 	GEOM_WINDING_CW = vertices are in clockwise order
 *  GEOM_WINDING_CCW = vertices are in counter clockwise order
 *  GEOM_WINDING_UNKNOWN = error/unable to determine order
 */
int geom_get_vertex_order ( geom_part *poly ) {
	int result = GEOM_WINDING_UNKNOWN; /* indeterminable by default */

	if ( poly != NULL && poly->is_empty == FALSE ) {
		double sum = 0.0;
		int i;
		for ( i = 1; i < poly->num_vertices; i ++ ) {
			/* this works, because the polygon is closed, i.e. first == last vertex */
			sum += ( poly->X[i] - poly->X[i-1] ) * ( poly->Y[i] + poly->Y[i-1] );
		}
		if ( sum < 0 ) {
			/* counter clockwise */
			result = GEOM_WINDING_CCW;
		} else {
			/* clockwise */
			result = GEOM_WINDING_CW;
		}
	}

	return ( result );
}


/**
 * Helper function for use by any function that uses 3rd party
 * 'multclip' operations: This library requires that vertices
 * on all outer polygon boundaries are sorted in CCW order, and
 * vice versa for inner boundaries ('holes'). This function will
 * _modify_(!) the polygon geometry part passed to it so that the
 * above winding rule is enforced.
 */
void geom_tools_sort_part_reverse ( geom_part *poly_part )
{
	int i;

	if ( poly_part == NULL ) {
		return;
	}

	int result = geom_get_vertex_order ( poly_part );

	if ( ( 	poly_part->is_hole == FALSE && result == GEOM_WINDING_CW ) ||
		(	poly_part->is_hole == TRUE && result == GEOM_WINDING_CCW ) ) {
		/* Vertices need reordering! */
		int first = 0;
		int last = poly_part->num_vertices-1;
		for ( i = 0; i < (int)(poly_part->num_vertices/2); i ++) {
			/* swap X */
			double copy = poly_part->X[first];
			poly_part->X[first] = poly_part->X[last];
			poly_part->X[last] = copy;
			/* swap Y */
			copy = poly_part->Y[first];
			poly_part->Y[first] = poly_part->Y[last];
			poly_part->Y[last] = copy;
			/* swap Z */
			copy = poly_part->Z[first];
			poly_part->Z[first] = poly_part->Z[last];
			poly_part->Z[last] = copy;
			/* update index counters */
			first ++;
			last --;
		}
	}
}


/*
 * This function sorts the vertices of all polygons into a
 * defined order (different output formats require different
 * vertex/winding orders).
 *
 * The order is defined by "mode" and can be passed as:
 *
 * GEOM_WINDING_CW (clockwise)
 *
 * GEOM_WINDING_CCW (counter clockwise)
 *
 * If mode == GEOM_WINDING_AUTO:
 *
 * - Vertices of outer rings (polygon boundaries) are sorted
 *   in clockwise order.
 * - Vertices of inner rings (holes) are sorted in counter-
 *   clockwise order.
 *
 * GEOM_WINDING_REVERSE = the opposite of GEOM_WINDING_AUTO
 *
 * Nothing will be done if the vertices already have the correct
 * order or the sort order cannot be determined (in the latter
 * case a warning message will also be issued).
 *
 * The returned value is the total number of polygon rings
 * (boundaries and holes) for which the sort order was changed.
 */
unsigned int geom_topology_sort_vertices ( geom_store *gs, int mode ) {
	unsigned int num_sorted = 0;
	int i, j, k;
	int result;
	/* possible vertex order test outcomes */
	int error = -1;


	/* unknown ordering mode */
	if ( mode != GEOM_WINDING_CW && mode != GEOM_WINDING_CCW && mode != GEOM_WINDING_AUTO ) {
		return ( 0 );
	}

	/* CLOCKWISE SORTING */
	if ( mode == GEOM_WINDING_CW ) {
		for ( i = 0; i < gs->num_polygons; i++ ) {
			if ( gs->polygons[i].is_selected == TRUE && gs->polygons[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					result = geom_get_vertex_order ( &gs->polygons[i].parts[j] );
					if ( result == GEOM_WINDING_CCW ) {
						/* Vertices need reordering! */
						int first = 0;
						int last = gs->polygons[i].parts[j].num_vertices-1;
						for ( k = 0; k < (int)(gs->polygons[i].parts[j].num_vertices/2); k ++) {
							/* swap X */
							double copy = gs->polygons[i].parts[j].X[first];
							gs->polygons[i].parts[j].X[first] = gs->polygons[i].parts[j].X[last];
							gs->polygons[i].parts[j].X[last] = copy;
							/* swap Y */
							copy = gs->polygons[i].parts[j].Y[first];
							gs->polygons[i].parts[j].Y[first] = gs->polygons[i].parts[j].Y[last];
							gs->polygons[i].parts[j].Y[last] = copy;
							/* swap Z */
							copy = gs->polygons[i].parts[j].Z[first];
							gs->polygons[i].parts[j].Z[first] = gs->polygons[i].parts[j].Z[last];
							gs->polygons[i].parts[j].Z[last] = copy;
							/* update index counters */
							first ++;
							last --;
						}
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nForced vertices of inner ring (hole) into clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nForced vertices of outer ring (boundary) into clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
						num_sorted ++; /* Resorting cannot really fail. */
					} else if ( result == error ) {
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of inner ring (hole)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of outer ring (boundary)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
					}
				}
			}
		}
	}

	/* COUNTER CLOCKWISE SORTING */
	if ( mode == GEOM_WINDING_CCW ) {
		for ( i = 0; i < gs->num_polygons; i++ ) {
			if ( gs->polygons[i].is_selected == TRUE && gs->polygons[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					result = geom_get_vertex_order ( &gs->polygons[i].parts[j] );
					if ( result == GEOM_WINDING_CW ) {
						/* Vertices need reordering! */
						int first = 0;
						int last = gs->polygons[i].parts[j].num_vertices-1;
						for ( k = 0; k < (int)(gs->polygons[i].parts[j].num_vertices/2); k ++) {
							/* swap X */
							double copy = gs->polygons[i].parts[j].X[first];
							gs->polygons[i].parts[j].X[first] = gs->polygons[i].parts[j].X[last];
							gs->polygons[i].parts[j].X[last] = copy;
							/* swap Y */
							copy = gs->polygons[i].parts[j].Y[first];
							gs->polygons[i].parts[j].Y[first] = gs->polygons[i].parts[j].Y[last];
							gs->polygons[i].parts[j].Y[last] = copy;
							/* swap Z */
							copy = gs->polygons[i].parts[j].Z[first];
							gs->polygons[i].parts[j].Z[first] = gs->polygons[i].parts[j].Z[last];
							gs->polygons[i].parts[j].Z[last] = copy;
							/* update index counters */
							first ++;
							last --;
						}
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nForced vertices of inner ring (hole) into counter clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nForced vertices of outer ring (boundary) into counter clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
						num_sorted ++; /* Resorting cannot really fail. */
					} else if ( result == error ) {
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of inner ring (hole)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of outer ring (boundary)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
					}
				}
			}
		}
	}

	/* AUTOMATIC SORTING */
	if ( mode == GEOM_WINDING_AUTO ) {
		/* Step through all polygons and check if a ring needs reordering. */
		for ( i = 0; i < gs->num_polygons; i++ ) {
			if ( gs->polygons[i].is_selected == TRUE && gs->polygons[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					result = geom_get_vertex_order ( &gs->polygons[i].parts[j] );
					if ( ( 	gs->polygons[i].parts[j].is_hole == TRUE && result == GEOM_WINDING_CW ) ||
							(	gs->polygons[i].parts[j].is_hole == FALSE && result == GEOM_WINDING_CCW ) ) {
						/* Vertices need reordering! */
						int first = 0;
						int last = gs->polygons[i].parts[j].num_vertices-1;
						for ( k = 0; k < (int)(gs->polygons[i].parts[j].num_vertices/2); k ++) {
							/* swap X */
							double copy = gs->polygons[i].parts[j].X[first];
							gs->polygons[i].parts[j].X[first] = gs->polygons[i].parts[j].X[last];
							gs->polygons[i].parts[j].X[last] = copy;
							/* swap Y */
							copy = gs->polygons[i].parts[j].Y[first];
							gs->polygons[i].parts[j].Y[first] = gs->polygons[i].parts[j].Y[last];
							gs->polygons[i].parts[j].Y[last] = copy;
							/* swap Z */
							copy = gs->polygons[i].parts[j].Z[first];
							gs->polygons[i].parts[j].Z[first] = gs->polygons[i].parts[j].Z[last];
							gs->polygons[i].parts[j].Z[last] = copy;
							/* update index counters */
							first ++;
							last --;
						}
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nForced vertices of inner ring (hole) into counter-clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nForced vertices of outer ring (boundary) into clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
						num_sorted ++; /* Resorting cannot really fail. */
					} else if ( result == error ) {
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of inner ring (hole)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of outer ring (boundary)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
					}
				}
			}
		}
	}

	/* REVERSED AUTOMATIC SORTING */
	if ( mode == GEOM_WINDING_REVERSE ) {
		/* Step through all polygons and check if a ring needs reordering. */
		for ( i = 0; i < gs->num_polygons; i++ ) {
			if ( gs->polygons[i].is_selected == TRUE && gs->polygons[i].is_empty == FALSE ) {
				for ( j = 0; j < gs->polygons[i].num_parts; j++ ) {
					result = geom_get_vertex_order ( &gs->polygons[i].parts[j] );
					if ( ( 	gs->polygons[i].parts[j].is_hole == FALSE && result == GEOM_WINDING_CW ) ||
							(	gs->polygons[i].parts[j].is_hole == TRUE && result == GEOM_WINDING_CCW ) ) {
						/* Vertices need reordering! */
						int first = 0;
						int last = gs->polygons[i].parts[j].num_vertices-1;
						for ( k = 0; k < (int)(gs->polygons[i].parts[j].num_vertices/2); k ++) {
							/* swap X */
							double copy = gs->polygons[i].parts[j].X[first];
							gs->polygons[i].parts[j].X[first] = gs->polygons[i].parts[j].X[last];
							gs->polygons[i].parts[j].X[last] = copy;
							/* swap Y */
							copy = gs->polygons[i].parts[j].Y[first];
							gs->polygons[i].parts[j].Y[first] = gs->polygons[i].parts[j].Y[last];
							gs->polygons[i].parts[j].Y[last] = copy;
							/* swap Z */
							copy = gs->polygons[i].parts[j].Z[first];
							gs->polygons[i].parts[j].Z[first] = gs->polygons[i].parts[j].Z[last];
							gs->polygons[i].parts[j].Z[last] = copy;
							/* update index counters */
							first ++;
							last --;
						}
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nForced vertices of inner ring (hole) into clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nForced vertices of outer ring (boundary) into counter-clockwise order\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
						num_sorted ++; /* Resorting cannot really fail. */
					} else if ( result == error ) {
						err_show (ERR_NOTE,"");
						if ( gs->polygons[i].parts[j].is_hole == TRUE ) {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of inner ring (hole)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						} else {
							err_show (ERR_WARN, _("\nUnable to determine vertex order of outer ring (boundary)\n(part #%i, read from '%s', line %u)."),
									j, gs->polygons[i].source, gs->polygons[i].line );
						}
					}
				}
			}
		}
	}

	return ( num_sorted );
}




/*
 * For topologically correct data, we must make sure that all
 * vertices of one polygon lie in the same plane. Planarization
 * may also be desirable for polylines features or even fields
 * of points if small differences due to micro-topgraphy and
 * measurement errors are to be discarded.
 * There are two modes for this.
 *
 * GEOM_PLANAR_MODE_CONST
 * All Z coordinates will be set to the Z coordinate of the first vertex
 *
 * GEOM_PLANAR_MODE_TREND
 * A trend surface ("plane equation") will be fitted through the vertices of
 * the geometry and the coordinates of all vertices will then be projected
 * onto that plane:
 *
 * http://cs.haifa.ac.il/~gordon/plane.pdf
 * http://tog.acm.org/resources/GraphicsGems/gemsiii/newell.c
 * http://www.matheboard.de/archive/401323/thread.html
 *
 * http://mathworld.wolfram.com/PlanarPolygon.html
 *
 */
void geom_topology_planarize() {
	/* TODO */
}




/*
 * Reprojects/re-orients X/Y/Z input measurements into a synthetic
 * local Y-Z system. Caller must make sure that input data has
 * valid "Z" data!
 */
void geom_reorient_local_xz(parser_data_store *storage) {

	if ( storage == NULL ) {
		return;
	}

	if ( storage->num_records < 1 ) {
		return;
	}

	/* key parameters */
	int refPoint = -1; /* index of vertex used to determine rough profile orientation */
	/* true extreme coordinates of data */
	double minX = 0.0;
	double maxX = 0.0;
	int minXIdx = -1;
	int maxXIdx = -1;
	/* Z statistics for weighing measurement distances */
	double maxDZ;
	double *weights = NULL;


	{
		weights = malloc (sizeof (double) * storage->num_records);
		int i;
		for ( i = 0; i < storage->num_records; i ++ ) {
			weights[i] = 1.0; /* initial value for all weights */
		}
	}

	/* Step 1: Set first valid, non-empty input vertex as reference point (RP) */
	{
		int i;
		for ( i = 0; i < storage->num_records; i ++ ) {
			if ( storage->records[i].is_empty == FALSE && storage->records[i].is_valid == TRUE ) {
				refPoint = i;
				minX = storage->records[i].x; /* proper X minimum will be determined in the next step */
				maxX = storage->records[i].x; /* ditto */
				minXIdx = i;
				maxXIdx = i;
				break;
			}
		}
	}
	if ( refPoint < 0 ) {
		/* no valid data found */
		t_free ( weights );
		return;
	}

	/* Step 2: Compute weights for all X coordinates */
	{
		int i;
		BOOLEAN maxSet = FALSE;
		/* get maximal Z difference between reference point and any other */
		for ( i = 0; i < storage->num_records; i ++ ) {
			if ( storage->records[i].is_empty == FALSE && storage->records[i].is_valid == TRUE ) {
				if ( i != refPoint ) {
					if ( maxSet == FALSE ) {
						maxDZ = fabs(storage->records[refPoint].z - storage->records[i].z);
						maxSet = TRUE;
					} else {
						double d = fabs(storage->records[refPoint].z - storage->records[i].z);
						if (d > maxDZ) {
							maxDZ = d;
						}
					}
				}
			}
		}

		if ( maxDZ == 0.0 ) {
			/* extreme case: guard against div by zero */
			maxDZ = 1.0; /* essentially: no weighting applied */
		}

		/* normalize weights to range [0;1] */
		for ( i = 0; i < storage->num_records; i ++ ) {
			if ( storage->records[i].is_empty == FALSE && storage->records[i].is_valid == TRUE ) {
				if ( i != refPoint ) {
					double d = fabs(storage->records[refPoint].z - storage->records[i].z);
					double norm = d / maxDZ;
					weights[i] = 1 - (norm);
				}
			}
		}
	}

	/* Step 3: Determine profile orientation and true left-most X coordinate of input data */
	{
		double sumOfDists = 0.0;
		{
			int i;
			for ( i = 0; i < storage->num_records; i ++ ) {
				if ( storage->records[i].is_empty == FALSE && storage->records[i].is_valid == TRUE ) {
					if ( i != refPoint ) {
						/* compute weighted sum of X deltas */
						sumOfDists += (storage->records[i].x - storage->records[refPoint].x) * weights[i];
						/* update min/max stats */
						if ( storage->records[i].x < minX ) {
							minX = storage->records[i].x;
							minXIdx = i;
						}
						if ( storage->records[i].x > maxX ) {
							maxX = storage->records[i].x;
							maxXIdx = i;
						}
					}
				}
			}
		}

		/* set profile orientation and find new reference point with true smallest X */
		if ( sumOfDists > 0.0 ) { /* extreme case */
			err_show (ERR_NOTE, _("\nLocal profile orientation determined to be 'eastward'."));
			err_show (ERR_NOTE, _("Min. (left-most)  input X coordinate is: '%f'."), minX);
			err_show (ERR_NOTE, _("Max. (right-most) input X coordinate is: '%f'."), maxX);
			refPoint = minXIdx;
		}
		if ( sumOfDists < 0.0 ) {
			err_show (ERR_NOTE, _("\nLocal profile orientation determined to be 'westward'."));
			err_show (ERR_NOTE, _("Min. (left-most)  input X coordinate is: '%f'."), minX);
			err_show (ERR_NOTE, _("Max. (right-most) input X coordinate is: '%f'."), maxX);
			refPoint = maxXIdx;
		}
		if ( sumOfDists == 0.0 ) {
			err_show (ERR_WARN, _("\nUnable to determine profile orientation. Set to default 'westward'."));
			err_show (ERR_NOTE, _("Min. (left-most)  input X coordinate is: '%f'."), minX);
			err_show (ERR_NOTE, _("Max. (right-most) input X coordinate is: '%f'."), maxX);
			refPoint = minXIdx;
		}
	}

	/* Step 4: step through all measurements and reproject X/Y to X/Z */
	{
		int i;
		for ( i = 0; i < storage->num_records; i ++ ) {
			if ( storage->records[i].is_empty == FALSE && storage->records[i].is_valid == TRUE ) {
				/* set transformed coordinates (NOTE: coordinates of reference point are set _after_ this loop!) */
				if ( i != refPoint ) {
					double x_new;
					double y_new;
					double z_new;
					/* new X is straight line distance between X/Y of this measurement and the reference point */
					double t1 = storage->records[refPoint].x - storage->records[i].x;
					double t2 = storage->records[refPoint].y - storage->records[i].y;
					x_new = sqrt( (t1*t1) + (t2*t2) ); /* argument is always positive */
					/* new Y is old Z */
					y_new = storage->records[i].z;
					/* Z simply becomes "0.0" */
					z_new = 0.0;
					/* store new coordinates */
					storage->records[i].x = x_new;
					storage->records[i].y = y_new;
					storage->records[i].z = z_new;
				}
			}
		}
	}

	/* coordinates of reference point are always "0/z/0" */
	storage->records[refPoint].x = 0.0;
	storage->records[refPoint].y = storage->records[refPoint].z;
	storage->records[refPoint].z = 0.0;

	/* clean up */
	t_free ( weights );
}




/*
 * Checks if the stored coordinates in the provided
 * geometry store have an actual extent on the Z axis.
 */
BOOLEAN geom_ds_has_z(parser_data_store *storage) {
	int i;
	double min;
	double max;
	for ( i = 0; i < storage->num_records ; i ++ ) {
		double z = storage->records[i].z;
		if ( i == 0 ) {
			min = z;
			max = z;
		} else {
			if (  z < min ) {
				min = z;
			}
			else if (  z > max ) {
				max = z;
			}
		}
	}
	return ( (max - min) > 0.0 );
}
