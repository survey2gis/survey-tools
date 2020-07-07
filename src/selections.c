/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	selections.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Functions to manage a list of selection expressions.
 * 				The latter are used to reduce the program output
 * 				to only those records for which the field content(s)
 * 				passe(s) all specified selection criteria.
 *
 * COPYRIGHT:	(C) 2015 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <getopt.h>
#include <stdlib.h>

#include "slre/slre.h"

#include "global.h"
#include "errors.h"
#include "i18n.h"
#include "options.h"
#include "parser.h"
#include "selections.h"
#include "tools.h"


char *SELECTION_TYPE_NAME[NUM_SELECTION_TYPES] = {
		SELECTION_TYPE_EQ_NAME,
		SELECTION_TYPE_NEQ_NAME,
		SELECTION_TYPE_LT_NAME,
		SELECTION_TYPE_GT_NAME,
		SELECTION_TYPE_LTE_NAME,
		SELECTION_TYPE_GTE_NAME,
		SELECTION_TYPE_SUB_NAME,
		SELECTION_TYPE_REGEXP_NAME,
		SELECTION_TYPE_RANGE_NAME,
		SELECTION_TYPE_ALL_NAME
};

char *SELECTION_TYPE_NAME_FULL[NUM_SELECTION_TYPES] = {
		SELECTION_TYPE_EQ_NAME_FULL,
		SELECTION_TYPE_NEQ_NAME_FULL,
		SELECTION_TYPE_LT_NAME_FULL,
		SELECTION_TYPE_GT_NAME_FULL,
		SELECTION_TYPE_LTE_NAME_FULL,
		SELECTION_TYPE_GTE_NAME_FULL,
		SELECTION_TYPE_SUB_NAME_FULL,
		SELECTION_TYPE_REGEXP_NAME_FULL,
		SELECTION_TYPE_RANGE_NAME_FULL,
		SELECTION_TYPE_ALL_NAME_FULL
};

char *SELECTION_TYPE_NAME_ADD[NUM_SELECTION_TYPES] = {
		SELECTION_TYPE_EQ_NAME"+",
		SELECTION_TYPE_NEQ_NAME"+",
		SELECTION_TYPE_LT_NAME"+",
		SELECTION_TYPE_GT_NAME"+",
		SELECTION_TYPE_LTE_NAME"+",
		SELECTION_TYPE_GTE_NAME"+",
		SELECTION_TYPE_SUB_NAME"+",
		SELECTION_TYPE_REGEXP_NAME"+",
		SELECTION_TYPE_RANGE_NAME"+",
		SELECTION_TYPE_ALL_NAME"+"
};

char *SELECTION_TYPE_NAME_SUB[NUM_SELECTION_TYPES] = {
		SELECTION_TYPE_EQ_NAME"-",
		SELECTION_TYPE_NEQ_NAME"-",
		SELECTION_TYPE_LT_NAME"-",
		SELECTION_TYPE_GT_NAME"-",
		SELECTION_TYPE_LTE_NAME"-",
		SELECTION_TYPE_GTE_NAME"-",
		SELECTION_TYPE_SUB_NAME"-",
		SELECTION_TYPE_REGEXP_NAME"-",
		SELECTION_TYPE_RANGE_NAME"-",
		SELECTION_TYPE_ALL_NAME"-"
};

char *SELECTION_GEOM_TYPE_NAME[NUM_SELECTION_GEOMS] = {
		SELECTION_GEOM_POINT_NAME,
		SELECTION_GEOM_RAW_NAME,
		SELECTION_GEOM_LINE_NAME,
		SELECTION_GEOM_POLY_NAME,
		SELECTION_GEOM_ALL_NAME
};

char *SELECTION_GEOM_TYPE_NAME_FULL[NUM_SELECTION_GEOMS] = {
		SELECTION_GEOM_POINT_NAME_FULL,
		SELECTION_GEOM_RAW_NAME_FULL,
		SELECTION_GEOM_LINE_NAME_FULL,
		SELECTION_GEOM_POLY_NAME_FULL,
		SELECTION_GEOM_ALL_NAME_FULL
};


/*
 * Add one selection to the list of max. OPTIONS_MAX_SELECTIONS selections.
 * The selection must be passed int the string "selection" and be in the format:
 *
 * 	selection_type_name:geom_type_name:field_name:expression
 *
 * Return values:
 *
 * TRUE:	Selection added to list.
 * FALSE:	Selection not added (selection list limit reached).
 *
 */
BOOLEAN selection_add ( char *selection, options *opt )
{
	int i;

	for ( i=0; i < PRG_MAX_SELECTIONS; i++ ) {
		if ( opt->selection[i] == NULL ) {
			opt->selection[i] = strdup (selection);
			return ( TRUE );
		}
	}

	return ( FALSE );
}


/*
 * Returns "TRUE" if the first character of "seltype" is the
 * SELECTION_MOD_INV character, with "seltype" containing the
 * "selection type" token of the selection string.
 *
 * Use "selection_get_seltype(seltype)" prior to calling this function,
 * to check whether "seltype" is actually a valid selection type name!
 */
BOOLEAN selection_is_mod_inv ( char *seltype ) {
	if ( seltype == NULL || strlen ( seltype ) < 3 ) {
		return ( FALSE );
	}
	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) return ( FALSE );

	if ( cmp[0] == SELECTION_MOD_INV ) return ( TRUE );

	return ( FALSE );
}


/*
 * Returns "TRUE" if the last character of "type" is the
 * SELECTION_MOD_ADD character, with "seltype" containing the
 * "selection type" token of the selection string.
 *
 * Use "selection_get_seltype(seltype)" prior to calling this function,
 * to check whether "seltype" is actually a valid selection type name!
 */
BOOLEAN selection_is_mod_add ( char *seltype ) {
	if ( seltype == NULL || strlen ( seltype ) < 3 ) {
		return ( FALSE );
	}
	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) return ( FALSE );

	if ( cmp[strlen(cmp)-1] == SELECTION_MOD_ADD ) return ( TRUE );

	return ( FALSE );
}


/*
 * Returns "TRUE" if the last character of "type" is the
 * SELECTION_MOD_SUB character, with "seltype" containing the
 * "selection type" token of the selection string.
 *
 * Use "selection_get_seltype(seltype)" prior to calling this function,
 * to check whether "seltype" is actually a valid selection type name!
 */
BOOLEAN selection_is_mod_sub ( char *seltype ) {
	if ( seltype == NULL || strlen ( seltype ) < 3 ) {
		return ( FALSE );
	}
	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) return ( FALSE );

	if ( cmp[strlen(cmp)-1] == SELECTION_MOD_SUB ) return ( TRUE );

	return ( FALSE );
}


/*
 * Returns "TRUE" if the selection "seltype" is case-sensitive.
 * That's all. Use "selection_get_seltype(seltype)" to check
 * whether "seltype" is actually a valid selection type name!
 */
BOOLEAN selection_is_case_sensitive ( char *seltype ) {
	if ( seltype == NULL || strlen ( seltype ) < 2 ) {
		return ( FALSE );
	}

	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) return ( FALSE );

	int i = 0;
	for ( i = 0; i < NUM_SELECTION_TYPES; i++ ) {
		if ( selection_is_mod_inv ( cmp ) == TRUE ) {
			char *p = cmp; p ++;
			if ( !strcmp ( p, SELECTION_TYPE_NAME[i] ) ) {
				t_free ( cmp );
				return ( TRUE );
			}
		} else {
			if ( !strcmp ( cmp, SELECTION_TYPE_NAME[i] ) ) {
				t_free ( cmp );
				return ( TRUE );
			}
		}
	}

	t_free ( cmp );
	return ( FALSE );
}


/*
 * Returns selection type ID (see definitions above) or
 * -1 (SELECTION_TYPE_INVALID).
 */
short selection_get_seltype ( char *seltype )
{
	if ( seltype == NULL || strlen ( seltype ) < 2 ) {
		return ( SELECTION_TYPE_INVALID );
	}

	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) {
		return ( SELECTION_TYPE_INVALID );
	}

	int i = 0;
	for ( i = 0; i < NUM_SELECTION_TYPES; i++ ) {
		if ( selection_is_mod_inv ( cmp ) == TRUE ) {
			char *p = cmp; p ++;
			if ( !strcasecmp ( p, SELECTION_TYPE_NAME[i] ) ||
					!strcasecmp ( p, SELECTION_TYPE_NAME_ADD[i]) ||
					!strcasecmp ( p, SELECTION_TYPE_NAME_SUB[i]) ) {
				t_free ( cmp );
				return ( i );
			}
		} else {
			if ( !strcasecmp ( cmp, SELECTION_TYPE_NAME[i] ) ||
					!strcasecmp ( cmp, SELECTION_TYPE_NAME_ADD[i] ) ||
					!strcasecmp ( cmp, SELECTION_TYPE_NAME_SUB[i] ) ) {
				t_free ( cmp );
				return ( i );
			}
		}
	}

	t_free ( cmp );
	return ( SELECTION_TYPE_INVALID );
}


/*
 * Returns geometry type ID (see definitions above) or
 * -1 (SELECTION_GEOM_INVALID).
 */
short selection_get_geomtype ( char *seltype )
{
	if ( seltype == NULL || strlen ( seltype ) < 3 ) {
		return ( SELECTION_GEOM_INVALID );
	}

	char *cmp = t_str_pack ( seltype );
	if ( cmp == NULL ) {
		return ( SELECTION_GEOM_INVALID );
	}

	int i = 0;
	for ( i = 0; i < NUM_SELECTION_GEOMS; i++ ) {
		if ( !strcasecmp ( cmp, SELECTION_GEOM_TYPE_NAME[i] ) ) {
			t_free ( cmp );
			return ( i );
		}
	}

	t_free ( cmp );
	return ( SELECTION_GEOM_INVALID );
}


/*
 * Checks whether the "field" token of the selection string
 * is a valid field name and the type of the field matches
 * that of the selection.
 */
BOOLEAN selection_is_valid_field ( char *field, int seltype, parser_desc *parser )
{
	if ( field == NULL && parser == NULL ) return (FALSE);
	if ( strlen (field) < 1 ) return (FALSE);

	char *cmp = t_str_pack ( field );

	/* Check that the field exists */
	int i = 0;
	while ( i < PRG_MAX_FIELDS ) {
		if ( parser->fields[i] != NULL ) {
			parser_field *pf = parser->fields[i];
			if ( pf->name != NULL ) {
				if ( !strcasecmp ( pf->name, field ) ) {
					t_free ( cmp );
					if ( ( seltype == SELECTION_TYPE_SUB || seltype == SELECTION_TYPE_REGEXP ) && pf->type != PARSER_FIELD_TYPE_TEXT ) {
						return ( FALSE );
					}
					if ( seltype == SELECTION_TYPE_RANGE && ( pf->type != PARSER_FIELD_TYPE_INT && pf->type != PARSER_FIELD_TYPE_DOUBLE ) ) {
						return ( FALSE );
					}
					return ( TRUE );
				}
			}
		}
		i ++;
	}

	t_free ( cmp );
	return ( FALSE );
}


/*
 * Returns the array index of the field named "field" in the
 * parser struct "parser".
 *
 * Returns -1 on error.
 */
int selection_get_field_idx ( char *field, parser_desc *parser )
{
	if ( field == NULL && parser == NULL ) return (-1);
	if ( strlen (field) < 1 ) return (-1);

	char *cmp = t_str_pack ( field );

	/* Get field index. */
	int i = 0;
	while ( i < PRG_MAX_FIELDS ) {
		if ( parser->fields[i] != NULL ) {
			parser_field *pf = parser->fields[i];
			if ( pf->name != NULL ) {
				if ( !strcasecmp ( pf->name, field ) ) {
					t_free ( cmp );
					return ( i );
				}
			}
		}
		i ++;
	}

	t_free ( cmp );
	return ( -1 );
}


/*
 * Checks if "regexp" contains a valid (in the sense of SLRE)
 * regular expression. Returns SLRE error code.
 */
int selection_is_valid_regexp ( char *regexp )
{
	return ( slre_match ( regexp, "abc", 3, NULL, 0, 0 ) );
}


/*
 * Returns the "min" part of a range expression in the format:
 *
 *  "min;max"
 *
 * This function does not perform any error checking on the
 * format of "range". So "selection_is_valid_range(range)" MUST
 * be run first and have returned "TRUE"!
 *
 */
double selection_get_range_min ( char *range )
{
	char *str = strdup ( range );
	char *min = strtok ( str, SELECTION_RANGE_SEP );
	double result = t_str_to_dbl ( min, 0, 0, NULL, NULL );
	t_free ( str );

	return ( result );
}


/*
 * Returns the "max" part of a range expression in the format:
 *
 *  "min;max"
 *
 * This function does not perform much error checking on the
 * format of "range". So "selection_is_valid_range(range)" MUST
 * be run first and have returned "TRUE"!
 *
 * "0.0" is returned in case of error -- which is another reason
 * to run "selection_is_valid_range()" first!!
 *
 */
double selection_get_range_max ( char *range )
{
	if ( range == NULL ) {
		return ( 0.0 );
	}

	char *str = strdup ( range );
	char *max = strtok ( str, SELECTION_RANGE_SEP );
	if ( max != NULL ) {
		max = strtok ( NULL, SELECTION_RANGE_SEP );
		if ( max != NULL ) {
			double result = t_str_to_dbl ( max, 0, 0, NULL, NULL );
			t_free ( str );
			return ( result );
		}
	}

	t_free ( str );
	return ( 0.0 );
}


/*
 * Checks validity of a range type expression in the format:
 *
 *  "min;max"
 *
 */
BOOLEAN selection_is_valid_range ( char *range )
{
	if ( range == NULL || strlen ( range ) < 3 ) return ( FALSE );

	char *max = NULL;
	char *str = strdup ( range );
	double mind = 0.0;
	double maxd = 0.0;

	char *min = strtok ( str, SELECTION_RANGE_SEP );
	if ( min == NULL || strlen ( min ) < 1 ) {
		t_free ( str );
		return ( FALSE );
	}

	max = strtok ( NULL, SELECTION_RANGE_SEP );
	if ( max == NULL || strlen ( max ) < 1 ) {
		t_free ( str );
		return ( FALSE );
	}

	BOOLEAN error = FALSE;
	BOOLEAN overflow = FALSE;

	mind = t_str_to_dbl ( min, 0, 0, &error, &overflow );
	if ( error == TRUE ) {
		t_free ( str );
		return ( FALSE );
	}

	maxd = t_str_to_dbl ( max, 0, 0, &error, &overflow );
	if ( error == TRUE ) {
		t_free ( str );
		return ( FALSE );
	}

	t_free ( str );

	/* check that max >= min */
	if ( maxd < mind ) {
		return ( FALSE );
	}

	return ( TRUE );
}


/*
 * Checks validity of one selection string in the format:
 *
 * 	selection_type_name:geom_type_name:field_name:expression
 *
 * Returns "TRUE" if the selection is valid, "FALSE" otherwise.
 */
BOOLEAN selection_is_valid ( char *selection, parser_desc *parser )
{
	if ( selection == NULL || strlen (selection) < 7 ) return ( FALSE );

	char *str = strdup (selection);
	int selection_type = SELECTION_TYPE_INVALID;
	int selection_geomtype = SELECTION_GEOM_INVALID;


	/* 1: selection type */
	char *token = strtok ( str, SELECTION_TOKEN_SEP );
	if ( token == NULL || strlen ( token ) < 2 ) {
		t_free ( str );
		err_show ( ERR_NOTE, _("Invalid or empty selection type.") );
		return ( FALSE );
	}
	selection_type = selection_get_seltype ( token );
	if ( selection_type == SELECTION_TYPE_INVALID ) {
		err_show ( ERR_NOTE, _("Invalid or empty selection type.") );
		t_free ( str );
		return ( FALSE );
	}

	/* 2: geometry type */
	token = strtok ( NULL, SELECTION_TOKEN_SEP );
	if ( token == NULL || strlen ( token ) < 3 ) {
		t_free ( str );
		err_show ( ERR_NOTE, _("Invalid or empty geometry type.") );
		return ( FALSE );
	}
	selection_geomtype = selection_get_geomtype ( token );
	if ( selection_geomtype == SELECTION_GEOM_INVALID ) {
		t_free ( str );
		err_show ( ERR_NOTE, _("Invalid or empty geometry type.") );
		return ( FALSE );
	}

	if ( selection_type != SELECTION_TYPE_ALL ) {
		/* 3: field name */
		token = strtok ( NULL, SELECTION_TOKEN_SEP );
		if ( token == NULL || strlen ( token ) < 1 ) {
			t_free ( str );
			err_show ( ERR_NOTE, _("Empty field name in selection.") );
			return ( FALSE );
		}
		if ( selection_is_valid_field ( token, selection_type, parser ) == FALSE ) {
			t_free ( str );
			err_show ( ERR_NOTE, _("Invalid field name or type in selection: '%s'"), token );
			return ( FALSE );
		}

		/* 4: selection expression (not much to check here) */
		token = strtok ( NULL, SELECTION_TOKEN_SEP );
		if ( token == NULL || strlen ( token ) < 1 ) {
			t_free ( str );
			err_show ( ERR_NOTE, _("Empty selection expression.") );
			return ( FALSE );
		}
		if ( selection_type == SELECTION_TYPE_REGEXP ) {
			int slre_err = selection_is_valid_regexp ( token );
			if ( slre_err < SLRE_NO_MATCH ) {
				err_show ( ERR_NOTE, _("Invalid regular expression in selection: '%s'"), token );
				if ( slre_err == SLRE_UNEXPECTED_QUANTIFIER ) {
					err_show ( ERR_NOTE, _("Problem: Unexpected quantifier."), token );
				}
				if ( slre_err == SLRE_UNBALANCED_BRACKETS ) {
					err_show ( ERR_NOTE, _("Problem: Unbalanced brackets."), token );
				}
				if ( slre_err == SLRE_INTERNAL_ERROR ) {
					err_show ( ERR_NOTE, _("Problem: Internal error."), token );
				}
				if ( slre_err == SLRE_INVALID_CHARACTER_SET ) {
					err_show ( ERR_NOTE, _("Problem: Invalid character set."), token );
				}
				if ( slre_err == SLRE_INVALID_METACHARACTER ) {
					err_show ( ERR_NOTE, _("Problem: Invalid meta character."), token );
				}
				if ( slre_err == SLRE_CAPS_ARRAY_TOO_SMALL ) {
					err_show ( ERR_NOTE, _("Problem: Caps array too small."), token );
				}
				if ( slre_err == SLRE_TOO_MANY_BRANCHES ) {
					err_show ( ERR_NOTE, _("Problem: Too many branches."), token );
				}
				if ( slre_err == SLRE_TOO_MANY_BRACKETS ) {
					err_show ( ERR_NOTE, _("Problem: Too many brackets."), token );
				}
				t_free ( str );
				return ( FALSE );
			}
		}

		if ( selection_type == SELECTION_TYPE_RANGE ) {
			if ( selection_is_valid_range ( token ) == FALSE ) {
				t_free ( str );
				err_show ( ERR_NOTE, _("Invalid range specification in selection.") );
				return ( FALSE );
			}
		}

	}

	/* All Ok! */
	t_free ( str );
	return ( TRUE );
}


/*
 * Checks validity of _only_ the syntax of one selection string
 * in the format:
 *
 * 	selection_type_name:geom_type_name:field_name:expression
 *
 * Use "selection_is_valid()" to check more tightly, e.g. for
 * existence of specified field.
 *
 * Returns "TRUE" if the selection _syntax_ is valid, "FALSE" otherwise.
 */
BOOLEAN selection_is_valid_syntax ( char *selection )
{
	if ( selection == NULL || strlen (selection) < 7 ) return ( FALSE );

	char *str = strdup (selection);
	int selection_type = SELECTION_TYPE_INVALID;
	int selection_geomtype = SELECTION_GEOM_INVALID;

	/* 1: selection type */
	char *token = strtok ( str, SELECTION_TOKEN_SEP );
	if ( token == NULL || strlen ( token ) < 2 ) {
		t_free ( str );
		return ( FALSE );
	}
	selection_type = selection_get_seltype ( token );
	if ( selection_type == SELECTION_TYPE_INVALID ) {
		return ( FALSE );
	}

	/* 2: geometry type */
	token = strtok ( NULL, SELECTION_TOKEN_SEP );
	if ( token == NULL || strlen ( token ) < 3 ) {
		t_free ( str );
		return ( FALSE );
	}
	selection_geomtype = selection_get_geomtype ( token );
	if ( selection_geomtype == SELECTION_GEOM_INVALID ) {
		t_free ( str );
		return ( FALSE );
	}

	if ( selection_type != SELECTION_TYPE_ALL ) {
		/* 3: field name given? */
		token = strtok ( NULL, SELECTION_TOKEN_SEP );
		if ( token == NULL || strlen ( token ) < 1 ) {
			t_free ( str );
			return ( FALSE );
		}

		/* 4: selection expression (not much to check here) */
		token = strtok ( NULL, SELECTION_TOKEN_SEP );
		if ( token == NULL || strlen ( token ) < 1 ) {
			t_free ( str );
			return ( FALSE );
		}
		if ( selection_type == SELECTION_TYPE_REGEXP ) {
			int slre_err = selection_is_valid_regexp ( token );
			if ( slre_err < SLRE_NO_MATCH ) {
				t_free ( str );
				return ( FALSE );
			}
		}
		if ( selection_type == SELECTION_TYPE_RANGE ) {
			if ( selection_is_valid_range ( token ) == FALSE ) {
				t_free ( str );
				return ( FALSE );
			}
		}
	}

	/* All Ok! */
	t_free ( str );
	return ( TRUE );
}


/*
 * Validate all given selection expressions.
 * This is just a syntax check. It does not test whether
 * the selection(s) would return an actual match.
 *
 * This function must be called _after_ all options have been
 * parsed and _after_ all field names and types have been assigned.
 *
 * Return values:
 *
 * TRUE:	All selection expressions OK.
 * FALSE:	One or more expressions invalid.
 *
 */
BOOLEAN selections_validate ( options *opt, parser_desc *parser )
{
	int i = 0;
	for ( i = 0; i < PRG_MAX_SELECTIONS; i++ ) {
		if ( opt->selection[i] != NULL ) {
			err_show (ERR_NOTE, _("\nValidating selection: '%s'"), opt->selection[i]);
			if ( selection_is_valid ( opt->selection[i], parser ) == FALSE ) {
				err_show ( ERR_EXIT, _("Invalid selection specification: '%s'"), opt->selection[i] );
				return ( FALSE );
			}
		}
	}
	return ( TRUE );
}


/*
 * Converts a string to a double and throws a fatal error if
 * there is a conversion problem.
 */
double selection_str_to_dbl ( char *str )
{
	BOOLEAN error = FALSE;
	BOOLEAN overflow = FALSE;

	double result = t_str_to_dbl ( str, 0, 0, &error, &overflow );
	if ( overflow == TRUE ) {
		err_show ( ERR_EXIT, _("Invalid selection: Overflow error in numeric value: '%s'."), str );
	} else {
		if ( error == TRUE ) {
			err_show ( ERR_EXIT, _("Invalid selection: Malformed numeric value: '%s'."), str );
		}
	}

	return ( result );
}


/*
 * Applies one selection with the format:
 *
 *	selection_type_name:geom_type_name:field_content:expression
 *
 * Returns "TRUE" if the value in "content" passes the expression "expr", "FALSE" otherwise.
 */
BOOLEAN selection_apply_one ( short seltype, BOOLEAN case_sensitive, short field_type, char *content, char *expr )
{
	if ( seltype == SELECTION_TYPE_INVALID ) return ( TRUE );

	BOOLEAN match = FALSE;

	/* EQUAL SELECTION */
	if ( seltype == SELECTION_TYPE_EQ ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 == val2 ) {
				match = TRUE;
			}
		}
	}

	/* NOT EQUAL SELECTION */
	if ( seltype == SELECTION_TYPE_NEQ ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) != 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) != 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 != val2 ) {
				match = TRUE;
			}
		}
	}

	/* LESS THAN SELECTION */
	if ( seltype == SELECTION_TYPE_LT ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) < 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) < 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 < val2 ) {
				match = TRUE;
			}
		}
	}

	/* GREATER THAN SELECTION */
	if ( seltype == SELECTION_TYPE_GT ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) > 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) > 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 > val2 ) {
				match = TRUE;
			}
		}
	}

	/* LESS THAN OR EQUAL SELECTION */
	if ( seltype == SELECTION_TYPE_LTE ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) < 0 || strcmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) < 0 || strcasecmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 <= val2 ) {
				match = TRUE;
			}
		}
	}

	/* GREATER THAN OR EQUAL SELECTION */
	if ( seltype == SELECTION_TYPE_GTE ) {
		if ( field_type == PARSER_FIELD_TYPE_TEXT ) {
			if ( case_sensitive == TRUE ) {
				if ( strcmp ( content, expr ) > 0 || strcmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			} else {
				if ( strcasecmp ( content, expr ) > 0 || strcasecmp ( content, expr ) == 0 ) {
					match = TRUE;
				}
			}
		}
		if ( field_type == PARSER_FIELD_TYPE_DOUBLE || field_type == PARSER_FIELD_TYPE_INT ) {
			double val1 = selection_str_to_dbl ( content );
			double val2 = selection_str_to_dbl ( expr );
			if ( val1 >= val2 ) {
				match = TRUE;
			}
		}
	}

	/* SUBSTRING SELECTION */
	if ( seltype == SELECTION_TYPE_SUB ) {
		if ( case_sensitive == TRUE ) {
			if ( strstr ( content, expr ) != NULL ) {
				match = TRUE;
			}
		} else {
			char *upper_content = t_str_to_upper ( content );
			char *upper_expr = t_str_to_upper ( expr );
			if ( strstr ( upper_content, upper_expr ) != 0 ) {
				match = TRUE;
				free ( upper_content );
				free ( upper_expr );
			}
		}
	}

	/* REGEXP SELECTION */
	if ( seltype == SELECTION_TYPE_REGEXP ) {
		int flag = 0;
		if ( case_sensitive == TRUE ) {
			flag = SLRE_IGNORE_CASE;
		}
		if ( slre_match ( expr, content, strlen(content), NULL, 0, flag ) > SLRE_NO_MATCH ) {
			match = TRUE;
		}
	}

	/* RANGE SELECTION */
	if ( seltype == SELECTION_TYPE_RANGE ) {
		double min = selection_get_range_min ( expr );
		double max = selection_get_range_max ( expr );
		double val = selection_str_to_dbl ( content );
		if ( val >= min && val <= max ) {
			match = TRUE;
		}
	}

	/* ALL SELECTION */
	if ( seltype == SELECTION_TYPE_ALL ) {
		match = TRUE;
	}

	return ( match );
}


/*
 * Sets all geometries that are not of type "geomtype" to
 * "not selected".
 */
void selection_set_non_match_geomtypes ( short geomtype, geom_store *gs )
{
	if ( gs == NULL || geomtype == SELECTION_GEOM_ALL ) {
		return;
	}

	int i;
	if ( geomtype != SELECTION_GEOM_POINT ) {
		for ( i = 0; i < gs->num_points; i ++ ) {
			gs->points[i].is_selected = FALSE;
		}
	}
	if ( geomtype != SELECTION_GEOM_RAW ) {
		for ( i = 0; i < gs->num_points_raw; i ++ ) {
			gs->points_raw[i].is_selected = FALSE;
		}
	}
	if ( geomtype != SELECTION_GEOM_LINE ) {
		for ( i = 0; i < gs->num_lines; i ++ ) {
			gs->lines[i].is_selected = FALSE;
		}
	}
	if ( geomtype != SELECTION_GEOM_POLY ) {
		for ( i = 0; i < gs->num_polygons; i ++ ) {
			gs->polygons[i].is_selected = FALSE;
		}
	}
}


/*
 * Sets the "is_selected" member for "geom", based on "match".
 * Switches objects in geometry store based on "geomtype".
 *
 * The logics in this function apply the modifiers "add", "sub" and "invert".
 * It returns "TRUE" if the match is still a match after applying the above,
 * "FALSE "otherwise.
 *
 */
BOOLEAN selection_set ( short geomtype, int idx, BOOLEAN is_match,
		BOOLEAN add, BOOLEAN sub, BOOLEAN invert, geom_store *gs )
{

	/* Invert match? */
	BOOLEAN match = is_match;
	if ( invert == TRUE ) {
		match = 1-match;
	}

	if ( match == TRUE ) {
		/* Add to selection? */
		if ( add == TRUE ) {
			if ( geomtype == SELECTION_GEOM_POINT ) gs->points[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_RAW ) gs->points_raw[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_LINE ) gs->lines[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_POLY) gs->polygons[idx].is_selected = TRUE;
		}
		/* Subtract from selection? */
		if ( sub == TRUE ) {
			if ( geomtype == SELECTION_GEOM_POINT ) gs->points[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_RAW ) gs->points_raw[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_LINE ) gs->lines[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_POLY) gs->polygons[idx].is_selected = FALSE;
		}
		/* Replace selection (1/2)? */
		if ( add == FALSE && sub == FALSE ) {
			if ( geomtype == SELECTION_GEOM_POINT ) gs->points[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_RAW ) gs->points_raw[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_LINE ) gs->lines[idx].is_selected = TRUE;
			if ( geomtype == SELECTION_GEOM_POLY) gs->polygons[idx].is_selected = TRUE;
		}
	} else {
		/* Replace selection (2/2)? */
		if ( add == FALSE && sub == FALSE ) {
			if ( geomtype == SELECTION_GEOM_POINT ) gs->points[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_RAW ) gs->points_raw[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_LINE ) gs->lines[idx].is_selected = FALSE;
			if ( geomtype == SELECTION_GEOM_POLY) gs->polygons[idx].is_selected = FALSE;
		}
	}

	return ( match );
}


/*
 * DEBUG: Print a selection command and its details to the screen.
 */
void selection_dump ( char *selection, parser_desc *parser )
{
	fprintf ( stderr, "\n*** SELECTION ***\n" );
	fprintf ( stderr, "'%s'\n", selection );
	BOOLEAN valid = selection_is_valid ( selection, parser );
	fprintf ( stderr, "Is valid: %i\n", valid );
	if ( valid ) {
		char *str = strdup ( selection );
		char *seltype = strtok ( str, SELECTION_TOKEN_SEP );
		fprintf ( stderr, "Selection type: '%s'\n", SELECTION_TYPE_NAME [( selection_get_seltype ( seltype ) )] );
		fprintf ( stderr, "Selection mode: ");
		if ( selection_is_mod_add ( seltype ) ) {
			fprintf ( stderr, "Add.\n");
		}
		if ( selection_is_mod_sub ( seltype ) ) {
			fprintf ( stderr, "Sub.\n");
		}
		if ( !selection_is_mod_add ( seltype ) &&  !selection_is_mod_sub ( seltype ) ) {
			fprintf ( stderr, "New.\n");
		}
		fprintf ( stderr, "Invert: %i\n", selection_is_mod_inv ( seltype ) );
		char *geomtype = strtok ( NULL, SELECTION_TOKEN_SEP );
		fprintf ( stderr, "Geometry type: '%s'\n", SELECTION_GEOM_TYPE_NAME [( selection_get_geomtype ( geomtype ) )] );
		char *field = strtok ( NULL, SELECTION_TOKEN_SEP );
		if ( ( selection_get_seltype ( seltype ) ) != SELECTION_TYPE_ALL ) {
			fprintf ( stderr, "Field name: '%s'\n", parser->fields[selection_get_field_idx ( field, parser )]->name );
			char *expr = strtok ( NULL, SELECTION_TOKEN_SEP );
			fprintf ( stderr, "Expression: '%s'\n", expr );
			fprintf ( stderr, "Expr. valid: '%i'\n", selection_is_valid_regexp ( expr ) );
		} else {
			fprintf ( stderr, "Field name: n.a.\n" );
			fprintf ( stderr, "Expression: n.a.\n" );
			fprintf ( stderr, "Expr. valid: n.a.\n" );
		}
		t_free ( str );
	}
}


/*
 * Selects geometries in geometry store "gs", using all selections
 * defined in "opt" (selection chain).
 *
 * All selections must be in the format:
 *
 *	selection_type_name:geom_type_name:field_content:expression
 *
 * All selections _must_ have been validated and geometries must
 * have been built _before_ calling this function!
 *
 */
void selections_apply_all ( options *opt, parser_desc *parser, geom_store *gs )
{
	if ( gs == NULL || opt == NULL || parser == NULL ) return;

	/* Apply all selections. */
	int i = 0;
	for ( i = 0; i < PRG_MAX_SELECTIONS; i ++ ) {
		/* Walk through points, lines, polygons in geom store. */
		if ( opt->selection[i] != NULL ) {
			/* DEBUG */
			/* selection_dump ( opt->selection[i], parser ); */
			err_show (ERR_NOTE, _("\nApplying selection: '%s'"), opt->selection[i]);
			char *selection = strdup  ( opt->selection[i] );
			/* Get selection type: */
			char *token = strtok ( selection, SELECTION_TOKEN_SEP );
			short seltype = selection_get_seltype ( token );
			/* Get selection modifiers: */
			BOOLEAN case_sensitive = selection_is_case_sensitive ( token );
			BOOLEAN add = selection_is_mod_add ( token );
			BOOLEAN sub = selection_is_mod_sub ( token );
			BOOLEAN inv = selection_is_mod_inv ( token );
			/* Get geometry type: */
			token = strtok ( NULL, SELECTION_TOKEN_SEP );
			short geomtype = selection_get_geomtype ( token );
			int field_idx = 0;
			char *expr = NULL;
			if ( seltype != SELECTION_TYPE_ALL ) {
				/* Get field index: */
				token = strtok ( NULL, SELECTION_TOKEN_SEP );
				field_idx = selection_get_field_idx ( token, parser );
				/* Get selection expression: */
				expr = strtok ( NULL, SELECTION_TOKEN_SEP );
			} else {
				field_idx = 0; /* There is always a field '0' (GEOM_ID)! */
			}
			/* Apply selection with attribute field content. */
			if ( field_idx >= 0 ) {
				parser_field *pf = parser->fields[field_idx];
				/* MATCH POINTS */
				int matched = 0;
				int j;
				for ( j = 0; j < gs->num_points; j ++ ) {
					/* Matches attribute? */
					BOOLEAN is_match = selection_apply_one ( seltype, case_sensitive, pf->type, gs->points[j].atts[field_idx], expr );
					/* Matches geometry type? */
					is_match = is_match * ( geomtype == SELECTION_GEOM_POINT || geomtype == SELECTION_GEOM_ALL );
					is_match = selection_set ( SELECTION_GEOM_POINT, j, is_match, add, sub, inv, gs );
					matched += is_match;
				}
				err_show (ERR_NOTE, _("\tMatched %i point(s)."), matched);
				/* MATCH RAW POINTS/VERTICES */
				if ( opt->dump_raw == TRUE ) {
					matched = 0;
					for ( j = 0; j < gs->num_points_raw; j ++ ) {
						/* Matches attribute? */
						BOOLEAN is_match = selection_apply_one ( seltype, case_sensitive, pf->type, gs->points_raw[j].atts[field_idx], expr );
						/* Matches geometry type? */
						is_match = is_match * ( geomtype == SELECTION_GEOM_RAW || geomtype == SELECTION_GEOM_ALL );
						is_match = selection_set ( SELECTION_GEOM_RAW, j, is_match, add, sub, inv, gs );
						matched += is_match;
					}
					err_show (ERR_NOTE, _("\tMatched %i raw point(s)."), matched);
				}
				/* MATCH LINES */
				matched = 0;
				for ( j = 0; j < gs->num_lines; j ++ ) {
					/* Matches attribute? */
					BOOLEAN is_match = selection_apply_one ( seltype, case_sensitive, pf->type, gs->lines[j].atts[field_idx], expr );
					/* Matches geometry type? */
					is_match = is_match * ( geomtype == SELECTION_GEOM_LINE || geomtype == SELECTION_GEOM_ALL );
					is_match = selection_set ( SELECTION_GEOM_LINE, j, is_match, add, sub, inv, gs );
					matched += is_match;
				}
				err_show (ERR_NOTE, _("\tMatched %i line(s)."), matched);
				/* MATCH POLYGONS */
				matched = 0;
				for ( j = 0; j < gs->num_polygons; j ++ ) {
					/* Matches attribute? */
					BOOLEAN is_match = selection_apply_one ( seltype, case_sensitive, pf->type, gs->polygons[j].atts[field_idx], expr );
					/* Matches geometry type? */
					is_match = is_match * ( geomtype == SELECTION_GEOM_POLY || geomtype == SELECTION_GEOM_ALL );
					is_match = selection_set ( SELECTION_GEOM_POLY, j, is_match, add, sub, inv, gs );
					matched += is_match;
				}
				err_show (ERR_NOTE, _("\tMatched %i polygon(s)."), matched);
			}
			if ( selection != NULL ) {
				free ( selection );
			}
		}
	}
}


/*
 * Returns number of selection expressions currently registered.
 */
unsigned int selections_get_count ( options *opt )
{
	int count = 0;
	int i = 0;

	for ( i=0; i < PRG_MAX_SELECTIONS; i++ ) {
		if ( opt->selection[i] != NULL ) {
			count ++;
		}
	}

	return ( count );
}


/*
 * Returns total count of selected geometries in geometry store "gs".
 * Geometry type must be specified by user. It can be specified as
 * "GEOM_TYPE_ALL" to include all geometry types.
 */
int selections_get_num_selected ( short geom_type, geom_store *gs )
{	
	int count = 0;

	if ( gs == NULL || geom_type == GEOM_TYPE_NONE ) return (0);

	int i = 0;
	if ( geom_type == GEOM_TYPE_POINT || geom_type == GEOM_TYPE_ALL ) {
		for ( i = 0; i < gs->num_points; i ++ ) {
			count += gs->points[i].is_selected;
		}
	}
	if ( geom_type == GEOM_TYPE_POINT_RAW || geom_type == GEOM_TYPE_ALL ) {
		for ( i = 0; i < gs->num_points_raw; i ++ ) {
			count += gs->points_raw[i].is_selected;
		}
	}
	if ( geom_type == GEOM_TYPE_LINE || geom_type == GEOM_TYPE_ALL ) {
		for ( i = 0; i < gs->num_lines; i ++ ) {
			count += gs->lines[i].is_selected;
		}
	}
	if ( geom_type == GEOM_TYPE_POLY || geom_type == GEOM_TYPE_ALL ) {
		for ( i = 0; i < gs->num_polygons; i ++ ) {
			count += gs->polygons[i].is_selected;
		}
	}

	return ( count );
}
