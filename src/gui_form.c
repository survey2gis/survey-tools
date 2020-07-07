/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	gui_form.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Provides a very basic GUI form that can be furnished with
 * 				option input fields, a main menu and a preferences window.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

#include "config.h"

#ifdef GUI

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "global.h"
#include "errors.h"
#include "tools.h"
#include "gui_conf.h"
#include "gui_form.h"
#include "gui_field.h"
#include "i18n.h"

#include "header.xpm"
#include "wicon.xpm"

/*
 * GTK SIGNALS
 */

static void gui_form_event_destroy (GtkWidget *window, gpointer data)
{
	/* TODO: release memory for field and form widgets! */

	gtk_main_quit ();
}

static gboolean gui_form_event_delete (GtkWidget *window, GdkEvent *event, gpointer data)
{
	return FALSE;
}


/*
 * Connect GTK signals to form.
 */
void gui_form_connect_signals ( GObject *window ) {
	/* Connect the main window to the destroy and delete-event signals. */
	g_signal_connect (G_OBJECT (window), "destroy",
			G_CALLBACK (gui_form_event_destroy), NULL);
	g_signal_connect (G_OBJECT (window), "delete_event",
			G_CALLBACK (gui_form_event_delete), NULL);
}


/*
 * Helper function:
 * Replaces all occurrences of "|" (pipe) in string "str" with
 * ASCII #030 = RS (Record Separator).
 * If "reverse" is "TRUE", then the replacement operation is reversed.
 *
 * This is necessary to avoid trouble with "|" (pipe) being used
 * at the same time as part of regular expressions and list
 * separator in settings files.
 *
 */
void gui_form_settings_replace_pipe_char ( gchar* str, BOOLEAN reverse ) {
	char replace = PRG_SETTINGS_LIST_SEP;
	char with = PRG_SETTINGS_LIST_SEP_ALT;
	char *p = NULL;

	if ( str == NULL || strlen ( str ) < 1 ) {
		return;
	}

	if ( reverse == TRUE ) {
		replace = PRG_SETTINGS_LIST_SEP_ALT;
		with = PRG_SETTINGS_LIST_SEP;
	}

	p = str;
	while ( *p != 0 ) {
		if ( *p == replace ) {
			*p = with;
		}
		p ++;
	}
}


/* Returns current GUI form settings (field contents) as a new string. */
/* "data" must point to the currently active GUI form.*/
/* Memory allocated must be released by caller.*/
char *gui_form_settings_get (gpointer data)
{
	gui_form *form;
	GKeyFile* settings;
	int i, j, n;
	gchar **input;
	BOOLEAN valid;
	GtkTreeIter iter;
	char *result = NULL;


	form = data;

	/* make a new settings object and populate it */
	settings = g_key_file_new ();

	g_key_file_set_list_separator ( settings, PRG_SETTINGS_LIST_SEP );

	g_key_file_set_comment ( settings, NULL, NULL, "Survey2GIS settings storage; all content case sensitive", NULL );

	/* save to disk */
	/* this will only save option settings that actually exist (non-blank fields) */
	i = 0;
	while ( form->fields[i] != NULL ) {
		if ( 	form->fields[i]->type == GUI_FIELD_TYPE_STRING ||
				form->fields[i]->type == GUI_FIELD_TYPE_INTEGER ||
				form->fields[i]->type == GUI_FIELD_TYPE_FILE_OUT ||
				form->fields[i]->type == GUI_FIELD_TYPE_DOUBLE ) {
			if ( form->fields[i]->buffer != NULL ) {
				if ( strlen ( gtk_entry_buffer_get_text (form->fields[i]->buffer)) > 0 ) {
					g_key_file_set_string ( settings, form->fields[i]->section, form->fields[i]->key,
							gtk_entry_buffer_get_text (form->fields[i]->buffer));
				}
			} else {
				if ( form->fields[i]->combo_box != NULL ) {
					g_key_file_set_string ( settings, form->fields[i]->section, form->fields[i]->key,
							gtk_combo_box_get_active_text (GTK_COMBO_BOX (form->fields[i]->combo_box)));
				}
			}
		}
		if ( form->fields[i]->type == GUI_FIELD_TYPE_BOOLEAN ) {
			g_key_file_set_boolean ( settings, form->fields[i]->section, form->fields[i]->key,
					gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (form->fields[i]->checkbox)));
		}
		if ( 	( form->fields[i]->type == GUI_FIELD_TYPE_FILE_IN && form->fields[i]->multiple == FALSE ) ||
				form->fields[i]->type == GUI_FIELD_TYPE_DIR_OUT ) {								
			if ( form->fields[i]->file_button != NULL && GTK_IS_FILE_CHOOSER ( form->fields[i]->file_button ) ) {

				if ( gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (form->fields[i]->file_button) ) != NULL ) {

					g_key_file_set_string ( settings, form->fields[i]->section, form->fields[i]->key,
							gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (form->fields[i]->file_button)));
				}
			}
		}
		if (  ( form->fields[i]->type == GUI_FIELD_TYPE_FILE_IN && form->fields[i]->multiple == TRUE ) ||
				form->fields[i]->type == GUI_FIELD_TYPE_SELECTIONS ) {
			/* multi file inputs/selection commmands must be stored as a list of strings */
			n = 0;
			valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(form->fields[i]->list), &iter);
			while (valid) {
				n++;
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(form->fields[i]->list), &iter);
			}
			if ( n > 0 ) {
				input = malloc(sizeof(gchar*) * (n+1));
				input [n] = NULL;
				/* store content in string array */
				j = 0;
				valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(form->fields[i]->list), &iter);
				while (valid) {
					char *str_data;
					gtk_tree_model_get(GTK_TREE_MODEL(form->fields[i]->list), &iter, 0, &str_data, -1);
					input[j] = strdup(str_data);
					gui_form_settings_replace_pipe_char ( input[j], FALSE );
					valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(form->fields[i]->list), &iter);
					j++;
				}
				g_key_file_set_string_list (settings, form->fields[i]->section, form->fields[i]->key,(const gchar * const*)input,n);
				/* free memory */
				j = 0;
				while ( input[j] != NULL ) {
					free ( input[j]);
					j++;
				}
				free ( input );
			}
		}
		i ++;
	}

	result = g_key_file_to_data ( settings, NULL, NULL );

	/* destroy settings object and release mem */
	g_key_file_free ( settings );

	return ( result );
}


/*
 * Sets the contents of all GUI fields to the settings
 * stored in the string "contents".
 */
void gui_form_settings_put ( const char *contents, size_t length, gpointer data )
{
	gui_form *form;
	GKeyFile* settings;
	gchar **keys = NULL;


	form = data;

	settings = g_key_file_new ();

	g_key_file_set_list_separator ( settings, PRG_SETTINGS_LIST_SEP );

	g_key_file_load_from_data ( settings, (const gchar*) contents, (gsize) length, G_KEY_FILE_NONE, NULL );

	/* first retrieve groups from key file */
	gchar **groups = g_key_file_get_groups ( settings, NULL );
	if ( groups == NULL ) {
		GtkDialog *dialog;
		gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Invalid settings file."))));
		gtk_widget_destroy ( GTK_WIDGET (dialog) );
	} else {
		int i = 0;
		while ( groups[i] != NULL ) {
			/* get all keys in group */
			keys = g_key_file_get_keys ( settings, groups[i], NULL, NULL );
			if ( keys == NULL ) {
				GtkDialog *dialog;
				gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						_("Invalid settings file."))));
				gtk_widget_destroy ( GTK_WIDGET (dialog) );
			} else {
				int j = 0;
				while ( keys[j] != NULL ) {
					/* set widget contents */
					int k = 0;
					/* go through all fields and check for a match for current key */
					while ( form->fields[k] != NULL ) {
						if ( !strcmp ( form->fields[k]->key, keys[j] ) ) {
							/* got a match */
							if ( 	form->fields[k]->type == GUI_FIELD_TYPE_STRING ||
									form->fields[k]->type == GUI_FIELD_TYPE_INTEGER ||
									form->fields[k]->type == GUI_FIELD_TYPE_FILE_OUT ||
									form->fields[k]->type == GUI_FIELD_TYPE_DOUBLE )
							{
								if ( form->fields[k]->buffer != NULL ) {
									/* simple text entry buffer */
									if ( g_key_file_get_string (settings, groups[i], keys[j], NULL ) != NULL ) {
										gtk_entry_buffer_set_text (	GTK_ENTRY_BUFFER(form->fields[k]->buffer),
												g_key_file_get_string (settings, groups[i], keys[j], NULL),
												-1);
									}
								} else {
									if ( form->fields[k]->combo_box != NULL ) {
										/* combo box setting: need to walk through all choices and set the
										   right one */
										GtkTreeModel *model;
										GtkTreeIter iter;
										char *item = NULL;
										gboolean next;

										model = gtk_combo_box_get_model (GTK_COMBO_BOX (form->fields[k]->combo_box));
										next = gtk_tree_model_get_iter_first ( model, &iter );
										while ( next == TRUE ) {
											gtk_tree_model_get ( model, &iter, 0, &item, -1 );
											if ( !strcmp ( item, g_key_file_get_string (settings, groups[i],
													keys[j], NULL) ) ) {
												gtk_combo_box_set_active_iter (GTK_COMBO_BOX (form->fields[k]->combo_box), &iter);
												next = FALSE;
											} else {
												next = gtk_tree_model_iter_next ( model, &iter );
											}
										}
									}
								}
							}
							if ( form->fields[k]->type == GUI_FIELD_TYPE_BOOLEAN ) {
								/* toggle button */
								gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (form->fields[k]->checkbox),
										g_key_file_get_boolean ( settings, groups[i], keys[j], NULL));

							}
							if ( 	( form->fields[k]->type == GUI_FIELD_TYPE_FILE_IN && form->fields[k]->multiple == FALSE ) ||
									form->fields[k]->type == GUI_FIELD_TYPE_DIR_OUT ) {
								/* single file or directory */
								if ( form->fields[k]->file_button != NULL && GTK_IS_FILE_CHOOSER ( form->fields[k]->file_button ) ) {
									if ( g_key_file_get_string (settings, groups[i], keys[j], NULL ) != NULL ) {
										gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (form->fields[k]->file_button),
												g_key_file_get_string (settings, groups[i], keys[j], NULL));
									}
								}
							}
							if ( ( 	form->fields[k]->type == GUI_FIELD_TYPE_FILE_IN && form->fields[k]->multiple == TRUE  ) ||
									form->fields[k]->type == GUI_FIELD_TYPE_SELECTIONS ) {
								gsize length;
								GtkTreeIter iter;

								/* multiple options, stored as a list of strings */
								gchar **items = g_key_file_get_string_list ( settings, groups[i], keys[j], &length, NULL );
								if ( items != NULL ) {
									int l = 0;
									if ( length > 0 ) {
										/* first, we must delete any existing list */
										gtk_list_store_clear ( form->fields[k]->list );
										/* now we append as many rows as we have strings to store */
										while ( items[l] != NULL ) {
											gui_form_settings_replace_pipe_char ( items[l], TRUE );
											gtk_list_store_append ( form->fields[k]->list, &iter );
											gtk_list_store_set ( form->fields[k]->list, &iter, 0, items[l], -1 );
											l ++;
										}
									}
									/* update button label */
									char buf[PRG_MAX_STR_LEN];
									if ( form->fields[k]->type == GUI_FIELD_TYPE_FILE_IN ) {
										snprintf (buf,PRG_MAX_STR_LEN,_("Files: %i"), l);
									}
									if ( form->fields[k]->type == GUI_FIELD_TYPE_SELECTIONS ) {
										snprintf (buf,PRG_MAX_STR_LEN,_("Commands: %i"), l);
									}
									gtk_button_set_label (GTK_BUTTON (form->fields[k]->browse_button_m),buf);
									/* release memory for list of strings */
									if ( items != NULL ) {
										l = 0;
										while ( items[l] != NULL ) {
											free (items[l]);
											l++;
										}
										free (items);
									}
								}
							}
						}
						k++;
					}
					j ++;
				}
			}
			i ++;
		}
	}

	/* destroy settings object and release mem */
	g_key_file_free ( settings );

	if ( groups != NULL ) {
		int i = 0;
		while ( groups[i] != NULL ) {
			free ( groups[i] );
			i++;
		}
		free ( groups );
	}

	if ( keys != NULL ) {
		int i = 0;
		while ( keys[i] != NULL ) {
			free ( keys[i] );
			i++;
		}
		free ( keys );
	} else {
		GtkDialog *dialog;
		gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Invalid settings file."))));
		gtk_widget_destroy ( GTK_WIDGET (dialog) );
	}
}


/* Loads options from a simple key file. */
/* data is a pointer to the current form */
void gui_form_settings_open (GtkEntry *button, gpointer data)
{
	gui_form *form;
	GtkWidget *browser;
	GtkFileFilter *filter_s2g, *filter_all;
	FILE *fp = NULL;
	char store;

	form = data;


	browser = gtk_file_chooser_dialog_new(_("Select file"), GTK_WINDOW (form->window),
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	filter_all = gtk_file_filter_new ();
	gtk_file_filter_add_pattern ( filter_all, "*.*" );
	gtk_file_filter_set_name ( filter_all, "All files (*.*)" );
	gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER (browser), filter_all );
	filter_s2g = gtk_file_filter_new ();
	gtk_file_filter_add_pattern ( filter_s2g, PRG_SETTINGS_FILE_PATTERN );
	gtk_file_filter_set_name ( filter_s2g, PRG_SETTINGS_FILE_DESC );
	gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER (browser), filter_s2g );
	gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER (browser), filter_s2g );
	if (gtk_dialog_run(GTK_DIALOG(browser)) == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(browser));
		fp = t_fopen_utf8 ( filename, "r" );
		if ( fp == NULL ) {
			GtkDialog *dialog;
			gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					_("Error reading file '%s': %s"),
					filename, g_strerror (errno) ) ) );
			gtk_widget_destroy ( GTK_WIDGET (dialog) );
		} else {
			/* read contents of file into a string */
			size_t bytes = 0;
			while ( fread ( &store, 1, 1, fp) > 0 ) {
				bytes ++;
			}
			rewind ( fp );
			char *settings = malloc ( sizeof (char) * (bytes+1) );
			fread ( settings, 1, bytes, fp );
			fclose ( fp );
			settings[bytes]='\0';
			gui_form_settings_put ( settings, bytes, data );
		}
		form->filename = strdup (filename);
		gtk_widget_set_sensitive (GTK_WIDGET(form->menu_settings_save),TRUE);
		free ( filename );
	}
	gtk_widget_destroy(browser);
}


/* Saves options to a simple key file. */
/* Allow the user to pick the file to save to. */
/* data is a pointer to the current form */
void gui_form_settings_save_as (GtkEntry *button, gpointer data )
{
	gui_form *form;
	GtkWidget *browser;
	GtkFileFilter *filter_s2g, *filter_all;
	FILE *fp = NULL;
	gint result;


	form = data;

	/* Show file browser and let user select file to save to */
	browser = gtk_file_chooser_dialog_new(_("Select file"), GTK_WINDOW (form->window),
			GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	filter_all = gtk_file_filter_new ();
	gtk_file_filter_add_pattern ( filter_all, "*.*" );
	gtk_file_filter_set_name ( filter_all, "All files (*.*)" );
	gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER (browser), filter_all );
	filter_s2g = gtk_file_filter_new ();
	gtk_file_filter_add_pattern ( filter_s2g, PRG_SETTINGS_FILE_PATTERN );
	gtk_file_filter_set_name ( filter_s2g, PRG_SETTINGS_FILE_DESC );
	gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER (browser), filter_s2g );
	gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER (browser), filter_s2g );
	if (gtk_dialog_run(GTK_DIALOG(browser)) == GTK_RESPONSE_ACCEPT) {
		/* save settings string to file */
		char *settings = gui_form_settings_get (data);
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(browser));
		/* append ".s2g" extension if necessary */
		char *filename_s2g = strdup (filename);
		char *cp = rindex (filename,'.');
		if ( cp == NULL || strcmp (cp, PRG_SETTINGS_FILE_EXT) != 0 ) {
			free (filename_s2g);
			filename_s2g = malloc ( sizeof (char) * (strlen(filename)+strlen(PRG_SETTINGS_FILE_EXT)+1) );
			filename_s2g[0] = '\0';
			strcat (filename_s2g, filename);
			strcat (filename_s2g, PRG_SETTINGS_FILE_EXT);
		}
		/* check if file exists */
		if( access( filename_s2g, F_OK ) != -1 ) {
			GtkDialog *dialog;
			dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					_("File exists. Overwrite it?"))) ;
			gtk_dialog_add_buttons(GTK_DIALOG(dialog),
					_("Cancel"), 100,
					_("Yes"), 101,
					_("No"), 102, NULL);
			result = gtk_dialog_run (dialog);
			gtk_widget_destroy ( GTK_WIDGET (dialog) );
		} else {
			result = 101;
		}
		if ( result == 101 ) {
			/* attempt to open file for writing */
			fp = t_fopen_utf8(filename_s2g, "w");
			if ( fp == NULL ) {
				GtkDialog *dialog;
				gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						_("Error saving file '%s': %s"),
						filename, g_strerror (errno) ) ) );
				gtk_widget_destroy ( GTK_WIDGET (dialog) );
			} else {
				fwrite(settings, strlen(settings), 1, fp);
				fclose(fp);
				form->filename = strdup (filename_s2g);
				gtk_widget_set_sensitive (GTK_WIDGET(form->menu_settings_save),TRUE);
			}
		}
		free ( settings );
		free ( filename );
		free ( filename_s2g );
	}
	gtk_widget_destroy(browser);
}


/* Saves options to a simple key file. */
/* data is a pointer to the current form */
void gui_form_settings_save (GtkEntry *button, gpointer data )
{
	gui_form *form;
	FILE *fp = NULL;


	form = data;

	if ( form->filename == NULL ) {
		gui_form_settings_save_as (button,data);
	} else {
		char *settings = gui_form_settings_get (data);
		/* attempt to open file for writing */
		fp = t_fopen_utf8(form->filename, "w");
		if ( fp == NULL ) {
			GtkDialog *dialog;
			gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					_("Error saving file '%s': %s"),
					form->filename, g_strerror (errno) ) ) );
			gtk_widget_destroy ( GTK_WIDGET (dialog) );
		} else {
			fwrite(settings, strlen(settings), 1, fp);
			fclose(fp);
		}
		free ( settings );
	}

}


/* Restores previous options settings from auto-save file.
 * Settings are stored to disk automatically after each
 * _successful_ run.
 * The auto-save file is located in the same directory from
 * which Survey2GIS was launched.
 *
 * data is a pointer to the current form
 */
void gui_form_settings_restore (GtkEntry *button, gpointer data)
{
	gui_form *form;
	size_t bytes;
	char store;


	form = data;

	bytes = 0;
	FILE *fp = t_fopen_utf8 ( PRG_SETTINGS_FILE_DEFAULT, "r" );
	if ( fp == NULL ) {
		GtkDialog *dialog;
		gtk_dialog_run (dialog=GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(form->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Error reading default settings from file '%s': %s"),
				PRG_SETTINGS_FILE_DEFAULT, g_strerror (errno) ) ) );
		gtk_widget_destroy ( GTK_WIDGET (dialog) );
	} else {
		/* read contents of file into a string */
		while ( fread ( &store, 1, 1, fp) > 0 ) {
			bytes ++;
		}
		rewind ( fp );
		char *settings = malloc ( sizeof (char) * (bytes+1) );
		fread ( settings, 1, bytes, fp );
		fclose ( fp );
		settings[bytes]='\0';
		gui_form_settings_put ( settings, bytes, data );
	}

}


/* clears the contents of the text view connected to this form */
/* data is a pointer to the current form */
void gui_form_clear_text (GtkEntry *button, gpointer data)
{
	gui_form *form;

	form = data;

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER(form->text_buffer),"",0);

}

/* copies selected content of text view to system clipboard */
void gui_form_copy_text (GtkEntry *button, gpointer data)
{
	gui_form *form;

	form = data;

	gtk_text_buffer_copy_clipboard( GTK_TEXT_BUFFER (form->text_buffer), form->clipboard );
}


/* toggle interface language */
void gui_form_set_language (GtkEntry *button, gpointer data)
{
	GtkWidget *dialog;
	gui_form *form;

	form = data;

	if ( gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_DE) ) ) {

		dialog = gtk_message_dialog_new (GTK_WINDOW(form->window), GTK_DIALOG_MODAL,
				GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
				"Sprachänderung wird erst nach Neustart wirksam.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		BOOLEAN result = gui_conf_set_string ( "i18n", "gui_language", "DE" );
		if ( result == FALSE ) {
			dialog = gtk_message_dialog_new (GTK_WINDOW(form->window), GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					"Spracheinstellung konnte nicht gespeichert werden.");
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}

		return;
	}

	if ( gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_EN) ) ) {

		dialog = gtk_message_dialog_new (GTK_WINDOW(form->window), GTK_DIALOG_MODAL,
				GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
				"New language setting will be effective after application restart.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		BOOLEAN result = gui_conf_set_string ( "i18n", "gui_language", "EN" );
		if ( result == FALSE ) {
			dialog = gtk_message_dialog_new (GTK_WINDOW(form->window), GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					"Failed to save language settings .");
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}

		return;
	}

}


/*
 * TODO: Move this to a separate library.
 */
int gui_init ()
{

	int m_argc;
	char **m_argv;

	/* make some dummy arguments to pass to gtk_init() */
	m_argc = 1;
	m_argv = malloc ( sizeof (char*) );
	m_argv[0] = strdup (" ");

	/* we have already set the locale manually using i18n_init ()! */
	gtk_disable_setlocale();

	gtk_init ( &m_argc, &m_argv );

	/* DEBUG */
	/* fprintf ( stdout, "*** Initialized GTK (version %i.%i.%i) ***\n", (int) gtk_major_version, (int) gtk_minor_version, (int) gtk_micro_version ); */

	GUI_TEXT_VIEW = NULL;
	GUI_STATUS_BAR = NULL;

	return (0);
}


/*
 * Sets default values for a gui_form struct.
 * "gform" must be a valid struct for which memory was already
 * allocated.
 * User should never have to call this.
 */
void gui_form_init ( gui_form* form )
{
	int i;


	form->title = NULL;

	form->filename = NULL;

	form->fields = malloc ( sizeof (gui_form*) * GUI_FORM_MAX_FIELDS );
	for ( i = 0; i < GUI_FORM_MAX_FIELDS; i ++ )
		form->fields[i] = NULL;

	form->num_fields = 0;

	form->window = NULL;

	form->wicon_xpm = NULL;

	form->main_container = NULL;
	form->view_container = NULL;
	form->view_container_left = NULL;
	form->view_container_right = NULL;

	form->menu_group = NULL;
	form->menu_main = NULL;
	form->menu_main_a = NULL;
	form->menu_m_file = NULL;
	form->menu_m_edit = NULL;
	form->menu_m_settings = NULL;
	form->menu_m_help = NULL;
	form->menu_file = NULL;
	form->menu_edit = NULL;
	form->menu_settings = NULL;
	form->menu_help = NULL;
	form->menu_file_exec = NULL;
	form->menu_file_sep01 = NULL;
	form->menu_file_quit = NULL;
	form->menu_edit_cut = NULL;
	form->menu_edit_copy = NULL;
	form->menu_edit_paste = NULL;
	form->menu_edit_sep01 = NULL;
	form->menu_edit_clear = NULL;
	form->menu_edit_sep02 = NULL;
	form->menu_edit_prefs = NULL;
	form->menu_settings_open = NULL;
	form->menu_settings_save = NULL;
	form->menu_settings_save_as = NULL;
	form->menu_settings_sep01 = NULL;
	form->menu_settings_restore = NULL;
	form->menu_settings_sep02 = NULL;
	form->menu_settings_language = NULL;
	form->menu_help_contents = NULL;
	form->menu_help_about = NULL;

	form->header = NULL;
	form->header_a = NULL;
	form->header_xpm = NULL;

	form->tab_label = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->tab_container = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );

	form->field_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->field_label_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->field_entry_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->flag_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->flag_label_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	form->flag_check_box = malloc ( sizeof (GtkWidget*) * GUI_FORM_MAX_FIELDS );
	for ( i = 0; i < GUI_FORM_MAX_FIELDS; i ++ ) {
		form->tab_label[i] = NULL;
		form->tab_container[i] = NULL;
		form->field_box[i] = NULL;
		form->field_label_box[i] = NULL;
		form->field_entry_box[i] = NULL;
		form->flag_box[i] = NULL;
		form->flag_label_box[i] = NULL;
		form->flag_check_box[i] = NULL;
	}

	form->button_container = NULL;
	form->button_run = NULL;
	form->button_restore = NULL;
	form->button_open = NULL;
	form->button_save = NULL;
	form->button_clear = NULL;

	form->status_bar = NULL;

	form->text_buffer = NULL;
	form->text_scroller = NULL;
	form->text_view = NULL;
	form->text_bg_color.pixel = 0;
	form->text_bg_color.red = 65535;
	form->text_bg_color.green = 65535;
	form->text_bg_color.blue = 65535;
	form->text_fg_color.pixel = 1;
	form->text_fg_color.red = 0;
	form->text_fg_color.green = 0;
	form->text_fg_color.blue = 0;

	/* set up clipboard */
	form->clipboard = gtk_clipboard_get( GDK_NONE );

	form->empty = TRUE;
}


/*
 * Release all memory associated with a GUI form.
 */
void gui_form_destroy ( gui_form* gform )
{
	if ( gform->title != NULL )
		free ( gform->title );

	if ( gform->filename != NULL )
		free ( gform->filename );

	/* TODO: this will not delete the actual field structs! */
	if ( gform->fields != NULL )
		free ( gform->fields );

	/* TODO: destroy GTK widgets */
}


/*
 * Returns NULL on failure, otherwise a valid gui_form struct.
 *
 * title:		Title for window. Required.
 *
 */
gui_form* gui_form_create ( const char *title )
{
	gui_form* form;


	form = NULL;

	if ( title == NULL )
		return ( NULL );

	if ( strlen ( title) < 1 )
		return ( NULL );

	/* make new, empty gui_form */
	form = malloc ( sizeof ( gui_form ));
	gui_form_init ( form );

	/* create new form window */
	form->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	form->title = strdup ( title );
	gtk_window_set_title (GTK_WINDOW (form->window), title );

	/* window icon */
	form->wicon_xpm = gdk_pixbuf_new_from_xpm_data ((const char**) wicon_xpm);
	gtk_window_set_icon (GTK_WINDOW(form->window), form->wicon_xpm);

	/* connect signals */
	gui_form_connect_signals ( G_OBJECT (form->window) );

	return (form);
}


/*
 * Add a gui_field to a gui_form.
 *
 * Returns -1 on error, otherwise number of fields now on form.
 */
int gui_form_add_field ( gui_form *form, gui_field *field )
{
	if ( form == NULL || field == NULL )
		return ( -1 );

	/*  TODO: probably need a debug/error message here! */
	if ( form->num_fields == GUI_FORM_MAX_FIELDS )
		return ( -1 );

	/* add to form list of fields */
	form->fields[form->num_fields] = (gui_field*) field;
	form->num_fields ++;
	field->parent = GTK_WINDOW (form->window);

	form->empty = FALSE;
	return ( form->num_fields );
}


/*
 * Compose the GUI form with all attached fields
 * and other widgets.
 */
void gui_form_compose ( gui_form *form )
{
	int i, j;
	char **section_names;
	PangoFontDescription *font_desc;
	GdkColor color;


	/* main widget container */
	form->main_container = gtk_vbox_new ( FALSE, GUI_PAD_NONE );

	/* left/right view containers */
	form->view_container = gtk_hpaned_new ();
	form->view_container_left = gtk_vbox_new ( FALSE, GUI_PAD_MED );
	form->view_container_right = gtk_vbox_new ( FALSE, GUI_PAD_MED );

	/* menu bar */
	form->menu_group = gtk_accel_group_new ();
	form->menu_main = gtk_menu_bar_new ();
	form->menu_m_file = gtk_menu_item_new_with_mnemonic (_("_Process"));
	form->menu_m_edit = gtk_menu_item_new_with_mnemonic (_("_Edit"));
	form->menu_m_settings = gtk_menu_item_new_with_mnemonic (_("_Settings"));
	form->menu_m_help = gtk_menu_item_new_with_mnemonic (_("_Help"));
	form->menu_file = gtk_menu_new ();
	form->menu_edit = gtk_menu_new ();
	form->menu_settings = gtk_menu_new ();
	form->menu_help = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (form->menu_m_file), form->menu_file);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (form->menu_m_edit), form->menu_edit);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (form->menu_m_settings), form->menu_settings);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (form->menu_m_help), form->menu_help);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_main), form->menu_m_file);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_main), form->menu_m_edit);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_main), form->menu_m_settings);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_main), form->menu_m_help);

	/* FILE MENU */
	form->menu_file_exec = gtk_image_menu_item_new_from_stock (GTK_STOCK_EXECUTE, form->menu_group);
	form->menu_file_sep01 = gtk_separator_menu_item_new ();
	form->menu_file_quit = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, form->menu_group);
	gtk_menu_item_set_label ( GTK_MENU_ITEM (form->menu_file_exec), _("_Run"));
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_file), form->menu_file_exec);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_file), form->menu_file_sep01);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_file), form->menu_file_quit);

	/* EDIT MENU */
	form->menu_edit_cut = gtk_image_menu_item_new_from_stock (GTK_STOCK_CUT, form->menu_group);
	form->menu_edit_copy = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, form->menu_group);
	g_signal_connect (G_OBJECT (form->menu_edit_copy),
			"activate", G_CALLBACK (gui_form_copy_text), form);
	form->menu_edit_paste = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, form->menu_group);
	form->menu_edit_sep01 = gtk_separator_menu_item_new ();
	form->menu_edit_clear = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, form->menu_group);
	g_signal_connect (G_OBJECT (form->menu_edit_clear),
			"activate", G_CALLBACK (gui_form_clear_text), form);
	form->menu_edit_sep02 = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_edit), form->menu_edit_copy);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_edit), form->menu_edit_sep01);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_edit), form->menu_edit_clear);

	/* SETTINGS MENU */
	form->menu_settings_open = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, form->menu_group);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_open);
	g_signal_connect (G_OBJECT (form->menu_settings_open),
			"activate", G_CALLBACK (gui_form_settings_open), form);
	form->menu_settings_save = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE, form->menu_group);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_save);
	g_signal_connect (G_OBJECT (form->menu_settings_save),
			"activate", G_CALLBACK (gui_form_settings_save), form);
	if ( form->filename != NULL )
		gtk_widget_set_sensitive (GTK_WIDGET(form->menu_settings_save),TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET(form->menu_settings_save),FALSE);
	form->menu_settings_save_as = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, form->menu_group);
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_save_as);
	g_signal_connect (G_OBJECT (form->menu_settings_save_as),
			"activate", G_CALLBACK (gui_form_settings_save_as), form);
	form->menu_settings_sep01 = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_sep01);
	form->menu_settings_restore = gtk_image_menu_item_new_from_stock (GTK_STOCK_REDO, form->menu_group);
	gtk_menu_item_set_label ( GTK_MENU_ITEM (form->menu_settings_restore), _("_Defaults"));
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_restore);
	g_signal_connect (G_OBJECT (form->menu_settings_restore),
			"activate", G_CALLBACK (gui_form_settings_restore), form);
	form->menu_settings_sep02 = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_sep02);

	/* LANGUAGE SUBMENU */
	form->menu_settings_m_language = gtk_menu_item_new_with_mnemonic (_("_Language"));
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings), form->menu_settings_m_language);
	form->menu_settings_language = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (form->menu_settings_m_language),
			GTK_WIDGET (form->menu_settings_language));
	form->menu_settings_language_group = NULL;
	/* DE */
	form->menu_settings_m_language_DE = gtk_radio_menu_item_new_with_label ( form->menu_settings_language_group,
			_("DE - Deutsch"));
	form->menu_settings_language_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (form->menu_settings_m_language_DE));
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings_language), form->menu_settings_m_language_DE);
	/* EN */
	form->menu_settings_m_language_EN = gtk_radio_menu_item_new_with_label ( form->menu_settings_language_group,
			_("EN - English"));
	form->menu_settings_language_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (form->menu_settings_m_language_EN));
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_settings_language), form->menu_settings_m_language_EN);
	/* set current language */
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_EN), FALSE);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_DE), FALSE);
	if ( i18n_is_locale_DE() ) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_DE), TRUE);
	}
	if ( i18n_is_locale_EN() ) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (form->menu_settings_m_language_EN), TRUE);
	}
	/* connect callbacks */
	g_signal_connect (G_OBJECT (form->menu_settings_m_language_DE), "toggled", G_CALLBACK (gui_form_set_language), form);

	/* HELP MENU */
	form->menu_help_contents = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, form->menu_group);
	form->menu_help_about = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, form->menu_group);
	/* gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_help), form->menu_help_contents); */
	gtk_menu_shell_append (GTK_MENU_SHELL (form->menu_help), form->menu_help_about);
	gtk_window_add_accel_group (GTK_WINDOW (form->window), form->menu_group );
	form->menu_main_a = gtk_alignment_new ( 0, 0, 1, 0 );
	gtk_container_add (GTK_CONTAINER (form->menu_main_a), form->menu_main);

	/* header graphics */
	form->header_xpm = gdk_pixbuf_new_from_xpm_data ((const char**) header_xpm);
	form->header = gtk_image_new_from_pixbuf (form->header_xpm);
	form->header_a = gtk_alignment_new ( 0, 0, 0, 0);
	gtk_container_add (GTK_CONTAINER (form->header_a), form->header);

	/* analyze "section" labels and create as many tabs as necessary */
	section_names = malloc ( sizeof (char*) * (GUI_FORM_MAX_FIELDS+1) );
	/* the first section is always labeled "basic" */
	section_names[0] = strdup (_("Basic"));
	for ( i = 1; i < GUI_FORM_MAX_FIELDS+1; i ++ )
		section_names[i] = NULL;
	i = 0;
	while ( form->fields[i] != NULL ) {
		j = 0;
		BOOLEAN found = FALSE;
		while ( section_names[j] != NULL ) {
			if ( !strcmp ( section_names[j], form->fields[i]->section ) ) {
				found = TRUE;
			}
			j ++;
		}
		if ( found == FALSE )
			section_names [j] = strdup ( form->fields[i]->section );
		i++;
	} /* Now we have a NULL terminated list of unique section names. */

	/* Add notebook and all required pages (tabs) */
	form->notebook = gtk_notebook_new ();
	i = 0;
	while ( section_names[i] != NULL ) {

		/* add new tab (notebook page ) */
		form->tab_label[i] = gtk_label_new ( section_names[i] );
		form->tab_container[i] = gtk_vbox_new ( FALSE, GUI_PAD_MED );
		gtk_notebook_append_page (GTK_NOTEBOOK(form->notebook), form->tab_container[i], form->tab_label[i] );

		/* space for fields with input widgets */
		form->field_box[i] = gtk_hbox_new ( FALSE, GUI_PAD_MED );
		gtk_container_set_border_width ( GTK_CONTAINER(form->field_box[i]), GUI_PAD_MAX );
		/* spaces for labels and input widgets */
		form->field_label_box[i] = gtk_vbox_new ( FALSE, GUI_PAD_MED );
		form->field_entry_box[i] = gtk_vbox_new ( FALSE, GUI_PAD_MED );
		gtk_box_set_spacing (GTK_BOX (form->field_entry_box[i]), GUI_PAD_MAX );
		gtk_container_add (GTK_CONTAINER (form->field_box[i]), form->field_label_box[i]);
		gtk_container_add (GTK_CONTAINER (form->field_box[i]), form->field_entry_box[i]);
		gtk_box_set_child_packing ( GTK_BOX (form->field_box[i]), form->field_label_box[i],
				FALSE, FALSE, GUI_PAD_MED, GTK_PACK_START );
		gtk_container_add (GTK_CONTAINER (form->tab_container[i]), form->field_box[i]);

		/* space for flags with checkboxes */
		form->flag_box[i] = gtk_hbox_new ( FALSE, GUI_PAD_MED );
		gtk_container_set_border_width ( GTK_CONTAINER(form->flag_box[i]), GUI_PAD_MAX );
		/* spaces for flag labels and checkboxes */
		form->flag_label_box[i] = gtk_vbox_new ( FALSE, GUI_PAD_MED );
		form->flag_check_box[i] = gtk_vbox_new ( FALSE, GUI_PAD_MED );
		gtk_box_set_spacing (GTK_BOX (form->flag_check_box[i]), GUI_PAD_MAX );
		gtk_container_add (GTK_CONTAINER (form->flag_box[i]), form->flag_label_box[i]);
		gtk_container_add (GTK_CONTAINER (form->flag_box[i]), form->flag_check_box[i]);
		gtk_box_set_child_packing ( GTK_BOX (form->flag_box[i]), form->flag_check_box[i],
				FALSE, FALSE, GUI_PAD_MED, GTK_PACK_START );
		gtk_container_add (GTK_CONTAINER (form->tab_container[i]), form->flag_box[i]);

		/* add all fields to the right containers */
		j = 0;
		while ( form->fields[j] != NULL ) {
			/* go through all fields that should be attached to this form */
			if ( !strcmp (form->fields[j]->section, section_names[i] ) ) {
				/* This field should go into the current section! */
				/* Now check what type of field it is and add the proper widgets. */
				if ( form->fields[j]->type == GUI_FIELD_TYPE_STRING ) {
					gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
					if ( form->fields[j]->lookup == FALSE ) {
						gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->i_widget);
					} else {
						gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->combo_box);
					}
				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_INTEGER ||
						form->fields[j]->type == GUI_FIELD_TYPE_DOUBLE    )
				{
					gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
					gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->i_widget);
				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_FILE_IN ) {
					if ( form->fields[j]->multiple == FALSE ) {
						gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
						gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->file_button);
					} else {
						gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
						gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->browse_button_m);
						g_signal_connect (G_OBJECT (form->fields[j]->browse_button_m),
								"clicked", G_CALLBACK (gui_field_event_multi_file_browse), form->fields[j]);
					}
				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_SELECTIONS ) {
					gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
					gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->browse_button_m);
					g_signal_connect (G_OBJECT (form->fields[j]->browse_button_m),
							"clicked", G_CALLBACK (gui_field_event_edit_selections), form->fields[j]);

				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_FILE_OUT ) {
					gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
					gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->browse_hbox);
					g_signal_connect (G_OBJECT (form->fields[j]->browse_button),
							"clicked", G_CALLBACK (gui_field_event_file_browse_save), form->fields[j]);
				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_DIR_OUT ) {
					gtk_container_add (GTK_CONTAINER (form->field_label_box[i]), form->fields[j]->al_widget);
					gtk_container_add (GTK_CONTAINER (form->field_entry_box[i]), form->fields[j]->file_button);
				}
				if (	form->fields[j]->type == GUI_FIELD_TYPE_BOOLEAN ) {
					gtk_container_add (GTK_CONTAINER (form->flag_label_box[i]), form->fields[j]->al_widget);
					gtk_container_add (GTK_CONTAINER (form->flag_check_box[i]), form->fields[j]->checkbox);
				}
			}
			j ++;
		}
		/* done (adding all fields to their containers */

		i++; /* next section (tab) */
	} /* done (adding all tabs to notebook widget) */

	/* area for main control buttons */
	form->button_container = gtk_hbutton_box_new();
	form->button_run = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
	gtk_button_set_label ( GTK_BUTTON (form->button_run), _("_Run"));
	gtk_button_set_image ( GTK_BUTTON (form->button_run), gtk_image_new_from_stock (GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON) );
	form->button_restore = gtk_button_new_from_stock (GTK_STOCK_REDO);
	gtk_button_set_label ( GTK_BUTTON (form->button_restore), _("_Defaults"));
	gtk_button_set_image ( GTK_BUTTON (form->button_restore), gtk_image_new_from_stock (GTK_STOCK_REDO, GTK_ICON_SIZE_BUTTON) );
	g_signal_connect (G_OBJECT (form->button_restore),
			"clicked", G_CALLBACK (gui_form_settings_restore), form);
	form->button_open = gtk_button_new_from_stock (GTK_STOCK_OPEN);
	form->button_save = gtk_button_new_from_stock (GTK_STOCK_SAVE);
	form->button_clear = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
	g_signal_connect (G_OBJECT (form->button_clear),
			"clicked", G_CALLBACK (gui_form_clear_text), form);

	gtk_container_add (GTK_CONTAINER (form->button_container), form->button_run);
	gtk_container_add (GTK_CONTAINER (form->button_container), form->button_restore);
	gtk_container_add (GTK_CONTAINER (form->button_container), form->button_clear);
	gtk_button_box_set_layout ( GTK_BUTTON_BOX(form->button_container),GTK_BUTTONBOX_SPREAD);
	/* status bar, with resize handle */
	form->status_bar = gtk_statusbar_new ();

	/* scrolling, paned text view */
	form->text_frame = gtk_frame_new (_("Messages"));
	form->text_scroller = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(form->text_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	form->text_buffer = gtk_text_buffer_new ( NULL );
	form->text_view = gtk_text_view_new_with_buffer ( form->text_buffer );
	gtk_widget_set_size_request (form->text_view, 11*50, -1);
	gtk_text_view_set_editable (GTK_TEXT_VIEW(form->text_view), FALSE );
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(form->text_view), FALSE );
	gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW(form->text_view), FALSE );
	gdk_color_parse ("#0f1a2a", &color);
	gtk_widget_modify_base (form->text_view, GTK_STATE_NORMAL, &color);
	gdk_color_parse ("#f3f633", &color);
	gtk_widget_modify_text (form->text_view, GTK_STATE_NORMAL, &color);
	font_desc = pango_font_description_from_string(GUI_FONT_CONSOLE);
	gtk_widget_modify_font(form->text_view, font_desc);
	pango_font_description_free(font_desc);
	/*  these tags can be used to output text in different colors on this buffer */
	gdk_color_parse ("#f3f633", &color);
	gtk_text_buffer_create_tag (form->text_buffer, "font_yellow", "foreground-gdk", &color, NULL);
	gdk_color_parse ("#ffffff", &color);
	gtk_text_buffer_create_tag (form->text_buffer, "font_white", "foreground-gdk", &color, NULL);
	gdk_color_parse ("#ff5d5d", &color);
	gtk_text_buffer_create_tag (form->text_buffer, "font_red", "foreground-gdk", &color, NULL);
	gdk_color_parse ("#7ebfff", &color);
	gtk_text_buffer_create_tag (form->text_buffer, "font_blue", "foreground-gdk", &color, NULL);
	gdk_color_parse ("#5dff5d", &color);
	gtk_text_buffer_create_tag (form->text_buffer, "font_green", "foreground-gdk", &color, NULL);
	gtk_text_buffer_create_tag (form->text_buffer, "font_bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (form->text_buffer, "font_italic", "style", PANGO_STYLE_ITALIC, NULL);


	/* add main menu bar to main container */
	gtk_container_add (GTK_CONTAINER (form->main_container), form->menu_main_a);
	gtk_box_set_child_packing ( GTK_BOX (form->main_container), form->menu_main_a,
			FALSE, FALSE, GUI_PAD_NONE, GTK_PACK_START );

	/* add left/right view containers to main container */
	gtk_container_add (GTK_CONTAINER (form->main_container), form->view_container);
	gtk_container_add (GTK_CONTAINER (form->view_container), form->view_container_left);
	gtk_container_add (GTK_CONTAINER (form->view_container), form->view_container_right);
	gtk_widget_set_size_request (form->view_container_left, 400, -1);

	/* add header graphics to the left pane */
	gtk_container_add (GTK_CONTAINER (form->view_container_left), form->header_a);
	gtk_box_set_child_packing ( GTK_BOX (form->view_container_left), form->header_a,
			FALSE, FALSE, GUI_PAD_NONE, GTK_PACK_START );

	/* add the notebook to the left pane*/
	gtk_container_add (GTK_CONTAINER (form->view_container_left), form->notebook);
	gtk_box_set_child_packing ( GTK_BOX (form->view_container_left), form->notebook,
			FALSE, FALSE, GUI_PAD_NONE, GTK_PACK_START );

	/* add form control buttons to left pane */
	gtk_container_add (GTK_CONTAINER (form->view_container_left), form->button_container);
	gtk_box_set_child_packing ( GTK_BOX (form->view_container_left), form->button_container,
			FALSE, FALSE, GUI_PAD_MAX, GTK_PACK_START );

	/* add text view to right pane */
	gtk_container_add (GTK_CONTAINER (form->view_container_right), form->text_frame);
	gtk_container_add (GTK_CONTAINER (form->text_frame), form->text_scroller);
	gtk_container_add (GTK_CONTAINER (form->text_scroller), form->text_view);

	/* add status bar across main container */
	gtk_container_add (GTK_CONTAINER (form->main_container), form->status_bar);
	gtk_box_set_child_packing ( GTK_BOX (form->main_container), form->status_bar,
			FALSE, FALSE, GUI_PAD_NONE, GTK_PACK_START );
	snprintf ( form->status_msg, PRG_MAX_STR_LEN, _("Ready.") );
	gtk_statusbar_push (GTK_STATUSBAR(form->status_bar), 0, form->status_msg);

	/* add main container to top-level window */
	gtk_container_add (GTK_CONTAINER (form->window), form->main_container);
}


/*
 * This connect's the form's GtkTextView widget with the global
 * text output stream. Everything output by the funcs in errors.c
 * will end up on that widget.
 */
void gui_form_connect_text_view ( gui_form *form ) {

	/* connect console output to this form's text view pane */
	GUI_TEXT_VIEW = form->text_view;

}


/*
 * This connect's the form's Status Bar widget with a global
 * pointer.
 */
void gui_form_connect_status_bar ( gui_form *form ) {

	GUI_STATUS_BAR = form->status_bar;

}


/*
 * Show the GUI form and let the user input/edit options.
 * When the user presses "Run" or closes the form, control
 * is returned to the caller (who must dispose of the form
 * and all other widgets that are no longer necessary).
 */

int gui_form_show ( gui_form *form )
{
	/* draw form */
	gtk_widget_show_all (form->window);

	/* enter GTK main event loop */
	gtk_main ();

	/* TODO: Return an exit status, so caller will know
	 * whether user chose "Run" or "Exit"  */
	return (0);
}


#endif /* ifdef GUI */
