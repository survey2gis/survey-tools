/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	gui_field.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	See gui_fields.c.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

#ifdef GUI

#ifndef GUI_FIELDS_H
#define GUI_FIELDS_H

#include "global.h"

#include <gtk/gtk.h>

/* conversion modes */
#define GUI_FIELD_CONVERT_TO_NONE		0
#define GUI_FIELD_CONVERT_TO_UPPER		1
#define GUI_FIELD_CONVERT_TO_LOWER		2

/* basic field types */
#define GUI_FIELD_TYPE_NONE				0
#define GUI_FIELD_TYPE_STRING			1
#define GUI_FIELD_TYPE_INTEGER			2
#define GUI_FIELD_TYPE_DOUBLE			3
#define GUI_FIELD_TYPE_BOOLEAN			4
#define GUI_FIELD_TYPE_FILE_IN			5
#define GUI_FIELD_TYPE_FILE_OUT			6
#define GUI_FIELD_TYPE_DIR_IN			7
#define GUI_FIELD_TYPE_DIR_OUT			8
#define GUI_FIELD_TYPE_BASENAME			9
#define GUI_FIELD_TYPE_SELECTIONS		10

/* responses for widgets */
#define GUI_FIELD_RESPONSE_ADD			1
#define GUI_FIELD_RESPONSE_DEL			2
#define GUI_FIELD_RESPONSE_EDIT			3

/* list of allowed characters for specific types of string fields */
#define GUI_FIELD_STR_ALLOW_FIELD_NAME	"0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* list of maximum lengths for specific types of string fields */
#define GUI_FIELD_STR_LEN_FIELD_NAME	10

typedef struct gui_field_limits gui_field_limits;

/*
 * This structure is an array of restrictions that can
 * be set to limit user input into text entry widgets.
 * It can be set and passed to gui_field_create_string.
 * However, consider letting the user enter anything and
 * parsing it for correctness afterwards.
 */
struct gui_field_limits
{
	int length; /* maximum length of one input line or -1 */
	char* allowed; /* string of allowed characters */
	BOOLEAN strict_numeric; /* TRUE for restrictive numeric input */
	BOOLEAN decimal_sep; /* allow decimal separator (for numeric input) */
	BOOLEAN decimal_grp; /* allow decimal grouping symbol (for numeric input) */
	BOOLEAN sign; /* allow first char to be + or - (for numeric input) */
	BOOLEAN convert_to; /* convert all input to upper or upper case */
};


typedef struct gui_field gui_field;

/*
 * A simple structure that contains all definitions
 * for a field that can get added to the main GUI form.
 * This structure is complete, in that it covers all types
 * of possible input fields. However, only part of the
 * data will be in actual use, depending on the field's type.
 */
struct gui_field
{
	char *key; /* unique one-word, all lower case option name */
	char *name; /* verbose name, e.g. used for labeling widgets */
	char *description; /* description, used for tooltip */
	char *section; /* used to group fields into tabs */
	int type; /* see GUI_FIELD_TYPE_* above */
	BOOLEAN required; /* TRUE if this field cannot be left empty by the user */
	BOOLEAN multiple; /* TRUE if this field stores a list of values */
	BOOLEAN lookup; /* TRUE if value is to be selected from a look-up list */
	char **content; /* field content, stored as a list of strings
					   with NULL terminator */
	int length; /* number of values stored in **content */
	char *default_value; /* default value (always a string) */

	/* limitations for acceptable values */
	char **answers; /* a list of acceptable values for string input, NULL terminated */
	/* Range specifications for numeric input: there must be num_ranges items in the
	 * arrays range_min and range_max. One pair of range_min[i] and range_max[i]
	 * define the i-th acceptable value range. */
	int num_ranges;
	double *range_min;
	double *range_max;

	/* input restrictions */
	gui_field_limits *limits;

	/* GUI widgets */
	GtkWidget *l_widget; /* GTK label widget */
	GtkWidget *al_widget; /* GTK aligned label widget */
	GtkWidget *i_widget; /* GTK input field widget */
	GtkWidget *combo_box; /* GTK combo box for a list of preset values */
	GtkEntryBuffer *buffer; /* GTK widget for entry buffer */
	GtkWidget *checkbox; /* GTK checkbox for yes/no type fields */
	GtkWidget *file_button; /* GTK file chooser button */
	GtkWidget *browse_button; /* similar to file_button, but smaller */
	GtkWidget *browse_hbox; /* combine and input widget and a file browser button */
	GtkWidget *browse_button_m; /* a button for multiple entries */
	GtkListStore *list; /* stores this field's (list of) value(s) */
	GtkWindow *parent; /* form (GtkWindow) to which this field is attached */
	BOOLEAN empty; /* TRUE if there is nothing in this field */
};


/* functions for creating fields to go on a GUI form */
gui_field *gui_field_create_string ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value,
		int length, const char *allowed,
		int convert_to, const char **choices );
gui_field *gui_field_create_integer ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value, BOOLEAN allow_negative );
gui_field *gui_field_create_double ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value, BOOLEAN allow_negative );
gui_field *gui_field_create_boolean ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN on );
gui_field *gui_field_create_file_in ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value );
gui_field *gui_field_create_file_in_multi ( const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char **default_values );
gui_field *gui_field_create_edit_selections ( const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char **default_values );
gui_field *gui_field_create_file_out ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value );
gui_field *gui_field_create_dir_out ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value );
void gui_field_destroy ( gui_field *gfield );

/* Handlers for gui field widgets that must be connected after
 * creating the actual field and adding it to a form.
 */
void gui_field_event_file_browse_save (GtkEntry *button, gpointer data);
void gui_field_event_multi_file_browse (GtkEntry *button, gpointer data);
void gui_field_event_edit_selections (GtkEntry *button, gpointer data);


#endif /* GUI_FIELDS_H */

#endif /* #ifdef GUI */
