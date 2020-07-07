/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	gui_form.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	See gui_form.c.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

#ifdef GUI

#ifndef GUI_FORM_H
#define GUI_FORM_H

#include "global.h"

#include "gui_field.h"

#include <gtk/gtk.h>


/* This should always point to the GTK text view
 * is supposed to receive console output.
 *
 * TODO: Move this to a separate library. */

#ifdef MAIN
GtkWidget *GUI_TEXT_VIEW;
GtkWidget *GUI_STATUS_BAR;
#else
extern GtkWidget *GUI_TEXT_VIEW;
extern GtkWidget *GUI_STATUS_BAR;
#endif

/* Font for GUI console output. */
#define GUI_FONT_CONSOLE	"Mono 10"


/* TODO: Move these to a separate library */
/* default pixel spacings for all widgets */
#define GUI_PAD_NONE 		0
#define GUI_PAD_MIN 		2
#define GUI_PAD_MED 		4
#define GUI_PAD_MAX 		8

/* Maximum number of fields that can be attached
 * to a form.
 */
#define GUI_FORM_MAX_FIELDS	100


typedef struct gui_form gui_form;

/*
 * A simple structure that contains all definitions
 * a GUI form. This is a top-level window, to which
 * a header graphic, main menu, fields and some other
 * stuff can be attached.
 */
struct gui_form
{
	char *title; /* form window title */

	char *filename; /* file name of last saving action */

	int num_fields; /* number of fields currently on this form */
	gui_field **fields; /* array of gui_field */

	char status_msg[PRG_MAX_STR_LEN+1];

	/* GTK GUI widgets for this form */
	GtkWidget *window; /* main form window */
	GdkPixbuf *wicon_xpm; /* XPM for window icon */
	/* main menu bar */
	GtkAccelGroup *menu_group;
	GtkWidget *menu_main; /* main form menubar */
	GtkWidget *menu_main_a; /* menubar aligner */
	GtkWidget *menu_m_file; /* "File" menu entry */
	GtkWidget *menu_m_edit; /* "Edit" menu entry */
	GtkWidget *menu_m_settings; /* "Settings" menu entry */
	GtkWidget *menu_m_help; /* "Help" menu entry */
	GtkWidget *menu_file; /* "File" submenu */
	GtkWidget *menu_edit; /* "Edit" submenu */
	GtkWidget *menu_help; /* "Help" submenu */
	GtkWidget *menu_settings; /* "Settings" submenu */
	/* menu items */
	GtkWidget *menu_file_exec;
	GtkWidget *menu_file_sep01;
	GtkWidget *menu_file_quit;
	GtkWidget *menu_edit_cut;
	GtkWidget *menu_edit_copy;
	GtkWidget *menu_edit_paste;
	GtkWidget *menu_edit_sep01;
	GtkWidget *menu_edit_clear;
	GtkWidget *menu_edit_sep02;
	GtkWidget *menu_edit_prefs;
	GtkWidget *menu_settings_open;
	GtkWidget *menu_settings_save;
	GtkWidget *menu_settings_save_as;
	GtkWidget *menu_settings_sep01;
	GtkWidget *menu_settings_restore;
	GtkWidget *menu_settings_sep02;
	GtkWidget *menu_settings_language;
	GSList 	  *menu_settings_language_group;
	GtkWidget *menu_settings_m_language;
	GtkWidget *menu_settings_m_language_DE;
	GtkWidget *menu_settings_m_language_EN;
	GtkWidget *menu_help_contents;
	GtkWidget *menu_help_about;
	/* widgets for input fields */
	GtkWidget *main_container; /* main vert. container widget */
	GtkWidget *view_container; /* horiz. container for widget and text view areas */
	GtkWidget *view_container_left; /* vert. container input fields area */
	GtkWidget *view_container_right; /* vert. container for view widget area */
	GtkWidget *notebook; /* notebook with tabs */
	GtkWidget *header; /* header image */
	GtkWidget *header_a; /* aligner for header image */
	GdkPixbuf *header_xpm; /* XPM for header image */
	GtkWidget **tab_label; /* labels for notebook tabs */
	GtkWidget **tab_container; /* a vertical container for the widgets on a tab */
	GtkWidget **field_box; /* space for input fields (one for each tab) */
	GtkWidget **field_label_box; /* space for input field labels (one for each tab) */
	GtkWidget **field_entry_box; /* space for input field entry widgets (one for each tab) */
	GtkWidget **flag_box; /* spaces for flags (=BOOLEAN type fields; one for each tab) */
	GtkWidget **flag_label_box; /* space for flag labels (one for each tab) */
	GtkWidget **flag_check_box; /* space for flag checkboxes (one for each tab) */
	/* main control buttons */
	GtkWidget *button_container;
	GtkWidget *button_run;
	GtkWidget *button_restore;
	GtkWidget *button_open;
	GtkWidget *button_save;
	GtkWidget *button_clear;
	/* console text view */
	GtkTextBuffer *text_buffer; /* buffer that stores the console text */
	GtkWidget *text_frame; /* scrolling window frame */
	GtkWidget *text_scroller; /* scrolling window for the text view */
	GtkWidget *text_view; /* console text view */
	GdkColor text_bg_color; /* default white text background */
	GdkColor text_fg_color; /* default black text foreground */
	/* status bar */
	GtkWidget *status_bar;
	/* system clipboard */
	GtkClipboard *clipboard;
	BOOLEAN empty; /* set to TRUE if this form is empty */
};


int gui_init ();
gui_form* gui_form_create ( const char *title );
int gui_form_add_field ( gui_form *form, gui_field *field );
void gui_form_compose ( gui_form *form );
void gui_form_connect_text_view ( gui_form *form );
void gui_form_connect_status_bar ( gui_form *form );
int gui_form_show ( gui_form *form );


#endif

#endif /* #ifdef GUI */

