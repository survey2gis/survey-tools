/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	options.c
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


#include <getopt.h>
#include <stdlib.h>

#include "global.h"
#include "errors.h"
#include "reproj.h"
#include "selections.h"
#include "i18n.h"
#include "options.h"
#include "tools.h"


/* int values for CLI arguments that have only long names */
#define ARG_ID_LABEL_MODE_POINT	1000
#define ARG_ID_LABEL_MODE_LINE	1001
#define ARG_ID_LABEL_MODE_POLY 	1002
#define ARG_ID_PROJ_IN			2000
#define ARG_ID_PROJ_OUT			2001
#define ARG_ID_WGS84_TRANS_DX	2002
#define ARG_ID_WGS84_TRANS_DY	2003
#define ARG_ID_WGS84_TRANS_DZ	2004
#define ARG_ID_WGS84_TRANS_RX	2005
#define ARG_ID_WGS84_TRANS_RY	2006
#define ARG_ID_WGS84_TRANS_RZ	2007
#define ARG_ID_WGS84_TRANS_DS	2008
#define ARG_ID_WGS84_TRANS_GRID	2009

/*
 * Print usage instructions, then exit.
 * (-h, --help)
 */
void options_help ( void )
{
	int i;


	/* TODO: in GUI mode, this should be displayed in a text window (?) */

	fprintf (stdout, _("Usage: %s -p FILE -o DIR -n NAME [OPTION]... [FILE]... \n"), t_get_cmd_name ());
	fprintf (stdout, _("Read geometry and attribute descriptions from a survey protocol file\n\
in ASCII format and convert them to common GIS and CAD output formats.\n"));

	fprintf (stdout, _("\nPossible OPTIONs are:\n"));
	fprintf (stdout, _("  -p, --parser=\t\tname of file with parsing schema (required)\n"));
	fprintf (stdout, _("  -o, --output=\t\tdirectory name for output file(s) (required)\n"));
	fprintf (stdout, _("  -n, --name=\t\tbase name for output file(s) (required)\n"));
	fprintf (stdout, _("  -f, --format=\t\toutput format (see list below; default: \"%s\")\n"),
			PRG_OUTPUT_EXT[PRG_OUTPUT_DEFAULT]);
	i = 0;
	while ( strcmp ( PRG_OUTPUT_EXT[i],"" ) != 0 ) {
		fprintf (stdout, _("  \t\t\t\"%s\" (%s)\n"), PRG_OUTPUT_EXT[i], PRG_OUTPUT_DESC[i] );
		i ++;
	}
	fprintf (stdout, _("  -L, --label=\t\tchoose field for labels (see manual for details)\n"));
	fprintf (stdout, _("      --label-mode-point=label mode for points (default: \"%s\")\n"),
			OPTIONS_LABEL_MODE_NAMES[OPTIONS_DEFAULT_LABEL_MODE_POINT]);
	fprintf (stdout, _("      --label-mode-line=label mode for lines (default: \"%s\")\n"),
			OPTIONS_LABEL_MODE_NAMES[OPTIONS_DEFAULT_LABEL_MODE_LINE]);
	fprintf (stdout, _("      --label-mode-poly=label mode for polygons (default: \"%s\")\n"),
			OPTIONS_LABEL_MODE_NAMES[OPTIONS_DEFAULT_LABEL_MODE_POLY]);
	i = 0;
	while ( strcmp ( OPTIONS_LABEL_MODE_NAMES[i],"" ) != 0 ) {
		fprintf (stdout, _("  \t\t\t\"%s\" (%s)\n"), OPTIONS_LABEL_MODE_NAMES[i], gettext(OPTIONS_LABEL_MODE_DESC[i]) );
		i ++;
	}
	fprintf (stdout, _("  -O, --orientation=\tchoose output orientation (default: \"%s\")\n"), OPTIONS_ORIENT_MODE_NAMES[OPTIONS_ORIENT_MODE_WORLD_XYZ]);
	i = 0;
	while ( strcmp ( OPTIONS_ORIENT_MODE_NAMES[i],"" ) != 0 ) {
		fprintf (stdout, _("  \t\t\t\"%s\" (%s)\n"), OPTIONS_ORIENT_MODE_NAMES[i], gettext(OPTIONS_ORIENT_MODE_DESC[i]) );
		i ++;
	}	
	fprintf (stdout, _("  -T, --topology=\ttopological processing level (default: \"%s\")\n"), OPTIONS_TOPO_LEVEL_NAMES[OPTIONS_TOPO_LEVEL_FULL]);
	i = 0;
	while ( strcmp ( OPTIONS_TOPO_LEVEL_NAMES[i],"" ) != 0 ) {
		fprintf (stdout, _("  \t\t\t\"%s\" (%s)\n"), OPTIONS_TOPO_LEVEL_NAMES[i], gettext(OPTIONS_TOPO_LEVEL_DESC[i]) );
		i ++;
	}	
	fprintf (stdout, _("  -S, --selection=\tselect data by field content (see manual for details)\n"));
	fprintf (stdout, _("  -l, --log=\t\toutput name for error log file (default: none)\n"));
	fprintf (stdout, _("  -t, --tolerance=\tdistance threshold for coordinates (default: %.1f)\n"), OPTIONS_DEFAULT_TOLERANCE);
	fprintf (stdout, _("  -s, --snapping=\tsnapping dist. for boundary nodes (default: %.1f = off)\n"), OPTIONS_DEFAULT_SNAPPING);
	fprintf (stdout, _("  -D, --dangling=\tsnapping dist. for line dangles (default: %.1f = off)\n"), OPTIONS_DEFAULT_DANGLING);
	fprintf (stdout, _("  -x, --x-offset=\tconstant offset to add to x coordinates (default: %.1f)\n"), OPTIONS_DEFAULT_OFFSET_X);
	fprintf (stdout, _("  -y, --y-offset=\tconstant offset to add to y coordinates (default: %.1f)\n"), OPTIONS_DEFAULT_OFFSET_Y);
	fprintf (stdout, _("  -z, --z-offset=\tconstant offset to add to z coordinates (default: %.1f)\n"), OPTIONS_DEFAULT_OFFSET_Z);
	fprintf (stdout, _("  -d, --decimal-places=\tdecimal places for numeric DBF attributes (default: %i)\n"), OPTIONS_DEFAULT_DECIMAL_PLACES);
	fprintf (stdout, _("  -i, --decimal-point=\tdecimal point character in input data (default: auto)\n"));
	fprintf (stdout, _("  -g, --decimal-group=\tnumeric group character in input data (default: auto)\n"));
	fprintf (stdout, _("  -r, --raw-data\tsave raw vertex data as additional points output\n"));
	fprintf (stdout, _("  -2, --force-2d\tforce 2D output, even if input data is 3D\n"));
	fprintf (stdout, _("  -c, --strict\t\tuse stricter input validation\n"));
	fprintf (stdout, _("  -v, --validate-parser\tvalidate parser schema and exit\n"));
	fprintf (stdout, "  -e, --english\t\tforce English messages and numeric notation\n");
#ifdef GUI
	fprintf (stdout, _("  -u, --show-gui\talways show GUI form\n"));
#endif
	fprintf (stdout, _("  -h, --help\t\tdisplay this information and exit\n"));
	fprintf (stdout, _("\nREPROJECTION options (PROJ.4):\n"));
	fprintf (stdout, _("  --proj-in=\t\tcoordinate reference system of input data\n"));
	fprintf (stdout, _("  --proj-out=\t\tcoordinate reference system of output data\n"));
	fprintf (stdout, _("  --proj-dx=\t\tdatum transform to WGS 84 (X shift; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_DX);
	fprintf (stdout, _("  --proj-dy=\t\tdatum transform to WGS 84 (Y shift; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_DY);
	fprintf (stdout, _("  --proj-dz=\t\tdatum transform to WGS 84 (Z shift; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_DZ);
	fprintf (stdout, _("  --proj-rx=\t\tdatum transform to WGS 84 (X rotation; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_RX);
	fprintf (stdout, _("  --proj-ry=\t\tdatum transform to WGS 84 (Y rotation; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_RY);
	fprintf (stdout, _("  --proj-rz=\t\tdatum transform to WGS 84 (Z rotation; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_RZ);
	fprintf (stdout, _("  --proj-ds=\t\tdatum transform to WGS 84 (scaling; default: %.1f)\n"), OPTIONS_DEFAULT_WGS84_TRANS_DS);
	fprintf (stdout, _("  --proj-grid=\t\tlocal datum transformation grid (file name)\n"));
	fprintf (stdout, _("\nGeometries and attributes will be read from one or more input files.\n"));
	fprintf (stdout, _("\nWith no input file(s), data will be read from the \"stdin\" stream.\n"));
	fprintf (stdout, _("The combined output will be written into at least one output file.\n"));
	fprintf (stdout, _("Depending on the output file format and the geometries in the input file(s),\n\
more than one output file with a common base name may be produced.\n"));
	fprintf (stdout, _("Duplicate measurements will not be stored in the output file(s).\n"));
	fprintf (stdout, _("The \"--tolerance=\" setting determines the threshold of distance above\n\
which two coordinates are considered to be distinct.\n"));
	fprintf (stdout, _("\nThis program is free software under the GNU General Public License (>=v2).\n\
Read http://www.gnu.org/licenses/gpl.html for details."));
	fprintf (stdout, _("\nVersion %s\n"), t_get_prg_version());
	fprintf (stdout, _("Includes PROJ.4 %s\n"), pj_get_release());
}


/*
 * Create a new and empty options object.
 */
options *options_create (int argc, char *argv[])
{
	options *newOpts;
	int i;
	int len = sizeof (char) * PRG_MAX_STR_LEN;

	newOpts = malloc ( sizeof (options));
	newOpts->cmd_name = NULL;
	newOpts->schema_file = NULL;
	newOpts->input = NULL;
	newOpts->num_input = 0;
	newOpts->format = 0;
	for ( i=0; i < PRG_MAX_SELECTIONS; i ++ ) {
		newOpts->selection[i] = NULL;
	}
	newOpts->output = NULL;
	newOpts->base = NULL;
	newOpts->label_field = NULL;
	newOpts->label_mode_point = OPTIONS_DEFAULT_LABEL_MODE_POINT;
	newOpts->label_mode_line = OPTIONS_DEFAULT_LABEL_MODE_LINE;
	newOpts->label_mode_poly = OPTIONS_DEFAULT_LABEL_MODE_POLY;
	newOpts->orient_mode = OPTIONS_DEFAULT_ORIENT_MODE;
	newOpts->topo_level = OPTIONS_TOPO_LEVEL_FULL;
	newOpts->log = NULL;
	newOpts->tolerance = OPTIONS_DEFAULT_TOLERANCE;
	newOpts->tolerance_str = malloc ( len );
	t_dbl_to_str (newOpts->tolerance, newOpts->tolerance_str);
	newOpts->snapping = OPTIONS_DEFAULT_SNAPPING;
	newOpts->snapping_str = malloc (len );
	t_dbl_to_str (newOpts->snapping, newOpts->snapping_str);
	newOpts->dangling = OPTIONS_DEFAULT_DANGLING;
	newOpts->dangling_str = malloc (len );
	t_dbl_to_str (newOpts->dangling, newOpts->dangling_str);
	newOpts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
	newOpts->decimal_places_str = malloc ( len );
	snprintf ( newOpts->decimal_places_str, len, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
	newOpts->offset_x = OPTIONS_DEFAULT_OFFSET_X;
	newOpts->offset_x_str = malloc ( len );
	t_dbl_to_str (newOpts->offset_x, newOpts->offset_x_str);
	newOpts->offset_y = OPTIONS_DEFAULT_OFFSET_Y;
	newOpts->offset_y_str = malloc ( sizeof (char) * (PRG_MAX_STR_LEN+1) );
	t_dbl_to_str (newOpts->offset_y, newOpts->offset_y_str);
	newOpts->offset_z = OPTIONS_DEFAULT_OFFSET_Z;
	newOpts->offset_z_str = malloc ( sizeof (char) * (PRG_MAX_STR_LEN+1) );
	t_dbl_to_str (newOpts->offset_z, newOpts->offset_z_str);
	newOpts->decimal_point[0] = '\0';
	newOpts->decimal_point[1] = '\0';
	newOpts->decimal_group[0] = '\0';
	newOpts->decimal_group[1] = '\0';
	newOpts->proj_in = NULL;
	newOpts->proj_out = NULL;
	newOpts->proj4_data_dir = NULL;
	newOpts->proj4_in = NULL;
	newOpts->proj4_out = NULL;
	newOpts->wgs84_trans_dx = OPTIONS_DEFAULT_WGS84_TRANS_DX;
	newOpts->wgs84_trans_dx_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_dx, newOpts->wgs84_trans_dx_str);
	newOpts->wgs84_trans_dy = OPTIONS_DEFAULT_WGS84_TRANS_DY;
	newOpts->wgs84_trans_dy_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_dy, newOpts->wgs84_trans_dy_str);
	newOpts->wgs84_trans_dz = OPTIONS_DEFAULT_WGS84_TRANS_DZ;
	newOpts->wgs84_trans_dz_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_dz, newOpts->wgs84_trans_dz_str);
	newOpts->wgs84_trans_rx = OPTIONS_DEFAULT_WGS84_TRANS_RX;
	newOpts->wgs84_trans_rx_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_rx, newOpts->wgs84_trans_rx_str);
	newOpts->wgs84_trans_ry = OPTIONS_DEFAULT_WGS84_TRANS_RY;
	newOpts->wgs84_trans_ry_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_ry, newOpts->wgs84_trans_ry_str);
	newOpts->wgs84_trans_rz = OPTIONS_DEFAULT_WGS84_TRANS_RZ;
	newOpts->wgs84_trans_rz_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_rz, newOpts->wgs84_trans_rz_str);
	newOpts->wgs84_trans_ds = OPTIONS_DEFAULT_WGS84_TRANS_DS;
	newOpts->wgs84_trans_ds_str = malloc (len );
	t_dbl_to_str (newOpts->wgs84_trans_ds, newOpts->wgs84_trans_ds_str);
	newOpts->wgs84_trans_grid = NULL;
	newOpts->force_2d = FALSE;
	newOpts->strict = FALSE;
	newOpts->force_english = FALSE;
	newOpts->show_gui = FALSE;
	newOpts->just_dump_help = FALSE;
	newOpts->just_dump_parser = FALSE;
	newOpts->dump_raw = FALSE;
	newOpts->window = NULL;
	newOpts->empty = TRUE;

	/* pass reference to original CLI options */
	newOpts->argc = argc;
	newOpts->argv = argv;

	return ( newOpts );
}


/*
 * Destroy an options object, release all memory.
 */
void options_destroy ( options *opts )
{
	if ( opts != NULL ) {
		if ( opts->cmd_name != NULL )
			free ( opts->cmd_name );
		if ( opts->output != NULL )
			free ( opts->output );
		if ( opts->base != NULL )
			free ( opts->base );
		if ( opts->label_field != NULL )
			free ( opts->label_field );
		if ( opts->schema_file != NULL )
			free ( opts->schema_file );
		int i = 0;
		for ( i=0; i < PRG_MAX_SELECTIONS; i++ ) {
			if ( opts->selection[i] != NULL ) {
				free  ( opts->selection[i] );
			}
		}
		if ( opts->log != NULL )
			free ( opts->log );
		if ( opts->input != NULL ) {
			int i = 0;
			while ( opts->input[i] != NULL ) {
				free ( opts->input[i] );
				i++;
			}
			free ( opts->input );
		}
		free ( opts->tolerance_str );
		free ( opts->snapping_str );
		free ( opts->dangling_str );
		free ( opts->decimal_places_str );
		free ( opts->offset_x_str );
		free ( opts->offset_y_str );
		free ( opts->offset_z_str );
		if ( opts->proj_in != NULL ) {
			free (opts->proj_in);
		}
		if ( opts->proj_out != NULL ) {
			free (opts->proj_out);
		}
		if ( opts->proj4_data_dir != NULL ) {
			free (opts->proj4_data_dir);
		}
		if ( opts->proj4_in != NULL ) {
			pj_free (opts->proj4_in);
		}
		if ( opts->proj4_out != NULL ) {
			pj_free (opts->proj4_out);
		}
		if ( opts->wgs84_trans_grid != NULL ) {
			free (opts->wgs84_trans_grid);
		}
		free ( opts->wgs84_trans_dx_str );
		opts->wgs84_trans_dx_str = NULL;
		free ( opts->wgs84_trans_dy_str );
		opts->wgs84_trans_dy_str = NULL;
		free ( opts->wgs84_trans_dz_str );
		opts->wgs84_trans_dz_str = NULL;
		free ( opts->wgs84_trans_rx_str );
		opts->wgs84_trans_rx_str = NULL;
		free ( opts->wgs84_trans_ry_str );
		opts->wgs84_trans_ry_str = NULL;
		free ( opts->wgs84_trans_rz_str );
		opts->wgs84_trans_rz_str = NULL;
		free ( opts->wgs84_trans_ds_str );
		opts->wgs84_trans_ds_str = NULL;
		/* remove references to CLI options */
		opts->argc = 0;
		opts->argv = NULL;

		/* free entire struct */
		free (opts);
		opts = NULL;
	}
}


/*
 * Cross-plattform option parsing helper function:
 * 
 * Copies an option argument value to a new string.
 * This function takes care of translating the argument value
 * to the internal UTF-8 encoding on Windows.
 * If UTF-8 translation fails, then the raw, untranslated string
 * is copied. On systems other than Windows, no translation takes
 * place and the unmodified argument value is returned.
 * 
 * Returns a newly allocated string which must be free'd by the
 * caller when no longer needed. May return NULL if the argument
 * value is NULL or there is an error other than an invalid
 * byte sequence.
 * 
 */
char* options_get_optarg ( const char* value ) {
	if ( value == NULL) {
		return ( NULL );
	}
#ifdef MINGW
	char *convbuf = NULL;
	int result = t_str_enc(I18N_WIN_CODEPAGE_CONSOLE, "UTF-8", (char*)value, &convbuf);
	if ( convbuf == NULL || result < 0 ) {
		if ( errno == EILSEQ ) {
			/* Got an illegal byte sequence: maybe the
			 * string is already in UTF-8 encoding. If so:
			 * Return the original string unmodified.
			 */ 
			return ( strdup (value) );
		} else {
			return ( NULL );
		}		
	} else {
		return ( convbuf );
	}
#else
	return ( strdup (value) );
#endif
}


/*
 * Parse command line options and store all settings.
 * The exit behaviour of this function is different in GUI
 * and CLI modes.
 *
 * In CLI mode:
 *   Exits program execution with a fatal error if there is any problem
 *   with the specified options.
 *
 * In GUI mode:
 *   Returns "0" if all options were parsed successfully, number of
 *   errors otherwise.
 */
int options_parse ( options *opts )
{
	int option_index = 0;
	int num_errors = 0;
	static struct option long_options[] = {
			{ "parser", required_argument, NULL, 'p' },
			{ "output", required_argument, NULL, 'o' },
			{ "name", required_argument, NULL, 'n' },
			{ "format", required_argument, NULL, 'f' },
			{ "label", required_argument, NULL, 'L' },
			{ "label-mode-point", required_argument, NULL, ARG_ID_LABEL_MODE_POINT },
			{ "label-mode-line", required_argument, NULL, ARG_ID_LABEL_MODE_LINE },
			{ "label-mode-poly", required_argument, NULL, ARG_ID_LABEL_MODE_POLY },
			{ "orientation", required_argument, NULL, 'O' },
			{ "topology", required_argument, NULL, 'T' },
			{ "selection", required_argument, NULL, 'S' },
			{ "log", required_argument, NULL, 'l' },
			{ "tolerance", required_argument, NULL, 't' },
			{ "snapping", required_argument, NULL, 's' },
			{ "dangling", required_argument, NULL, 'D' },
			{ "x-offset", required_argument, NULL, 'x' },
			{ "y-offset", required_argument, NULL, 'y' },
			{ "z-offset", required_argument, NULL, 'z' },
			{ "decimal-places", required_argument, NULL, 'd' },
			{ "decimal-point", required_argument, NULL, 'i' },
			{ "decimal-group", required_argument, NULL, 'g' },
			{ "force-2d", no_argument, NULL, '2' },
			{ "raw-data", no_argument, NULL, 'r' },
			{ "strict", no_argument, NULL, 'c' },
			{ "validate-parser", no_argument, NULL, 'v' },
			{ "english", no_argument, NULL, 'e' },
			{ "proj-in", required_argument, NULL, ARG_ID_PROJ_IN },
			{ "proj-out", required_argument, NULL, ARG_ID_PROJ_OUT },
			{ "proj-dx", required_argument, NULL, ARG_ID_WGS84_TRANS_DX },
			{ "proj-dy", required_argument, NULL, ARG_ID_WGS84_TRANS_DY },
			{ "proj-dz", required_argument, NULL, ARG_ID_WGS84_TRANS_DZ },
			{ "proj-rx", required_argument, NULL, ARG_ID_WGS84_TRANS_RX },
			{ "proj-ry", required_argument, NULL, ARG_ID_WGS84_TRANS_RY },
			{ "proj-rz", required_argument, NULL, ARG_ID_WGS84_TRANS_RZ },
			{ "proj-ds", required_argument, NULL, ARG_ID_WGS84_TRANS_DS },
			{ "proj-grid", required_argument, NULL, ARG_ID_WGS84_TRANS_GRID },
#ifdef GUI
			{ "show-gui", 0, NULL, 'u' },
#endif
			{ "help", 0, NULL, 'h' },
			{ 0, 0, 0, 0 }
	};

	int num_valid_opts = 0;
	BOOLEAN error;
	BOOLEAN p_given = FALSE;
	BOOLEAN o_given = FALSE;
	BOOLEAN n_given = FALSE;
	unsigned int num_files;
	char *v_format=NULL;
	char *v_label_field=NULL;
	char *v_label_mode_point=NULL;
	char *v_label_mode_line=NULL;
	char *v_label_mode_poly=NULL;
	char *v_orient_mode=NULL;
	char *v_topo_level=NULL;
	char *v_selection=NULL;
	char *v_tolerance=NULL;
	char *v_snapping=NULL;
	char *v_dangling=NULL;
	char *v_decimal_places=NULL;
	char *v_offset_x=NULL;
	char *v_offset_y=NULL;
	char *v_offset_z=NULL;
	char *v_decimal=NULL;
	char *v_group=NULL;
	char *v_proj_in=NULL;
	char *v_proj_out=NULL;
	char *v_proj_dx=NULL;
	char *v_proj_dy=NULL;
	char *v_proj_dz=NULL;
	char *v_proj_rx=NULL;
	char *v_proj_ry=NULL;
	char *v_proj_rz=NULL;
	char *v_proj_ds=NULL;
	char *v_proj_grid=NULL;
	char *p;


	/* get basename of command */
	p = rindex ( opts->argv[0], PRG_FILE_SEPARATOR );
	if ( p == NULL ) {
		opts->cmd_name=strdup(opts->argv[0]);
	} else {
		p++;
		opts->cmd_name=strdup(p);
	}

	/* first we check for --help */
	int i;
	for ( i = 0; i < opts->argc; i++  ) {
		if (!strcmp ( "--help", opts->argv[i] ) ) {
			/* just show help and exit */
			opts->just_dump_help = TRUE;
			num_valid_opts ++;
		}
	}

#ifdef GUI
	/* now we check for --show-gui */
	for ( i = 0; i < opts->argc; i++  ) {
		if (!strcmp ( "--show-gui", opts->argv[i] ) ) {
			opts->show_gui = TRUE;
			num_valid_opts ++;
			if ( opts->argc == 2 ) {
				/* only option? just return! */
				return (0);
			}
		}
	}
#endif

	/* process all other options in the regular way */
	if ( opts->just_dump_help == FALSE ) {

		static const char *optString = "p:o:n:f:L:O:T:S:l:t:s:D:y:z:d:i:g:2rcvehu";
		int option = getopt_long ( opts->argc, opts->argv, optString, long_options, &option_index );

		while ( option  != -1 ) {

			if ( option == '?' ) {
				err_show ( ERR_EXIT, _("'%s' is not a valid option."), opts->argv[optind-1] );
			}

			if ( option == 'v' ) {
				/* just show parser definition and exit */
				opts->just_dump_parser = TRUE;
				num_valid_opts ++;
			}

			/* check that all options are complete */
			if ( option == ':' ) {
				if (optopt == 'p') {
					opts->schema_file = NULL;
					err_show ( ERR_EXIT, _("No parser file specified (option '-p')."));
				}
				if (optopt == 'o') {
					opts->output = NULL;
					err_show ( ERR_EXIT, _("No output directory name specified (option '-o')."));
				}
				if (optopt == 'n') {
					opts->base = NULL;
					err_show ( ERR_EXIT, _("No output base name specified (option '-n')."));
				}
				if (optopt == 'f') {
					v_format = NULL;
					err_show ( ERR_EXIT, _("No output format specified (option '-f')."));
				}
				if (optopt == 'L') {
					v_label_field = NULL;
					err_show ( ERR_EXIT, _("No label field specified (option '-L')."));
				}
				if (optopt == ARG_ID_LABEL_MODE_POINT) {
					v_label_mode_point = NULL;
					err_show ( ERR_EXIT, _("No label mode for points specified (option '--label-mode-point')."));
				}
				if (optopt == ARG_ID_LABEL_MODE_LINE) {
					v_label_mode_line = NULL;
					err_show ( ERR_EXIT, _("No label mode for lines specified (option '--label-mode-line')."));
				}
				if (optopt == ARG_ID_LABEL_MODE_POLY) {
					v_label_mode_poly = NULL;
					err_show ( ERR_EXIT, _("No label mode for polygons specified (option '--label-mode-poly')."));
				}
				if (optopt == 'O') {
					v_orient_mode = NULL;
					err_show ( ERR_EXIT, _("No orientation mode specified (option '-O')."));
				}
				if (optopt == 'T') {
					v_topo_level = NULL;
					err_show ( ERR_EXIT, _("No topology level specified (option '-T')."));
				}
				if (optopt == 'S') {
					v_selection = NULL;
					err_show ( ERR_EXIT, _("No selection command specified (option '-S')."));
				}
				if (optopt == 'l') {
					opts->log = NULL;
					err_show ( ERR_EXIT, _("No log file name specified (option '-l')."));
				}
				if (optopt == 't') {
					v_tolerance = NULL;
					err_show ( ERR_EXIT, _("No tolerance setting specified (option '-t')."));
				}
				if (optopt == 's') {
					v_snapping = NULL;
					err_show ( ERR_EXIT, _("No snapping setting specified (option '-s')."));
				}
				if (optopt == 'D') {
					v_dangling = NULL;
					err_show ( ERR_EXIT, _("No dangling setting specified (option '-D')."));
				}
				if (optopt == 'x') {
					v_offset_x = NULL;
					err_show ( ERR_EXIT, _("No X coordinate offset specified (option '-x')."));
				}
				if (optopt == 'y') {
					v_offset_y = NULL;
					err_show ( ERR_EXIT, _("No Y coordinate offset specified (option '-y')."));
				}
				if (optopt == 'z') {
					v_offset_z = NULL;
					err_show ( ERR_EXIT, _("No Z coordinate offset specified (option '-z')."));
				}
				if (optopt == 'd') {
					v_decimal_places = NULL;
					err_show ( ERR_EXIT, _("Number of decimal places not given (option '-d')."));
				}
				if (optopt == 'i') {
					v_decimal = NULL;
					err_show ( ERR_EXIT, _("No decimal point symbol specified (option '-i')."));
				}
				if (optopt == 'g') {
					v_group = NULL;
					err_show ( ERR_EXIT, _("No decimal grouping symbol specified (option '-g')."));
				}
				if (optopt == ARG_ID_PROJ_IN) {
					v_proj_in = NULL;
					err_show ( ERR_EXIT, _("No input coordinate reference system given (option '--proj-in')."));
				}
				if (optopt == ARG_ID_PROJ_IN) {
					v_proj_out = NULL;
					err_show ( ERR_EXIT, _("No output coordinate reference system given (option '--proj-out')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_DX) {
					v_proj_dx = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum shift X given (option '--proj-dx')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_DY) {
					v_proj_dy = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum shift Y given (option '--proj-dy')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_DZ) {
					v_proj_dz = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum shift Z given (option '--proj-dz')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_RX) {
					v_proj_rx = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum rotation X given (option '--proj-rx')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_RY) {
					v_proj_ry = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum rotation Y given (option '--proj-ry')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_RZ) {
					v_proj_rz = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum rotation Z given (option '--proj-rz')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_DS) {
					v_proj_ds = NULL;
					err_show ( ERR_EXIT, _("No output WGS 84 datum scaling given (option '--proj-ds')."));
				}
				if (optopt == ARG_ID_WGS84_TRANS_GRID) {
					v_proj_grid = NULL;
					err_show ( ERR_EXIT, _("No datum grid file given (option '--proj-grid')."));
				}
				num_errors ++;
			}

			/* schema file name */
			if ( option == 'p' ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					opts->schema_file = options_get_optarg (optarg);
					num_valid_opts ++;
					p_given = TRUE;
				} else {
					err_show ( ERR_EXIT, _("No parser file specified (option '-p')."));
					num_errors ++;
				}
			}

			/* output directory */
			if ( option == 'o' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					opts->output = options_get_optarg (optarg);
					num_valid_opts ++;
					o_given = TRUE;
				} else {
					err_show ( ERR_EXIT, _("No output directory name specified (option '-o')."));
					num_errors ++;
				}
			}

			/* base name for output */
			if ( option == 'n' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					opts->base = options_get_optarg (optarg);
					num_valid_opts ++;
					n_given = TRUE;
				} else {
					err_show ( ERR_EXIT, _("No output base name specified (option '-n')."));
					num_errors ++;
				}
			}

			/* output file format */
			if ( option == 'f' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_format = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-f/--format=");
					num_errors ++;
				}
			}

			/* label field */
			if ( option == 'L' ) {
				if ( optarg != NULL ) {
					v_label_field = t_str_pack (options_get_optarg (optarg));
					if ( strlen (v_label_field) > 0 ) {
						num_valid_opts ++;
					} else {
						err_show ( ERR_EXIT, _("Label field name is empty."));
						num_errors ++;
					}
				}
			}

			/* Label modes for points, lines and polygons */
			if ( option == ARG_ID_LABEL_MODE_POINT ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_label_mode_point = t_str_pack (t_str_to_lower(options_get_optarg (optarg)));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--label-mode-point=");
					num_errors ++;
				}
			}
			if ( option == ARG_ID_LABEL_MODE_LINE ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_label_mode_line = t_str_pack (t_str_to_lower(options_get_optarg (optarg)));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--label-mode-line=");
					num_errors ++;
				}
			}
			if ( option == ARG_ID_LABEL_MODE_POLY ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_label_mode_poly = t_str_pack (t_str_to_lower(options_get_optarg (optarg)));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--label-mode-poly=");
					num_errors ++;
				}
			}

			/* orientation mode */
			if ( option == 'O' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_orient_mode = t_str_pack (t_str_to_lower(options_get_optarg (optarg)));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-O/--orientation=");
					num_errors ++;
				}
			}
			
			/* topology level */
			if ( option == 'T' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_topo_level = t_str_pack (t_str_to_lower(options_get_optarg (optarg)));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-T/--topology=");
					num_errors ++;
				}
			}

			/* selection expression */
			if ( option == 'S' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_selection = options_get_optarg (optarg);
					if ( selection_add ( v_selection, opts ) == FALSE ) {
						err_show ( ERR_EXIT, _("Cannot add another selection (limit: %i)."), PRG_MAX_SELECTIONS );
						num_errors ++;
					}
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-S/--selection=");
					num_errors ++;
				}
			}

			/* log file name */
			if ( option == 'l' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					opts->log = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-l/--log=");
					num_errors ++;
				}
			}

			/* tolerance for distinct coordinates */
			if ( option == 't' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_tolerance = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-t/--tolerance=");
					num_errors ++;
				}
			}

			/* snapping distance (polygon boundary vertices) */
			if ( option == 's' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_snapping = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-s/--snapping=");
					num_errors ++;
				}

			}

			/* snapping distance (line nodes/dangles) */
			if ( option == 'D' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_dangling = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-D/--dangling=");
					num_errors ++;
				}
			}

			if ( option == 'x' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_offset_x = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-x/--x-offset=");
					num_errors ++;
				}
			}

			if ( option == 'y' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_offset_y = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-y/--y-offset=");
					num_errors ++;
				}
			}

			if ( option == 'z' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_offset_z = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-z/--z-offset=");
					num_errors ++;
				}
			}

			if ( option == 'd' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_decimal_places = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-d/--decimal-places=");
					num_errors ++;
				}
			}

			if ( option == 'i' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_decimal = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-i/--decimal-point=");
					num_errors ++;
				}
			}

			if ( option == 'g' ) {
				if ( optarg != NULL && strlen ( optarg ) > 0 ) {
					v_group = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "-g/--decimal-group=");
					num_errors ++;
				}
			}

			if ( option == 'r' ) {
				opts->dump_raw = TRUE;
				num_valid_opts ++;
			}


			if ( option == '2' ) {
				opts->force_2d = TRUE;
				num_valid_opts ++;
			}

			if ( option == 'c' ) {
				opts->strict = TRUE;
				num_valid_opts ++;
			}

			if ( option == 'e' ) {
				opts->force_english = TRUE;
				i18n_force_english ();
				num_valid_opts ++;
			}

			/* input CRS */
			if ( option == ARG_ID_PROJ_IN ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_in = t_str_pack (options_get_optarg (optarg));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-in=");
					num_errors ++;
				}
			}

			/* output CRS */
			if ( option == ARG_ID_PROJ_OUT ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_out = t_str_pack (options_get_optarg (optarg));
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-out=");
					num_errors ++;
				}
			}

			/* WGS84 datum shift X */
			if ( option == ARG_ID_WGS84_TRANS_DX ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_dx = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-dx=");
					num_errors ++;
				}
			}

			/* WGS84 datum shift Y */
			if ( option == ARG_ID_WGS84_TRANS_DY ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_dy = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-dy=");
					num_errors ++;
				}
			}

			/* WGS84 datum shift Z */
			if ( option == ARG_ID_WGS84_TRANS_DZ ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_dz = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-dz=");
					num_errors ++;
				}
			}

			/* WGS84 datum rotation X */
			if ( option == ARG_ID_WGS84_TRANS_RX ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_rx = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-rx=");
					num_errors ++;
				}
			}

			/* WGS84 datum rotation Y */
			if ( option == ARG_ID_WGS84_TRANS_RY ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_ry = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-ry=");
					num_errors ++;
				}
			}

			/* WGS84 datum rotation Z */
			if ( option == ARG_ID_WGS84_TRANS_RZ ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_rz = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-rz=");
					num_errors ++;
				}
			}

			/* WGS84 datum scaling */
			if ( option == ARG_ID_WGS84_TRANS_DS ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_ds = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-ds=");
					num_errors ++;
				}
			}

			/* WGS84 datum transform grid */
			if ( option == ARG_ID_WGS84_TRANS_GRID ) {
				if ( optarg != NULL && strlen (optarg) > 0 ) {
					v_proj_grid = options_get_optarg (optarg);
					num_valid_opts ++;
				} else {
					err_show ( ERR_EXIT, _("Missing option value (option '%s')."), "--proj-grid=");
					num_errors ++;
				}
			}

			option = getopt_long ( opts->argc, opts->argv, optString, long_options, &option_index );

		}
	}


#ifndef GUI
	if ( num_valid_opts == 0 ) {
		err_show ( ERR_EXIT, _("Invalid command line given. Use \"%s -h\" for help."), t_get_cmd_name() );
	}
#else
	if ( num_valid_opts == 0 && opts->show_gui == FALSE ) {
		err_show ( ERR_EXIT, _("Invalid command line given. Use \"%s -h\" for help."), t_get_cmd_name() );
	}
#endif

	/* if -h, we can forget about further options processing */
	if ( opts->just_dump_help == TRUE ) {
		return (0);
	}

#ifndef GUI
	/* some basic sanity checking here */
	if ( p_given == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-p\" must be specified.") );
	}
#else
	if ( p_given == FALSE && opts->show_gui == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-p\" must be specified.") );
	}
#endif


	/* if -v, we can forget about further options processing */
	if ( opts->just_dump_parser == TRUE ) {
#ifdef GUI
		if ( opts->show_gui == FALSE ) {
			return (0);
		}
#else
		return 0;
#endif
	}

#ifndef GUI
	if ( o_given == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-o\" must be specified.") );
	}
#else
	if ( o_given == FALSE && opts->show_gui == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-o\" must be specified.") );
	}
#endif

#ifndef GUI
	if ( n_given == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-n\" must be specified.") );
	}
#else
	if ( n_given == FALSE && opts->show_gui == FALSE ) {
		err_show ( ERR_EXIT, _("Incomplete command line: option \"-n\" must be specified.") );
	}
#endif

	/* the following problems will generate an error message on the command line,
	   even if the GUI is supposed to pop up. */

	if ( opts->output != NULL && t_is_legal_path (opts->output) == FALSE ) {
		err_show ( ERR_EXIT, _("\"%s\" is not a valid directory (folder) name."), opts->output);
		num_errors ++;
		if ( opts->output != NULL ) {
			free ( opts->output );
		}
		opts->output = NULL;
	} else {
		if ( opts->output!= NULL ) {
			/* remove any trailing file separator char(s) from output folder path */
			int len = strlen(opts->output);
			len--;
			while ( len > 0 && opts->output[len] == PRG_FILE_SEPARATOR ) {
				opts->output[len] = '\0';
				len--;
			}
		}
	}

	if ( opts->base != NULL && t_is_legal_name (opts->base) == FALSE ) {
		err_show ( ERR_EXIT, _("\"%s\" is not a valid output file (base) name."), opts->base);
		num_errors ++;
		if ( opts->base != NULL )
			free ( opts->base );
		opts->base = NULL;
	}

	if ( opts->log != NULL && t_is_legal_path (opts->log) == FALSE ) {
		err_show ( ERR_EXIT, _("\"%s\" is not a valid log file name."), opts->log);
		num_errors ++;
		if ( opts->log != NULL )
			free ( opts->log );
		opts->log = NULL;
	}

	if ( v_label_field != NULL ) {
		if ( opts->label_field != NULL ) {
			free ( opts->label_field );
		}
		opts->label_field = strdup (v_label_field);
		free ( v_label_field );
		v_label_field = NULL;
	}

	if ( v_label_mode_point != NULL ) {
		BOOLEAN valid = FALSE;
		int i = 0;
		while ( strlen (OPTIONS_LABEL_MODE_NAMES[i]) > 0 ) {
			if ( !strcasecmp ( OPTIONS_LABEL_MODE_NAMES[i], v_label_mode_point ) ) {
				opts->label_mode_point = i;
				valid = TRUE;
				break;
			}
			i ++;
		}
		if ( valid == FALSE ) {
			err_show ( ERR_EXIT, _("Invalid point label mode ('%s')."), v_label_mode_point );
			num_errors ++;
		}
		free ( v_label_mode_point );
		v_label_mode_point = NULL;
	}

	if ( v_label_mode_line != NULL ) {
		BOOLEAN valid = FALSE;
		int i = 0;
		while ( strlen (OPTIONS_LABEL_MODE_NAMES[i]) > 0 ) {
			if ( !strcasecmp ( OPTIONS_LABEL_MODE_NAMES[i], v_label_mode_line ) ) {
				opts->label_mode_line = i;
				valid = TRUE;
				break;
			}
			i ++;
		}
		if ( valid == FALSE ) {
			err_show ( ERR_EXIT, _("Invalid line label mode ('%s')."), v_label_mode_line );
			num_errors ++;
		}
		free ( v_label_mode_line );
		v_label_mode_line = NULL;
	}

	if ( v_label_mode_poly != NULL ) {
		BOOLEAN valid = FALSE;
		int i = 0;
		while ( strlen (OPTIONS_LABEL_MODE_NAMES[i]) > 0 ) {
			if ( !strcasecmp ( OPTIONS_LABEL_MODE_NAMES[i], v_label_mode_poly ) ) {
				opts->label_mode_poly = i;
				valid = TRUE;
				break;
			}
			i ++;
		}
		if ( valid == FALSE ) {
			err_show ( ERR_EXIT, _("Invalid polygon label mode ('%s')."), v_label_mode_poly );
			num_errors ++;
		}
		free ( v_label_mode_poly );
		v_label_mode_line = NULL;
	}

	if ( v_orient_mode != NULL ) {
		BOOLEAN valid = FALSE;
		int i = 0;
		while ( strlen (OPTIONS_ORIENT_MODE_NAMES[i]) > 0 ) {
			if ( !strcasecmp ( OPTIONS_ORIENT_MODE_NAMES[i], v_orient_mode ) ) {
				opts->orient_mode = i;
				valid = TRUE;
				break;
			}
			i ++;
		}
		if ( valid == FALSE ) {
			err_show ( ERR_EXIT, _("Invalid orientation mode ('%s')."), v_orient_mode );
			num_errors ++;
		}
		free ( v_orient_mode );
		v_orient_mode = NULL;
	}
	
	if ( v_topo_level != NULL ) {
		BOOLEAN valid = FALSE;
		int i = 0;
		while ( strlen (OPTIONS_TOPO_LEVEL_NAMES[i]) > 0 ) {
			if ( !strcasecmp ( OPTIONS_TOPO_LEVEL_NAMES[i], v_topo_level ) ) {
				opts->topo_level = i;
				valid = TRUE;
				break;
			}
			i ++;
		}
		if ( valid == FALSE ) {
			err_show ( ERR_EXIT, _("Invalid topology level ('%s')."), v_topo_level );
			num_errors ++;
		}
		free ( v_topo_level );
		v_topo_level = NULL;
	}

	if ( v_format != NULL ) {
		BOOLEAN found = FALSE;
		int i = 0;
		while ( strcmp ( PRG_OUTPUT_EXT[i],"" ) != 0 ) {
			if ( !strcasecmp ( PRG_OUTPUT_EXT[i], v_format ) ) {
				opts->format = i;
				found = TRUE;
				free (v_format);
				break;
			}
			i ++;
		}
		if ( found == FALSE ) {
			err_show ( ERR_EXIT, _("The specified output format \"%s\" is unknown."), v_format);
			num_errors ++;
		}
	}

	if ( v_tolerance != NULL ) {
		opts->tolerance = t_str_to_dbl (v_tolerance, 0, 0, &error, NULL );
		snprintf ( opts->tolerance_str, PRG_MAX_STR_LEN, "%s", v_tolerance );
		free ( v_tolerance );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified tolerance setting is not a valid number."));
			num_errors ++;
			opts->tolerance = OPTIONS_DEFAULT_TOLERANCE;
			t_dbl_to_str (opts->tolerance, opts->tolerance_str);
		}
	}
	if ( opts->tolerance < 0.0 ) {
		err_show(ERR_NOTE, "");
		err_show ( ERR_WARN, _("Tolerance setting < 0. Identical vertices will not be removed."));
	}
	if ( v_snapping != NULL ) {
		opts->snapping = t_str_to_dbl (v_snapping, 0, 0, &error, NULL );
		snprintf ( opts->snapping_str, PRG_MAX_STR_LEN, "%s", v_snapping );
		free ( v_snapping );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified snapping setting is not a valid number."));
			num_errors ++;
			opts->snapping = OPTIONS_DEFAULT_SNAPPING;
			t_dbl_to_str (opts->snapping, opts->snapping_str);
		}
		/* snapping setting ignored in mode "topology=none" */		
		if ( opts->topo_level == OPTIONS_TOPO_LEVEL_NONE ) {
			err_show ( ERR_NOTE, "");
			err_show ( ERR_WARN, _("Setting for 'snapping' ignored when running with 'topology=%s'."), OPTIONS_TOPO_LEVEL_NAMES[OPTIONS_TOPO_LEVEL_NONE]);
		}
	}
	if ( opts->snapping < 0.0 ) {
		err_show ( ERR_EXIT, _("Snapping setting must be 0 or a positive number."));
		num_errors ++;
		opts->snapping = OPTIONS_DEFAULT_SNAPPING;
		t_dbl_to_str (opts->snapping, opts->snapping_str);		
	}	
	
	if ( v_dangling != NULL ) {
		opts->dangling = t_str_to_dbl (v_dangling, 0, 0, &error, NULL );
		snprintf ( opts->dangling_str, PRG_MAX_STR_LEN, "%s", v_dangling );
		free ( v_dangling );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified dangling setting is not a valid number."));
			num_errors ++;
			opts->dangling = OPTIONS_DEFAULT_DANGLING;
			t_dbl_to_str (opts->dangling, opts->dangling_str);
		}
		/* dangling setting ignored in mode "topology=none" */		
		if ( opts->topo_level == OPTIONS_TOPO_LEVEL_NONE ) {
			err_show ( ERR_NOTE, "");
			err_show ( ERR_WARN, _("Setting for 'dangling' ignored when running with 'topology=%s'."), OPTIONS_TOPO_LEVEL_NAMES[OPTIONS_TOPO_LEVEL_NONE]);
		}
	}
	if ( opts->dangling < 0.0 ) {
		err_show ( ERR_EXIT, _("Dangling setting must be 0 or a positive number."));
		num_errors ++;
		opts->dangling = OPTIONS_DEFAULT_DANGLING;
		t_dbl_to_str (opts->dangling, opts->dangling_str);
	}

	if ( v_offset_x != NULL ) {
		opts->offset_x = t_str_to_dbl (v_offset_x, 0, 0, &error, NULL );
		snprintf ( opts->offset_x_str, PRG_MAX_STR_LEN, "%s", v_offset_x );
		free ( v_offset_x );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified X coordinate offset is not a valid number."));
			num_errors ++;
			opts->offset_x = OPTIONS_DEFAULT_OFFSET_X;
			t_dbl_to_str (opts->offset_x, opts->offset_x_str);
		}
	}

	if ( v_offset_y != NULL ) {
		opts->offset_y = t_str_to_dbl (v_offset_y, 0, 0, &error, NULL );
		snprintf ( opts->offset_y_str, PRG_MAX_STR_LEN, "%s", v_offset_y );
		free ( v_offset_y );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified Y coordinate offset is not a valid number."));
			num_errors ++;
			opts->offset_y = OPTIONS_DEFAULT_OFFSET_Y;
			t_dbl_to_str (opts->offset_y, opts->offset_y_str);
		}
	}

	if ( v_offset_z != NULL ) {
		opts->offset_z = t_str_to_dbl (v_offset_z, 0, 0, &error, NULL );
		snprintf ( opts->offset_z_str, PRG_MAX_STR_LEN, "%s", v_offset_z );
		free ( v_offset_z );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified Z coordinate offset is not a valid number."));
			num_errors ++;
			opts->offset_z = OPTIONS_DEFAULT_OFFSET_Z;
			t_dbl_to_str (opts->offset_z, opts->offset_z_str);
		}
	}

	if ( v_decimal_places != NULL ) {
		opts->decimal_places = t_str_to_int (v_decimal_places, &error, NULL );
		snprintf ( opts->decimal_places_str, PRG_MAX_STR_LEN, "%s", v_decimal_places );
		free ( v_decimal_places );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("Specified decimal places is not a valid number."));
			num_errors ++;
			opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
			sprintf ( opts->decimal_places_str, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
		}
	}

	if ( opts->decimal_places < 0 ) {
		err_show ( ERR_EXIT, _("Number of decimal places must be 0 or a positive number."));
		num_errors ++;
		opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
		sprintf ( opts->decimal_places_str, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
	}

	if ( opts->decimal_places > PRG_MAX_DECIMAL_PLACES ) {
		err_show ( ERR_EXIT, _("Number of decimal places cannot exceed %i."), PRG_MAX_DECIMAL_PLACES);
		num_errors ++;
		opts->decimal_places = OPTIONS_DEFAULT_DECIMAL_PLACES;
		sprintf ( opts->decimal_places_str, "%i", OPTIONS_DEFAULT_DECIMAL_PLACES );
	}

	if ( v_decimal != NULL && v_group != NULL ) {
		if ( !strcmp ( v_decimal, v_group ) ) {
			err_show ( ERR_EXIT, _("Decimal point and grouping characters must not be identical."));
			num_errors ++;
		}
	}

	if ( ( v_decimal != NULL && v_group == NULL ) ||
			( v_decimal == NULL && v_group != NULL ) ) {
		if ( opts->show_gui == FALSE ) {
			err_show ( ERR_EXIT, _("Decimal point and grouping characters must both be specified."));
			num_errors ++;
		}
	}

	if ( v_decimal != NULL ) {
		if ( strlen (v_decimal) != 1 ) {
			err_show ( ERR_EXIT, _("Decimal point separator must be a single character."));
			num_errors ++;
		} else {
			opts->decimal_point[0] = v_decimal[0];
			opts->decimal_point[1] = '\0';
		}
		free ( v_decimal );
	}

	if ( v_group != NULL ) {
		if ( strlen (v_group) != 1 ) {
			err_show ( ERR_EXIT, _("Decimal grouping character must be a single character."));
			num_errors ++;
		} else {
			opts->decimal_group[0] = v_group[0];
			opts->decimal_group[1] = '\0';
		}
		free ( v_group );
	}


	/* Store SRS specifications */
	if ( v_proj_in != NULL ) {
		opts->proj_in = strdup (v_proj_in);
		t_free (v_proj_in);
		v_proj_in = NULL;
	}

	if ( v_proj_out != NULL ) {
		opts->proj_out = strdup (v_proj_out);
		t_free (v_proj_out);
		v_proj_out = NULL;
	}

	/* Warn if only output SRS is specified. */
	if ( opts->proj_out != NULL && opts->proj_in == NULL ) {
		err_show ( ERR_WARN, _("Only output SRS specified. Ignoring."));
	}

	/* Check input SRS. */
	if ( opts->proj_in != NULL ) {
		/* Error if input SRS is "local" and output SRS is anything else. */
		if ( opts->proj_out != NULL ) {
			if ( strcasecmp (opts->proj_in, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL]) == 0 ) {
				if ( strcasecmp (opts->proj_out, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL]) != 0 ) {
					err_show ( ERR_EXIT, _("Local survey data cannot be reprojected."));
					num_errors ++;
				}
			}
		}
		free ( v_proj_in );
	}

	/* True if there are any options re. datum transformation */
	BOOLEAN has_datum_opts = FALSE;

	/* Parse WGS84 datum transformation options */
	if ( v_proj_dx != NULL ) {
		opts->wgs84_trans_dx = t_str_to_dbl (v_proj_dx, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_dx_str, PRG_MAX_STR_LEN, "%s", v_proj_dx );
		free ( v_proj_dx );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum X shift is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_dx = OPTIONS_DEFAULT_WGS84_TRANS_DX;
			t_dbl_to_str (opts->wgs84_trans_dx, opts->wgs84_trans_dx_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_dy != NULL ) {
		opts->wgs84_trans_dy = t_str_to_dbl (v_proj_dy, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_dy_str, PRG_MAX_STR_LEN, "%s", v_proj_dy );
		free ( v_proj_dy );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum Y shift is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_dy = OPTIONS_DEFAULT_WGS84_TRANS_DY;
			t_dbl_to_str (opts->wgs84_trans_dy, opts->wgs84_trans_dy_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_dz != NULL ) {
		opts->wgs84_trans_dz = t_str_to_dbl (v_proj_dz, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_dz_str, PRG_MAX_STR_LEN, "%s", v_proj_dz );
		free ( v_proj_dz );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum Z shift is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_dz = OPTIONS_DEFAULT_WGS84_TRANS_DZ;
			t_dbl_to_str (opts->wgs84_trans_dz, opts->wgs84_trans_dz_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_rx != NULL ) {
		opts->wgs84_trans_rx = t_str_to_dbl (v_proj_rx, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_rx_str, PRG_MAX_STR_LEN, "%s", v_proj_rx );
		free ( v_proj_rx );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum X rotation is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_rx = OPTIONS_DEFAULT_WGS84_TRANS_RX;
			t_dbl_to_str (opts->wgs84_trans_rx, opts->wgs84_trans_rx_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_ry != NULL ) {
		opts->wgs84_trans_ry = t_str_to_dbl (v_proj_ry, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_ry_str, PRG_MAX_STR_LEN, "%s", v_proj_ry );
		free ( v_proj_ry );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum Y rotation is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_ry = OPTIONS_DEFAULT_WGS84_TRANS_RY;
			t_dbl_to_str (opts->wgs84_trans_ry, opts->wgs84_trans_ry_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_rz != NULL ) {
		opts->wgs84_trans_rz = t_str_to_dbl (v_proj_rz, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_rz_str, PRG_MAX_STR_LEN, "%s", v_proj_rz );
		free ( v_proj_rz );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum Z rotation is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_rz = OPTIONS_DEFAULT_WGS84_TRANS_RZ;
			t_dbl_to_str (opts->wgs84_trans_rz, opts->wgs84_trans_rz_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_ds != NULL ) {
		opts->wgs84_trans_ds = t_str_to_dbl (v_proj_ds, 0, 0, &error, NULL );
		snprintf ( opts->wgs84_trans_ds_str, PRG_MAX_STR_LEN, "%s", v_proj_ds );
		free ( v_proj_ds );
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("The specified WGS 84 datum scaling is not a valid number."));
			num_errors ++;
			opts->wgs84_trans_ds = OPTIONS_DEFAULT_WGS84_TRANS_DS;
			t_dbl_to_str (opts->wgs84_trans_ds, opts->wgs84_trans_ds_str);
		} else {
			has_datum_opts = TRUE;
		}
	}

	if ( v_proj_grid != NULL ) {
		opts->wgs84_trans_grid = strdup (v_proj_grid);
		t_free (v_proj_grid);
		v_proj_grid = NULL;
		/* Error if we have datum transform params specified, and also a transform grid */
		if ( has_datum_opts == TRUE ) {
			err_show ( ERR_EXIT, _("Specify either WGS 84 transformation parameters or a grid file, not both."));
			num_errors ++;
		} else {
			has_datum_opts = TRUE;
			if ( t_fopen_utf8 ( opts->wgs84_trans_grid, "r" ) == NULL ) {
				err_show ( ERR_EXIT, _("Specified transformation grid file is not a readable file."));
				num_errors ++;
			}
		}
	}

	/* Error if we have (a) datum transformation option(s) but no reprojection source and target. */
	if ( has_datum_opts == TRUE ) {
		if ( opts->proj_in == NULL && opts->proj_out == NULL ) {
			err_show ( ERR_EXIT, _("WGS 84 datum transformation requires input and output SRS specifications."));
			num_errors ++;
		}
	}

	/* All other tokens are input files and need to be added
	 * to the input files list.
	 */
	i = optind;
	num_files = 0;
	while ( opts->argv[i] != NULL ) {
		i++;
		num_files ++;
	}
	if ( num_files > 0 ) {
		opts->input = malloc ( sizeof (char*) * (num_files+1) );
		opts->num_input = num_files;
		int i = optind;
		int j = 0;
		while ( opts->argv[i] != NULL ) {
			if ( t_is_legal_path (opts->argv[i]) == FALSE ) {
				err_show ( ERR_EXIT, _("\"%s\" is not a valid file path."), opts->argv[i]);
				num_errors ++;
			} else {
				opts->input[j] = options_get_optarg(opts->argv[i]);
				j++;
			}
			i++;
		}
		opts->input[j] = NULL;
	}

	opts->empty = FALSE;

#ifdef GUI
	OPTIONS_GUI_MODE = opts->show_gui;
#endif

	return (num_errors);
}
