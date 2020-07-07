/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	gui_field.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	These functions produce GTK widgets that can act
 * 				as input fields on a GUI form.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "config.h"

#ifdef GUI

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "global.h"
#include "gui_field.h"
#include "gui_form.h"
#include "i18n.h"
#include "selections.h"
#include "tools.h"

#include <gtk/gtk.h>


/*
 * GTK SIGNAL
 */

/*
 * Controls text entry and allows limiting entry length, set of accepted characters,
 * as well as allows to enforce upper or lower case on all entered characters.
 */
void gui_field_event_insert_text (GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable = 0;
	int i, count = 0;
	gchar *result = 0;
	gui_field_limits* ptr;

	editable = GTK_EDITABLE(entry);	
	result = g_new (gchar, length+1);
	int len = (int) strlen(gtk_editable_get_chars (editable,0,1));
	if ( len < 1 ) {
		result[0] = '\0';
	}
	ptr = data;
	for (i=0; i < length; i++) {
		BOOLEAN check_convert = FALSE;
		if ( ptr->allowed == NULL ) {
			result[count++] = text[i];
			check_convert = TRUE;
		} else {
			/* check if char is in set of allowed chars */
			if ( strchr ( ptr->allowed, text[i] ) ) {
				if ( ptr->strict_numeric == TRUE ) {
					if ( strchr ( gtk_entry_get_text (entry), '+' ) != NULL ||
							strchr ( gtk_entry_get_text (entry), '-' ) != NULL ) {
						if ( *position != 0 )
							result[count++] = text[i];
					} else {
						result[count++] = text[i];
					}
				} else {
					result[count++] = text[i];
					check_convert = TRUE;
				}
			} else {
				/* check if we have an exception for a numeric value */
				if ( ptr->decimal_sep == TRUE &&
						strchr ( gtk_entry_get_text (entry), *I18N_DECIMAL_POINT ) == NULL &&
						text[i] == *I18N_DECIMAL_POINT ) {
					/* allow insertion of decimal point */
					result[count++] = text[i];
				}
				if ( ptr->decimal_grp == TRUE && text[i] == *I18N_THOUSANDS_SEP ) {
					/* allow insertion of number grouping char */
					result[count++] = text[i];
				}
				if ( ptr->sign == TRUE && *position==0 && ( text[i] == '+' || text[i] == '-' ) &&
						strchr ( gtk_entry_get_text (entry), '+' ) == NULL &&
						strchr ( gtk_entry_get_text (entry), '-' ) == NULL
				) {
					/* allow insertion of sign here */
					result[count++] = text[i];
				}
			}
		}
		if ( check_convert == TRUE ) {
			/* if we got a valid char, we may also have to convert it */
			if ( ptr->convert_to == GUI_FIELD_CONVERT_TO_UPPER ) {
				result[count-1] = toupper(result[count-1]);
			}
			if ( ptr->convert_to == GUI_FIELD_CONVERT_TO_LOWER )
				result[count-1] = tolower(result[count-1]);
		}
	}

	if (count > 0) {
		g_signal_handlers_block_by_func (G_OBJECT (editable), G_CALLBACK (gui_field_event_insert_text), data);			
		gtk_editable_insert_text (editable, result, count, position);
		g_signal_handlers_unblock_by_func (G_OBJECT (editable), G_CALLBACK (gui_field_event_insert_text), data);
	}
	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert_text");

	g_free (result);
}


/*
 * Controls text entry that represents a field name.
 * Basically, this limits text entry
 */
void gui_field_event_insert_field_name (GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable = GTK_EDITABLE(entry);
	int i, count=0;
	gchar *result = g_new (gchar, length);

	for (i=0; i < length; i++) {
		/* check if char is in set of allowed chars */
		if ( strchr ( GUI_FIELD_STR_ALLOW_FIELD_NAME, text[i] ) ) {
			result[count++] = text[i];
		}
		/* convert to lower case */
		result[count-1] = tolower(result[count-1]);
	}

	if (count > 0) {
		g_signal_handlers_block_by_func (G_OBJECT (editable), G_CALLBACK (gui_field_event_insert_field_name), data);
		gtk_editable_insert_text (editable, result, count, position);
		g_signal_handlers_unblock_by_func (G_OBJECT (editable), G_CALLBACK (gui_field_event_insert_field_name), data);
	}
	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert_text");

	g_free (result);
}

/*
 * Pops up a file browser letting the user pick a file to save.
 * Must be connected AFTER the field was added to a GUI form.
 * "data" must point to a valid GUI field.
 */
void gui_field_event_file_browse_save (GtkEntry *button, gpointer data)
{
	GtkWidget *browser;
	gui_field *ptr;


	ptr = (gui_field*) data;
	/* will not run if there is no parent window assigned */
	if (ptr->parent == NULL)
		return;

	browser = gtk_file_chooser_dialog_new(_("Select file"), ptr->parent,
			GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	if (gtk_dialog_run(GTK_DIALOG(browser)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(browser));
		/* save file name in widget */
		gtk_entry_buffer_set_text (ptr->buffer, filename, -1);
		g_free(filename);
	}
	gtk_widget_destroy(browser);

}

/*
 * Pops up a simple list that allows the user add/remove files
 * to/from a list. The file browser allows multiple file selections.
 * Must be connected AFTER the field was added to a GUI form.
 * "data" must point to a valid GUI field.
 */
void gui_field_event_multi_file_browse (GtkEntry *button, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *browser;
	GtkWidget *content_area;
	GtkWidget *scrolled_window;
	GtkWidget *list_view;
	GList *rows = NULL;
	GtkTreeRowReference **refs = NULL;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	gui_field *ptr;
	GtkTreeIter iter;
	BOOLEAN valid;
	BOOLEAN found;
	int answer;
	int i,j;
	char buf[PRG_MAX_STR_LEN];


	ptr = (gui_field*) data;
	/* will not run if there is no parent window assigned */
	if (ptr->parent == NULL)
		return;

	/* build tree view widget */
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ptr->list));
	renderer = gtk_cell_renderer_text_new ();
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(ptr->list), &iter);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(list_view), -1,
			_("List of input files:"), renderer, "text", 0, NULL);
	while ( valid ) {
		char *str_data;
		gtk_tree_model_get (GTK_TREE_MODEL(ptr->list), &iter, 0, &str_data, -1);
		/* fprintf (stderr, "GOT: %s\n", str_data); */
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(ptr->list), &iter);
	}
	gtk_widget_set_size_request (list_view, 500, 400);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW(list_view), TRUE );
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW(list_view), FALSE );

	/* attach multiple selection object to tree */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(list_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* show dialog for editing file selection */
	dialog = gtk_dialog_new_with_buttons(_("File selection"), ptr->parent,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_ADD, GUI_FIELD_RESPONSE_ADD,
			GTK_STOCK_REMOVE, GUI_FIELD_RESPONSE_DEL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
			NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
	gtk_container_add(GTK_CONTAINER(scrolled_window), list_view);

	gtk_widget_show_all(dialog);

	answer = 0;
	while (answer != GTK_RESPONSE_ACCEPT) {
		answer = gtk_dialog_run(GTK_DIALOG(dialog));

		/* add another file? */
		if (answer == GUI_FIELD_RESPONSE_ADD) {
			browser = gtk_file_chooser_dialog_new(_("Select file"),
					ptr->parent, GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD,
					GTK_RESPONSE_ACCEPT, NULL);
			gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER(browser), TRUE );
			if (gtk_dialog_run(GTK_DIALOG(browser)) == GTK_RESPONSE_ACCEPT) {
				GSList *filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(browser));
				while ( filenames )
				{
					gchar *filename = (gchar*) filenames->data;
					/* check if this file is not already in the list */
					found = FALSE;
					valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(ptr->list), &iter);
					while ( valid ) {
						char *str_data;
						gtk_tree_model_get (GTK_TREE_MODEL(ptr->list), &iter, 0, &str_data, -1);
						if ( !strcmp (filename, str_data ) )
							found = TRUE;
						valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(ptr->list), &iter);
					}
					/* save file name in list? */
					if ( found == FALSE ) {
						gtk_list_store_append(ptr->list, &iter);
						gtk_list_store_set(ptr->list, &iter, 0, filename, -1);
					}
					g_free(filenames->data);
					filenames = filenames->next;
				}
				g_slist_free (filenames);
			}
			gtk_widget_destroy(browser);
		}

		/* delete selected file(s) from list? */
		if (answer == GUI_FIELD_RESPONSE_DEL) {
			if ( gtk_tree_selection_count_selected_rows (selection) > 0 ) {
				model = GTK_TREE_MODEL(ptr->list);
				rows = gtk_tree_selection_get_selected_rows (selection, &model);
				refs =  malloc ( sizeof (GtkTreeRowReference*) * g_list_length (rows));
				for ( j = 0; j < g_list_length (rows); j ++ ) {
					refs[j] = gtk_tree_row_reference_new ( model, (GtkTreePath*) g_list_nth_data ( rows , j ) );
				}
				for ( j = 0; j < g_list_length (rows); j ++ ) {
					path = gtk_tree_row_reference_get_path (refs[j]);
					if ( gtk_tree_model_get_iter_from_string ( model, &iter, gtk_tree_path_to_string (path) ) == TRUE ) {
						gtk_list_store_remove (ptr->list,&iter);
					}
				}
				g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
				g_list_free (rows);
				g_free (refs);
			}
		}

		/* OK? */
		if (answer == GTK_RESPONSE_ACCEPT) {
			gtk_widget_destroy(dialog);
			break;
		}

		/* Aborted? */
		if (answer == GTK_RESPONSE_DELETE_EVENT) {
			gtk_widget_destroy(dialog);
			break;
		}
	}

	/* check how many files we have in the list and update button label */
	i = 0;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ptr->list), &iter);
	while (valid) {
		i ++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ptr->list), &iter);
	}

	snprintf (buf,PRG_MAX_STR_LEN,_("Files: %i"), i);
	gtk_button_set_label (GTK_BUTTON (ptr->browse_button_m),buf);

	/* clean up */
	/* g_object_unref (selection); */
}


/*
 * Checks syntax of selection command string "cmd" and displays
 * an error message "msg" if appropriate. Both parameters
 * can safely be passed as NULL.
 *
 * Returns:
 * 			TRUE = valid syntax
 * 			FALSE= syntax error
 */
BOOLEAN gui_field_is_valid_selection_command ( char* cmd, char *msg, GtkWindow *parent )
{
	BOOLEAN valid = FALSE;

	if ( cmd != NULL ) {
		if ( selection_is_valid_syntax( cmd ) == FALSE ) {
			char *err_msg = NULL;
			if ( msg == NULL ) {
				err_msg = strdup (_("Invalid selection command syntax."));
			} else {
				err_msg = strdup ( msg );
			}
			GtkWidget *dialog = gtk_message_dialog_new (parent,
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					(gchar*)err_msg);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			t_free ( err_msg );
		} else {
			valid = TRUE;
		}
	}

	return ( valid );
}

/*
 * Shows a dialog for composing or editing a selection command.
 * If "command" is passed as NULL, then a "New selection command"
 * dialog with blank/default value widgets is presented.
 *
 * Otherwise "command" is assumed to be a selection command string
 * with the syntax described in "selections.c", the widgets
 * will be pre-filled with the individual command components and
 * the dialog will be presented as an "Edit selection command" window.
 * If "command" has an invalid syntax, then all widget contents
 * will be reset to blank/default values.
 *
 * In both cases, the return value is either a string with a
 * new selection command string in correct syntax or NULL (if cancelled).
 */
char *gui_field_dialog_edit_selection_command ( char *selection_command, GtkWindow *parent )
{
	GtkWidget *dialog;
	GtkWidget *dlg_content_area;
	GtkWidget *dlg_field_box;
	GtkWidget *dlg_field_label_box;
	GtkWidget *dlg_field_entry_box;
	GtkWidget *dlg_flag_box;
	GtkWidget *dlg_flag_label_box;
	GtkWidget *dlg_flag_check_box;
	GtkWidget *dlg_selection_type;
	GtkWidget *dlg_geom_type;
	GtkWidget *dlg_field;
	GtkWidget *dlg_expression;
	GtkWidget *dlg_mode;
	GtkWidget *dlg_invert;
	char *title = NULL;
	char *str = NULL;
	char *result = NULL;
	char *field = NULL;
	char *expression = NULL;
	int selection_type = 0;
	int selection_mode = 0; /* replace */
	BOOLEAN invert = FALSE;
	int geom_type = SELECTION_GEOM_ALL;


	/* Valid command syntax? */
	if ( gui_field_is_valid_selection_command
			( selection_command,
					_("Invalid selection command syntax. Will reset to defaults."),
					parent ) == TRUE ) {
		str = strdup ( selection_command );
	}

	/* Present "New" or "Edit" dialog? */
	if ( str == NULL ) {
		title = strdup (_("New selection command"));
	} else {
		title = strdup (_("Edit selection command"));
		/* Decompose selection command: since we have already checked for valid
		 * syntax, this is easy */
		char *token = strtok ( str, SELECTION_TOKEN_SEP ); /* selection type & mode */
		selection_type = selection_get_seltype ( token );
		/* selection mode? */
		if ( selection_is_mod_add ( token ) ) {
			selection_mode = 1;
		}
		if ( selection_is_mod_sub ( token ) ) {
			selection_mode = 2;
		}
		invert = selection_is_mod_inv ( token );
		token = strtok ( NULL, SELECTION_TOKEN_SEP ); /* geometry type */
		geom_type = selection_get_geomtype ( token );
		if ( selection_type != SELECTION_TYPE_ALL ) {
			token = strtok ( NULL, SELECTION_TOKEN_SEP ); /* field name */
			field = strdup ( token );
			expression = strtok ( NULL, SELECTION_TOKEN_SEP ); /* expression */
		}
	}

	/* create main dialog */
	/* TODO: Default size/resize behavior */
	dialog = gtk_dialog_new_with_buttons( title, parent,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	dlg_content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	/* space for fields with input widgets */
	dlg_field_box = gtk_hbox_new ( FALSE, GUI_PAD_MED );
	gtk_container_set_border_width ( GTK_CONTAINER(dlg_field_box), GUI_PAD_MAX );

	/* spaces for labels and input widgets */
	dlg_field_label_box = gtk_vbox_new ( FALSE, GUI_PAD_MED );
	dlg_field_entry_box = gtk_vbox_new ( FALSE, GUI_PAD_MED );
	gtk_box_set_spacing (GTK_BOX (dlg_field_entry_box), GUI_PAD_MAX );
	gtk_container_add (GTK_CONTAINER (dlg_field_box), dlg_field_label_box);
	gtk_container_add (GTK_CONTAINER (dlg_field_box), dlg_field_entry_box);
	gtk_box_set_child_packing ( GTK_BOX (dlg_field_box), dlg_field_label_box,
			FALSE, FALSE, GUI_PAD_MED, GTK_PACK_START );
	gtk_container_add (GTK_CONTAINER (dlg_content_area), dlg_field_box);

	/* space for flags with checkboxes */
	dlg_flag_box = gtk_hbox_new ( FALSE, GUI_PAD_MED );
	gtk_container_set_border_width ( GTK_CONTAINER(dlg_flag_box), GUI_PAD_MAX );
	/* spaces for flag labels and checkboxes */
	dlg_flag_label_box = gtk_vbox_new ( FALSE, GUI_PAD_MED );
	dlg_flag_check_box = gtk_vbox_new ( FALSE, GUI_PAD_MED );
	gtk_box_set_spacing (GTK_BOX (dlg_flag_check_box), GUI_PAD_MAX );
	gtk_container_add (GTK_CONTAINER (dlg_flag_box), dlg_flag_label_box);
	gtk_container_add (GTK_CONTAINER (dlg_flag_box), dlg_flag_check_box);
	gtk_box_set_child_packing ( GTK_BOX (dlg_flag_box), dlg_flag_check_box,
			FALSE, FALSE, GUI_PAD_MED, GTK_PACK_START );
	gtk_container_add (GTK_CONTAINER (dlg_content_area), dlg_flag_box);

	/* combo box: "Selection type" */
	{
		dlg_selection_type = gtk_combo_box_new_text();
		int i = 0;
		for ( i = 0; i < NUM_SELECTION_TYPES; i ++ ) {
			gtk_combo_box_append_text ( GTK_COMBO_BOX (dlg_selection_type), SELECTION_TYPE_NAME_FULL[i] );
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (dlg_selection_type), selection_type);
		GtkWidget *label = gtk_label_new(_("Selection type:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_field_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_field_entry_box), dlg_selection_type);
	}
	/* combo box: "Geometry type" */
	{
		dlg_geom_type = gtk_combo_box_new_text();
		int i = 0;
		for ( i = 0; i < NUM_SELECTION_GEOMS; i ++ ) {
			gtk_combo_box_append_text ( GTK_COMBO_BOX (dlg_geom_type), SELECTION_GEOM_TYPE_NAME_FULL[i] );
		}
		gtk_combo_box_set_active ( GTK_COMBO_BOX (dlg_geom_type), geom_type);
		GtkWidget *label = gtk_label_new(_("Apply to:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_field_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_field_entry_box), dlg_geom_type);
	}
	/* text field: "Field name" */
	{
		dlg_field = gtk_entry_new();
		if ( field != NULL ) {
			gtk_entry_set_text (GTK_ENTRY(dlg_field),(gchar*)field);
		}
		/* limit text entry length */
		gtk_entry_buffer_set_max_length ( GTK_ENTRY_BUFFER ( gtk_entry_get_buffer( GTK_ENTRY ( dlg_field ) ) ),
				GUI_FIELD_STR_LEN_FIELD_NAME );
		/* connect handler for input restrictions */
		g_signal_connect(G_OBJECT(dlg_field), "insert_text", G_CALLBACK(gui_field_event_insert_field_name), (gpointer) NULL);
		/* add label and build layout */
		GtkWidget *label = gtk_label_new(_("Field:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_field_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_field_entry_box), dlg_field);
	}
	/* text field: "Expression" */
	{
		dlg_expression = gtk_entry_new();
		if ( expression != NULL ) {
			gtk_entry_set_text (GTK_ENTRY(dlg_expression),(gchar*)expression);
		}
		GtkWidget *label = gtk_label_new(_("Expression:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_field_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_field_entry_box), dlg_expression);
	}
	/* combo box: "Mode" */
	{
		dlg_mode = gtk_combo_box_new_text();
		gtk_combo_box_append_text ( GTK_COMBO_BOX (dlg_mode), SELECTION_MOD_REPLACE_NAME );
		gtk_combo_box_append_text ( GTK_COMBO_BOX (dlg_mode), SELECTION_MOD_ADD_NAME );
		gtk_combo_box_append_text ( GTK_COMBO_BOX (dlg_mode), SELECTION_MOD_SUB_NAME );
		gtk_combo_box_set_active (GTK_COMBO_BOX (dlg_mode), selection_mode);
		GtkWidget *label = gtk_label_new(_("Mode:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_field_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_field_entry_box), dlg_mode);
	}
	/* check box: "Invert" */
	{
		dlg_invert = gtk_check_button_new();
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON (dlg_invert), invert );
		GtkWidget *label = gtk_label_new(_("Invert:"));
		GtkWidget *align = gtk_alignment_new ( 1, 0.5, 0, 0 );
		gtk_container_add(GTK_CONTAINER(align), label);
		gtk_container_add(GTK_CONTAINER(dlg_flag_label_box), align);
		gtk_container_add(GTK_CONTAINER(dlg_flag_check_box), dlg_invert);
	}

	/* run dialog */
	gtk_widget_show_all(dialog);
	BOOLEAN valid = FALSE;
	while ( valid == FALSE ) {
		int answer = gtk_dialog_run(GTK_DIALOG(dialog));
		if ( answer == GTK_RESPONSE_OK ) {
			/* build expression command from widget contents */
			int len = strlen (SELECTION_TOKEN_SEP);
			int idx = 0;
			invert = (BOOLEAN) gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (dlg_invert) );
			selection_type = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_selection_type ) );
			selection_mode = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_mode ) );
			if ( gtk_entry_get_text_length (GTK_ENTRY(dlg_field)) > 0 ) {
				field = (char*) gtk_entry_get_text ( GTK_ENTRY ( dlg_field ));
			} else {
				field = NULL;
			}
			if ( gtk_entry_get_text_length (GTK_ENTRY(dlg_expression)) > 0 ) {
				expression = (char*) gtk_entry_get_text ( GTK_ENTRY ( dlg_expression ));
			} else {
				expression = NULL;
			}
			if ( selection_mode == 1 ) {
				len += strlen (SELECTION_MOD_ADD_STR);
			}
			if ( selection_mode == 2 ) {
				len += strlen (SELECTION_MOD_SUB_STR);
			}
			if ( invert == TRUE ) {
				len += strlen (SELECTION_MOD_INV_STR);
			}
			idx = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_selection_type ) );
			len += strlen ( SELECTION_TYPE_NAME[idx] );
			idx = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_geom_type ) );
			len += strlen ( SELECTION_GEOM_TYPE_NAME[idx] );
			if ( selection_type != SELECTION_TYPE_ALL ) {
				if ( field != NULL && expression != NULL ) {
					len += strlen ( SELECTION_TOKEN_SEP );
					len += strlen (t_str_pack ( field ) );
					len += strlen ( SELECTION_TOKEN_SEP );
					len += strlen (t_str_pack ( expression ) );
				}
			}
			len ++; /* '\0' string terminator */
			result = malloc ( len * sizeof(char) );
			result[0] =  ('\0');
			if ( invert == TRUE ) {
				strcat ( result, SELECTION_MOD_INV_STR );
			}
			idx = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_selection_type ) );
			strcat ( result, SELECTION_TYPE_NAME[idx] );
			if ( selection_mode == 1 ) {
				strcat ( result, SELECTION_MOD_ADD_STR );
			}
			if ( selection_mode == 2 ) {
				strcat ( result, SELECTION_MOD_SUB_STR );
			}
			strcat ( result, SELECTION_TOKEN_SEP );
			idx = (int) gtk_combo_box_get_active ( GTK_COMBO_BOX ( dlg_geom_type ) );
			strcat ( result, SELECTION_GEOM_TYPE_NAME[idx] );
			if ( selection_type != SELECTION_TYPE_ALL ) {
				if ( field != NULL && expression != NULL ) {
					strcat ( result, SELECTION_TOKEN_SEP );
					strcat ( result, t_str_pack ( field ) );
					strcat ( result, SELECTION_TOKEN_SEP );
					strcat ( result, t_str_pack ( expression ) );
				}
			}
			/* check syntax of selection command */
			valid = gui_field_is_valid_selection_command ( result, NULL, parent );
			if ( valid == FALSE ) {
				t_free ( result );
				result = NULL;
			}
		} else {
			t_free ( result );
			result = NULL;
			valid = TRUE;
		}
	}
	gtk_widget_destroy(dialog);

	/* return new/edited command or NULL */
	t_free ( title );
	t_free ( str );
	return ( result );
}


/*
 * Pops up a simple list that allows the user add/remove selection expressions.
 * to/from a list.
 * Must be connected AFTER the field was added to a GUI form.
 * "data" must point to a valid GUI field.
 */
void gui_field_event_edit_selections (GtkEntry *button, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *content_area;
	GtkWidget *scrolled_window;
	GtkWidget *list_view;
	GList *rows = NULL;
	GtkTreeRowReference **refs = NULL;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	gui_field *ptr;
	GtkTreeIter iter;
	BOOLEAN valid;
	int answer;
	int i,j;
	char buf[PRG_MAX_STR_LEN];


	ptr = (gui_field*) data;
	/* will not run if there is no parent window assigned */
	if (ptr->parent == NULL)
		return;

	/* build tree view widget */
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ptr->list));
	renderer = gtk_cell_renderer_text_new ();
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(ptr->list), &iter);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(list_view), -1,
			_("List of selection commands:"), renderer, "text", 0, NULL);
	while ( valid ) {
		char *str_data;
		gtk_tree_model_get (GTK_TREE_MODEL(ptr->list), &iter, 0, &str_data, -1);
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(ptr->list), &iter);
	}
	gtk_widget_set_size_request (list_view, 500, 400);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW(list_view), TRUE );
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW(list_view), FALSE );

	/* attach object to tree */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(list_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* show dialog for editing selection commands */
	dialog = gtk_dialog_new_with_buttons(_("Data selection"), ptr->parent,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_ADD, GUI_FIELD_RESPONSE_ADD,
			_("Edit"), GUI_FIELD_RESPONSE_EDIT,
			GTK_STOCK_REMOVE, GUI_FIELD_RESPONSE_DEL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
			NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
	gtk_container_add(GTK_CONTAINER(scrolled_window), list_view);

	gtk_widget_show_all(dialog);

	answer = 0;
	while (answer != GTK_RESPONSE_ACCEPT) {

		answer = gtk_dialog_run(GTK_DIALOG(dialog));

		/* add another selection command? */
		if (answer == GUI_FIELD_RESPONSE_ADD) {
			/* show dialog for composition of NEW selection command */
			char *cmd = gui_field_dialog_edit_selection_command( NULL, GTK_WINDOW(dialog) );
			if ( cmd != NULL ) {
				/* store new selection command */
				gtk_list_store_append(ptr->list, &iter);
				gtk_list_store_set(ptr->list, &iter, 0, (gchar*)cmd, -1);
			}
		}

		/* edit selection command? */
		if (answer == GUI_FIELD_RESPONSE_EDIT) {
			if ( gtk_tree_selection_count_selected_rows (selection) == 1 ) {
				model = GTK_TREE_MODEL(ptr->list);
				if ( gtk_tree_selection_get_selected (selection, &model, &iter) == TRUE ) {
					/* GValue *val = NULL; */
					/* gtk_tree_model_get_value (model, &iter, 0, val); */
					char *str_data;
					gtk_tree_model_get(model, &iter, 0, &str_data, -1);
					/* show dialog for EDITING existing selection command */
					char *cmd = gui_field_dialog_edit_selection_command( str_data, GTK_WINDOW(dialog) );
					if ( cmd != NULL ) {
						/* update selection command */
						gtk_list_store_set (ptr->list, &iter, 0, (gchar*)cmd, -1);
						t_free (str_data);
					}
				}
			}
		}

		/* delete selected command from list? */
		if (answer == GUI_FIELD_RESPONSE_DEL) {
			if ( gtk_tree_selection_count_selected_rows (selection) > 0 ) {
				model = GTK_TREE_MODEL(ptr->list);
				rows = gtk_tree_selection_get_selected_rows (selection, &model);
				refs =  malloc ( sizeof (GtkTreeRowReference*) * g_list_length (rows));
				for ( j = 0; j < g_list_length (rows); j ++ ) {
					refs[j] = gtk_tree_row_reference_new ( model, (GtkTreePath*) g_list_nth_data ( rows , j ) );
				}
				for ( j = 0; j < g_list_length (rows); j ++ ) {
					path = gtk_tree_row_reference_get_path (refs[j]);
					if ( gtk_tree_model_get_iter_from_string ( model, &iter, gtk_tree_path_to_string (path) ) == TRUE ) {
						gtk_list_store_remove (ptr->list,&iter);
					}
				}
				g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
				g_list_free (rows);
				g_free (refs);
			}
		}

		/* OK? */
		if (answer == GTK_RESPONSE_ACCEPT) {
			gtk_widget_destroy(dialog);
			break;
		}

		/* Aborted? */
		if (answer == GTK_RESPONSE_DELETE_EVENT) {
			gtk_widget_destroy(dialog);
			break;
		}
	}

	/* check how many selection commands we have in the list and update button label */
	i = 0;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ptr->list), &iter);
	while (valid) {
		i ++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ptr->list), &iter);
	}

	snprintf (buf,PRG_MAX_STR_LEN,_("Commands: %i"), i);
	gtk_button_set_label (GTK_BUTTON (ptr->browse_button_m),buf);

	/* clean up */
	/* g_object_unref (selection); */
}


/*
 * Initialize a new, empty gui_field struct.
 * Memory for "gfield" must have already been allocated by
 * the caller.
 *
 * The user should never need to call this, but use one of
 * the gui_field_create_* funcs instead, to create a gui_field
 * of a specific type.
 */
void gui_field_init ( gui_field *gfield ) {
	gfield->key = NULL;
	gfield->name = NULL;
	gfield->description = NULL;
	gfield->section = NULL;
	gfield->type = GUI_FIELD_TYPE_NONE;
	gfield->required = FALSE;
	gfield->multiple = FALSE;
	gfield->lookup = FALSE;
	gfield->content = NULL;
	gfield->length = 0;
	gfield->default_value = NULL;
	gfield->answers = NULL;
	gfield->num_ranges = 0;
	gfield->range_min = NULL;
	gfield->range_max = NULL;
	gfield->limits = malloc ( sizeof (gui_field_limits) );
	gfield->limits->length = -1;
	gfield->limits->strict_numeric = FALSE;
	gfield->limits->allowed = NULL;
	gfield->limits->decimal_sep = FALSE;
	gfield->limits->decimal_grp = FALSE;
	gfield->limits->sign = FALSE;
	gfield->limits->convert_to = GUI_FIELD_CONVERT_TO_NONE;
	gfield->l_widget = NULL;
	gfield->al_widget = NULL;
	gfield->i_widget = NULL;
	gfield->combo_box = NULL;
	gfield->buffer = NULL;
	gfield->checkbox = NULL;
	gfield->file_button = NULL;
	gfield->browse_button = NULL;
	gfield->browse_hbox = NULL;
	gfield->browse_button_m = NULL;
	gfield->list = NULL;
	gfield->parent = NULL;
	gfield->empty = TRUE;
}


/*
 * Destroy a gui_field and release all memory taken by
 * it and its associated GTK widgets.
 */
void gui_field_destroy ( gui_field *gfield )
{
	int i;

	if ( gfield->key != NULL )
		free ( gfield->key );

	if ( gfield->name != NULL )
		free ( gfield->name );

	if ( gfield->description != NULL )
		free ( gfield->description );

	if ( gfield->section != NULL )
		free ( gfield->section );

	if ( gfield->content != NULL ) {
		i = 0;
		while ( gfield->content[i] != NULL ) {
			free ( gfield->content[i] );
		}
		free ( gfield->content );
	}

	if ( gfield->default_value != NULL )
		free ( gfield->default_value );

	if ( gfield->answers != NULL ) {
		i = 0;
		while ( gfield->answers[i] != NULL ) {
			free ( gfield->answers[i] );
		}
		free ( gfield->answers );
	}

	if ( gfield->range_min != NULL )
		free ( gfield->range_min );

	if ( gfield->range_max != NULL )
		free ( gfield->range_max );

	if ( gfield->limits != NULL ) {
		if ( gfield->limits->allowed != NULL )
			free ( gfield->limits->allowed );
		free ( gfield->limits );
	}

	/* TODO: destroy GTK widgets */

	free ( gfield );
}


/*
 * Creates a simple string type input field.
 * Returns a proper new field, or NULL on error.
 *
 * key:				Internal field name. Must be provided.
 * name:			Field name as displayed in GUI. Must be provided.
 * description: 	Helpful descriptive text (for tooltip). Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "Basic". If this is a required field, then
 * 					the group will also always be "Basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 * length:			Set maximum input length (chars) or 0 (=unlimited length).
 * allowed:			String with all allowed input characters or NULL.
 * convert_to:		Convert case:
 * 					GUI_FIELD_CONVERT_TO_NONE		0
 * 					GUI_FIELD_CONVERT_TO_UPPER		1
 *					GUI_FIELD_CONVERT_TO_LOWER		2
 *  				(0) for no conversion
 * choices:			A string array with value choices (look-up list) or NULL.
 * 					Array can be terminated with NULL or an empty string ("").
 */
gui_field *gui_field_create_string ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value,
		int length, const char *allowed,
		int convert_to,  const char **choices )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;
	GtkTreeIter iter;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );

	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_STRING;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));
	if ( default_value != NULL )
		field->default_value = strdup ( default_value );

	/* make the GTK widget(s) */

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	if ( choices == NULL ) {
		field->lookup = FALSE;
		/* text input field */
		if ( default_value != NULL )
			field->buffer = gtk_entry_buffer_new ( (gchar*) default_value, strlen ( default_value ) );
		else
			field->buffer = gtk_entry_buffer_new ( NULL, -1 );
		field->i_widget = gtk_entry_new_with_buffer ( GTK_ENTRY_BUFFER (field->buffer) );
		if ( length > 0 ) {
			gtk_entry_buffer_set_max_length ( GTK_ENTRY_BUFFER (field->buffer), length );
		}
	} else {
		field->lookup = TRUE;
		/* combo box */
		field->combo_box = gtk_combo_box_new_text ();
		int i = 0;
		while ( choices[i] != NULL && strlen (choices[i]) > 0 ) {
			gtk_combo_box_append_text ( GTK_COMBO_BOX (field->combo_box), choices[i] );
			i ++;
		}
		/* set first item as default */
		GtkTreeModel *list = gtk_combo_box_get_model (GTK_COMBO_BOX(field->combo_box));
		gtk_tree_model_get_iter_first (list, &iter);
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (field->combo_box),&iter);
		if ( default_value != NULL ) {
			/* user-default given? process. */
			/* check if the default is part of the combo box */
			/* go through all comb box values and compare: */
			BOOLEAN valid = gtk_tree_model_get_iter_first (list, &iter);
			while ( valid ) {
				char *str_data;
				gtk_tree_model_get (list, &iter, 0, &str_data, -1);
				if ( !strcmp ( str_data, default_value ) ) {
					/* found user-supplied value: set and done */
					gtk_combo_box_set_active_iter (GTK_COMBO_BOX (field->combo_box),&iter);
					break;
				}
				valid = gtk_tree_model_iter_next (list, &iter);
			}
		}
	}

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	if ( field->lookup == FALSE )
		gtk_widget_set_tooltip_markup ( field->i_widget, (const char*) tooltip );
	else
		/* gtk_widget_set_tooltip_markup ( field->combo_box, (const char*) tooltip ); */
		free ( tooltip );

	/* sort into default or user-defined section */
	if ( required == TRUE || section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->required = required;
	field->limits->length = length;
	if ( allowed != NULL )
		field->limits->allowed = strdup ( allowed );
	field->limits->convert_to = convert_to;
	field->empty = FALSE;

	if ( field->lookup == FALSE ) {
		/* connect handler for input restrictions */
		g_signal_connect(G_OBJECT(field->i_widget), "insert_text",
				G_CALLBACK(gui_field_event_insert_text),
				(gpointer) field->limits);
	}

	return ( field );
}


/*
 * Creates a simple string type input field.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 * allow_negative:	Allow input of signed values;
 *
 * NOTE: This field restricts input to +/- and decimal digits. If this is not
 * desirable (e.g. to let the user also input hex digits), then use
 * gui_field_create_string() instead.
 *
 * NOTE 2: Field values are still handled as strings and must be converted
 * to integers from the GtkEntryBuffer.
 *
 */
gui_field *gui_field_create_integer ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value, BOOLEAN allow_negative )
{
	gui_field *field;

	field = gui_field_create_string ( 	key, name, description, section, required, default_value,
			0, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );

	field->type = GUI_FIELD_TYPE_INTEGER;

	/* integer input restrictions */
	field->limits->allowed = strdup ("0123456789");
	field->limits->strict_numeric = TRUE;
	field->limits->sign = allow_negative;
	field->limits->decimal_sep = FALSE;
	field->limits->decimal_grp = TRUE;

	return ( field );
}


/*
 * Creates a simple double type input field.
 * Returns a proper new field, or NULL on error.
 *
 * key:			    Internal field name. Must be provided.
 * name:			Field name as displayed in GUI. Must be provided.
 * description: 	Helpful descriptive text (used as tooltip). Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "Basic". If this is a required field, then
 * 					the group will also always be "Basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 * allow_negative:	Allow input of signed values;
 *
 * NOTE: This field restricts input to +/- and decimal digits. If this is not
 * desirable (e.g. to let the user also input hex digits), then use
 * gui_field_create_string() instead.
 *
 * NOTE 2: Field values are still handled as strings and must be converted
 * to integers from the GtkEntryBuffer.
 *
 */
gui_field *gui_field_create_double ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value, BOOLEAN allow_negative )
{
	gui_field *field;

	field = gui_field_create_string ( 	key, name, description, section, required, default_value,
			0, NULL, GUI_FIELD_CONVERT_TO_NONE, NULL );

	field->type = GUI_FIELD_TYPE_DOUBLE;

	/* integer input restrictions */
	field->limits->allowed = strdup ("0123456789");
	field->limits->strict_numeric = TRUE;
	field->limits->sign = allow_negative;
	field->limits->decimal_sep = TRUE;
	field->limits->decimal_grp = TRUE;

	return ( field );
}


/*
 * Creates a simple checkbox type input field (flag).
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * on:				TRUE if this flag is "on" by default, FALSE otherwise.
 */
gui_field *gui_field_create_boolean ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN on )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;

	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );

	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_BOOLEAN;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));

	/* make the GTK widget(s) */

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* boolean check box */
	field->checkbox = gtk_check_button_new ();

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->checkbox, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* on or off? */
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON (field->checkbox), on );

	/* set remaining field properties */
	field->empty = FALSE;

	return ( field );
}


/*
 * Creates a file name input field.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 */
gui_field *gui_field_create_file_in ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );

	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_FILE_IN;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));
	if ( default_value != NULL )
		field->default_value = strdup ( default_value );

	/* make the GTK widget(s) */

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* file chooser button widget */
	field->file_button = gtk_file_chooser_button_new ( _("Select file"), GTK_FILE_CHOOSER_ACTION_OPEN );
	if ( default_value != NULL ) {
		/* attempt to select given file */
		GFile *file = g_file_new_for_path ( default_value );
		gtk_file_chooser_set_file ( GTK_FILE_CHOOSER (field->file_button), file, NULL);
		g_object_unref(file);
	}

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->file_button, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( required == TRUE || section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->required = required;
	field->empty = FALSE;

	return ( field );
}


/*
 * Creates an output file name input field.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 */
gui_field *gui_field_create_file_out ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );

	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_FILE_OUT;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));
	if ( default_value != NULL )
		field->default_value = strdup ( default_value );

	/* make the GTK widget(s) */

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* file chooser button widget */
	field->browse_hbox = gtk_hbox_new ( FALSE, GUI_PAD_MED );
	if ( default_value != NULL )
		field->buffer = gtk_entry_buffer_new ( (gchar*) default_value, strlen ( default_value ) );
	else
		field->buffer = gtk_entry_buffer_new ( NULL, -1 );
	field->i_widget = gtk_entry_new_with_buffer ( GTK_ENTRY_BUFFER (field->buffer) );
	field->browse_button = gtk_button_new ();
	gtk_button_set_image ( GTK_BUTTON(field->browse_button),gtk_image_new_from_stock
			(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU) );
	gtk_container_add (GTK_CONTAINER (field->browse_hbox), field->i_widget);
	gtk_container_add (GTK_CONTAINER (field->browse_hbox), field->browse_button);

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->i_widget, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( required == TRUE || section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->required = required;
	field->empty = FALSE;

	return ( field );
}


/*
 * Creates an output directory chooser field.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_value:	Default value for field content or NULL.
 */
gui_field *gui_field_create_dir_out ( 	const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char *default_value )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );

	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_DIR_OUT;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));
	if ( default_value != NULL )
		field->default_value = strdup ( default_value );

	/* make the GTK widget(s) */

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* file chooser button widget */
	field->file_button = gtk_file_chooser_button_new ( _("Select folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
	if ( default_value != NULL ) {
		/* attempt to set given folder */
		gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER (field->file_button), default_value );
	}

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->file_button, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( required == TRUE || section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->required = required;
	field->empty = FALSE;

	return ( field );
}


/*
 * A selector field for multiple files.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_values:	NULL terminated string array with default entries, or NULL. Empty string ("")
 * 					is also a valid terminator.
 */
gui_field *gui_field_create_file_in_multi ( const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char **default_values )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;
	int num_items;
	char buf[PRG_MAX_STR_LEN+1];
	GtkTreeIter iter;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );


	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_FILE_IN;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));

	/* create a simple list of string values */
	field->list = gtk_list_store_new (1,G_TYPE_STRING);

	/* populate list store */
	num_items = 0;
	if ( default_values != NULL ) {
		while ( default_values[num_items] != NULL &&
				strlen (default_values[num_items]) > 0 )  {
			gtk_list_store_append (field->list, &iter);
			gtk_list_store_set (field->list, &iter, 0, default_values[num_items], -1 );
			num_items ++;
		}
	}
	snprintf (buf,PRG_MAX_STR_LEN,_("Files: %i"), num_items);

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* multi file browser */
	field->browse_button_m = gtk_button_new_with_label (buf);
	gtk_button_set_image ( GTK_BUTTON(field->browse_button_m),gtk_image_new_from_stock
			(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->browse_button_m, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->multiple = TRUE;
	field->empty = FALSE;

	return ( field );
}


/*
 * A widget for editing data selections.
 * Returns a proper new field, or NULL on error.
 *
 * name:			Field name. Must be provided.
 * description: 	Helpful descriptive text. Optional.
 * section:			Name of group to sort this into, or NULL.
 * 					If NULL, then group will be "basic". If this is a required field, then
 * 					the group will also always be "basic", no matter what the user passes here.
 * required:		TRUE if this field must get a value, FALSE otherwise.
 * default_values:	NULL terminated string array with default entries, or NULL. Empty string ("")
 * 					is also a valid terminator.
 */
gui_field *gui_field_create_edit_selections ( const char *key, const char *name, const char *description, const char *section,
		BOOLEAN required, const char **default_values )
{
	gui_field *field;
	char *tooltip;
	int ttip_len;
	int num_items;
	char buf[PRG_MAX_STR_LEN+1];
	GtkTreeIter iter;


	if ( ( key == NULL ) || ( name == NULL ) )
		return ( NULL );

	if ( ( strlen ( key ) < 1 ) || ( strlen ( name ) < 1 ) )
		return ( NULL );


	/* create a new, empty field structure */
	field = malloc ( sizeof ( gui_field ) );
	gui_field_init ( field );

	/* specific field settings */
	field->type = GUI_FIELD_TYPE_SELECTIONS;

	/* user settings */
	field->key = strdup ( key );
	field->name = strdup ( name );
	if ( description != NULL )
		field->description = strdup ( description );
	else
		field->description = strdup (_( "No description available." ));

	/* create a simple list of string values */
	field->list = gtk_list_store_new (1,G_TYPE_STRING);

	/* populate list store */
	num_items = 0;
	if ( default_values != NULL ) {
		while ( default_values[num_items] != NULL &&
				strlen (default_values[num_items]) > 0 )  {
			gtk_list_store_append (field->list, &iter);
			gtk_list_store_set (field->list, &iter, 0, default_values[num_items], -1 );
			num_items ++;
		}
	}
	snprintf (buf,PRG_MAX_STR_LEN,_("Commands: %i"), num_items);

	/* field label */
	field->l_widget = gtk_label_new ( name );
	gtk_label_set_justify (GTK_LABEL (field->l_widget), GTK_JUSTIFY_RIGHT );
	field->al_widget = gtk_alignment_new ( 1, 0.5, 0, 0 );
	gtk_container_add (GTK_CONTAINER (field->al_widget), field->l_widget);

	/* multi file browser */
	field->browse_button_m = gtk_button_new_with_label (buf);
	gtk_button_set_image ( GTK_BUTTON(field->browse_button_m),gtk_image_new_from_stock
			(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );

	/* tooltip */
	ttip_len = 1 + strlen ( name ) + strlen ("\n") + strlen ( description );
	tooltip = malloc ( sizeof(char) * ttip_len );
	sprintf ( tooltip, "%s\n%s", name, description );
	gtk_widget_set_tooltip_markup ( field->browse_button_m, (const char*) tooltip );
	free ( tooltip );

	/* sort into default or user-defined section */
	if ( section == NULL ) {
		field->section = strdup (_("Basic"));
	} else {
		field->section = strdup ( section );
	}

	/* set remaining field properties */
	field->multiple = TRUE;
	field->empty = FALSE;

	return ( field );
}

#endif /* ifdef GUI */

