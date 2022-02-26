
/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	main.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Flexible software for parsing ASCII survey data files, building
 *           	geometries and attribute data from lines containing measurements
 *           	and exporting the result to a variety of output formats.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

/*
 * [List of open issues moved to TODO]
 */


#define MAIN

#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "global.h"

#include "errors.h"
#include "export.h"
#include "selections.h"
#include "geom.h"
#include "gui_conf.h"
#include "gui_field.h"
#include "gui_form.h"
#include "i18n.h"
#include "options.h"
#include "reproj.h"
#include "parser.h"
#include "tools.h"

/* these are defined 'extern' in global.h */
char *PRG_NAME_CLI;
char *PRG_PATH_CLI;
char *PRG_DIR_CLI;

#ifdef GUI
#include "logo.xpm"
#include <gtk/gtk.h>
#endif


/* field widgets for GUI form */
#ifdef GUI
gui_form *gform;
gui_field *f_input;
gui_field *f_select;
gui_field *f_label_field;
gui_field *f_label_mode_point;
gui_field *f_label_mode_line;
gui_field *f_label_mode_poly;
gui_field *f_orient_mode;
gui_field *f_topo_level;
gui_field *f_parser;
gui_field *f_output;
gui_field *f_name;
gui_field *f_format;
gui_field *f_log;
gui_field *f_tolerance;
gui_field *f_snapping;
gui_field *f_dangling;
gui_field *f_x_offset;
gui_field *f_y_offset;
gui_field *f_z_offset;
gui_field *f_d_places;
gui_field *f_decimal_point;
gui_field *f_decimal_group;
gui_field *f_proj_in;
gui_field *f_proj_out;
gui_field *f_proj_dx;
gui_field *f_proj_dy;
gui_field *f_proj_dz;
gui_field *f_proj_rx;
gui_field *f_proj_ry;
gui_field *f_proj_rz;
gui_field *f_proj_ds;
gui_field *f_proj_grid;
gui_field *f_2d;
gui_field *f_strict;
gui_field *f_validate_only;
gui_field *f_dump_raw;
#endif


/*
 * Initialization message.
 */
void show_init_msg ( options *opts )
{
	time_t now;
	int i;

	time( &now );
	err_show (ERR_NOTE, _("\n* Initialized %s; %s"), t_get_prg_name_and_version(), ctime(&now));
	if ( opts->num_input > 0 ) {
		err_show (ERR_NOTE, _("* Input file(s):"));
		for ( i=0; i < opts->num_input ; i ++ )
			err_show (ERR_NOTE, _("%s"), opts->input[i]);
	} else {
		if ( opts->just_dump_parser == FALSE ) {
			err_show (ERR_EXIT, _("No input files."));
			return;
		}
	}
	err_show (ERR_NOTE, _("\n* Settings:"));
	err_show (ERR_NOTE, _("Parser schema file: %s"), opts->schema_file);
	err_show (ERR_NOTE, _("Output stored in: %s"), opts->output);
	err_show (ERR_NOTE, _("Base name for output: %s"), opts->base);
	if ( opts->label_field != NULL ) {
		err_show (ERR_NOTE, _("Label settings:"));
		err_show (ERR_NOTE, _("\tText field: %s"), opts->label_field);
		err_show (ERR_NOTE, _("\tPlacement (points): %s"), OPTIONS_LABEL_MODE_NAMES[opts->label_mode_point]);
		err_show (ERR_NOTE, _("\tPlacement (lines): %s"), OPTIONS_LABEL_MODE_NAMES[opts->label_mode_line]);
		err_show (ERR_NOTE, _("\tPlacement (polygons): %s"), OPTIONS_LABEL_MODE_NAMES[opts->label_mode_poly]);
	}
	err_show (ERR_NOTE, _("SRS and reprojection:"));
	char *srs_in = NULL;
	if ( opts->proj_in != NULL ) {
		srs_in = strdup(opts->proj_in);
	} else {
		srs_in = strdup(REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL]);
	}
	err_show (ERR_NOTE, _("\tInput SRS: '%s'"), srs_in);
	if ( opts->wgs84_trans_grid != NULL ) {
		err_show (ERR_NOTE, _("\tDatum transformation grid: '%s'"), opts->wgs84_trans_grid);
	}
	if ( 	opts->wgs84_trans_dx != OPTIONS_DEFAULT_WGS84_TRANS_DX ||
			opts->wgs84_trans_dy != OPTIONS_DEFAULT_WGS84_TRANS_DY ||
			opts->wgs84_trans_dz != OPTIONS_DEFAULT_WGS84_TRANS_DZ ||
			opts->wgs84_trans_rx != OPTIONS_DEFAULT_WGS84_TRANS_RX ||
			opts->wgs84_trans_ry != OPTIONS_DEFAULT_WGS84_TRANS_RY ||
			opts->wgs84_trans_rz != OPTIONS_DEFAULT_WGS84_TRANS_RZ ||
			opts->wgs84_trans_ds != OPTIONS_DEFAULT_WGS84_TRANS_DS
	) {
		err_show (ERR_NOTE, _("\tDatum transformation parameters:"));
		err_show (ERR_NOTE, _("\tX shift from WGS 84: %f"), opts->wgs84_trans_dx);
		err_show (ERR_NOTE, _("\tY shift from WGS 84: %f"), opts->wgs84_trans_dy);
		err_show (ERR_NOTE, _("\tZ shift from WGS 84: %f"), opts->wgs84_trans_dz);
		err_show (ERR_NOTE, _("\tX rotation from WGS 84: %f"), opts->wgs84_trans_rx);
		err_show (ERR_NOTE, _("\tY rotation from WGS 84: %f"), opts->wgs84_trans_ry);
		err_show (ERR_NOTE, _("\tZ rotation from WGS 84: %f"), opts->wgs84_trans_rz);
		err_show (ERR_NOTE, _("\tScaling from WGS 84: %f"), opts->wgs84_trans_ds);
	}
	char *srs_out = NULL;
	if ( opts->proj_out != NULL ) {
		srs_out = strdup(opts->proj_out);
	} else {
		srs_out = strdup(srs_in);
	}
	err_show (ERR_NOTE, _("\tOutput SRS: '%s'"), srs_out);
	free(srs_in);
	free(srs_out);
	err_show (ERR_NOTE, _("Output orientation: %s"), OPTIONS_ORIENT_MODE_NAMES[opts->orient_mode]);
	err_show (ERR_NOTE, _("Topological processing: %s"), OPTIONS_TOPO_LEVEL_NAMES[opts->topo_level]);
	if ( selections_get_count(opts) > 0 ) {
		err_show (ERR_NOTE, _("Selection commands:"));
		for ( i=0; i < selections_get_count(opts); i++ ) {
			err_show (ERR_NOTE, _("\t%s"), opts->selection[i]);
		}
	}
	if ( opts->tolerance == 0 ) {
		err_show (ERR_NOTE, _("Coordinate tolerance: 0"));
	} else {
		err_show (ERR_NOTE, _("Coordinate tolerance: %f"), opts->tolerance);
	}
	if ( opts->snapping == 0 ) {
		err_show (ERR_NOTE, _("Snapping dist. (boundary vertices): 0"));
	} else {
		err_show (ERR_NOTE, _("Snapping dist. (boundary vertices): %f"), opts->snapping);
	}
	if ( opts->dangling == 0 ) {
		err_show (ERR_NOTE, _("Snapping dist. (line nodes): 0"));
	} else {
		err_show (ERR_NOTE, _("Snapping dist. (line nodes): %f"), opts->dangling);
	}
	if ( opts->offset_x == 0 ) {
		err_show (ERR_NOTE, _("X coordinate offset: 0"), opts->offset_x);
	} else {
		err_show (ERR_NOTE, _("X coordinate offset: %f"), opts->offset_x);
	}
	if ( opts->offset_y == 0 ) {
		err_show (ERR_NOTE, _("Y coordinate offset: 0"), opts->offset_y);
	} else {
		err_show (ERR_NOTE, _("Y coordinate offset: %f"), opts->offset_y);
	}
	if ( opts->offset_z == 0 ) {
		err_show (ERR_NOTE, _("Z coordinate offset: 0"), opts->offset_z);
	} else {
		err_show (ERR_NOTE, _("Z coordinate offset: %f"), opts->offset_z);
	}
	if ( strlen ( opts->decimal_point ) > 0 ) {
		err_show (ERR_NOTE, _("Decimal point symbol set to: '%c'"), opts->decimal_point[0]);
	} else {
		err_show (ERR_NOTE, _("Decimal point symbol set to: '%s'"), I18N_DECIMAL_POINT);
	}
	if ( strlen ( opts->decimal_group ) > 0 ) {
		err_show (ERR_NOTE, _("Decimal grouping symbol set to: '%c'"), opts->decimal_group[0]);
	} else {
		err_show (ERR_NOTE, _("Decimal grouping symbol set to: '%s'"), I18N_THOUSANDS_SEP);
	}
	if ( opts->dump_raw == TRUE )
		err_show (ERR_NOTE, _("Raw vertex data will be saved as additional output."));
	if ( opts->force_2d == TRUE )
		err_show (ERR_NOTE, _("2D mode: Any Z data will be discarded from output."));
	if ( opts->strict == TRUE )
		err_show (ERR_NOTE, _("Parser running in 'strict' mode."));
	if ( opts->force_english == TRUE )
		err_show (ERR_NOTE, _("Messages and decimal notation set to English."));
	err_show (ERR_NOTE, _("\n* Processing messages follow below.\n"));
}


/*
 * Clean label attribute field contents of any excess tokens (if required).
 */
void clean_label_atts ( options *opts, parser_desc* parser, geom_store *gs ) {
	if (opts->label_field != NULL ) {
		{
			int i = 0;
			int j = 0;
			int label_field_idx = -1;
			char **atts = NULL;
			while ( parser->fields[i] != NULL ) {
				if ( !strcasecmp (parser->fields[i]->name, opts->label_field)  ) {
					label_field_idx = i;
					break;
				}
				i ++;
			}
			if ( label_field_idx >= 0 ) {
				for ( j = 0; j < parser_get_num_tags ( parser, GEOM_TYPE_POINT ); j ++ ) {
				//if ( parser->geom_tag_point != NULL ) { DELETE ME
					//char *geom_tag = parser->geom_tag_point; DELETE ME
					const char *geom_tag = parser_get_tag ( parser, GEOM_TYPE_POINT, j );
					int geom_tag_len = strlen(geom_tag);
					for ( i = 0; i < gs->num_points; i ++ ) {
						if ( gs->points[i].is_selected == TRUE ) {
							if ( gs->points[i].has_label == TRUE ) {
								atts = gs->points[i].atts;
								if ( atts[label_field_idx] != NULL ) {
									char *content = t_str_pack (atts[label_field_idx]); /* get trimmed copy of field content */
									if ( content != NULL && strlen (content) > 0 ) {
										if ( strlen (content) > geom_tag_len ) {
											/* There may be something here, other than the tag. */
											char *tag = strstr(content,geom_tag);
											if ( tag != NULL ) {
												/* Tag found: Attempt to remove! */
												int position = tag - content;
												if ( position == 0 ) {
													/* geom tag is at beginning of content */
													int p;
													for ( p = 0; p < geom_tag_len; p ++ ) {
														tag ++; /* move pointer by one char */
													}
													char *purged = t_str_pack (tag); /* we are now positioned after the tag */
													if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
														/* swap for new content */
														t_free (atts[label_field_idx]);
														atts[label_field_idx] = purged;
													}
													t_free (content);
												}
												else if ( position == ( strlen(content) - geom_tag_len ) ) {
													/* geom tag is at end of content */
													*tag = '\0'; /* cut off here */
													char *purged = t_str_pack (content);
													if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
														/* swap for new content */
														t_free (atts[label_field_idx]);
														atts[label_field_idx] = purged;
													}
													t_free (content);
												}
												/* in all other cases: we can do nothing */
											}
										}
									}
								}
							}
						}
					}
				}
				for ( j = 0; j < parser_get_num_tags ( parser, GEOM_TYPE_LINE ); j ++ ) {
				//if ( parser->geom_tag_line != NULL ) { DELETE ME
					//char *geom_tag = parser->geom_tag_line; DELETE ME
					const char *geom_tag = parser_get_tag ( parser, GEOM_TYPE_LINE, j );
					int geom_tag_len = strlen(geom_tag);
					for ( i = 0; i < gs->num_lines; i ++ ) {
						if ( gs->lines[i].is_selected == TRUE ) {
							int j;
							for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
								if ( gs->lines[i].parts[j].has_label == TRUE ) {
									atts = gs->lines[i].atts;
									if ( atts[label_field_idx] != NULL ) {
										char *content = t_str_pack (atts[label_field_idx]); /* get trimmed copy of field content */
										if ( content != NULL && strlen (content) > 0 ) {
											if ( strlen (content) > geom_tag_len ) {
												/* There may be something here, other than the tag. */
												char *tag = strstr(content,geom_tag);
												if ( tag != NULL ) {
													/* Tag found: Attempt to remove! */
													int position = tag - content;
													if ( position == 0 ) {
														/* geom tag is at beginning of content */
														int p;
														for ( p = 0; p < geom_tag_len; p ++ ) {
															tag ++; /* move pointer by one char */
														}
														char *purged = t_str_pack (tag); /* we are now positioned after the tag */
														if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
															/* swap for new content */
															t_free (atts[label_field_idx]);
															atts[label_field_idx] = purged;
														}
														t_free (content);
													}
													else if ( position == ( strlen(content) - geom_tag_len ) ) {
														/* geom tag is at end of content */
														*tag = '\0'; /* cut off here */
														char *purged = t_str_pack (content);
														if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
															/* swap for new content */
															t_free (atts[label_field_idx]);
															atts[label_field_idx] = purged;
														}
														t_free (content);
													}
													/* in all other cases: we can do nothing */
												}
											}
										}
									}
								}
							}
						}
					}
				}
				for ( j = 0; j < parser_get_num_tags ( parser, GEOM_TYPE_POLY ); j ++ ) {
				//if ( parser->geom_tag_poly != NULL ) { DELETE ME
					//char *geom_tag = parser->geom_tag_poly; DELETE ME
					const char *geom_tag = parser_get_tag ( parser, GEOM_TYPE_POLY, j );
					int geom_tag_len = strlen(geom_tag);
					for ( i = 0; i < gs->num_polygons; i ++ ) {
						if ( gs->polygons[i].is_selected == TRUE ) {
							int j;
							for ( j = 0; j < gs->polygons[i].num_parts; j ++ ) {
								if ( gs->polygons[i].parts[j].has_label == TRUE ) {
									atts = gs->polygons[i].atts;
									if ( atts[label_field_idx] != NULL ) {
										char *content = t_str_pack (atts[label_field_idx]); /* get trimmed copy of field content */
										if ( content != NULL && strlen (content) > 0 ) {
											if ( strlen (content) > geom_tag_len ) {
												/* There may be something here, other than the tag. */
												char *tag = strstr(content,geom_tag);
												if ( tag != NULL ) {
													/* Tag found: Attempt to remove! */
													int position = tag - content;
													if ( position == 0 ) {
														/* geom tag is at beginning of content */
														int p;
														for ( p = 0; p < geom_tag_len; p ++ ) {
															tag ++; /* move pointer by one char */
														}
														char *purged = t_str_pack (tag); /* we are now positioned after the tag */
														if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
															/* swap for new content */
															t_free (atts[label_field_idx]);
															atts[label_field_idx] = purged;
														}
														t_free (content);
													}
													else if ( position == ( strlen(content) - geom_tag_len ) ) {
														/* geom tag is at end of content */
														*tag = '\0'; /* cut off here */
														char *purged = t_str_pack (content);
														if ( purged != NULL && strlen (purged) > 0 ) { /* anything left? */
															/* swap for new content */
															t_free (atts[label_field_idx]);
															atts[label_field_idx] = purged;
														}
														t_free (content);
													}
													/* in all other cases: we can do nothing */
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
	} /* DONE (remove geom markers from label fields) */
}


/*
 * Show processing statistics after job is done.
 */
void show_stats ( unsigned int *topo_errors, options *opts, parser_data_store **storage ) {
	int i, j;
	unsigned int *num_total_lines;
	unsigned int *num_invalid_lines;
	unsigned int *num_points_rec;
	unsigned int *num_lines_rec;
	unsigned int *num_polygons_rec;
	unsigned int *num_unassigned;


	/* initialize validation statistics */
	num_total_lines = malloc ( sizeof (unsigned int) * opts->num_input );
	num_invalid_lines = malloc ( sizeof (unsigned int) * opts->num_input );
	num_points_rec = malloc ( sizeof (unsigned int) * opts->num_input );
	num_lines_rec = malloc ( sizeof (unsigned int) * opts->num_input );
	num_polygons_rec = malloc ( sizeof (unsigned int) * opts->num_input );
	num_unassigned = malloc ( sizeof (unsigned int) * opts->num_input );
	for ( i=0; i < opts->num_input ; i ++ ) {
		num_total_lines[i] = 0;
		num_invalid_lines[i] = 0;
		num_points_rec[i] = 0;
		num_lines_rec[i] = 0;
		num_polygons_rec[i] = 0;
		num_unassigned[i] = 0;
	}

	/* get statistics */
	for ( i=0; i < opts->num_input ; i ++ ) {
		for ( j=0; j < storage[i]->num_records; j++ ) {
			if ( 	storage[i]->records[j].is_empty == FALSE	) {
				num_total_lines[i] ++;
			}
			if ( 	storage[i]->records[j].is_valid == FALSE &&
					storage[i]->records[j].is_empty == FALSE	) {
				num_invalid_lines[i] ++;
			}
			if ( 	storage[i]->records[j].is_valid == TRUE &&
					storage[i]->records[j].is_empty == FALSE &&
					storage[i]->records[j].geom_type == GEOM_TYPE_NONE ) {
				num_unassigned[i] ++;
			}
			if ( 	storage[i]->records[j].is_valid == TRUE &&
					storage[i]->records[j].is_empty == FALSE &&
					storage[i]->records[j].geom_type == GEOM_TYPE_POINT ) {
				num_points_rec[i] ++;
			}
			if ( 	storage[i]->records[j].is_valid == TRUE &&
					storage[i]->records[j].is_empty == FALSE &&
					storage[i]->records[j].geom_type == GEOM_TYPE_LINE ) {
				num_lines_rec[i] ++;
			}
			if ( 	storage[i]->records[j].is_valid == TRUE &&
					storage[i]->records[j].is_empty == FALSE &&
					storage[i]->records[j].geom_type == GEOM_TYPE_POLY ) {
				num_polygons_rec[i] ++;
			}
		}
	}

	/* log data statistics */
	err_show (ERR_NOTE,_("\nParsing of %i input data source(s) completed. Validation statistics below."), opts->num_input );
	for ( i = 0; i < opts->num_input; i ++ ) {
		err_show (ERR_NOTE, "" );
		if ( !strcmp (opts->input[i], "-") ) {
			err_show (ERR_NOTE,_("%i\tData read from console input stream."), i+1 );
		} else {
			err_show (ERR_NOTE,_("%i\tData read from file \"%s\"."), i+1, opts->input[i] );
		}
		err_show (ERR_NOTE,_("\tTotal records/lines read: %i"), num_total_lines[i] );
		err_show (ERR_NOTE,_("\tNumber of invalid records: %i"), num_invalid_lines[i] );
		err_show (ERR_NOTE,_("\tNumber of valid records: %i"), storage[i]->slot - num_invalid_lines[i] );
		err_show (ERR_NOTE,_("\t\tAssigned to %i points: %i"), storage[i]->num_points, num_points_rec[i] );
		err_show (ERR_NOTE,_("\t\tAssigned to %i lines/parts: %i"), storage[i]->num_lines, num_lines_rec[i] );
		err_show (ERR_NOTE,_("\t\tAssigned to %i polygons/parts: %i"), storage[i]->num_polygons, num_polygons_rec[i] );
		err_show (ERR_NOTE,_("\t\tNot assigned to any geometry: %i"), num_unassigned[i] );
		err_show (ERR_NOTE,_("\tTotal topological error count: %i"), topo_errors[i] );
	}

	/* free memory for data statistics */
	free ( num_total_lines );
	free ( num_invalid_lines );
	free ( num_points_rec );
	free ( num_lines_rec );
	free ( num_polygons_rec );
	free ( num_unassigned );
}


/*
 * Runs all program operations once, outputs result(s) if any.
 */
#ifdef GUI
void run_once ( options *opts, GtkWidget *parent )
#else
void run_once ( options *opts )
#endif
{
	int i;
	int error;
	char error_msg[PRG_MAX_STR_LEN] = "";
	unsigned int *topo_errors;
	unsigned int topo_errors_after_fusion = 0;
	unsigned int duplicate_records;
	unsigned int build_errors;
	unsigned int fused_records;
	unsigned int overlays = 0;
	unsigned int removed_overlaps = 0;
	unsigned int snaps_poly = 0;
	unsigned int detected_intersections_ll = 0;
	unsigned int detected_intersections_lp = 0;
	unsigned int detected_intersections_pp = 0;
	unsigned int self_intersects_lines = 0;
	unsigned int self_intersects_polygons = 0;
	unsigned int added_intersections_ll = 0;
	unsigned int added_intersections_lp = 0;
	unsigned int added_intersections_pp = 0;
	unsigned int snapped_line_dangles = 0;
	unsigned int reversed_vertex_lists = 0;
	int bad_attributes;

	parser_desc *parser;
	parser_data_store **storage; /* data storage for each input file */
	geom_store *gs;


#ifdef GUI
	if (OPTIONS_GUI_MODE == TRUE) {
		/* refuse to run unless minimal options are given */
		if (opts->just_dump_parser == TRUE) {
			if (opts->schema_file == NULL) {
				if (parent != NULL) {
					GtkWidget *dialog
					= gtk_message_dialog_new(GTK_WINDOW(parent),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
							"No parser file specified.");
					gtk_message_dialog_format_secondary_text(
							GTK_MESSAGE_DIALOG(dialog),
							_("Please provide a valid parser schema file."));
					gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
					gtk_widget_show_all(dialog);
					gtk_dialog_run(GTK_DIALOG(dialog));
					gtk_widget_destroy(dialog);
				}
				return;
			}
		} else {
			if (opts->num_input < 1 || opts->schema_file == NULL || (opts->base
					== NULL) || (opts->output == NULL)) {
				if (parent != NULL) {
					GtkWidget *dialog
					= gtk_message_dialog_new(GTK_WINDOW(parent),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
							_("Incomplete or invalid options."));
					gtk_message_dialog_format_secondary_text(
							GTK_MESSAGE_DIALOG(dialog),
							_("Please provide valid input file(s),\nparser schema file, base output name,\nand output folder name."));
					gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
					gtk_widget_show_all(dialog);
					gtk_dialog_run(GTK_DIALOG(dialog));
					gtk_widget_destroy(dialog);
				}
				return;
			}
		}
	}
#endif

	/* initialize error logging */
	err_log_init ( opts );

	/* initialization message with program name, version number and numeric notation, as well as schema used */
	show_init_msg ( opts );

	/* initialize reprojection engine */
	reproj_init( opts );

	/* read parser schema from file provided */
	parser = parser_desc_create ();
	parser_desc_set ( parser, opts );

	/* if parser is empty at this point, then we have an error */
	if ( parser->empty == TRUE ) {
		parser_desc_destroy (parser);
		return;
	}

	/* just dump parser description? */
	if ( opts->just_dump_parser == TRUE ) {
		parser_desc_validate ( parser, opts );
#ifdef GUI
		if (OPTIONS_GUI_MODE == TRUE) {
			parser_dump_gui ( parser, opts );
		} else {
			parser_dump ( parser, opts );
		}
#else
		parser_dump ( parser, opts );
		exit (PRG_EXIT_OK);
#endif
		return;
	}

	/* validate parser schema */
	error = parser_desc_validate ( parser, opts );

	/* only continue, if the parser is OK */
	if ( error != 0 ) {
		parser_desc_destroy (parser);
		return;
	}

	/* Check reprojection settings (if any)
	 * We run this function as early as possible, because only after
	 * it has completed can we safely query 'opts' for SRS data of
	 * any kind!
	 */

	int reproj = reproj_parse_opts (opts);
	if ( reproj != REPROJ_STATUS_OK ) {
		/* a suitable error message will already have been produced by reproj_parse_opts() */
		/* we only need to clean up and exit */
		/* TODO: we have a lot of return statements that need to be checked for memory leaks */
		return;
	}

	/* check that label field is valid */
	if ( opts->label_field != NULL ) {
		BOOLEAN found = FALSE;
		for ( i = 0; i < PRG_MAX_FIELDS; i ++ ) {
			if ( parser->fields[i] != NULL ) {
				if ( !strcasecmp ( opts->label_field, parser->fields[i]->name ) ) {
					found = TRUE;
					break;
				}
			}
		}
		if ( found == FALSE ) {
			err_show ( ERR_EXIT, _("\nLabel field \"%s\" not found in parser definition."), opts->label_field);
			parser_desc_destroy (parser);
			return;
		}
	}

	/* validate selection expressions */
	if ( selections_validate ( opts, parser ) == FALSE ) {
		parser_desc_destroy (parser);
		return;
	}

	/* create data storage objects */
	storage = malloc ( sizeof ( parser_data_store* ) * opts->num_input );
	for ( i=0; i < opts->num_input ; i ++ ) {
		storage[i] = parser_data_store_create ( opts->input[i], parser,  opts );
		if ( storage[i] == NULL ) {
			err_show ( ERR_EXIT, _("\nFailed to create data storage object for data source #%i."), i+1 );
			/* release data storage */
			for ( i=0; i < opts->num_input ; i ++ ) {
				if ( storage[i] != NULL )
					parser_data_store_destroy ( storage[i] );
			}
			free ( storage );
			/* release parser object */
			parser_desc_destroy (parser);
			return;
		}
	}

	/* process input file(s) */
	parser_consume_input ( parser, opts, storage );

	/* Past this point we have valid input file records and coordinates for every measurement! */

	/* Reproject data to change axis orientation (if requested). */
	/* This must be done _before_ geom_store_build(), so that label point positions */
	/* can be computed correctly later on! */
	if ( opts->orient_mode == OPTIONS_ORIENT_MODE_LOCAL_XZ ) {
		if ( reproj_srs_in_latlon(opts) == TRUE ) {
			err_show ( ERR_EXIT, _("\nOrientation mode '%s' not supported for lat/lon input data."),
					OPTIONS_ORIENT_MODE_NAMES[OPTIONS_ORIENT_MODE_LOCAL_XZ]);
			free ( storage );
			parser_desc_destroy (parser);
			return;
		}
		/* check that a Z coordinate field has been declared */
		if ( parser->coor_z == NULL ) {
			err_show ( ERR_EXIT, _("\nCannot create local X-Z output for 2D input without Z field.") );
			free ( storage );
			parser_desc_destroy (parser);
			return;
		}
		if ( opts->num_input > 1 ) {
			err_show ( ERR_EXIT, _("\nCannot create local X-Z output for more than one data source."));
			free ( storage );
			parser_desc_destroy (parser);
			return;
		}
		/* check that there is actual Z data */
		if ( geom_ds_has_z (storage[0]) == FALSE ) {
			err_show ( ERR_EXIT, _("\nCannot create local X-Z output: Z extent of data is '0.0'."));
			free ( storage );
			parser_desc_destroy (parser);
			return;
		}

		/* perform re-orientation on first datastore ONLY */
		geom_reorient_local_xz(storage[0]);
	}

	/* Basic geometry processing:
	 * 1. Assign geom_id to all records that form a geometry.
	 * 2. Eliminate duplicate coordinates, zero-length lines and zero-area polygons ("splinters").
	 * 3. Snap coordinates
	 */
	/* TODO: Improve snapping and thresholding of points for lat/lon data*/
	if ( reproj_srs_in_latlon(opts) == TRUE ) {
		if ( opts->tolerance > 0.0 ) {
			err_show (ERR_WARN, _("\nTopological cleaning of lat/lon data has limited accuracy."));
			err_show (ERR_WARN, _("Results of eliminating duplicate vertices may be insufficient."));
		}
		if ( opts->snapping > 0.0 ) {
			err_show (ERR_WARN, _("\nTopological cleaning of lat/lon data has limited accuracy."));
			err_show (ERR_WARN, _("Results of snapping vertices may be insufficient."));
		}
	}
	topo_errors = malloc ( sizeof ( unsigned int ) * opts->num_input );
	for ( i=0; i < opts->num_input ; i ++ ) {
		/* LEVEL: ALL */
		topo_errors[i] = 0;
		/* multiplex geometries into points, lines and polygons */
		geom_multiplex ( storage[i], parser );
		/* remove duplicate vertices */
		topo_errors[i] += geom_topology_remove_duplicates ( storage[i], opts, FALSE );
		/* remove splintered geometries */
		topo_errors[i] += geom_topology_remove_splinters_lines ( storage[i], opts );
		topo_errors[i] += geom_topology_remove_splinters_polygons ( storage[i], opts );
	}

	/* fuse multi-part geometries: assign a "part_id" to all
	 * parts of a "master" geometry. */
	fused_records = parser_ds_fuse ( storage, opts, parser );

	/* evaluate "unique" attributes across ALL input data */
	duplicate_records = parser_ds_validate_unique ( storage, opts, parser );

	/* Advanced geometry processing */
	gs = geom_store_new ();
	/* 1. Build points, lines and polygons (also multi-part). */
	build_errors = geom_store_build ( gs, storage, parser, opts );
	if ( (gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons) < 1 ) {
		err_show (ERR_EXIT, _("\nNo valid input data found. Aborting."));
		free ( topo_errors );
		free ( storage );
		parser_desc_destroy (parser);
		geom_store_destroy ( gs );
		return;
	}

	/* apply selections (if any) to built geometries */
	if ( selections_get_count ( opts ) > 0 ) {
		selections_apply_all ( opts, parser, gs );
		if ( selections_get_num_selected ( GEOM_TYPE_ALL, gs ) < 1 ) {
			err_show (ERR_EXIT, _("\nNo valid input data left after selecting. Aborting."));
			free ( topo_errors );
			free ( storage );
			parser_desc_destroy (parser);
			geom_store_destroy ( gs );
			return;
		}
	}

	if ( geom_store_make_paths ( gs, opts, &error_msg[0] ) == 0 ) {

		/* self-intersections can be present in lat/lon and X/Y data */
		for ( i = 0; i < gs->num_lines; i ++ ) {
			int j;
			for ( j = 0; j < gs->lines[i].num_parts; j ++ ) {
				self_intersects_lines += gs->lines[i].parts[j].err_self_intersects;
			}
		}
		for ( i = 0; i < gs->num_polygons; i ++ ) {
			int j;
			for ( j = 0; j < gs->polygons[i].num_parts; j ++ ) {
				self_intersects_polygons += gs->polygons[i].parts[j].err_self_intersects;
			}
		}

		/* Most topo operations are untested for lat/lon data: WARN. */
		if ( opts->topo_level > OPTIONS_TOPO_LEVEL_NONE && reproj_srs_in_latlon(opts) == TRUE ) {
			err_show (ERR_WARN, _("\nHigh-level topological cleaning of lat/lon data not implemented."));
			err_show (ERR_WARN, _("Output data may suffer from topological defects."));
		}
		
		/* Run the following only if topological cleaning is enabled. */
		if ( opts->topo_level > OPTIONS_TOPO_LEVEL_NONE ) {
			/* LEVEL: BASIC AND ABOVE */
			/* Snap polygon boundaries */
			snaps_poly = geom_topology_snap_boundaries_2D ( gs, opts );
			geom_tools_update_bboxes (gs); /* update bounding boxes */
			
			/* Perform geometric AND operation to subtract polygon overlap areas */
			if ( opts->topo_level > OPTIONS_TOPO_LEVEL_BASIC ) {
				/* LEVEL: FULL */
				removed_overlaps = geom_topology_poly_remove_overlap_2D ( gs, parser, opts );
				geom_tools_update_bboxes (gs); /* update bounding boxes */
			}

			/* Stamp holes into overlaid polygons. */
			overlays = geom_topology_poly_overlay_2D ( gs, parser );

			/* Add intersection vertices at line/line intersections. */
			detected_intersections_ll = geom_topology_intersections_2D_detect ( gs, opts, GEOM_INTERSECT_LINE_LINE,
					&added_intersections_ll, &topo_errors_after_fusion );

			/* Add intersection vertices at line/polygon boundary intersections. */
			detected_intersections_lp = geom_topology_intersections_2D_detect ( gs, opts, GEOM_INTERSECT_LINE_POLY,
					&added_intersections_lp, &topo_errors_after_fusion );

			/* Add intersection vertices at polygon boundary/polygon boundary intersections. */
			detected_intersections_pp = geom_topology_intersections_2D_detect ( gs, opts, GEOM_INTERSECT_POLY_POLY,
					&added_intersections_pp, &topo_errors_after_fusion );

			/* Clean dangling line nodes. */
			if ( opts->topo_level > OPTIONS_TOPO_LEVEL_BASIC ) {
				/* LEVEL: FULL */
				snapped_line_dangles += geom_topology_clean_dangles_2D ( gs, opts, &topo_errors_after_fusion,
						&detected_intersections_ll, &added_intersections_ll );
				geom_tools_update_bboxes (gs); /* update bounding boxes */
			}
		}
		/* Unify vertex orders for polyons. */
		if ( opts->format == PRG_OUTPUT_KML	) {
			/* KML requires ccw vertex winding */
			reversed_vertex_lists += geom_topology_sort_vertices ( gs, GEOM_WINDING_CCW );
		} else {
			/* default mode is auto-winding */
			reversed_vertex_lists += geom_topology_sort_vertices ( gs, GEOM_WINDING_AUTO );
		}
		/* DEBUG */
		/* geom_store_print ( gs, FALSE ); */
	} else {
		if ( strlen (error_msg) > 0 ) {
			err_show (ERR_EXIT, _("\nUnable to create output file. Error was: '%s'."), error_msg );
		}
		free ( topo_errors );
		free ( storage );
		parser_desc_destroy (parser);
		if ( gs != NULL ) {
			geom_store_destroy ( gs );
		}
		return;
	}

	/* Reproject if required. */
	reproj = reproj_need_reprojection ( opts );
	if ( reproj == REPROJ_ACTION_ERROR ) {
		/* error message already produced by reproj_need_reprojection() */
		free ( topo_errors );
		free ( storage );
		parser_desc_destroy (parser);
		geom_store_destroy ( gs );
		return;
	}
	if ( reproj == REPROJ_ACTION_REPROJECT ) {
		if ( opts->orient_mode == OPTIONS_ORIENT_MODE_LOCAL_XZ ) {
			err_show (ERR_EXIT, _("\nCannot combine mode '%s' with reprojection. Aborting."),
					OPTIONS_ORIENT_MODE_NAMES[opts->orient_mode]);
			free ( topo_errors );
			free ( storage );
			parser_desc_destroy (parser);
			geom_store_destroy ( gs );
			return;
		}
		int reproj_status = reproj_do( opts, gs );
		if ( reproj_status == REPROJ_STATUS_ERROR ) {
			err_show (ERR_EXIT, _("\nFailed to reproject data. Aborting."));
			free ( topo_errors );
			free ( storage );
			parser_desc_destroy (parser);
			geom_store_destroy ( gs );
			return;
		}
	}

	/* remove geometry marker from label field contents, if required */
	clean_label_atts ( opts, parser, gs );

	/* create output */
	if ( opts->format == PRG_OUTPUT_SHP	) {
		err_show (ERR_NOTE, _("\nOutput format: %s"), PRG_OUTPUT_DESC[PRG_OUTPUT_SHP] );
		bad_attributes = export_SHP ( gs, parser, opts );
	} else if ( opts->format == PRG_OUTPUT_DXF	) {
		err_show (ERR_NOTE, _("\nOutput format: %s"), PRG_OUTPUT_DESC[PRG_OUTPUT_DXF] );
		bad_attributes = export_DXF ( gs, parser, opts );
	} else if ( opts->format == PRG_OUTPUT_GEOJSON	) {
		err_show (ERR_NOTE, _("\nOutput format: %s"), PRG_OUTPUT_DESC[PRG_OUTPUT_GEOJSON] );
		/* Warn or fail if input or output SRS is not lat/lon. */
		if ( reproj_srs_out_latlon(opts) == FALSE && reproj_srs_in_latlon(opts) == FALSE ) {
			if ( opts->strict == TRUE ) {
				err_show ( ERR_EXIT, "\nOutput format '%s' only available for lat/lon data in 'strict' mode. Aborting.",
						PRG_OUTPUT_DESC[PRG_OUTPUT_GEOJSON] );
				for ( i=0; i < opts->num_input ; i ++ ) {
					parser_data_store_destroy ( storage[i] );
				}
				free ( topo_errors );
				free ( storage );
				parser_desc_destroy (parser);
				geom_store_destroy ( gs );
				return;
			} else {
				err_show ( ERR_WARN, "\nOutput format '%s' with data other than lat/lon is not standard-conforming.",
						PRG_OUTPUT_DESC[PRG_OUTPUT_GEOJSON] );
			}
		}
		bad_attributes = export_GeoJSON ( gs, parser, opts );
	} else if ( opts->format == PRG_OUTPUT_KML	) {
		err_show (ERR_NOTE, _("\nOutput format: %s"), PRG_OUTPUT_DESC[PRG_OUTPUT_KML] );
		/* Fail if input or output SRS is not lat/lon. */
		if ( reproj_srs_out_latlon(opts) == FALSE && reproj_srs_in_latlon(opts) == FALSE ) {
			err_show ( ERR_EXIT, "\nOutput format '%s' only available for lat/lon data. Aborting.",
					PRG_OUTPUT_DESC[PRG_OUTPUT_KML] );
			for ( i=0; i < opts->num_input ; i ++ ) {
				parser_data_store_destroy ( storage[i] );
			}
			free ( topo_errors );
			free ( storage );
			parser_desc_destroy (parser);
			geom_store_destroy ( gs );
			return;
		}
		bad_attributes = export_KML ( gs, parser, opts );
	} else {
		err_show ( ERR_EXIT, "\nOutput format not yet implemented. Aborting." );
		/* release data storage */
		for ( i=0; i < opts->num_input ; i ++ ) {
			parser_data_store_destroy ( storage[i] );
		}
		free ( topo_errors );
		free ( storage );
		parser_desc_destroy (parser);
		geom_store_destroy ( gs );
		return;
	}

	/* show final statistics for each input file */
	show_stats ( topo_errors, opts, storage );
	free ( topo_errors );

	/* selection statistics */
	err_show (ERR_NOTE, _("\nSelected for export:"));
	err_show (ERR_NOTE, _("\tPoints: %i."), selections_get_num_selected ( GEOM_TYPE_POINT, gs ));
	if ( opts->dump_raw == TRUE ) {
		err_show (ERR_NOTE, _("\tRaw points: %i."), selections_get_num_selected ( GEOM_TYPE_POINT_RAW, gs ));
	}
	err_show (ERR_NOTE, _("\tLines: %i."), selections_get_num_selected ( GEOM_TYPE_LINE, gs ));
	err_show (ERR_NOTE, _("\tPolygons: %i."), selections_get_num_selected ( GEOM_TYPE_POLY, gs ));
	/* show total data extent */
	if ( reproj == TRUE ) {
		err_show (ERR_NOTE, _("\nTotal data extent (after reprojection):"));
	} else {
		err_show (ERR_NOTE, _("\nTotal data extent:"));
	}
	err_show (ERR_NOTE, _("\tX range: from %f to %f."), gs->min_x, gs->max_x);
	err_show (ERR_NOTE, _("\tY range: from %f to %f."), gs->min_y, gs->max_y);
	if ( opts->force_2d == FALSE ) {
		err_show (ERR_NOTE, _("\tZ range: from %f to %f."), gs->min_z, gs->max_z);
	}

	/* show summmary statistics for all input files */
	err_show (ERR_NOTE, _("\nParts added to multi-part geometries: %i"), fused_records );

	if ( build_errors > 0 )
		err_show (ERR_NOTE, _("\nGeometry build errors: %i"), build_errors );

	if ( duplicate_records > 0 )
		err_show (ERR_NOTE, _("\nInput contained duplicate attribute values that should be unique."));

	if ( bad_attributes > 0 )
		err_show (ERR_NOTE, _("\nAttribute write errors: %i"), bad_attributes );

	err_show (ERR_NOTE, _("\nDetected line self-intersections: %i"), self_intersects_lines );

	err_show (ERR_NOTE, _("\nDetected polygon self-intersections: %i"), self_intersects_polygons );

	err_show (ERR_NOTE, _("\nDetected polygon overlays: %i"), overlays );

	err_show (ERR_NOTE, _("\nRemoved polygon overlap areas: %i"), removed_overlaps );

	err_show (ERR_NOTE, _("\nSnapped polygon boundary vertices: %i"), snaps_poly );

	err_show (ERR_NOTE, _("\nDetected line/line intersections: %i"), detected_intersections_ll );

	err_show (ERR_NOTE, _("\nAdded vertices at line/line intersections: %i"), added_intersections_ll );

	err_show (ERR_NOTE, _("\nDetected line/polygon intersections: %i"), detected_intersections_lp );

	err_show (ERR_NOTE, _("\nAdded vertices at line/polygon intersections: %i"), added_intersections_lp );

	err_show (ERR_NOTE, _("\nDetected polygon/polygon intersections: %i"), detected_intersections_pp );

	err_show (ERR_NOTE, _("\nAdded vertices at polygon/polygon intersections: %i"), added_intersections_pp );

	err_show (ERR_NOTE, _("\nSnapped dangling line nodes: %i"), snapped_line_dangles );

	err_show (ERR_NOTE, _("\nCorrected vertex order of polygon boundaries and holes: %i"), reversed_vertex_lists );

	err_show (ERR_NOTE, _("\nAdditional topological errors in built geometries: %i"), topo_errors_after_fusion );

	/* check for any output produced */
	if ( (gs->num_points + gs->num_points_raw + gs->num_lines + gs->num_polygons) > 0 ) {
		err_show (ERR_NOTE, _("\nOutput files produced:") );
		if ( OPTIONS_GUI_MODE == TRUE ) {
			fprintf (stderr, "<OUTPUT_FORMAT>%s</OUTPUT_FORMAT>\n", PRG_OUTPUT_DESC[opts->format]);
		}
		/* if ( gs->num_points > 0 ) { */
		if ( selections_get_num_selected ( GEOM_TYPE_POINT, gs ) > 0 ) {
			if ( gs->path_points != NULL ) {
				err_show (ERR_NOTE, _("\t%s"), gs->path_points);
				if ( OPTIONS_GUI_MODE == TRUE ) {
					fprintf (stderr, "<OUTPUT_POINTS>%s</OUTPUT_POINTS>\n", gs->path_points);
				}
			}
		}
		if ( selections_get_num_selected ( GEOM_TYPE_POINT_RAW, gs ) > 0 ) {
			/* if ( gs->num_points_raw > 0 ) { */
			if ( gs->path_points_raw != NULL ) {
				err_show (ERR_NOTE, _("\t%s"), gs->path_points_raw);
				if ( OPTIONS_GUI_MODE == TRUE ) {
					fprintf (stderr, "<OUTPUT_POINTS_RAW>%s</OUTPUT_POINTS_RAW>\n", gs->path_points_raw);
				}
			}
		}
		if ( selections_get_num_selected ( GEOM_TYPE_LINE, gs ) > 0 ) {
			/* if ( gs->num_lines > 0 ) { */
			if ( gs->path_lines != NULL ) {
				err_show (ERR_NOTE, _("\t%s"), gs->path_lines);
				if ( OPTIONS_GUI_MODE == TRUE ) {
					fprintf (stderr, "<OUTPUT_LINES>%s</OUTPUT_LINES>\n", gs->path_lines);
				}
			}
		}
		if ( selections_get_num_selected ( GEOM_TYPE_POLY, gs ) > 0 ) {
			/* if ( gs->num_polygons > 0 ) { */
			if ( gs->path_polys != NULL ) {
				err_show (ERR_NOTE, _("\t%s"), gs->path_polys);
				if ( OPTIONS_GUI_MODE == TRUE ) {
					fprintf (stderr, "<OUTPUT_POLYGONS>%s</OUTPUT_POLYGONS>\n", gs->path_polys);
				}
			}
		}
		if ( opts->label_field != NULL ) {
			if ( gs->path_labels != NULL ) {
				err_show (ERR_NOTE, _("\t%s"), gs->path_labels);
				if ( OPTIONS_GUI_MODE == TRUE ) {
					fprintf (stderr, "<OUTPUT_LABELS>%s</OUTPUT_LABELS>\n", gs->path_labels);
				}
			}
		}
		if ( gs->path_all != NULL ) {
			err_show (ERR_NOTE, _("\t%s"), gs->path_all);
			if ( OPTIONS_GUI_MODE == TRUE ) {
				fprintf (stderr, "<OUTPUT_ALL>%s</OUTPUT_ALL>\n", gs->path_all);
			}
		}
		if ( gs->path_all_atts != NULL ) {
			err_show (ERR_NOTE, _("\t%s"), gs->path_all_atts);
			if ( OPTIONS_GUI_MODE == TRUE ) {
				fprintf (stderr, "<OUTPUT_ALL_ATTS>%s</OUTPUT_ALL_ATTS>\n", gs->path_all_atts);
			}
		}
	} else {
		err_show (ERR_NOTE, _("\nNo output files produced.") );
	}

	/* release data storage */
	for ( i=0; i < opts->num_input ; i ++ ) {
		parser_data_store_destroy ( storage[i] );
	}
	free ( storage );

	/* release parser object */
	parser_desc_destroy (parser);

	/* release geometry store */
	geom_store_destroy ( gs );

	/* SUCCESS */
	err_close ();
}


#ifdef GUI


/*
 * GUI FORM FUNCTIONS
 * The following functions will only be compiled if there
 * is GUI support. They handle building and showing the GUI form,
 * processing the GUI settings into program options and running
 * the actual processing as often as the user likes.
 */


/*
 * Creates the GUI form with all its widgets
 * and input fields.
 */
gui_form* make_gui_form (options *opts )
{
	char buf[PRG_MAX_STR_LEN+1];
	char **lookup_format;
	char **lookup_label_mode;
	char **lookup_orient_mode;
	char **lookup_topo_level;
	int i;
	int lookup_format_n;
	int lookup_label_mode_n;
	int lookup_orient_mode_n;
	int lookup_topo_level_n;
	GtkTextIter iter;
	gui_form *gform;


	gform = gui_form_create ( t_get_prg_name() );

	/* BUILD FIELDS */

	/* mandatory inputs */
	f_parser = gui_field_create_file_in ( 	"parser", (_("Parser schema:")), (_("Name of file with parsing schema.")), NULL,
			TRUE, opts->schema_file );
	f_output = gui_field_create_dir_out ( 	"output", (_("Output folder:")), (_("Directory name for output file(s).")), NULL,
			TRUE, opts->output );
	f_name = gui_field_create_string ( 	"name", (_("Output name:")), (_("Base name for output file(s).")), NULL,
			TRUE, opts->base, 0, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );

	/* output format */
	lookup_format_n = 0;
	while ( strlen ( PRG_OUTPUT_DESC [lookup_format_n] ) > 0 ) {
		lookup_format_n ++;
	}
	lookup_format = malloc ( sizeof ( char*) * ( lookup_format_n + 1 ) );
	for ( i = 0; i < lookup_format_n; i++ ) {
		lookup_format[i] = strdup ( PRG_OUTPUT_DESC[i] );
	}
	lookup_format[lookup_format_n] = NULL;
	f_format = gui_field_create_string ( 	"format", (_("Output format:")), (_("Output file format.")), (_("Basic")),
			FALSE, PRG_OUTPUT_DESC[opts->format], 0, NULL, GUI_FIELD_CONVERT_TO_NONE,
			(const char**) lookup_format);
	for ( i = 0; i < lookup_format_n; i++ )
		free (lookup_format[i]);
	free ( lookup_format );

	f_input = gui_field_create_file_in_multi ( 	"input", (_("Input:")), (_("Selection of input file(s).")), (_("Basic")),
			FALSE, (const char**) opts->input );

	/* log file */
	f_log = gui_field_create_file_out ( 	"log", (_("Log file:")), (_("File name for log file.")), (_("Basic")),
			FALSE, opts->log  );

	/* label field name and mode choices */
	f_label_field = gui_field_create_string ( 	"label-field", (_("Label field:")), (_("Field to use for labels (if any).")),
			(_("Extra")),
			FALSE, opts->label_field, GUI_FIELD_STR_LEN_FIELD_NAME,
			GUI_FIELD_STR_ALLOW_FIELD_NAME, GUI_FIELD_CONVERT_TO_LOWER, NULL );
	lookup_label_mode_n = 0;
	while ( strlen ( OPTIONS_LABEL_MODE_NAMES [lookup_label_mode_n] ) > 0 ) {
		lookup_label_mode_n ++;
	}
	lookup_label_mode = malloc ( sizeof ( char*) * ( lookup_label_mode_n + 1 ) );
	for ( i = 0; i < lookup_label_mode_n; i++ ) {
		lookup_label_mode[i] = strdup ( OPTIONS_LABEL_MODE_NAMES[i] );
	}
	lookup_label_mode[lookup_label_mode_n] = NULL;
	f_label_mode_point = gui_field_create_string ( 	"label-mode-point", (_("Label mode (points):")), (_("Label placement on point geometries.")),
			(_("Extra")), FALSE, OPTIONS_LABEL_MODE_NAMES[opts->label_mode_point], 0, NULL,
			GUI_FIELD_CONVERT_TO_NONE, (const char**) lookup_label_mode);
	f_label_mode_line = gui_field_create_string ( 	"label-mode-line", (_("Label mode (lines):")), (_("Label placement on line geometries.")),
			(_("Extra")), FALSE, OPTIONS_LABEL_MODE_NAMES[opts->label_mode_line], 0, NULL,
			GUI_FIELD_CONVERT_TO_NONE, (const char**) lookup_label_mode);
	f_label_mode_poly = gui_field_create_string ( 	"label-mode-poly", (_("Label mode (polygons):")), (_("Label placement on polygon geometries.")),
			(_("Extra")), FALSE, OPTIONS_LABEL_MODE_NAMES[opts->label_mode_poly], 0, NULL,
			GUI_FIELD_CONVERT_TO_NONE, (const char**) lookup_label_mode);

	/* output data orientation */
	lookup_orient_mode_n = 0;
	while ( strlen ( OPTIONS_ORIENT_MODE_NAMES [lookup_orient_mode_n] ) > 0 ) {
		lookup_orient_mode_n ++;
	}
	lookup_orient_mode = malloc ( sizeof ( char*) * ( lookup_orient_mode_n + 1 ) );
	for ( i = 0; i < lookup_orient_mode_n; i++ ) {
		lookup_orient_mode[i] = strdup ( OPTIONS_ORIENT_MODE_NAMES[i] );
	}
	lookup_orient_mode[lookup_orient_mode_n] = NULL;
	f_orient_mode = gui_field_create_string ( 	"orient-mode", (_("Output orientation:")), (_("Axis orientation for output geometries.")),
			(_("Extra")), FALSE, OPTIONS_ORIENT_MODE_NAMES[opts->orient_mode], 0, NULL,
			GUI_FIELD_CONVERT_TO_NONE, (const char**) lookup_orient_mode);

	/* data selections */
	f_select = gui_field_create_edit_selections ( 	"selection", (_("Selection:")), (_("Select subset(s) of input data.")), (_("Extra")),
			FALSE, (const char**) opts->selection );

	/* reprojection settings */
	f_proj_in = gui_field_create_string ( "proj-in", (_("Input CRS:")), (_("Coordinate reference system (CRS) of input data, in PROJ.4 format.")),
			(_("Reprojection")), FALSE, NULL, 0, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );
	f_proj_out = gui_field_create_string ("proj-out", (_("Output CRS:")), (_("Coordinate reference system (CRS) of output data, in PROJ.4 format.")),
			(_("Reprojection")), FALSE, NULL, 0, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );
	f_proj_dx = gui_field_create_double ( "proj-dx", (_("X shift to WGS 84:")), (_("X shift factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dx_str, TRUE );
	f_proj_dy = gui_field_create_double ( "proj-dy", (_("Y shift to WGS 84:")), (_("Y shift factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dy_str, TRUE );
	f_proj_dz = gui_field_create_double ( "proj-dz", (_("Z shift to WGS 84:")), (_("Z shift factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dz_str, TRUE );
	f_proj_rx = gui_field_create_double ( "proj-rx", (_("X rotation to WGS 84:")), (_("X rotation factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dx_str, TRUE );
	f_proj_ry = gui_field_create_double ( "proj-ry", (_("Y rotation to WGS 84:")), (_("Y rotation factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dy_str, TRUE );
	f_proj_rz = gui_field_create_double ( "proj-rz", (_("Z rotation to WGS 84:")), (_("Z rotation factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_dz_str, TRUE );
	f_proj_ds = gui_field_create_double ( "proj-ds", (_("Scaling to WGS 84:")), (_("Scaling factor for datum transformation to WGS 84.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_ds_str, TRUE );
	f_proj_grid = gui_field_create_file_in ( "proj-grid", (_("Datum transformation grid:")), (_("Name of input grid file for local datum transformation.")), (_("Reprojection")),
			FALSE, opts->wgs84_trans_grid );

	/* advanced settings (snapping tolerance, etc. */
	lookup_topo_level_n = 0; /* level of topological cleaning */
	while ( strlen ( OPTIONS_TOPO_LEVEL_NAMES [lookup_topo_level_n] ) > 0 ) {
		lookup_topo_level_n ++;
	}
	lookup_topo_level = malloc ( sizeof ( char*) * ( lookup_topo_level_n + 1 ) );
	for ( i = 0; i < lookup_topo_level_n; i++ ) {
		lookup_topo_level[i] = strdup ( OPTIONS_TOPO_LEVEL_NAMES[i] );
	}
	lookup_topo_level[lookup_topo_level_n] = NULL;
	f_topo_level = gui_field_create_string ( 	"topo-level", (_("Topology level:")), (_("Topological processing level.")),
			(_("Advanced")), FALSE, OPTIONS_TOPO_LEVEL_NAMES[opts->topo_level], 0, NULL,
			GUI_FIELD_CONVERT_TO_NONE, (const char**) lookup_topo_level);
	f_tolerance = gui_field_create_double ( "tolerance", (_("Coord. tolerance:")), (_("Distance threshold for coordinates.")), (_("Advanced")),
			FALSE, opts->tolerance_str, TRUE );
	f_snapping = gui_field_create_double ( "snapping", (_("Boundary snap dist.:")), (_("Snapping threshold for polygon boundary vertices.")), (_("Advanced")),
			FALSE, opts->snapping_str, FALSE );
	f_dangling = gui_field_create_double ( "dangling", (_("Dangle snap dist.:")), (_("Snapping threshold for dangling line nodes.")), (_("Advanced")),
			FALSE, opts->dangling_str, FALSE );
	f_x_offset = gui_field_create_double ( "x-offset", (_("Coord. X offset:")), (_("Constant offset for X coordinates.")), (_("Advanced")),
			FALSE, opts->offset_x_str, TRUE );
	f_y_offset = gui_field_create_double ( "y-offset", (_("Coord. Y offset:")), (_("Constant offset for Y coordinates.")), (_("Advanced")),
			FALSE, opts->offset_y_str, TRUE );
	f_z_offset = gui_field_create_double ( "z-offset", (_("Coord. Z offset:")), (_("Constant offset for Z coordinates.")), (_("Advanced")),
			FALSE, opts->offset_z_str, TRUE );
	f_d_places = gui_field_create_integer ( "decimal-places", (_("DBF decimal places:")), (_("Numeric precision for DBF floating point attributes.")), (_("Advanced")),
			FALSE, opts->decimal_places_str, FALSE );
	if ( strlen ( opts->decimal_point ) > 0 ) {
		buf[0] = opts->decimal_point[0];
		buf[1] = '\0';
	} else {
		sprintf ( buf, "%s", I18N_DECIMAL_POINT );
	}
	f_decimal_point = gui_field_create_string ( 	"decimal-point", (_("Decimal point:")), (_("Decimal point character in input data.")), (_("Advanced")),
			FALSE, buf, 1, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );
	if ( strlen ( opts->decimal_group ) > 0 ) {
		buf[0] = opts->decimal_group[0];
		buf[1] = '\0';
	} else {
		sprintf ( buf, "%s", I18N_THOUSANDS_SEP );
	}
	f_decimal_group = gui_field_create_string ( 	"decimal-group", (_("Decimal group:")), (_("Numeric group character in input data.")), (_("Advanced")),
			FALSE, buf, 1, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );

	/* boolean fields (flags) */
	f_dump_raw = gui_field_create_boolean ( "raw-data", (_("Raw vertex output:")), (_("Save raw vertex data as additional output.")), (_("Extra")),
			opts->strict );
	f_2d = gui_field_create_boolean ( "force-2d", (_("Force 2D output:")), (_("Discard any Z data from output.")), (_("Extra")),
			opts->strict );
	f_strict = gui_field_create_boolean ( "strict", (_("Strict parsing:")), (_("Use stricter input validation.")), (_("Extra")),
			opts->strict );
	f_validate_only = gui_field_create_boolean ( "validate-parser", (_("Validate only:")), (_("Just validate parser schema.")), (_("Extra")),
			opts->just_dump_parser );

	/* Add fields to GUI. Order matters! */
	gui_form_add_field ( gform, f_input );
	gui_form_add_field ( gform, f_parser );
	gui_form_add_field ( gform, f_output );
	gui_form_add_field ( gform, f_name );
	gui_form_add_field ( gform, f_format );
	gui_form_add_field ( gform, f_label_field );
	gui_form_add_field ( gform, f_label_mode_point );
	gui_form_add_field ( gform, f_label_mode_line );
	gui_form_add_field ( gform, f_label_mode_poly );
	gui_form_add_field ( gform, f_orient_mode );
	gui_form_add_field ( gform, f_topo_level );
	gui_form_add_field ( gform, f_select );
	gui_form_add_field ( gform, f_log );
	gui_form_add_field ( gform, f_proj_in );
	gui_form_add_field ( gform, f_proj_out );
	gui_form_add_field ( gform, f_proj_dx );
	gui_form_add_field ( gform, f_proj_dy );
	gui_form_add_field ( gform, f_proj_dz );
	gui_form_add_field ( gform, f_proj_rx );
	gui_form_add_field ( gform, f_proj_ry );
	gui_form_add_field ( gform, f_proj_rz );
	gui_form_add_field ( gform, f_proj_ds );
	gui_form_add_field ( gform, f_proj_grid );
	gui_form_add_field ( gform, f_tolerance );
	gui_form_add_field ( gform, f_snapping );
	gui_form_add_field ( gform, f_dangling );
	gui_form_add_field ( gform, f_x_offset );
	gui_form_add_field ( gform, f_y_offset );
	gui_form_add_field ( gform, f_z_offset );
	gui_form_add_field ( gform, f_d_places );
	gui_form_add_field ( gform, f_decimal_point );
	gui_form_add_field ( gform, f_decimal_group );
	gui_form_add_field ( gform, f_dump_raw );
	gui_form_add_field ( gform, f_2d );
	gui_form_add_field ( gform, f_strict );
	gui_form_add_field ( gform, f_validate_only );

	/* compose GUI form off-screen */
	gui_form_compose ( gform );

	/* connect text output and show welcome message */
	gui_form_connect_text_view ( gform );
	gui_form_connect_status_bar ( gform );
	gtk_text_buffer_get_iter_at_offset
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, -1);

	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, "", -1, "font_white", NULL);
	snprintf ( buf, PRG_MAX_STR_LEN, "%s", t_get_prg_name_and_version());
	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, buf, -1, "font_bold", "font_white", NULL);
	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, _("\nFill in options on the left, then click \"Run\".\n"), -1, "font_white", NULL);
	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, _("Messages, errors and warnings will appear in this area.\n"), -1, "font_white", NULL);
	gtk_text_buffer_insert_with_tags_by_name
	(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
			&iter, ("---\n"), -1, "font_white", NULL);

	return ( gform );
}


/*
 * Gets the contents of all the GUI input fields and stores
 * them in the appropriate program options.
 * This function is called every time the user presses the
 * "Execute" button.
 *
 * We do some minimal options error checking here.
 *
 * Returns "0" if all options are OK, error number, otherwise.
 *
 */
int process_gui_options ( options *opts )
{
	int i, n;
	BOOLEAN valid;
	GtkTreeIter iter;
	BOOLEAN error;
	int num_errors = 0;


	/* INPUT FILES */
	/* get size of new content */
	n = 0;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(f_input->list), &iter);
	while (valid) {
		n++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(f_input->list), &iter);
	}
	if ( n == 0 ) {
		/* free old option contents */
		if (opts->input != NULL) {
			for (i = 0; i < opts->num_input; i++) {
				if (opts->input[i] != NULL)
					free(opts->input[i]);
				opts->input[i] = NULL;
			}
			free(opts->input);
		}
		opts->num_input = 0;
	}
	if (n > 0) {
		/* free old option contents */
		if (opts->input != NULL) {
			for (i = 0; i < opts->num_input; i++) {
				if (opts->input[i] != NULL)
					free(opts->input[i]);
			}
			free(opts->input);
		}
		/* make room for new content */
		opts->input = malloc(sizeof(char*) * (n + 1));
		opts->input[n] = NULL; /* terminator */
		/* store new content */
		i = 0;
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(f_input->list), &iter);
		while (valid) {
			char *str_data;
			gtk_tree_model_get(GTK_TREE_MODEL(f_input->list), &iter, 0,
					&str_data, -1);
			opts->input[i] = strdup(str_data);
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(f_input->list),
					&iter);
			i++;
		}
		opts->num_input = n;
	}

	/* PARSER */
	if (gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_parser->file_button))
			!= NULL) {
		if (opts->schema_file != NULL)
			free(opts->schema_file);
		opts->schema_file = strdup(gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(f_parser->file_button)));
	}

	/* OUTPUT DIR */
	if (gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_output->file_button))
			!= NULL && strlen(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					f_output->file_button))) > 0) {
		if (opts->output != NULL)
			free(opts->output);
		opts->output = strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
				f_output->file_button)));
	}

	/* OUTPUT BASENAME */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_name->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(
					GTK_ENTRY_BUFFER(f_name->buffer))) > 0) {
		if (opts->base != NULL)
			free(opts->base);
		opts->base = strdup(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(
				f_name->buffer)));
		if (opts->base != NULL && t_is_legal_name(opts->base) == FALSE) {
			err_show(ERR_EXIT,
					_("\"%s\" is not a valid output file (base) name."),
					opts->base);
			if (opts->base != NULL)
				free(opts->base);
			opts->base = NULL;
			num_errors++;
		}
	}

	/* FORMAT */
	if (gtk_combo_box_get_active_text(GTK_COMBO_BOX(f_format->combo_box))
			!= NULL && strlen(gtk_combo_box_get_active_text(GTK_COMBO_BOX(
					f_format->combo_box))) > 0) {
		i = 0;
		while (strlen(PRG_OUTPUT_DESC[i]) > 0) {
			if (!strcmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(
					f_format->combo_box)), PRG_OUTPUT_DESC[i])) {
				opts->format = i;
				break;
			}
			i++;
		}
	}

	/* LOG FILE NAME */
	if (gtk_entry_get_text(GTK_ENTRY(f_log->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_log->i_widget))) > 0) {
		if (opts->log != NULL) {
			free(opts->log);
			opts->log = NULL;
		}
		opts->log = strdup(gtk_entry_get_text(GTK_ENTRY(f_log->i_widget)));
		if (opts->log != NULL && t_is_legal_path(opts->log) == FALSE) {
			err_show(ERR_EXIT, _("\"%s\" is not a valid log file name."),
					opts->log);
			if (opts->log != NULL) {
				free(opts->log);
			}
			opts->log = NULL;
			num_errors++;
		}
	}

	/* LABELING OPTIONS */
	/* label field */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_label_field->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(
					GTK_ENTRY_BUFFER(f_label_field->buffer))) > 0) {
		if (opts->label_field != NULL)
			free(opts->label_field);
		opts->label_field = strdup(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(
				f_label_field->buffer)));
	}
	/* label mode for points */
	gint label_mode_choice = 0;
	label_mode_choice =
			gtk_combo_box_get_active(GTK_COMBO_BOX(f_label_mode_point->combo_box));
	if ( (int) label_mode_choice > -1 ) {
		opts->label_mode_point = (int) label_mode_choice;
	}
	/* label mode for lines */
	label_mode_choice =
			gtk_combo_box_get_active(GTK_COMBO_BOX(f_label_mode_line->combo_box));
	if ( (int) label_mode_choice > -1 ) {
		opts->label_mode_line = (int) label_mode_choice;
	}
	/* label mode for polygons */
	label_mode_choice =
			gtk_combo_box_get_active(GTK_COMBO_BOX(f_label_mode_poly->combo_box));
	if ( (int) label_mode_choice > -1 ) {
		opts->label_mode_poly = (int) label_mode_choice;
	}

	/* ORIENTATION MODE */
	gint orient_mode_choice =
			gtk_combo_box_get_active(GTK_COMBO_BOX(f_orient_mode->combo_box));
	if ( (int) orient_mode_choice > -1 ) {
		opts->orient_mode = (int) orient_mode_choice;
	}
	
	/* TOPOLOGY LEVEL */
	gint topo_level_choice =
			gtk_combo_box_get_active(GTK_COMBO_BOX(f_topo_level->combo_box));
	if ( (int) topo_level_choice > -1 ) {
		opts->topo_level = (int) topo_level_choice;
	}

	/* SELECTION COMMANDS */
	n = 0;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(f_select->list), &iter);
	while (valid) {
		n++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(f_select->list), &iter);
	}
	if ( n == 0 ) {
		/* free old option contents */
		for (i = 0; i < PRG_MAX_SELECTIONS; i++) {
			if (opts->selection[i] != NULL) free(opts->selection[i]);
			opts->selection[i] = NULL;
		}
	}
	if (n > 0) {
		/* free old option contents */
		for (i = 0; i < PRG_MAX_SELECTIONS; i++) {
			if (opts->selection[i] != NULL) free(opts->selection[i]);
			opts->selection[i] = NULL;
		}
		/* store new content */
		i = 0;
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(f_select->list), &iter);
		while ( valid && i < PRG_MAX_SELECTIONS ) {
			char *str_data;
			gtk_tree_model_get(GTK_TREE_MODEL(f_select->list), &iter, 0, &str_data, -1);
			opts->selection[i] = strdup(str_data);
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(f_select->list), &iter);
			i++;
		}
	}

	/* COORD TOLERANCE */
	if (gtk_entry_get_text(GTK_ENTRY(f_tolerance->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_tolerance->i_widget))) > 0) {
		snprintf(opts->tolerance_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_tolerance->i_widget)));
		opts->tolerance = t_str_to_dbl(opts->tolerance_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(
					ERR_EXIT,
					_("The specified tolerance setting is not a valid number."));
			opts->tolerance = OPTIONS_DEFAULT_TOLERANCE;
			t_dbl_to_str (opts->tolerance, opts->tolerance_str);
			num_errors++;
		}
		if ( opts->tolerance < 0.0 ) {
			err_show(ERR_NOTE, "");
			err_show ( ERR_WARN, _("Tolerance setting < 0. Identical vertices will not be removed."));
		}
	}


	/* SNAPPING (POLY BOUNDARIES) */
	if (gtk_entry_get_text(GTK_ENTRY(f_snapping->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_snapping->i_widget))) > 0) {
		snprintf(opts->snapping_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_snapping->i_widget)));
		opts->snapping = t_str_to_dbl(opts->snapping_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(
					ERR_EXIT,
					_("The specified snapping setting for boundaries is not a valid number."));
			opts->snapping = OPTIONS_DEFAULT_SNAPPING;
			t_dbl_to_str (opts->snapping, opts->snapping_str);
			num_errors++;
		}
		if ( opts->snapping < 0.0 ) {
			err_show ( ERR_EXIT, _("Snapping setting for boundaries must be 0 or a positive number."));
			opts->snapping = OPTIONS_DEFAULT_SNAPPING;
			t_dbl_to_str (opts->snapping, opts->snapping_str);
			num_errors ++;
		}
	}

	/* SNAPPING (DANGLING LINE NODES) */
	if (gtk_entry_get_text(GTK_ENTRY(f_dangling->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_dangling->i_widget))) > 0) {
		snprintf(opts->dangling_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_dangling->i_widget)));
		opts->dangling = t_str_to_dbl(opts->dangling_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(
					ERR_EXIT,
					_("The specified snapping setting for dangles is not a valid number."));
			opts->dangling = OPTIONS_DEFAULT_DANGLING;
			t_dbl_to_str (opts->dangling, opts->dangling_str);
			num_errors++;
		}
		if ( opts->dangling < 0.0 ) {
			err_show ( ERR_EXIT, _("Snapping setting for dangles must be 0 or a positive number."));
			opts->dangling = OPTIONS_DEFAULT_DANGLING;
			t_dbl_to_str (opts->dangling, opts->dangling_str);
			num_errors ++;
		}
	}


	/* COORD X OFFSET */
	if (gtk_entry_get_text(GTK_ENTRY(f_x_offset->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_x_offset->i_widget))) > 0) {
		snprintf(opts->offset_x_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_x_offset->i_widget)));
		opts->offset_x = t_str_to_dbl(opts->offset_x_str, 0, 0, &error, NULL );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified X coordinate offset is not a valid number."));
			opts->offset_x = OPTIONS_DEFAULT_OFFSET_X;
			t_dbl_to_str (opts->offset_x, opts->offset_x_str);
			num_errors ++;
		}
	}


	/* COORD Y OFFSET */
	if (gtk_entry_get_text(GTK_ENTRY(f_y_offset->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_y_offset->i_widget))) > 0) {
		snprintf(opts->offset_y_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_y_offset->i_widget)));
		opts->offset_y = t_str_to_dbl(opts->offset_y_str, 0, 0, &error, NULL );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified Y coordinate offset is not a valid number."));
			opts->offset_y = OPTIONS_DEFAULT_OFFSET_Y;
			t_dbl_to_str (opts->offset_y, opts->offset_y_str);
			num_errors ++;
		}
	}


	/* COORD Z OFFSET */
	if (gtk_entry_get_text(GTK_ENTRY(f_z_offset->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_z_offset->i_widget))) > 0) {
		snprintf(opts->offset_z_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_z_offset->i_widget)));
		opts->offset_z = t_str_to_dbl(opts->offset_z_str, 0, 0, &error, NULL );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified Z coordinate offset is not a valid number."));
			opts->offset_z = OPTIONS_DEFAULT_OFFSET_Z;
			t_dbl_to_str (opts->offset_z, opts->offset_z_str);
			num_errors ++;
		}
	}


	/* DECIMAL PLACES */
	if (gtk_entry_get_text(GTK_ENTRY(f_d_places->i_widget)) != NULL && strlen(
			gtk_entry_get_text(GTK_ENTRY(f_d_places->i_widget))) > 0) {
		snprintf(opts->decimal_places_str, PRG_MAX_STR_LEN, "%s",
				gtk_entry_get_text(GTK_ENTRY(f_d_places->i_widget)));
		opts->decimal_places = t_str_to_int (opts->decimal_places_str, &error, NULL );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("Specified decimal places is not a valid number."));
			opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
			snprintf ( opts->decimal_places_str, PRG_MAX_STR_LEN, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
			num_errors ++;
		}
		if ( opts->decimal_places < 0 ) {
			err_show ( ERR_EXIT, _("Number of decimal places must be 0 or a positive number."));
			opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
			snprintf ( opts->decimal_places_str, PRG_MAX_STR_LEN, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
			num_errors ++;
		}
		if ( opts->decimal_places > PRG_MAX_DECIMAL_PLACES ) {
			err_show ( ERR_EXIT, _("Number of decimal places cannot exceed %i."), PRG_MAX_DECIMAL_PLACES);
			opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
			snprintf ( opts->decimal_places_str, PRG_MAX_STR_LEN, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
			num_errors ++;
		}
	}


	/* DECIMAL POINT */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(
					GTK_ENTRY_BUFFER(f_decimal_point->buffer))) > 0) {
		opts->decimal_point[0] = gtk_entry_buffer_get_text(
				GTK_ENTRY_BUFFER(f_decimal_point->buffer))[0];
		opts->decimal_point[1] = '\0';
	}


	/* DECIMAL GROUP */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(
					GTK_ENTRY_BUFFER(f_decimal_group->buffer))) > 0) {
		opts->decimal_group[0] = gtk_entry_buffer_get_text(
				GTK_ENTRY_BUFFER(f_decimal_group->buffer))[0];
		opts->decimal_group[1] = '\0';
	}


	/* DECIMAL POINT & GROUP */
	if ( ( gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)) != NULL &&
			gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) == NULL ) ||
			( gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)) == NULL &&
					gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) != NULL ) )
	{
		err_show ( ERR_EXIT, _("Decimal point and grouping characters must both be specified."));
		num_errors ++;
	}

	if ( gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)) != NULL &&
			gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) != NULL     )
	{
		if (	strlen (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer))) < 1 ||
				strlen (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer))) < 1 ) {
			err_show ( ERR_EXIT, _("Decimal point and grouping characters must both be specified."));
			num_errors ++;
		}
	}

	if ( gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)) != NULL &&
			gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) != NULL     )
	{
		if ( !strcmp ( gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_point->buffer)),
				gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_decimal_group->buffer)) ) )
		{
			err_show ( ERR_EXIT, _("Decimal point and grouping characters must not be identical."));
			num_errors ++;
		}
	}


	/* SRS & REPROJECTION */
	/* input SRS */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_in->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_in->buffer))) > 0) {
		if (opts->proj_in != NULL) {
			free(opts->proj_in);
		}
		opts->proj_in = strdup(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_in->buffer)));
	}
	/* output SRS */
	if (gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_out->buffer)) != NULL
			&& strlen(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_out->buffer))) > 0) {
		if (opts->proj_out != NULL) {
			free(opts->proj_out);
		}
		opts->proj_out = strdup(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(f_proj_out->buffer)));
	}
	/* WGS 84 dX */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_dx->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_dx->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_dx_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_dx->i_widget)));
		opts->wgs84_trans_dx = t_str_to_dbl(opts->wgs84_trans_dx_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum X shift is not a valid number."));
			opts->wgs84_trans_dx = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_dx, opts->wgs84_trans_dx_str);
			num_errors++;
		}
	}
	/* WGS 84 dY */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_dy->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_dy->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_dy_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_dy->i_widget)));
		opts->wgs84_trans_dy = t_str_to_dbl(opts->wgs84_trans_dy_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum Y shift is not a valid number."));
			opts->wgs84_trans_dy = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_dy, opts->wgs84_trans_dy_str);
			num_errors++;
		}
	}
	/* WGS 84 dZ */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_dz->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_dz->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_dz_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_dz->i_widget)));
		opts->wgs84_trans_dz = t_str_to_dbl(opts->wgs84_trans_dz_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum Z shift is not a valid number."));
			opts->wgs84_trans_dz = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_dy, opts->wgs84_trans_dz_str);
			num_errors++;
		}
	}
	/* WGS 84 rX */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_rx->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_rx->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_rx_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_rx->i_widget)));
		opts->wgs84_trans_rx = t_str_to_dbl(opts->wgs84_trans_rx_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum X rotation is not a valid number."));
			opts->wgs84_trans_rx = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_rx, opts->wgs84_trans_rx_str);
			num_errors++;
		}
	}
	/* WGS 84 rY */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_ry->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_ry->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_ry_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_ry->i_widget)));
		opts->wgs84_trans_ry = t_str_to_dbl(opts->wgs84_trans_ry_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum Y rotation is not a valid number."));
			opts->wgs84_trans_ry = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_ry, opts->wgs84_trans_ry_str);
			num_errors++;
		}
	}
	/* WGS 84 rZ */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_rz->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_rz->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_rz_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_rz->i_widget)));
		opts->wgs84_trans_rz = t_str_to_dbl(opts->wgs84_trans_rz_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum Z rotation is not a valid number."));
			opts->wgs84_trans_rz = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_rz, opts->wgs84_trans_rz_str);
			num_errors++;
		}
	}
	/* WGS 84 dS */
	if (gtk_entry_get_text(GTK_ENTRY(f_proj_ds->i_widget)) != NULL &&
			strlen( gtk_entry_get_text(GTK_ENTRY(f_proj_ds->i_widget))) > 0)
	{
		snprintf(opts->wgs84_trans_ds_str, PRG_MAX_STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(f_proj_ds->i_widget)));
		opts->wgs84_trans_ds = t_str_to_dbl(opts->wgs84_trans_ds_str, 0, 0, &error, NULL );
		if (error == TRUE) {
			err_show(ERR_EXIT, _("The specified WGS 84 datum scaling is not a valid number."));
			opts->wgs84_trans_ds = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_ds, opts->wgs84_trans_ds_str);
			num_errors++;
		}
	}
	/* grid shift file */
	if (gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_proj_grid->file_button)) != NULL) {
		if (opts->wgs84_trans_grid != NULL) {
			free(opts->wgs84_trans_grid);
		}
		opts->wgs84_trans_grid = strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_proj_grid->file_button)));
	}

	/* FLAGS */
	opts->dump_raw = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			f_dump_raw->checkbox));
	opts->force_2d = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			f_2d->checkbox));
	opts->strict = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			f_strict->checkbox));
	opts->just_dump_parser = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			f_validate_only->checkbox));

	return (num_errors);
}


/* Shows an About dialog */
void about_handler (GtkEntry *button, gpointer data)
{
	GtkAboutDialog *dialog;
	GdkPixbuf *pb_logo_xpm;
	char buf[PRG_MAX_STR_LEN+1];

	dialog = GTK_ABOUT_DIALOG (gtk_about_dialog_new());
	gtk_about_dialog_set_name (dialog, t_get_prg_name());
	snprintf (buf,PRG_MAX_STR_LEN,"%s", t_get_prg_version() );
	gtk_about_dialog_set_version (dialog, buf);
	gtk_about_dialog_set_copyright (dialog, "(c) 2013-2018 by the survey-tools.org development team (http://www.survey-tools.org).\nSponsored by State Heritage Management Baden-Wuerttemberg (http://www.denkmalpflege-bw.de).\nDevelopment coordinated by CSGIS GbR (http://www.csgis.de).\nDesign and implementation by Benjamin Ducke (benducke@fastmail.fm)");
	gtk_about_dialog_set_comments (dialog, _("Software for exporting survey data to GIS standard formats."));
	gtk_about_dialog_set_license (dialog,_("This is free software under the GNU Public License.\nA copy of the license agreement has been delivered to you in the file LICENSE.\nTranslations can be found at http://www.gnu.org/licenses/gpl.html\nThis software includes LGPL-licensed code from shapelib: http://shapelib.maptools.org"));

	/* logo graphics */
	pb_logo_xpm = gdk_pixbuf_new_from_xpm_data ((const char**) logo_xpm);
	gtk_about_dialog_set_logo (dialog, pb_logo_xpm);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
	gtk_widget_show_all(GTK_WIDGET(dialog));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


/* Quits the application */
void quit_handler (GtkEntry *button, gpointer data)
{
	exit (PRG_EXIT_OK);
}

/*
 * Runs the processing.
 */
void execute_handler (GtkEntry *button, gpointer data)
{
	options *opts;
	int opt_err;
	GtkWidget *dialog;
	GtkTextIter iter;


	opts = (options*) data;

	/* read new option settings from GUI widgets */
	opt_err = process_gui_options ( opts );

	OPTIONS_GUI_MODE = TRUE;

	if ( opt_err == 0 ) {

		/* reset error and warnings status */
		ERR_STATUS = 0;
		WARN_STATUS = 0;

		/* disable all controls until we are done here !*/
		gtk_widget_set_sensitive (opts->window, FALSE);

		/* run only if all options are OK */
		run_once ( opts, opts->window );

		/* output visual text marker */
		GtkTextBuffer *tbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW));
		GtkTextMark *mark = gtk_text_buffer_get_insert(tbuffer);
		gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);
		gtk_text_buffer_insert_with_tags_by_name
		(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
				&iter, _("(End of output)\n"), -1, "font_bold", "font_white", NULL);
		gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);

		/* enable controls again !*/
		gtk_widget_set_sensitive (opts->window, TRUE);

		/* update status message */
		if ( WARN_STATUS != 0 )
			gtk_statusbar_push (GTK_STATUSBAR(GUI_STATUS_BAR), 0, "Processing finished with warnings. Please check.");
		if ( ERR_STATUS != 0 )
			gtk_statusbar_push (GTK_STATUSBAR(GUI_STATUS_BAR), 0, "Processing finished with errors. Please check.");
		if ( ERR_STATUS == 0 && WARN_STATUS == 0 )
			gtk_statusbar_push (GTK_STATUSBAR(GUI_STATUS_BAR), 0, "Processing finished without errors or warnings.");

	} else {
		if (OPTIONS_GUI_MODE == TRUE) {
			/* refuse to run with bad options */
			if (opts->num_input < 1 || opts->schema_file == NULL || (opts->base
					== NULL) || (opts->output == NULL)) {
				if (opts->window != NULL) {
					dialog = gtk_message_dialog_new(GTK_WINDOW(opts->window),
							GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
							GTK_BUTTONS_CLOSE, "Invalid option(s) given.");
					gtk_message_dialog_format_secondary_text(
							GTK_MESSAGE_DIALOG(dialog),
							_("There was a problem with at least one option setting.\nPlease check the \"Messages\" area for details."));
					gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
					gtk_widget_show_all(dialog);
					gtk_dialog_run(GTK_DIALOG(dialog));
					gtk_widget_destroy(dialog);
				}
			}
		}
	}
}


#endif


/*
 *
 * MAIN FUNCTION
 * Processes all data and shows summary statistics.
 *
 */
int main(int argc, char *argv[])
{
	options *opts;
	int i;

	/* Windows: Set CLI codepage to the closest thing to UTF-8 */
#ifdef MINGW
	SetConsoleOutputCP(65001);
#endif


	/* store program name and path as called */
	if ( argv[0] != NULL && strlen(argv[0]) > 0 ) {
		PRG_NAME_CLI = strdup(basename(argv[0]));
		PRG_PATH_CLI = realpath(argv[0],NULL);
		PRG_DIR_CLI = realpath(dirname(argv[0]),NULL);
	} else {
		PRG_NAME_CLI = strdup(PRG_NAME_DEFAULT);
		PRG_PATH_CLI = strdup("");
		PRG_DIR_CLI = strdup("");
	}

	/* default to "no GUI" */
	OPTIONS_GUI_MODE = FALSE;

	/* force English messages and number format */
	OPTIONS_FORCE_ENGLISH = FALSE;

	/* initialize i18n translation engine */
	i18n_init ();

	/* init command line options */
	opts = options_create ( argc, argv );

#ifdef GUI
	if ( argc < 2 || opts->show_gui == TRUE ) {
		OPTIONS_GUI_MODE = TRUE;
		opts->show_gui = TRUE;
	}
	/* now we check for --show-gui and --help */
	for ( i = 0; i < opts->argc; i++  ) {
		if (!strcmp ( "--english", opts->argv[i]) ||
				!strcmp ( "-e", opts->argv[i] )  )
		{
			OPTIONS_FORCE_ENGLISH = TRUE;
			opts->force_english = TRUE;
		}
		if (!strcmp ( "--show-gui", opts->argv[i]) ||
				!strcmp ( "-u", opts->argv[i] )  )
		{
			OPTIONS_GUI_MODE = TRUE;
			opts->show_gui = TRUE;
		}		
		if (!strcmp ( "--help", opts->argv[i]) ||
				!strcmp ( "-h", opts->argv[i] )  )
		{
			opts->just_dump_help = TRUE;
		} else {
			opts->just_dump_help = FALSE;
		}
	}
#endif

	for ( i = 0; i < opts->argc; i++  ) {
		if (!strcmp ( "--english", opts->argv[i]) ||
				!strcmp ( "-e", opts->argv[i] )  )
		{
			OPTIONS_FORCE_ENGLISH = TRUE;
			opts->force_english = TRUE;
		}
		if (!strcmp ( "--help", opts->argv[i]) ||
				!strcmp ( "-h", opts->argv[i] )  )
		{
			opts->just_dump_help = TRUE;
		} else {
			opts->just_dump_help = FALSE;
		}
	}

	/* parse options may abort if in CLI mode and options are bad */
	if ( opts->just_dump_help == FALSE )
	{
		/* just show help text */
		if ( opts->force_english == TRUE ) {
			i18n_force_english();
		}
		options_parse ( opts );
	}
	else
	{
		/* just show help text */
		if ( opts->force_english == TRUE ) {
			i18n_force_english();
		}
		options_help();
		exit (PRG_EXIT_OK);
	}

#ifdef GUI
	/*
	 * If we have GUI support, then we must now decide
	 * whether to display the GUI form!
	 */
	if ( opts->show_gui == TRUE ) {
		gui_init ();
		gui_form *gform = make_gui_form ( opts );
		opts->window = (GtkWidget*) gform->window;
		/* register handler for "Execute" button */
		g_signal_connect (G_OBJECT (gform->button_run),
				"clicked", G_CALLBACK (execute_handler), opts);
		/* register menu item actions */
		g_signal_connect (G_OBJECT (gform->menu_file_exec),
				"activate", G_CALLBACK (execute_handler), opts);
		g_signal_connect (G_OBJECT (gform->menu_file_quit),
				"activate", G_CALLBACK (quit_handler), opts);
		g_signal_connect (G_OBJECT (gform->menu_help_about),
				"activate", G_CALLBACK (about_handler), opts);
		/* show form and hand control over to GUI */
		gui_form_show ( gform );
		exit (PRG_EXIT_OK);
	}

	/* CLI mode: just run once */
	run_once ( opts, NULL ) ;

#else
	/* CLI mode: just run once */
	run_once ( opts );
#endif

	/* release memory for prg options */
	options_destroy (opts);

	/* release mem for i18n */
	i18n_free ();

	return (PRG_EXIT_OK); /* = EXIT_SUCCESS */
}
