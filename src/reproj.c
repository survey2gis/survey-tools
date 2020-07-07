/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	reproj.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Provide helper functions and declarations for reprojection support
 *  		    using PROJ.4.
 *
 * COPYRIGHT:	(C) 2017 the authors
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "global.h"

#include "errors.h"
#include "geom.h"
#include "i18n.h"
#include "options.h"
#include "reproj.h"
#include "tools.h"

/*
 *
 * Initalizes reprojection system and checks internal "database" for consistency.
 * Must be called prior to using any other reproj_*() function.
 */
void reproj_init( options *opts ) {

	/* 1: Make sure that lists in reproj.h are consistent. */
	int num_shortcuts_name = 0;
	while ( strlen (REPROJ_SHORTCUT_NAME[num_shortcuts_name]) > 0 ) {
		num_shortcuts_name ++;
	}

	int num_shortcuts_desc = 0;
	while ( strlen (REPROJ_SHORTCUT_DESC[num_shortcuts_desc]) > 0 ) {
		num_shortcuts_desc ++;
	}

	int num_shortcuts_epsg = 0;
	while ( REPROJ_SHORTCUT_EPSG[num_shortcuts_epsg] > 0 ) {
		num_shortcuts_epsg ++;
	}

	BOOLEAN check = ( num_shortcuts_name == num_shortcuts_desc ) &&  ( num_shortcuts_desc == num_shortcuts_epsg );
	if ( check != TRUE ) {
		err_show ( ERR_EXIT, _("\nInternal reprojection database is inconsistent. Failed to initialize.") );
	}

	/* 2. Test PROJ.4 initialization. */
	projPJ pj_merc;
	if (!(pj_merc = pj_init_plus("+proj=merc +ellps=clrk66 +lat_ts=33")) ) {
		err_show ( ERR_EXIT, _("\nFailed to initialize PROJ.4 reprojection engine.") );
	}

	/* 3. Set PROJ.4 data directory (this may fail) */
	char *proj4_data_dir = t_set_data_dir ( REPROJ_PROJ4_ENVVAR, REPROJ_PROJ4_DATA_DIR, NULL );

	if ( proj4_data_dir != NULL ) {
		/* Check that the "epsg" database actually exists in the given path! */
		{
			int len = strlen(proj4_data_dir) + strlen (PRG_FILE_SEPARATOR_STR) + strlen(REPROJ_PROJ4_EPSG_FILE) + 1;
			char *buf=malloc(sizeof(char)*len);
			snprintf(buf, len-1, "%s", proj4_data_dir);
			strncat(buf, PRG_FILE_SEPARATOR_STR, len-1);
			strncat(buf, REPROJ_PROJ4_EPSG_FILE, len-1);
			FILE *fp;
#ifdef MINGW
			fp = t_fopen_utf8 ( buf, "rt" );
#else
			fp = t_fopen_utf8 ( buf, "r" );
#endif
			if ( fp == NULL ) {
				err_show ( ERR_WARN, _("\nPROJ.4 EPSG database file not found/readable. EPSG conversions might not be available.\nPROJ.4 EPSG path was: '%s'."),buf);
				return;
			}
		}
		char **search_path;
		search_path = malloc ( sizeof(char*) );
		search_path[0] = proj4_data_dir;
		pj_set_searchpath(1,(const char**)search_path);
		err_show ( ERR_NOTE, _("PROJ.4 data path is: '%s'"), search_path[0]);
	} else {
		err_show ( ERR_WARN, _("\nFailed to set PROJ.4 data path. EPSG conversions might not be available."));
	}

}


/*
 * Convenience function: Returns number of tokens in
 * a NULL-terminated token array as produced by
 * reproj_proj4_tokenize().
 *
 * Never run this function on an array that is not
 * NULL-terminated! Unpredictable behaviour will ensue.
 *
 */
int reproj_proj4_num_tokens ( char **def_arr)
{
	int num = 0;

	if ( def_arr != NULL ) {
		while ( def_arr[num] != NULL ) {
			num ++;
		}
	}

	return ( num );
}


/*
 * Splits a full PROJ.4 SRS definition string into its
 * individual tokens and returns them as an array of strings.
 *
 * This function will _only_ work correctly if both A and B are
 * valid PROJ.4 definition strings, where each element (incl.
 * the first one) is preceded by a " +" (i.e a space and a plus)
 * token (see REPROJ_PROJ4_SEP in reproj.h)!
 *
 * The result array is NULL-terminated.
 * Use reproj_proj4_num_tokens() to count the number of tokens
 * in such an array.
 *
 * Memory for the result is allocated by this function and must
 * be free'd by the caller.
 *
 * Returns:
 * NULL on error or empty argument
 * a newly allocated array of token strings, otherwise (NULL-terminated).
 *
 */
char** reproj_proj4_tokenize ( const char *def )
{
	char **result = NULL;

	if ( def != NULL && strlen (def) > 0 ) {
		char *tmp = strdup(def);

		/* count items in SRS def */
		int num_tokens = 0;
		{
			char *token = strtok(tmp,REPROJ_PROJ4_SEP);
			while ( token != NULL ) {
				num_tokens ++;
				token = strtok(NULL,REPROJ_PROJ4_SEP);
			}
		}
		num_tokens ++; /* add one for terminator */

		/* dump and get fresh copy */
		t_free(tmp);
		tmp = strdup(def);
		result = malloc (sizeof (char*) * num_tokens);
		{
			int i=0;
			char *token = strtok(tmp,REPROJ_PROJ4_SEP);
			while ( token != NULL ) {
				result[i] = strdup (token);
				i++;
				token = strtok(NULL,REPROJ_PROJ4_SEP);
			}
			result[num_tokens-1]=NULL; /* add terminator */
		}

		/* clean up */
		t_free(tmp);
	}

	/* return result */
	return ( result );
}


/*
 * Checks whether two PROJ.4 strings describe the same SRS.
 * This function will _only_ work correctly if both A and B are
 * valid PROJ.4 definition strings, where each element (incl.
 * the first one) is preceded by a " +" (i.e a space and a plus)
 * token (see REPROJ_PROJ4_SEP in reproj.h)!
 *
 * Note that this check may not be correct in the case of +towgs84
 * datum definitions, where default parameters may or may not have
 * been provided by the user. But heck, if people insist on screwing
 * up that much ...
 *
 * Return values:
 *
 * TRUE = both strings describe the same SRS
 * FALSE = strings describe different SRS or at least one is erroneous
 */
BOOLEAN reproj_check_proj4_equal ( const char* A, const char* B ) {

	if ( A == NULL || B == NULL ) {
		return ( FALSE );
	}

	if ( A == B ) {
		return ( TRUE );
	}

	char **tokens_A = reproj_proj4_tokenize(A);
	int num_tokens_A = reproj_proj4_num_tokens(tokens_A);
	char **tokens_B = reproj_proj4_tokenize(B);
	int num_tokens_B = reproj_proj4_num_tokens(tokens_B);

	/* not same number of tokens? not same SRS */
	if ( num_tokens_A != num_tokens_B ) {
		int k;
		for (k=0; k < num_tokens_A; k++) {
			t_free (tokens_A[k]);
		}
		t_free (tokens_A);
		for (k=0; k < num_tokens_B; k++) {
			t_free (tokens_B[k]);
		}
		t_free (tokens_B);
		return ( FALSE );
	}

	/* PAST THIS point, the NUMBER of tokens in A and B is EQUAL! */

	/* check for any tokens that are not shared between A and B */
	{
		int i;
		for (i = 0; i < num_tokens_A; i++) {
			BOOLEAN match = FALSE;
			int j;
			for (j = 0; j < num_tokens_B; j++) {
				if ( !strcasecmp ( tokens_A[i], tokens_B[j]) ) {
					match = TRUE;
					break;
				}
			}
			if ( match == FALSE ) {
				/* clean and return FALSE */
				int k;
				for (k=0; k < num_tokens_A; k++) {
					t_free (tokens_A[k]);
				}
				t_free (tokens_A);
				for (k=0; k < num_tokens_B; k++) {
					t_free (tokens_B[k]);
				}
				t_free (tokens_B);
				return ( FALSE );
			}
		}
	}

	/* release memory */
	{
		int k;
		for (k=0; k < num_tokens_A; k++) {
			t_free (tokens_A[k]);
			t_free (tokens_B[k]);
		}
		t_free (tokens_A);
		t_free (tokens_B);
	}

	return ( TRUE );
}


/*
 *
 * This function will parse SRS definitions in any of the formats
 * supported by Survey2GIS (see reproj.h) and converts them to
 * PROJ.4 strings (or their closest equivalents).
 *
 * In addition, this function parses and validates datum transformation
 * options (if any are given) and appends them to the PROJ.4 strings
 * as needed.
 *
 * IMPORTANT!
 * Once this function has run, the members "proj4_in" and "proj4_out"
 * will either be NULL (if the respective SRS have not been defined)
 * _or_ will contain a valid PROJ4 SRS struct. In either case, alot
 * of program logics will rely on these data having been set correctly,
 * so this function must run _early_ in main.c!
 *
 * NOTE: This function assumes that input and output SRS definitions
 * in *opts (if any) have previously been converted to all lower case!
 *
 * Return values:
 *   1 = No error (=REPROJ_STATUS_OK)
 *  -1 = error (=REPROJ_STATUS_ERROR)
 */
int reproj_parse_opts( options *opts ) {
	/* Check input and output SRS whether any of them is an EPSG code
	 * or an internal SRS shortcut. */

	/* Grid file specified? Path might need some adjustments before passing it to PROJ.4! */
	if ( opts->wgs84_trans_grid != NULL && strlen (opts->wgs84_trans_grid) > 0 ) {
		/* Attempt to replace path to grid file with absolute path. */
		char *abs = realpath(opts->wgs84_trans_grid,NULL);
		if ( abs != NULL ) {
			t_free (opts->wgs84_trans_grid);
			opts->wgs84_trans_grid = abs;
		}
#ifdef MINGW
		/* Attempt to convert encoding to Windows' file system encoding */
		char *buf_out = NULL;
		int result = t_str_enc("UTF-8", I18N_WIN_CODEPAGE_FILES, (char*) opts->wgs84_trans_grid, &buf_out);
		if ( result >= 0 && buf_out != NULL ) {
			/* Convert to Windows encoding */
			t_free ( opts->wgs84_trans_grid );
			opts->wgs84_trans_grid = strdup ( buf_out );
		}
		t_free ( buf_out );
#endif
	}

	/* SRS defs: We start with checking for shorthand SRS definitions (as defined in reproj.h) */
	BOOLEAN was_abbrev_in = FALSE;
	BOOLEAN was_abbrev_out = FALSE;
	{
		int i;
		for ( i = 0; i < 2; i ++ ) { /* two runs: input and output SRS */
			char *current = opts->proj_in; /* let's start with input SRS */
			if ( i > 0 ) {
				current = opts->proj_out; /* 2nd pass: switch to output SRS */
			}
			if ( current == NULL ) {
				break; /* this can happen, because one of the SRS may be missing */
			}
			/* check if the SRS string matches an internal SRS abbreviation (as defined in reproj.h) */
			int j = 0;
			BOOLEAN found = FALSE;
			while ( strlen(REPROJ_SHORTCUT_NAME[j]) > 0 ) {
				if ( !strcasecmp(REPROJ_SHORTCUT_NAME[j],current) ) {
					found = TRUE;
					break;
				}
				j ++;
			}
			if ( found == TRUE ) {
				/* resolve SRS shorthand (as defined in reproj.h) */
				if ( i > 0 ) {
					t_free (opts->proj_out); /* replace current SRS output string with EPSG code */
					opts->proj_out = (char*) malloc(sizeof(char) * PRG_MAX_STR_LEN);
					current = opts->proj_out;
				} else {
					t_free (opts->proj_in); /* replace current SRS input string with EPSG code */
					opts->proj_in = (char*) malloc(sizeof(char) * PRG_MAX_STR_LEN);
					current = opts->proj_in;
				}
				snprintf(current,PRG_MAX_STR_LEN,"epsg:%i",REPROJ_SHORTCUT_EPSG[j]);
				if ( i == 0 ) {
					was_abbrev_in = TRUE;
				} else {
					was_abbrev_out = TRUE;
				}
			}
		}
	}

	/* At this point, input and output SRS are EPSG or PROJ.4 strings or invalid. */

	BOOLEAN srs_is_web_in = FALSE;
	BOOLEAN srs_is_web_out = FALSE;
	/* resolve EPSG IDs (if any) to PROJ.4 definitions */
	{
		int i;
		char *current = NULL;
		for ( i = 0; i < 2; i ++ ) {
			if ( i == 0 ) {
				current = strdup(opts->proj_in);
			} else {
				current = strdup(opts->proj_out);
			}
			if ( current == NULL ) {
				break; /* this can happen, because one of the SRS may be missing */
			}
			// Convert "epsg" prefix to lower case (if applicable)
			char *lcase = t_str_to_lower(current);
			if ( strstr(lcase, "epsg") == lcase ) {
				t_free (current);
				current = lcase;
			} else {
				t_free (lcase);
			}
			// Check for EPSG definition
			if ( strstr(current, "epsg") == current ) {
				char *idx = strtok(current,":");
				idx = strtok(NULL,":");
				if ( idx != NULL ) {
					BOOLEAN ierror;
					BOOLEAN ioverflow;
					char *epsg = NULL;
					epsg = t_str_pack(idx);
					if ( epsg != NULL ) {
						int code = t_str_to_int( epsg, &ierror, &ioverflow );
						t_free (epsg);
						if ( ierror == TRUE ) {
							if ( i > 0 ) {
								err_show ( ERR_NOTE, "\n");
								err_show ( ERR_EXIT, _("\nInvalid EPSG code in output SRS definition.") );
								return(REPROJ_STATUS_ERROR);
							} else {
								err_show ( ERR_NOTE, "\n");
								err_show ( ERR_EXIT, _("\nInvalid EPSG code in input SRS definition.") );
								return(REPROJ_STATUS_ERROR);
							}
						}
						/* warn if we convert from an EPSG code that is not identical with any internal SRS-shortcut */
						if ( ( i == 0 && was_abbrev_in == FALSE ) || ( i == 1 && was_abbrev_out == FALSE ) ) {
							err_show ( ERR_NOTE, "\n");
							err_show ( ERR_WARN, _("\nConverted EPSG ID %i in SRS definition to PROJ.4 SRS string.\nConversion may incur loss of information. Please verify result."), code );
						}
						/* convert EPSG code to PROJ.4 string: '+init=epsg:nnnnn' */
						t_free ( current );
						if ( i > 0 ) {
							t_free (opts->proj_out); /* replace current SRS output string with EPSG code */
							opts->proj_out = (char*) malloc(sizeof(char) * PRG_MAX_STR_LEN);
							current = opts->proj_out;
						} else {
							t_free (opts->proj_in); /* replace current SRS input string with EPSG code */
							opts->proj_in = (char*) malloc(sizeof(char) * PRG_MAX_STR_LEN);
							current = opts->proj_in;
						}
						snprintf(current,PRG_MAX_STR_LEN,"+init=epsg:%i",code);
						if ( code == 3857 ) {
							/* If we have a Web Mercator system then we need to remember that! */
							if ( i > 0 ) {
								srs_is_web_in = TRUE;
							} else {
								srs_is_web_out = TRUE;
							}
						}
					}
				} else {
					if ( i > 0 ) {
						err_show ( ERR_NOTE, "\n");
						err_show ( ERR_EXIT, _("\nInvalid EPSG code in output SRS definition.") );
						return(REPROJ_STATUS_ERROR);
					} else {
						err_show ( ERR_NOTE, "\n");
						err_show ( ERR_EXIT, _("\nInvalid EPSG code in input SRS definition.") );
						return(REPROJ_STATUS_ERROR);
					}
				}
			}
		}
	}

	/* PAST THIS POINT, we deal exclusively with PROJ.4 definition strings.
	 * All other representations (EPSG ID, internal abbreviations) have been
	 * converted. */

	/* Expand SRS definitions into full, explicit PROJ.4 strings */
	if ( opts->proj_in != NULL ) {
		projPJ pj_in;
		if (!(pj_in = pj_init_plus((const char*)opts->proj_in)) ) {
			err_show ( ERR_EXIT, _("\nInvalid input SRS definition.\nPROJ.4 message: %s"),
					pj_strerrno( pj_errno ) );
			return(REPROJ_STATUS_ERROR);
		} else {
			/* Store and report expanded SRS definition */
			char *tmp = pj_get_def(pj_in,0);
			t_free (opts->proj_in);
			opts->proj_in = tmp;
			err_show (ERR_NOTE,_("\nInput SRS (expanded): '%s'"), opts->proj_in);
		}
		pj_free (pj_in);
	}
	if ( opts->proj_out != NULL ) {
		projPJ pj_out;
		if (!(pj_out = pj_init_plus((const char*)opts->proj_out)) ) {
			err_show ( ERR_EXIT, _("\nInvalid output SRS definition.\nPROJ.4 message: %s"),
					pj_strerrno( pj_errno ) );
			return(REPROJ_STATUS_ERROR);
		} else {
			/* Store and report expanded SRS definition */
			char *tmp = pj_get_def(pj_out,0);
			t_free (opts->proj_out);
			opts->proj_out = tmp;
			err_show (ERR_NOTE,_("Output SRS (expanded): '%s'"), opts->proj_out);
		}
		pj_free (pj_out);
	}

	/* DEBUG */
	/*
	{
		if ( opts->proj_in != NULL ) {
			char **in = reproj_proj4_tokenize(opts->proj_in);
			int n_in = reproj_proj4_num_tokens(in);
			fprintf(stderr,"SRS IN: %i TOKENS:\n", n_in);
			int i;
			for (i=0; i<n_in; i++) {
				fprintf(stderr,"%i\t%s\n", (i+1), in[i]);
			}
		}
	}
	*/
	/* DEBUG */
	/*
	{
		if ( opts->proj_out != NULL ) {
			char **out = reproj_proj4_tokenize(opts->proj_out);
			int n_out = reproj_proj4_num_tokens(out);
			fprintf(stderr,"SRS OUT: %i TOKENS:\n", n_out);
			int i;
			for (i=0; i<n_out; i++) {
				fprintf(stderr,"%i\t%s\n", (i+1), out[i]);
			}
		}
	}
	*/

	/* Normalize "towgs84" parameter for datum shifts */
	/* Process datum shift options and add to PROJ.4 string */
	/* Might have to replace previously existing +towgs84 parameter! */

	/* Check if user has set any of the transformation parameters. */
	BOOLEAN wgs_trans_set = FALSE;
	if ( opts->wgs84_trans_dx != (double) OPTIONS_DEFAULT_WGS84_TRANS_DX ||
		 opts->wgs84_trans_dy != (double) OPTIONS_DEFAULT_WGS84_TRANS_DY ||
		 opts->wgs84_trans_dz != (double) OPTIONS_DEFAULT_WGS84_TRANS_DZ ||
		 opts->wgs84_trans_rx != (double) OPTIONS_DEFAULT_WGS84_TRANS_RX ||
		 opts->wgs84_trans_ry != (double) OPTIONS_DEFAULT_WGS84_TRANS_RY ||
		 opts->wgs84_trans_rz != (double) OPTIONS_DEFAULT_WGS84_TRANS_RZ ||
		 opts->wgs84_trans_ds != (double) OPTIONS_DEFAULT_WGS84_TRANS_DS )
	{
		wgs_trans_set = TRUE;
	}

	/* Check if user has specified as seven parameters transformation. */
	BOOLEAN wgs_7_params = FALSE;
	if ( opts->wgs84_trans_rx != (double) OPTIONS_DEFAULT_WGS84_TRANS_RX ||
		 opts->wgs84_trans_ry != (double) OPTIONS_DEFAULT_WGS84_TRANS_RY ||
		 opts->wgs84_trans_rz != (double) OPTIONS_DEFAULT_WGS84_TRANS_RZ ||
		 opts->wgs84_trans_ds != (double) OPTIONS_DEFAULT_WGS84_TRANS_DS )
	{
		wgs_7_params = TRUE;
	}

	/* ERROR if we are dealing with Web Mercator! */
	if ( srs_is_web_in || srs_is_web_out ) {
		if ( wgs_trans_set == TRUE ) {
			err_show ( ERR_EXIT, _("\nDatum transformation not possible for SRS of type Web Mercator.") );
			return(REPROJ_STATUS_ERROR);
		}
		if ( opts->wgs84_trans_grid != NULL ) {
			err_show ( ERR_EXIT, _("\nGrid file application not possible for SRS of type Web Mercator.") );
			return(REPROJ_STATUS_ERROR);
		}
	}

	/* Get both SRS definitions as arrays of tokens (much easier). */
	char **srs_in_arr = reproj_proj4_tokenize(opts->proj_in);
	char **srs_out_arr = reproj_proj4_tokenize(opts->proj_out);

	/* HANDLE +TOWGS84= IN INPUT SRS */
	char *towgs84_str = NULL;

	if ( srs_in_arr != NULL ) {
		/* Get position of towgs84 param (if any). */
		int towgs84_pos = -1;
		{
			int i;
			int num_tokens = reproj_proj4_num_tokens(srs_in_arr);
			for (i=0; i < num_tokens; i++) {
				if ( strstr ((const char*)srs_in_arr[i],(const char*)REPROJ_PROJ4_TOKEN_TOWGS84) ) {
					towgs84_pos = i; /* remember which token it is */
					break;
				}
			}
		}
		if ( wgs_trans_set == TRUE ) {
			err_show ( ERR_NOTE, _("\nReprojection with user-supplied datum transformation parameters.") );
			towgs84_str = malloc ( sizeof(char) * PRG_MAX_STR_LEN);
			if ( wgs_7_params == TRUE ) {
				err_show ( ERR_NOTE, _("User has specified a seven-parameter transformation.") );
				snprintf (towgs84_str,PRG_MAX_STR_LEN,"%f,%f,%f,%f,%f,%f,%f",
						opts->wgs84_trans_dx, opts->wgs84_trans_dy, opts->wgs84_trans_dz,
						opts->wgs84_trans_rx, opts->wgs84_trans_ry, opts->wgs84_trans_rz,
						opts->wgs84_trans_ds);
			} else {
				err_show ( ERR_NOTE, _("User has specified a three-parameter transformation.") );
				snprintf (towgs84_str,PRG_MAX_STR_LEN,"%f,%f,%f", opts->wgs84_trans_dx, opts->wgs84_trans_dy, opts->wgs84_trans_dz);
			}
			/* If PROJ.4 string already contains a +towgs84 def: Warn */
			if ( towgs84_pos >= 0 ) {
				/* Overriding existing "towgs84" parameters. */
				err_show ( ERR_WARN, _("Existing WGS 84 datum transformation in input SRS will be overridden.") );
			}
		} else {
			if ( towgs84_pos >= 0 ) {
				/* towgs84 param is included in EPSG definition of source SRS */
				towgs84_str = strdup(srs_in_arr[towgs84_pos]);
			}
		}
	}

	/* HANDLE +NADGRIDS= IN INPUT SRS */
	char *nadgrids_str = NULL;

	if ( srs_in_arr != NULL ) {
		/* Get position of nadgrids param (if any). */
		int nadgrids_pos = -1;
		{
			int i;
			int num_tokens = reproj_proj4_num_tokens(srs_in_arr);
			for (i=0; i < num_tokens; i++) {
				if ( strstr ((const char*)srs_in_arr[i],(const char*)REPROJ_PROJ4_TOKEN_NADGRIDS) ) {
					nadgrids_pos = i; /* remember which token it is */
					break;
				}
			}
		}
		if ( opts->wgs84_trans_grid != NULL ) {
			err_show ( ERR_NOTE, _("\nReprojection with user-supplied grid file.") );
			nadgrids_str = strdup(opts->wgs84_trans_grid);
			/* If PROJ.4 string already contains a +nadgrids def: Warn */
			if ( nadgrids_pos >= 0 ) {
				/* Overriding existing "nadgrids" parameters. */
				err_show ( ERR_WARN, _("Existing grid file specification(s) in input SRS will be overridden.") );
			}
		} else {
			if ( nadgrids_pos >= 0 ) {
				/* nadgrids param is included in EPSG definition of source SRS */
				nadgrids_str = strdup(srs_in_arr[nadgrids_pos]);
			}
		}
	}

	/* Compose new input SRS proj4 string */
	char *srs_in_final = NULL;
	if ( srs_in_arr != NULL ) {
		{
			int i;
			int len = 0;
			int num_tokens = reproj_proj4_num_tokens(srs_in_arr);
			for (i=0; i < num_tokens; i++) {
				/* 1st pass: compute max. string length*/
				len += strlen(" +") + strlen(srs_in_arr[i]);
			}
			if ( towgs84_str != NULL ) {
				len += strlen(" +") + strlen(towgs84_str);
			}
			if ( nadgrids_str != NULL ) {
				len += strlen(" +") + strlen(nadgrids_str);
			}
			srs_in_final = malloc ( sizeof(char) * (len+100) ); /* we need a little extra buffer for quoting file paths, etc. */
			sprintf(srs_in_final," ");
			for (i=0; i < num_tokens; i++) {
				if ( strstr ((const char*)srs_in_arr[i],(const char*)REPROJ_PROJ4_TOKEN_TOWGS84) == NULL ) {
					if ( strstr ((const char*)srs_in_arr[i],(const char*)REPROJ_PROJ4_TOKEN_NADGRIDS) == NULL ) {
						if ( strstr ((const char*)srs_in_arr[i],(const char*)REPROJ_PROJ4_TOKEN_NODEFS) == NULL ) {
							/* append all tokens that are _not_ towgs84 and _not_ nadgrids and _not_ "no_defs" */
							if ( i == 0 ) {
								strcat(srs_in_final,"+");
							} else {
								strcat(srs_in_final," +");
							}
							strncat(srs_in_final,(const char*)srs_in_arr[i],strlen(srs_in_arr[i]));
						}
					}
				}
			}
			/* append unified towgs84 token */
			if ( towgs84_str != NULL ) {
				strcat(srs_in_final," +towgs84=");
				strncat(srs_in_final,(const char*)towgs84_str,strlen(towgs84_str));
			}
			/* append unified nadgrids token */
			if ( nadgrids_str != NULL ) {
				strcat(srs_in_final," +nadgrids=");
				strncat(srs_in_final,(const char*)nadgrids_str,strlen(nadgrids_str));
			}
			/* append null grid file for Web Mercator */
			if ( srs_is_web_in == TRUE ) {
				strcat(srs_in_final," +nadgrids=@null");
			}
			/* ALWAYS append "+no_defs" to ignore any PROJ.4 defaults! */
			strcat(srs_in_final," +no_defs");
		}
		/* free memory for tokenized PROJ.4 string */
		{
			int i;
			int num_tokens = reproj_proj4_num_tokens(srs_in_arr);
			for (i=0; i < num_tokens; i++) {
				t_free (srs_in_arr[i]);
			}
			t_free (srs_in_arr);
		}
	}

	/* Compose new output SRS proj4 string */
	char *srs_out_final = NULL;
	if ( srs_out_arr != NULL ) {
		{
			int i;
			int len = 0;
			int num_tokens = reproj_proj4_num_tokens(srs_out_arr);
			for (i=0; i < num_tokens; i++) {
				/* 1st pass: compute max. string length*/
				len += strlen(" +") + strlen(srs_out_arr[i]);
			}
			srs_out_final = malloc ( sizeof(char) * (len+100) ); /* we need a little extra buffer for quoting file paths, etc. */
			sprintf(srs_out_final," ");
			for (i=0; i < num_tokens; i++) {
				if ( strstr ((const char*)srs_out_arr[i],(const char*)REPROJ_PROJ4_TOKEN_NODEFS) == NULL ) {
					/* append all tokens that are _not_ "no_defs" */
					if ( i == 0 ) {
						strcat(srs_out_final,"+");
					} else {
						strcat(srs_out_final," +");
					}
					strncat(srs_out_final,(const char*)srs_out_arr[i],strlen(srs_out_arr[i]));
				}
			}
			/* Append a valid "towgs84" token if there is none:
			 * Since PROJ.4 version 4.6, input and output SRS must have a valid
			 * towgs84 token for a datum transformation to occur. */
			{
				int towgs84_pos = -1;
				{
					int i;
					int num_tokens = reproj_proj4_num_tokens(srs_out_arr);
					for (i=0; i < num_tokens; i++) {
						if ( strstr ((const char*)srs_out_arr[i],(const char*)REPROJ_PROJ4_TOKEN_TOWGS84) ) {
							towgs84_pos = i; /* remember which token it is */
							break;
						}
					}
				}
				if ( towgs84_pos < 0 ) {
					strcat(srs_out_final," +towgs84=0,0,0");
				}
			}
			/* append null grid file for Web Mercator */
			if ( srs_is_web_out == TRUE ) {
				strcat(srs_out_final," +nadgrids=@null");
			}
			/* ALWAYS append "+no_defs" to ignore any PROJ.4 defaults! */
			strcat(srs_out_final," +no_defs");
		}
		/* free memory for tokenized PROJ.4 string */
		{
			int i;
			int num_tokens = reproj_proj4_num_tokens(srs_out_arr);
			for (i=0; i < num_tokens; i++) {
				t_free (srs_out_arr[i]);
			}
			t_free (srs_out_arr);
		}
	}

	/* convert final SRS strings to PROJ.4 structs */
	if ( srs_in_final != NULL ) {
		if (!(opts->proj4_in = pj_init_plus((const char*)srs_in_final) )) {
			t_free (towgs84_str);
			t_free (nadgrids_str);
			t_free (srs_in_final);
			t_free (srs_out_final);
			err_show ( ERR_EXIT, _("\nFailed parsing of input SRS definition.") );
			return (REPROJ_STATUS_ERROR);
		}
	}
	if ( srs_out_final != NULL ) {
		if (!(opts->proj4_out = pj_init_plus((const char*)srs_out_final) )) {
			t_free (towgs84_str);
			t_free (nadgrids_str);
			t_free (srs_in_final);
			t_free (srs_out_final);
			err_show ( ERR_EXIT, _("\nFailed parsing of output SRS definition.") );
			return (REPROJ_STATUS_ERROR);
		}
	}

	/* report final PROJ.4 strings */
	if ( srs_in_final != NULL ) {
		err_show ( ERR_NOTE, _("\nFinal PROJ.4 input SRS: '%s'"), pj_get_def(opts->proj4_in,0));
	}
	if ( srs_out_final != NULL ) {
		err_show ( ERR_NOTE, _("\nFinal PROJ.4 output SRS: '%s'"), pj_get_def(opts->proj4_out,0));
	}

	/* clean up */
	t_free (towgs84_str);
	t_free (nadgrids_str);
	t_free (srs_in_final);
	t_free (srs_out_final);

	return (REPROJ_STATUS_OK);
}


/*
* This function determines whether data reprojection is required.
* It also checks for logical error conditions in the SRS specifications.
*
* NOTE: reproj_parse_opts() must have run before this!
*
* Return values:
*
*  0 = No reprojection needed (=REPROJ_ACTION_NONE)
*  1 = Reprojection required (=REPROJ_ACTION_REPROJECT)
* -1 = Logical error in SRS options (=REPROJ_ACTION_ERROR)
*/
int reproj_need_reprojection ( options *opts )
{

	/* No SRS? Do nothing. */
	if ( opts->proj_in == NULL && opts->proj_out == NULL ) {
		return (REPROJ_ACTION_NONE);
	}

	/* Only input SRS? Do nothing. */
	if ( opts->proj_in != NULL && opts->proj_out == NULL ) {
		/* Validity of SRS def. must have been checked previously by reproj_parse_opts()! */
		return (REPROJ_ACTION_NONE);
	}

	/* basic SRS options validity checks */
	if ( opts->proj_in == NULL && opts->proj_out != NULL ) {
		err_show (ERR_NOTE, ("\n"));
		err_show ( ERR_EXIT, _("\nOnly output SRS defined. No reprojection possible.") );
		return (REPROJ_ACTION_ERROR);
	}
	if ( !strcasecmp( opts->proj_in, opts->proj_out) ) {
		err_show (ERR_NOTE, ("\n"));
		err_show ( ERR_WARN, _("\nInput and output SRS identical. No reprojection will be performed.") );
		return(REPROJ_ACTION_NONE);
	}

	/* check for 'local' SRS */
	if ( !strcasecmp( opts->proj_in, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] ) ||
			!strcasecmp( opts->proj_out, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] ) ) {
		if ( !strcasecmp( opts->proj_in, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] )
				&&  strcasecmp( opts->proj_out, REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] ) != 0 ) {
			/* input is 'local' but output is sth. else: error */
			err_show (ERR_NOTE, ("\n"));
			err_show ( ERR_EXIT, _("\nInput SRS is '%s'. Unable to reproject."),
					REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] );
			return (REPROJ_ACTION_ERROR);
		}
		/* either one is local, but nothing else set: just warn and skip reprojection */
		err_show (ERR_NOTE, ("\n"));
		err_show ( ERR_WARN, _("\nInput or output SRS is '%s'. No reprojection will be performed."),
				REPROJ_SHORTCUT_NAME[REPROJ_SHORTCUT_LOCAL] );
		return(REPROJ_ACTION_NONE);
	}

	/* check whether SRS definitions are in fact different */
	if ( opts->proj_in != NULL && opts->proj_out != NULL ) {
		/* check that input and output SRS are not equal */
		if ( reproj_check_proj4_equal ( (const char*) opts->proj_in, (const char*) opts->proj_out ) == TRUE ) {
			err_show (ERR_NOTE, ("\n"));
			err_show ( ERR_WARN, _("\nInput and output SRS identical. No reprojection will be performed.") );
			return(REPROJ_ACTION_NONE);
		}
	}

	/* all checks passed and we need to reproject */
	return (REPROJ_ACTION_REPROJECT);
}


/*
 * Checks if the input SRS currently in use is a lat/lon system.
 * Returns TRUE if so, FALSE otherwise.
 *
 * This function will also return FALSE if the SRS has not yet
 * been set/parsed! This matches the program behaviour that
 * data is assumed to be euclidean if a lat/lon system is not
 * explicitly set.
 *
 * NOTE: reproj_parse_opts() must have run before this!
 */
BOOLEAN reproj_srs_in_latlon ( options *opts ) {
	if ( opts == NULL || opts->proj4_in == NULL ) {
		return ( FALSE );
	}
	return (pj_is_latlong(opts->proj4_in));
}


/*
 * Checks if the output SRS currently in use is a lat/lon system.
 * Returns TRUE if so, FALSE otherwise.
 *
 * This function will also return FALSE if the SRS has not yet
 * been set/parsed! This matches the program behaviour that
 * data is assumed to be euclidean if a lat/lon system is not
 * explicitly set.
 *
 * NOTE: reproj_parse_opts() must have run before this!
 */
BOOLEAN reproj_srs_out_latlon ( options *opts ) {
	if ( opts == NULL || opts->proj4_out == NULL ) {
		return ( FALSE );
	}
	return (pj_is_latlong(opts->proj4_out));
}


/*
 * Converts a value from degrees to radians.
 */
double reproj_deg_to_rad( double deg )
{
	return (deg * (REPROJ_PI/180.0));
}

/*
 * Converts a value from radians to degrees.
 */
double reproj_rad_to_deg( double rad )
{
	return (rad * (180/REPROJ_PI));
}


/*
 * Converts all vertex data of a geometry part (line or polygon) from
 * degrees to radians. This is necessary because PROJ.4's 'pj_transform()'
 * function expects geographic lat/lon data to be in radians.
 *
 * Conversion is done IN-PLACE and will have to be reverted using
 * 'reproj_rad_to_deg_part()' after 'pj_transform()' has done its job!
 */
void reproj_deg_to_rad_part ( geom_part *p )
{
	if ( p == NULL ) {
		return;
	}

	int i;
	for ( i=0; i < p->num_vertices; i++ ) {
		p->X[i] = reproj_deg_to_rad(p->X[i]);
		p->Y[i] = reproj_deg_to_rad(p->Y[i]);
	}
}


/*
 * Converts all vertex data of a geometry part (line or polygon) from
 * radians to degrees. This is necessary because PROJ.4's 'pj_transform()'
 * function expects geographic lat/lon data to be in radians.
 *
 * This function must be run IMMEDIATELY after 'pj_transform()' has done its job!
 *
 * Conversion is done IN-PLACE.
 */
void reproj_rad_to_deg_part ( geom_part *p )
{
	if ( p == NULL ) {
		return;
	}

	int i;
	for ( i=0; i < p->num_vertices; i++ ) {
		p->X[i] = reproj_rad_to_deg(p->X[i]);
		p->Y[i] = reproj_rad_to_deg(p->Y[i]);
	}
}


/*
 * Helper function for reproj_update_extent().
 */
double reproj_set_min ( double val, double min, BOOLEAN min_set ) {

	if ( min_set == FALSE ) {
		return (val);
	}

	if ( val < min ) {
		return ( val );
	}

	return ( min );
}


/*
 * Helper function for reproj_update_extent().
 */
double reproj_set_max ( double val, double max, BOOLEAN max_set ) {

	if ( max_set == FALSE ) {
		return (val);
	}

	if ( val > max ) {
		return ( val );
	}

	return ( max );
}


/*
 * Run after reprojection to update the data extent statistics
 * within the new CRS.
 */
void reproj_update_extent(geom_store *gs) {

	BOOLEAN min_set_x = FALSE;
	BOOLEAN max_set_x = FALSE;
	BOOLEAN min_set_y = FALSE;
	BOOLEAN max_set_y = FALSE;
	BOOLEAN min_set_z = FALSE;
	BOOLEAN max_set_z = FALSE;
	double min_x = 0.0;
	double max_x = 0.0;
	double min_y = 0.0;
	double max_y = 0.0;
	double min_z = 0.0;
	double max_z = 0.0;

	err_show ( ERR_NOTE, _("\nRecomputing data extents after reprojection.") );

	/* POINTS */
	if ( gs->num_points > 0 ) {
		int i;
		for ( i=0; i < gs->num_points; i ++ ) {
			min_x = reproj_set_min (gs->points[i].X, min_x, min_set_x); min_set_x = TRUE;
			max_x = reproj_set_max (gs->points[i].X, max_x, max_set_x); max_set_x = TRUE;
			min_y = reproj_set_min (gs->points[i].Y, min_y, min_set_y); min_set_y = TRUE;
			max_y = reproj_set_max (gs->points[i].Y, max_y, max_set_y); max_set_y = TRUE;
			min_z = reproj_set_min (gs->points[i].Z, min_z, min_set_z); min_set_z = TRUE;
			max_z = reproj_set_max (gs->points[i].Z, max_z, max_set_z); max_set_z = TRUE;
		}
	}

	/* RAW POINTS */
	/* Skip: These are redundant coordinates. */

	/* LINES */
	if ( gs->num_lines > 0 ) {
		int i;
		for ( i=0; i < gs->num_lines; i ++ ) {
			int j;
			for ( j=0; j < gs->lines[i].num_parts; j ++ ) {
				int v;
				for ( v=0; v < gs->lines[i].parts[j].num_vertices; v ++ ) {
					min_x = reproj_set_min (gs->lines[i].parts[j].X[v], min_x, min_set_x); min_set_x = TRUE;
					max_x = reproj_set_max (gs->lines[i].parts[j].X[v], max_x, max_set_x); max_set_x = TRUE;
					min_y = reproj_set_min (gs->lines[i].parts[j].Y[v], min_y, min_set_y); min_set_y = TRUE;
					max_y = reproj_set_max (gs->lines[i].parts[j].Y[v], max_y, max_set_y); max_set_y = TRUE;
					min_z = reproj_set_min (gs->lines[i].parts[j].Z[v], min_z, min_set_z); min_set_z = TRUE;
					max_z = reproj_set_max (gs->lines[i].parts[j].Z[v], max_z, max_set_z); max_set_z = TRUE;
				}
			}
		}
	}

	/* POLYGONS */
	if ( gs->num_polygons > 0 ) {
		int i;
		for ( i=0; i < gs->num_polygons; i ++ ) {
			int j;
			for ( j=0; j < gs->polygons[i].num_parts; j ++ ) {
				int v;
				for ( v=0; v < gs->polygons[i].parts[j].num_vertices; v ++ ) {
					min_x = reproj_set_min (gs->polygons[i].parts[j].X[v], min_x, min_set_x); min_set_x = TRUE;
					max_x = reproj_set_max (gs->polygons[i].parts[j].X[v], max_x, max_set_x); max_set_x = TRUE;
					min_y = reproj_set_min (gs->polygons[i].parts[j].Y[v], min_y, min_set_y); min_set_y = TRUE;
					max_y = reproj_set_max (gs->polygons[i].parts[j].Y[v], max_y, max_set_y); max_set_y = TRUE;
					min_z = reproj_set_min (gs->polygons[i].parts[j].Z[v], min_z, min_set_z); min_set_z = TRUE;
					max_z = reproj_set_max (gs->polygons[i].parts[j].Z[v], max_z, max_set_z); max_set_z = TRUE;
				}
			}
		}
	}

	/* update */
	gs->min_x = min_x;
	gs->max_x = max_x;
	gs->min_y = min_y;
	gs->max_y = max_y;
	gs->min_z = min_z;
	gs->max_z = max_z;

}


/*
 * Performs reprojection. User _must_ call reproj_parst_opts()
 * prior to this function to ensure that SRS strings and datum
 * transform (if applicable) are valid!
 *
 * Reprojection is done using PROJ.4' pj_transform():
 * https://github.com/OSGeo/proj.4/wiki/pj_transform
 *
 * Return values:
 *   1 = No error and reprojection performed (=REPROJ_STATUS_OK)
 *   0 = No error, but also no reprojection performed (=REPROJ_STATUS_NONE)
 * < 0 = error (<=REPROJ_STATUS_ERROR)
 */
int reproj_do( options *opts, geom_store *gs ) {

	if ( gs == NULL || gs->is_empty == TRUE ) {
		err_show ( ERR_WARN, _("\nEmpty or missing geometry store. Reprojection skipped.") );
		return (REPROJ_STATUS_NONE);
	}

	/* POINTS */
	if ( gs->num_points > 0 ) {
		err_show ( ERR_NOTE, _("\nReprojecting %i points in current geometry store."), gs->num_points );
		int i;
		for ( i=0; i < gs->num_points; i ++ ) {
			double X[1];
			double Y[1];
			double Z[1];
			X[0] = gs->points[i].X;
			Y[0] = gs->points[i].Y;
			Z[0] = gs->points[i].Z;
			if ( reproj_srs_in_latlon(opts) ) {
				/* pj_transform() expects lat/lon data to be in radians */
				X[0] = reproj_deg_to_rad (X[0]);
				Y[0] = reproj_deg_to_rad (Y[0]);
			}
			int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, Z);
			if ( error != 0 ) {
				err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
				err_show ( ERR_EXIT, _("\nReprojection failed at point #%i."), i+1);
				return (REPROJ_STATUS_ERROR);
			}
			if ( reproj_srs_out_latlon(opts) ) {
				/* pj_transform() has produced lat/lon data in radians */
				X[0] = reproj_rad_to_deg (X[0]);
				Y[0] = reproj_rad_to_deg (Y[0]);
			}
			/* re-write point coordinates from reprojected values */
			gs->points[i].X = X[0];
			gs->points[i].Y = Y[0];
			gs->points[i].Z = Z[0];
			if ( gs->points[i].has_label == TRUE ) {
				/* also transform label point (if any) */
				double X[1];
				double Y[1];
				X[0] = gs->points[i].label_x;
				Y[0] = gs->points[i].label_y;
				if ( reproj_srs_in_latlon(opts) ) {
					X[0] = reproj_deg_to_rad (X[0]);
					Y[0] = reproj_deg_to_rad (Y[0]);
				}
				int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, NULL);
				if ( error != 0 ) {
					err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
					err_show ( ERR_EXIT, _("\nReprojection of label point failed at point #%i."), i+1);
					return (REPROJ_STATUS_ERROR);
				}
				if ( reproj_srs_out_latlon(opts) ) {
					X[0] = reproj_rad_to_deg (X[0]);
					Y[0] = reproj_rad_to_deg (Y[0]);
				}
				gs->points[i].label_x = X[0];
				gs->points[i].label_y = Y[0];
			}
		}
	}

	/* RAW POINTS */
	if ( gs->num_points_raw > 0 ) {
		err_show ( ERR_NOTE, _("\nReprojecting %i raw vertices in current geometry store."), gs->num_points_raw );
		int i;
		for ( i=0; i < gs->num_points_raw; i ++ ) {
			double X[1];
			double Y[1];
			double Z[1];
			X[0] = gs->points_raw[i].X;
			Y[0] = gs->points_raw[i].Y;
			Z[0] = gs->points_raw[i].Z;
			if ( reproj_srs_in_latlon(opts) ) {
				/* pj_transform() expects lat/lon data to be in radians */
				X[0] = reproj_deg_to_rad (X[0]);
				Y[0] = reproj_deg_to_rad (Y[0]);
			}
			int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, Z);
			if ( error != 0 ) {
				err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
				err_show ( ERR_EXIT, _("\nReprojection failed at raw vertex #%i."), i+1);
				return (REPROJ_STATUS_ERROR);
			}
			if ( reproj_srs_out_latlon(opts) ) {
				/* pj_transform() has produced lat/lon data in radians */
				X[0] = reproj_rad_to_deg (X[0]);
				Y[0] = reproj_rad_to_deg (Y[0]);
			}
			/* re-write point coordinates from reprojected values */
			gs->points_raw[i].X = X[0];
			gs->points_raw[i].Y = Y[0];
			gs->points_raw[i].Z = Z[0];
			if ( gs->points_raw[i].has_label == TRUE ) {
				/* also transform label point (if any) */
				double X[1];
				double Y[1];
				X[0] = gs->points_raw[i].label_x;
				Y[0] = gs->points_raw[i].label_y;
				if ( reproj_srs_in_latlon(opts) ) {
					X[0] = reproj_deg_to_rad (X[0]);
					Y[0] = reproj_deg_to_rad (Y[0]);
				}
				int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, NULL);
				if ( error != 0 ) {
					err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
					err_show ( ERR_EXIT, _("\nReprojection of label point failed at raw vertex #%i."), i+1);
					return (REPROJ_STATUS_ERROR);
				}
				if ( reproj_srs_out_latlon(opts) ) {
					X[0] = reproj_rad_to_deg (X[0]);
					Y[0] = reproj_rad_to_deg (Y[0]);
				}
				gs->points_raw[i].label_x = X[0];
				gs->points_raw[i].label_y = Y[0];
			}
		}
	}

	/* LINES */
	if ( gs->num_lines > 0 ) {
		err_show ( ERR_NOTE, _("\nReprojecting %i lines in current geometry store."), gs->num_lines );
		int i;
		for ( i=0; i < gs->num_lines; i ++ ) {
			int j;
			for ( j=0; j < gs->lines[i].num_parts; j ++ ) {
				if ( reproj_srs_in_latlon(opts) ) {
					/* pj_transform() expects lat/lon data to be in radians */
					reproj_deg_to_rad_part(&gs->lines[i].parts[j]);
				}
				int error = pj_transform( opts->proj4_in, opts->proj4_out, gs->lines[i].parts[j].num_vertices, 1,
								&gs->lines[i].parts[j].X[0], &gs->lines[i].parts[j].Y[0], &gs->lines[i].parts[j].Z[0] );
				if ( error != 0 ) {
					err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
					err_show ( ERR_EXIT, _("\nReprojection failed at line #%i, part #%i."), i+1, j+1);
					return (REPROJ_STATUS_ERROR);
				}
				if ( reproj_srs_out_latlon(opts) ) {
					/* pj_transform() has produced lat/lon data in radians */
					reproj_rad_to_deg_part(&gs->lines[i].parts[j]);
				}
				if ( gs->lines[i].parts[j].has_label == TRUE ) {
					/* also transform label point (if any) */
					double X[1];
					double Y[1];
					X[0] = gs->lines[i].parts[j].label_x;
					Y[0] = gs->lines[i].parts[j].label_y;
					if ( reproj_srs_in_latlon(opts) ) {
						X[0] = reproj_deg_to_rad (X[0]);
						Y[0] = reproj_deg_to_rad (Y[0]);
					}
					int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, NULL);
					if ( error != 0 ) {
						err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
						err_show ( ERR_EXIT, _("\nReprojection of label point failed at line #%i, part #%i."), i+1, j+1);
						return (REPROJ_STATUS_ERROR);
					}
					if ( reproj_srs_out_latlon(opts) ) {
						X[0] = reproj_rad_to_deg (X[0]);
						Y[0] = reproj_rad_to_deg (Y[0]);
					}
					gs->lines[i].parts[j].label_x = X[0];
					gs->lines[i].parts[j].label_y = Y[0];
				}
			}
		}
	}

	/* POLYGONS */
	if ( gs->num_polygons > 0 ) {
		err_show ( ERR_NOTE, _("\nReprojecting %i polygons in current geometry store."), gs->num_polygons );
		int i;
		for ( i=0; i < gs->num_polygons; i ++ ) {
			int j;
			for ( j=0; j < gs->polygons[i].num_parts; j ++ ) {
				if ( reproj_srs_in_latlon(opts) ) {
					/* pj_transform() expects lat/lon data to be in radians */
					reproj_deg_to_rad_part(&gs->polygons[i].parts[j]);
				}
				int error = pj_transform( opts->proj4_in, opts->proj4_out, gs->polygons[i].parts[j].num_vertices, 1,
						gs->polygons[i].parts[j].X, gs->polygons[i].parts[j].Y, gs->polygons[i].parts[j].Z );
				if ( error != 0 ) {
					err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
					err_show ( ERR_EXIT, _("\nReprojection failed at polygon #%i, part #%i."), i+1, j+1);
					return (REPROJ_STATUS_ERROR);
				}
				if ( reproj_srs_out_latlon(opts) ) {
					/* pj_transform() has produced lat/lon data in radians */
					reproj_rad_to_deg_part(&gs->polygons[i].parts[j]);
				}
				if ( gs->polygons[i].parts[j].has_label == TRUE ) {
					/* also transform label point (if any) */
					double X[1];
					double Y[1];
					X[0] = gs->polygons[i].parts[j].label_x;
					Y[0] = gs->polygons[i].parts[j].label_y;
					if ( reproj_srs_in_latlon(opts) ) {
						X[0] = reproj_deg_to_rad (X[0]);
						Y[0] = reproj_deg_to_rad (Y[0]);
					}
					int error = pj_transform( opts->proj4_in, opts->proj4_out, 1, 1, X, Y, NULL);
					if ( error != 0 ) {
						err_show ( ERR_NOTE, _("PROJ.4 error:'%s'"), pj_strerrno(error));
						err_show ( ERR_EXIT, _("\nReprojection of label point failed at polygon #%i, part #%i."), i+1, j+1);
						return (REPROJ_STATUS_ERROR);
					}
					if ( reproj_srs_out_latlon(opts) ) {
						X[0] = reproj_rad_to_deg (X[0]);
						Y[0] = reproj_rad_to_deg (Y[0]);
					}
					gs->polygons[i].parts[j].label_x = X[0];
					gs->polygons[i].parts[j].label_y = Y[0];
				}
			}
		}
	}

	reproj_update_extent(gs);

	return (REPROJ_STATUS_OK);
}
