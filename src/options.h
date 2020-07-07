/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	options.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Functions to parse command line options and save them to
 * 				an options object.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "global.h"


#ifndef OPTIONS_H
#define OPTIONS_H

/* label modes for lines and polygons */
#define OPTIONS_LABEL_MODE_CENTER	0
#define OPTIONS_LABEL_MODE_FIRST	1
#define OPTIONS_LABEL_MODE_LAST		2
#define OPTIONS_LABEL_MODE_NONE		3

/* orientation modes for pseudo 3D output */
#define OPTIONS_ORIENT_MODE_WORLD_XYZ	0
#define OPTIONS_ORIENT_MODE_LOCAL_XZ	1

/* Default option values */
#define OPTIONS_DEFAULT_TOLERANCE 			0.0
#define OPTIONS_DEFAULT_SNAPPING 			0.0
#define OPTIONS_DEFAULT_DANGLING 			0.0
#define OPTIONS_DEFAULT_OFFSET_X 			0.0
#define OPTIONS_DEFAULT_OFFSET_Y 			0.0
#define OPTIONS_DEFAULT_OFFSET_Z 			0.0
#define OPTIONS_DEFAULT_DECIMAL_PLACES 		3
#define OPTIONS_DEFAULT_WGS84_TRANS_DX 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_DY 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_DZ 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_RX 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_RY 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_RZ 		0.0
#define OPTIONS_DEFAULT_WGS84_TRANS_DS 		1.0
#define OPTIONS_DEFAULT_LABEL_MODE_POINT	OPTIONS_LABEL_MODE_CENTER
#define OPTIONS_DEFAULT_LABEL_MODE_LINE		OPTIONS_LABEL_MODE_CENTER
#define OPTIONS_DEFAULT_LABEL_MODE_POLY		OPTIONS_LABEL_MODE_CENTER
#define OPTIONS_DEFAULT_ORIENT_MODE			OPTIONS_ORIENT_MODE_WORLD_XYZ

/*
 * GETTEXT NOTES:
 *
 * We translated only the longer option descriptions, not the short option names!
 *
 * The keyword "gettext_noop", defined below, has to be passed to xgettext:
 *
 *   xgettext -k"gettext_noop"
 *
 * These translations have to be gettext'ed explicitly, like this:
 *
 *   fprintf (gettext ("STRING"))
 *
 * See: https://www.gnu.org/software/gettext/manual/html_node/Special-cases.html
 *
 * */
#define gettext_noop(String) String

/* label mode names (not case sensitive) */
static const char OPTIONS_LABEL_MODE_NAMES[][10] = {
		"center",
		"first",
		"last",
		"none",
		""
};

static const char OPTIONS_LABEL_MODE_DESC[][30] = {
		gettext_noop ("Place at center of geometry"),
		gettext_noop ("Place on first vertex"),
		gettext_noop ("Place on last vertex"),
		gettext_noop ("Do not label"),
		""
};

/* orientation mode names (not case sensitive) */
static const char OPTIONS_ORIENT_MODE_NAMES[][10] = {
		"world",
		"localxz",
		""
};

/* orientation mode descriptions */
static const char OPTIONS_ORIENT_MODE_DESC[][30] = {
		gettext_noop ("World: original X/Y/(Z)"),
		gettext_noop ("Local: X-Z cross section"),
		""
};



#ifdef MAIN
/* set to TRUE if running in GUI MODE */
BOOLEAN OPTIONS_GUI_MODE;
/* set to TRUE to force English messages and number format */
BOOLEAN OPTIONS_FORCE_ENGLISH;
#else
extern BOOLEAN OPTIONS_GUI_MODE;
#endif

typedef struct options options;

/*
 * A simple structure that contains all command line
 * options in a form that makes them easy to use.
 */
struct options
{
	char *cmd_name; /* base name of command used to run this program */
	char *schema_file; /* path and file name for parser description file */
	char **input;	/* NULL terminated names of input file(s), NULL to read from stdin */
	char *selection[PRG_MAX_SELECTIONS]; /* list of filters to apply to field contents */
	int num_input; /* number if input files. "0" if reading from stdin */
	char *output;	/* directory name for output file(s) */
	char *base;	/* base name for output file(s) */
	char *label_field; /* name of field to use for labels */
	int label_mode_point; /* mode for label placement (points) */
	int label_mode_line; /* mode for label placement (lines) */
	int label_mode_poly; /* mode for label placement (polygons) */
	int orient_mode; /* orientation mode (optional re-mapping of X/Y/Z axes) */
	char *log; /* path and file name for log file or NULL */
	double tolerance; /* cluster tolerance for coordinate values */
	char *tolerance_str; /* copy of the original (string) option value */
	double snapping; /* snapping distance between point pairs */
	char *snapping_str; /* copy of the original (string) option value */
	double dangling; /* snapping distance for dangling line nodes */
	char *dangling_str; /* copy of the original (string) option value */
	int decimal_places; /*  decimal precision with which to store doubles in DBFs */
	char *decimal_places_str; /* copy of the original (string) option value */
	double offset_x; /* offsets for abbreviated coordinate values */
	char *offset_x_str; /* copy of the original (string) option value */
	double offset_y;
	char *offset_y_str; /* copy of the original (string) option value */
	double offset_z;
	char *offset_z_str; /* copy of the original (string) option value */
	char decimal_point[2]; /* user-specified char or empty */
	char decimal_group[2]; /* user-specified char or empty */
	unsigned short format; /* output file format (see OPTIONS_FORMAT_* above) */
	BOOLEAN force_2d; /* always export data as 2D (default: FALSE) */
	BOOLEAN strict; /* use strict input parsing (default: FALSE) */
	BOOLEAN force_english; /* force English messages and numeric notation (default: FALSE)? */
	BOOLEAN just_dump_help;
	BOOLEAN just_dump_parser;
	BOOLEAN dump_raw; /* write raw vertices to separate output file */
	BOOLEAN show_gui; /* always pop up the GUI */
	char *proj_in; /* input CRS string, as provided by user */
	char *proj_out; /* output CRS string, as provided by user */
	projPJ proj4_in; /* input CRS in PROJ.4 format (or NULL) */
	projPJ proj4_out; /* output CRS in PROJ.4 format (or NULL) */
	double wgs84_trans_dx; /* datum transformation from WGS84 (X shift) */
	char *wgs84_trans_dx_str; /* copy of the original (string) option value */
	double wgs84_trans_dy; /* datum transformation from WGS84 (Y shift) */
	char *wgs84_trans_dy_str; /* copy of the original (string) option value */
	double wgs84_trans_dz; /* datum transformation from WGS84 (Z shift) */
	char *wgs84_trans_dz_str; /* copy of the original (string) option value */
	double wgs84_trans_rx; /* datum transformation from WGS84 (X rotation) */
	char *wgs84_trans_rx_str; /* copy of the original (string) option value */
	double wgs84_trans_ry; /* datum transformation from WGS84 (Y rotation) */
	char *wgs84_trans_ry_str; /* copy of the original (string) option value */
	double wgs84_trans_rz; /* datum transformation from WGS84 (Z rotation) */
	char *wgs84_trans_rz_str; /* copy of the original (string) option value */
	double wgs84_trans_ds; /* datum transformation from WGS84 (scaling) */
	char *wgs84_trans_ds_str; /* copy of the original (string) option value */
	char *wgs84_trans_grid; /* datum transformation from WGS84 (grid file name) */
	/* internally used, not visible from CLI */
	void *window; /* this is only used in GUI mode, to display error dialogs */
	int argc; /* value of original CLI options counter */
	char **argv; /* reference to original CLI options array */
	BOOLEAN empty;
};

/* print usage instructions, then exit */
void options_help ( void );

/* create an empty options storage object */
options *options_create ( int argc, char *argv[] );

/* parse command line options and store all settings */
int options_parse ( options *opts );

/* destroy an options object */
void options_destroy ( options *opts );

#endif /* OPTIONS_H */

