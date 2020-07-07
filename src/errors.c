/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	errors.c
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Facilities for displaying and logging errors, warnings
 * 				and status messages.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "global.h"
#include "errors.h"
#include "options.h"
#include "tools.h"

#ifdef GUI
#include <gtk/gtk.h>
#include "gui_form.h"
#endif


/* file handle for error log file */
static FILE *ERR_LOG_OUTPUT = NULL;


/*
 * Store an error message in the global message string.
 */
void err_msg_set ( const char *msg ) {
	strncpy ( err_msg, msg, ERR_MSG_LENGTH );
}


/*
 * Clear global error message string buffer.
 */
void err_msg_clear () {
	strncpy ( err_msg, "", ERR_MSG_LENGTH );
}


/*
 * Display an error message straight to the console.
 * If a log file has been specified, then the message
 * will also be written to the log.
 */
void err_show ( unsigned short type, const char *format, ... )
{
	char buffer[ERR_MSG_LENGTH+1];
	va_list argp;
#ifdef GUI
	GtkTextIter iter;
	GtkTextBuffer *tbuffer;
	GtkTextMark *mark;


	if ( GUI_TEXT_VIEW != NULL ) {
		if ( OPTIONS_GUI_MODE == TRUE ) {
			tbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW));
			mark = gtk_text_buffer_get_insert(tbuffer);
		}
	}
#endif
	va_start(argp, format);

	vsnprintf ( buffer, ERR_MSG_LENGTH, format, argp );

	if ( type == ERR_EXIT ) {
		ERR_STATUS = 1;
		if ( ERR_LOG_OUTPUT != NULL ) {
			fprintf ( ERR_LOG_OUTPUT, _("ERROR: "));
			fprintf ( ERR_LOG_OUTPUT, "%s", buffer );
			fprintf ( ERR_LOG_OUTPUT, "\n");
		}
#ifdef GUI
		if ( OPTIONS_GUI_MODE == TRUE ) {
			if ( GUI_TEXT_VIEW != NULL ) {
				gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, _("ERROR: "), -1, "font_bold", "font_red", NULL);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, buffer, -1, "font_red", NULL);
				gtk_text_buffer_insert_at_cursor
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						"\n", 1 );
				gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);
			}
			fprintf ( stderr, "<ERROR_START>\n" );
		}
		fprintf ( stderr, _("ERROR: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
		if ( OPTIONS_GUI_MODE == TRUE )
			fprintf ( stderr, "<ERROR_END>\n" );
		if ( OPTIONS_GUI_MODE == FALSE )
			exit (PRG_EXIT_ERR);
#else
		fprintf ( stderr, _("ERROR: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
		/* in non-GUI mode we exit right here! */
		exit (PRG_EXIT_ERR);
#endif
	}
	if ( type == ERR_WARN ) {
		WARN_STATUS = 1;
		if ( ERR_LOG_OUTPUT != NULL ) {
			fprintf ( ERR_LOG_OUTPUT, _("WARNING: "));
			fprintf ( ERR_LOG_OUTPUT, "%s", buffer );
			fprintf ( ERR_LOG_OUTPUT, "\n");
		}
#ifdef GUI
		if ( OPTIONS_GUI_MODE == TRUE ) {
			if ( GUI_TEXT_VIEW != NULL ) {
				gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, _("WARNING: "), -1, "font_bold", "font_yellow", NULL);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, buffer, -1, "font_yellow", NULL);
				gtk_text_buffer_insert_at_cursor
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						"\n", 1 );
				gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);
			}
			fprintf ( stderr, "<WARNING_START>\n" );
		}
		fprintf ( stderr, _("WARNING: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
		if ( OPTIONS_GUI_MODE == TRUE )
			fprintf ( stderr, "<WARNING_END>\n" );
#else
		fprintf ( stderr, _("WARNING: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
#endif
	}
	if ( type == ERR_DBUG ) {
		if ( ERR_LOG_OUTPUT != NULL ) {
			fprintf ( ERR_LOG_OUTPUT, _("DEBUG: "));
			fprintf ( ERR_LOG_OUTPUT, "%s", buffer );
			fprintf ( ERR_LOG_OUTPUT, "\n");
		}
#ifdef GUI
		if ( OPTIONS_GUI_MODE == TRUE ) {
			if ( GUI_TEXT_VIEW != NULL ) {
				gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, _("DEBUG: "), -1, "font_bold", "font_green", NULL);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, buffer, -1, "font_green", NULL);
				gtk_text_buffer_insert_at_cursor
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						"\n", 1 );
				gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);
			}
			fprintf ( stderr, "<DEBUG_START>\n" );
		}
		fprintf ( stderr, _("DEBUG: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
		if ( OPTIONS_GUI_MODE == TRUE )
			fprintf ( stderr, "<DEBUG_END>\n" );
#else
		fprintf ( stderr, _("DEBUG: "));
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
#endif
	}
	if ( type == ERR_NOTE ) {
		if ( ERR_LOG_OUTPUT != NULL ) {
			fprintf ( ERR_LOG_OUTPUT, "%s", buffer );
			fprintf ( ERR_LOG_OUTPUT, "\n");
		}
#ifdef GUI
		if ( OPTIONS_GUI_MODE == TRUE ) {
			if ( GUI_TEXT_VIEW != NULL ) {
				gtk_text_buffer_get_iter_at_mark(tbuffer, &iter, mark);
				gtk_text_buffer_insert_with_tags_by_name
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						&iter, buffer, -1, "font_blue", NULL);
				gtk_text_buffer_insert_at_cursor
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(GUI_TEXT_VIEW)),
						"\n", 1 );
				gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(GUI_TEXT_VIEW), mark);
			}
			fprintf ( stderr, "<NOTE_START>\n" );
		}
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
		if ( OPTIONS_GUI_MODE == TRUE )
			fprintf ( stderr, "<NOTE_END>\n" );
#else
		fprintf ( stderr, "%s", buffer );
		fprintf ( stderr, "\n");
#endif
	}
	va_end(argp);
}


/* initialize message output facility */
void err_log_init ( options *opts )
{
	if ( opts->log != NULL ) {
		/* open logfile and assign output */
#ifdef MINGW
		ERR_LOG_OUTPUT = t_fopen_utf8 ( opts->log, "wt");
#else
		ERR_LOG_OUTPUT = t_fopen_utf8 ( opts->log, "w");
#endif
		if ( ERR_LOG_OUTPUT == NULL ) {
			err_show ( ERR_EXIT, _("Cannot open log file for writing ('%s').\nReason: %s"),
					opts->log, strerror (errno));
		}
	}
}


/* close error log file */
void err_close ( )
{
	if ( ERR_LOG_OUTPUT != NULL ) {
		fclose ( ERR_LOG_OUTPUT );
	}
}
