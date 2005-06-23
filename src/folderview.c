/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkversion.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "summary_search.h"
#include "inputdialog.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "statusbar.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_filter.h"
#include "prefs_folder_item.h"
#include "account.h"
#include "folder.h"
#include "inc.h"

enum
{
	COL_FOLDER_NAME,
	COL_NEW,
	COL_UNREAD,
	COL_TOTAL,
	COL_FOLDER_ITEM,
	COL_PIXBUF,
	COL_PIXBUF_OPEN,
	COL_FOREGROUND,
	COL_BOLD,
	N_COLS
};

#define COL_FOLDER_WIDTH	150
#define COL_NUM_WIDTH		32

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->folderview_cid, str); \
	gtkut_widget_draw_now(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->folderview_cid); \
}

static GList *folderview_list = NULL;

static GdkPixbuf *inbox_pixbuf;
static GdkPixbuf *outbox_pixbuf;
static GdkPixbuf *folder_pixbuf;
static GdkPixbuf *folderopen_pixbuf;
static GdkPixbuf *foldernoselect_pixbuf;
static GdkPixbuf *trash_pixbuf;

static void folderview_select_row	(FolderView	*folderview,
					 GtkTreeIter	*iter);
static void folderview_select_row_ref	(FolderView	*folderview,
					 GtkTreeRowReference *row);

static void folderview_set_folders	(FolderView	*folderview);
static void folderview_append_folder	(FolderView	*folderview,
					 Folder		*folder);

static void folderview_update_row	(FolderView	*folderview,
					 GtkTreeIter	*iter);

static gint folderview_folder_name_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);

/* callback functions */
static gboolean folderview_button_pressed	(GtkWidget	*treeview,
						 GdkEventButton	*event,
						 FolderView	*folderview);
static gboolean folderview_button_released	(GtkWidget	*treeview,
						 GdkEventButton	*event,
						 FolderView	*folderview);

static gboolean folderview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 FolderView	*folderview);

static void folderview_selection_changed(GtkTreeSelection	*selection,
					 FolderView		*folderview);

static void folderview_row_expanded	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 FolderView		*folderview);
static void folderview_row_collapsed	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 FolderView		*folderview);

static void folderview_popup_close	(GtkMenuShell	*menu_shell,
					 FolderView	*folderview);

static void folderview_col_resized	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 FolderView	*folderview);

static void folderview_download_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_update_tree_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_new_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rename_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_delete_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_empty_trash_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_remove_mailbox_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_rm_imap_server_cb (FolderView	*folderview,
					  guint		 action,
					  GtkWidget	*widget);

static void folderview_new_news_group_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_group_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_server_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_search_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_property_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview);
static void folderview_drag_leave_cb     (GtkWidget        *widget,
					  GdkDragContext   *context,
					  guint             time,
					  FolderView       *folderview);
static void folderview_drag_received_cb  (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);

GtkTargetEntry folderview_drag_types[] =
{
	{"text/plain", GTK_TARGET_SAME_APP, 0}
};

static GtkItemFactoryEntry folderview_mail_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_imap_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_news_popup_entries[] =
{
	{N_("/Su_bscribe to newsgroup..."),
					NULL, folderview_new_news_group_cb, 0, NULL},
	{N_("/_Remove newsgroup"),	NULL, folderview_rm_news_group_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};


FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *mail_popup;
	GtkWidget *news_popup;
	GtkWidget *imap_popup;
	GtkItemFactory *mail_factory;
	GtkItemFactory *news_factory;
	GtkItemFactory *imap_factory;
	gint n_entries;

	debug_print(_("Creating folder view...\n"));
	folderview = g_new0(FolderView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwin),
		 GTK_POLICY_AUTOMATIC,
		 prefs_common.folderview_vscrollbar_policy);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrolledwin,
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);

	store = gtk_tree_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER,
				   GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF,
				   GDK_TYPE_COLOR, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					COL_FOLDER_NAME,
					folderview_folder_name_compare,
					NULL, NULL);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					COL_FOLDER_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);
	/* g_object_set(treeview, "fixed-height-mode", TRUE, NULL); */

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	/* create folder icon + name column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 2);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_folder);
	gtk_tree_view_column_set_resizable(column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_title(column, _("Folder"));
	gtk_tree_view_column_set_attributes
		(column, renderer,
		 "pixbuf", COL_PIXBUF,
		 "pixbuf-expander-open", COL_PIXBUF_OPEN,
		 "pixbuf-expander-closed", COL_PIXBUF,
		 NULL);

	renderer = gtk_cell_renderer_text_new();
#if GTK_CHECK_VERSION(2, 6, 0)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0,
		     NULL);
#else
	g_object_set(renderer, "ypad", 0, NULL);
#endif
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_FOLDER_NAME,
					    "foreground-gdk", COL_FOREGROUND,
					    "weight-set", COL_BOLD,
					    NULL);
	g_object_set(G_OBJECT(renderer), "weight", PANGO_WEIGHT_BOLD, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("New"), renderer, "text", COL_NEW, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_new);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Unread"), renderer, "text", COL_UNREAD, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_unread);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("#"), renderer, "text", COL_TOTAL, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_total);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     COL_FOLDER_NAME,
					     GTK_SORT_ASCENDING);

	/* popup menu */
	n_entries = sizeof(folderview_mail_popup_entries) /
		sizeof(folderview_mail_popup_entries[0]);
	mail_popup = menu_create_items(folderview_mail_popup_entries,
				       n_entries,
				       "<MailFolder>", &mail_factory,
				       folderview);
	n_entries = sizeof(folderview_imap_popup_entries) /
		sizeof(folderview_imap_popup_entries[0]);
	imap_popup = menu_create_items(folderview_imap_popup_entries,
				       n_entries,
				       "<IMAPFolder>", &imap_factory,
				       folderview);
	n_entries = sizeof(folderview_news_popup_entries) /
		sizeof(folderview_news_popup_entries[0]);
	news_popup = menu_create_items(folderview_news_popup_entries,
				       n_entries,
				       "<NewsFolder>", &news_factory,
				       folderview);

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(folderview_button_pressed), folderview);
	g_signal_connect(G_OBJECT(treeview), "button_release_event",
			 G_CALLBACK(folderview_button_released), folderview);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(folderview_key_pressed), folderview);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(folderview_selection_changed), folderview);

	g_signal_connect_after(G_OBJECT(treeview), "row-expanded",
			       G_CALLBACK(folderview_row_expanded),
			       folderview);
	g_signal_connect_after(G_OBJECT(treeview), "row-collapsed",
			       G_CALLBACK(folderview_row_collapsed),
			       folderview);

	g_signal_connect(G_OBJECT(mail_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(imap_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(news_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);

        /* drop callback */
	gtk_drag_dest_set(treeview, GTK_DEST_DEFAULT_ALL,
			  folderview_drag_types, 1,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(treeview), "drag-motion",
			 G_CALLBACK(folderview_drag_motion_cb),
			 folderview);
	g_signal_connect(G_OBJECT(treeview), "drag-leave",
			 G_CALLBACK(folderview_drag_leave_cb),
			 folderview);
	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
			 G_CALLBACK(folderview_drag_received_cb),
			 folderview);

	folderview->scrolledwin  = scrolledwin;
	folderview->treeview     = treeview;
	folderview->store        = store;
	folderview->selection    = selection;
	folderview->mail_popup   = mail_popup;
	folderview->mail_factory = mail_factory;
	folderview->imap_popup   = imap_popup;
	folderview->imap_factory = imap_factory;
	folderview->news_popup   = news_popup;
	folderview->news_factory = news_factory;

	gtk_widget_show_all(scrolledwin);

	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *treeview = folderview->treeview;

	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_INBOX, &inbox_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_OUTBOX, &outbox_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_DIR_CLOSE, &folder_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_DIR_OPEN, &folderopen_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_DIR_NOSELECT,
			 &foldernoselect_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_TRASH, &trash_pixbuf);
}

FolderView *folderview_get(void)
{
	return (FolderView *)folderview_list->data;
}

void folderview_set(FolderView *folderview)
{
	MainWindow *mainwin = folderview->mainwin;
	GtkTreeIter iter;

	debug_print(_("Setting folder info...\n"));
	STATUSBAR_PUSH(mainwin, _("Setting folder info..."));

	main_window_cursor_wait(mainwin);

	folderview_unselect(folderview);

	gtk_tree_store_clear(folderview->store);

	folderview_set_folders(folderview);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(folderview->store),
					  &iter))
		folderview_select_row(folderview, &iter);

	main_window_cursor_normal(mainwin);
	STATUSBAR_POP(mainwin);
}

void folderview_set_all(void)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next)
		folderview_set((FolderView *)list->data);
}

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkTreeIter iter;

	if (!item) return;

	if (gtkut_tree_model_find_by_column_data
		(GTK_TREE_MODEL(folderview->store), &iter, NULL,
		 COL_FOLDER_ITEM, item))
		folderview_select_row(folderview, &iter);
}

static void folderview_select_row(FolderView *folderview, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path;

	g_return_if_fail(iter != NULL);

	path = gtk_tree_model_get_path(model, iter);

	gtkut_tree_view_expand_parent_all(GTK_TREE_VIEW(folderview->treeview),
					  iter);

	folderview->open_folder = TRUE;
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(folderview->treeview), path,
				 NULL, FALSE);
	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->total > 0)
		gtk_widget_grab_focus(folderview->summaryview->treeview);
	else
		gtk_widget_grab_focus(folderview->treeview);

	gtk_tree_path_free(path);
}

static void folderview_select_row_ref(FolderView *folderview,
				      GtkTreeRowReference *row)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!row) return;

	path = gtk_tree_row_reference_get_path(row);
	if (!path)
		return;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);

	folderview_select_row(folderview, &iter);
}

void folderview_unselect(FolderView *folderview)
{
	if (folderview->selected) {
		gtk_tree_row_reference_free(folderview->selected);
		folderview->selected = NULL;
	}
	if (folderview->opened) {
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
}

static gboolean folderview_find_next_unread(GtkTreeModel *model,
					    GtkTreeIter *next,
					    GtkTreeIter *iter)
{
	FolderItem *item;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		iter_ = *iter;
		valid = gtkut_tree_model_next(model, &iter_);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		item = NULL;
		gtk_tree_model_get(model, &iter_, COL_FOLDER_ITEM, &item, -1);
		if (item && item->unread > 0 && item->stype != F_TRASH) {
			if (next)
				*next = iter_;
			return TRUE;
		}

		valid = gtkut_tree_model_next(model, &iter_);
	}

	return FALSE;
}

void folderview_select_next_unread(FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter, next;

	if (folderview->opened) {
		GtkTreePath *path;

		path = gtk_tree_row_reference_get_path(folderview->opened);
		if (!path)
			return;
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_path_free(path);
	} else {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}
	if (folderview_find_next_unread(model, &next, &iter)) {
		folderview_select_row(folderview, &next);
		return;
	}

	if (!folderview->opened)
		return;

	/* search again from the first row */
	if (folderview_find_next_unread(model, &next, NULL))
		folderview_select_row(folderview, &next);
}

FolderItem *folderview_get_selected_item(FolderView *folderview)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	FolderItem *item = NULL;

	if (!folderview->selected)
		return NULL;

	path = gtk_tree_row_reference_get_path(folderview->selected);
	if (!path)
		return NULL;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), &iter,
			   COL_FOLDER_ITEM, &item, -1);

	return item;
}

void folderview_set_opened_item(FolderView *folderview, FolderItem *item)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter;
	GtkTreePath *path;

	gtk_tree_row_reference_free(folderview->opened);
	folderview->opened = NULL;

	if (!item)
		return;

	if (gtkut_tree_model_find_by_column_data
		 (model, &iter, NULL, COL_FOLDER_ITEM, item)) {
		path = gtk_tree_model_get_path(model, &iter);
		folderview->opened = gtk_tree_row_reference_new(model, path);
		gtk_tree_path_free(path);
	}
}

void folderview_update_opened_msg_num(FolderView *folderview)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!folderview->opened)
		return;

	path = gtk_tree_row_reference_get_path(folderview->opened);
	if (!path)
		return;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);

	folderview_update_row(folderview, &iter);
}

gboolean folderview_append_item(FolderView *folderview, GtkTreeIter *iter,
				FolderItem *item)
{
	FolderItem *parent_item;
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_, child;
	GtkTreeIter *iter_p = &iter_;

	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);

	parent_item = item->parent;

	if (!parent_item)
		iter_p = NULL;
	else if (!gtkut_tree_model_find_by_column_data
		(model, iter_p, NULL, COL_FOLDER_ITEM, parent_item))
		return FALSE;

	if (!gtkut_tree_model_find_by_column_data
		(model, &child, iter_p, COL_FOLDER_ITEM, item)) {
		gtk_tree_store_append(folderview->store, &child, iter_p);
		gtk_tree_store_set(folderview->store, &child,
				   COL_FOLDER_NAME, item->name,
				   COL_FOLDER_ITEM, item,
				   -1);
		folderview_update_row(folderview, &child);
		if (iter)
			*iter = child;
		return TRUE;
	}

	return FALSE;
}

static void folderview_set_folders(FolderView *folderview)
{
	GList *list;

	list = folder_get_list();

	for (; list != NULL; list = list->next)
		folderview_append_folder(folderview, FOLDER(list->data));
}

static void folderview_scan_tree_func(Folder *folder, FolderItem *item,
				      gpointer data)
{
	GList *list;
	gchar *rootpath;

	if (FOLDER_IS_LOCAL(folder))
		rootpath = LOCAL_FOLDER(folder)->rootpath;
	else if (FOLDER_TYPE(folder) == F_IMAP && folder->account &&
		 folder->account->recv_server)
		rootpath = folder->account->recv_server;
	else if (FOLDER_TYPE(folder) == F_NEWS && folder->account &&
		 folder->account->nntp_server)
		rootpath = folder->account->nntp_server;
	else
		return;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;

		if (item->path)
			str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
					      rootpath, G_DIR_SEPARATOR,
					      item->path);
		else
			str = g_strdup_printf(_("Scanning folder %s ..."),
					      rootpath);

		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

static GtkWidget *label_window_create(const gchar *str)
{
	GtkWidget *window;
	GtkWidget *label;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 380, 60);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), str);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(window));

	label = gtk_label_new(str);
	gtk_container_add(GTK_CONTAINER(window), label);
	gtk_widget_show(label);

	gtk_widget_show_now(window);

	return window;
}

static void folderview_rescan_tree(FolderView *folderview, Folder *folder)
{
	GtkWidget *window;
	AlertValue avalue;

	g_return_if_fail(folder != NULL);

	if (!folder->klass->scan_tree) return;

	avalue = alertpanel
		(_("Rebuild folder tree"),
		 _("The folder tree will be rebuilt. Continue?"),
		 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (avalue != G_ALERTDEFAULT) return;

	if (!FOLDER_IS_LOCAL(folder) &&
	    !main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	inc_lock();
	window = label_window_create(_("Rebuilding folder tree..."));

	summary_show(folderview->summaryview, NULL, FALSE);
	GTK_EVENTS_FLUSH();

	folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
	if (folder->klass->scan_tree(folder) < 0)
		alertpanel_error(_("Rebuilding of the folder tree failed."));
	folder_set_ui_func(folder, NULL, NULL);

	folder_write_list();
	folderview_set_all();
	statusbar_pop_all();

	gtk_widget_destroy(window);
	inc_unlock();
}

void folderview_check_new(Folder *folder)
{
	FolderItem *item;
	FolderView *folderview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	folderview = (FolderView *)folderview_list->data;
	model = GTK_TREE_MODEL(folderview->store);

	if (folder && !FOLDER_IS_LOCAL(folder)) {
		if (!main_window_toggle_online_if_offline
			(folderview->mainwin))
			return;
	}

	inc_lock();
	main_window_lock(folderview->mainwin);
	gtk_widget_set_sensitive(folderview->treeview, FALSE);
	GTK_EVENTS_FLUSH();

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid; valid = gtkut_tree_model_next(model, &iter)) {
		item = NULL;
		gtk_tree_model_get(model, &iter,
				   COL_FOLDER_ITEM, &item, -1);
		if (!item || !item->path || !item->folder) continue;
		if (item->no_select) continue;
		if (folder && folder != item->folder) continue;
		if (!folder && !FOLDER_IS_LOCAL(item->folder)) continue;

		folderview_scan_tree_func(item->folder, item, NULL);
		if (folder_item_scan(item) < 0) {
			if (folder && !FOLDER_IS_LOCAL(folder))
				break;
		}
		folderview_update_row(folderview, &iter);
	}

	gtk_widget_set_sensitive(folderview->treeview, TRUE);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
	statusbar_pop_all();

	folder_write_list();
}

void folderview_check_new_all(void)
{
	GList *list;
	GtkWidget *window;
	FolderView *folderview;

	folderview = (FolderView *)folderview_list->data;

	inc_lock();
	main_window_lock(folderview->mainwin);
	window = label_window_create
		(_("Checking for new messages in all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		folderview_check_new(folder);
	}

	gtk_widget_destroy(window);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
}

static gboolean folderview_search_new_recursive(GtkTreeModel *model,
						GtkTreeIter *iter)
{
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
		if (item) {
			if (item->new > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_new_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_have_new_children(FolderView *folderview,
					     GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_;
	gboolean valid;

	if (iter)
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_new_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_search_unread_recursive(GtkTreeModel *model,
						   GtkTreeIter *iter)
{
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
		if (item) {
			if (item->unread > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_unread_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_have_unread_children(FolderView *folderview,
						GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_;
	gboolean valid;

	if (iter)
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_unread_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static void folderview_update_row(FolderView *folderview, GtkTreeIter *iter)
{
	GtkTreeStore *store = folderview->store;
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GtkTreePath *path;
	GtkTreeIter parent;
	FolderItem *item = NULL;
	GdkPixbuf *pixbuf, *open_pixbuf;
	gchar *name, *str;
	gchar new_s[11], unread_s[11], total_s[11];
	gboolean add_unread_mark;
	gboolean use_bold, use_color;
	GdkColor *foreground = NULL;

	gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);

	switch (item->stype) {
	case F_INBOX:
		pixbuf = open_pixbuf = inbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, INBOX_DIR) ? _("Inbox") :
				item->name);
		break;
	case F_OUTBOX:
		pixbuf = open_pixbuf = outbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, OUTBOX_DIR) ? _("Sent") :
				item->name);
		break;
	case F_QUEUE:
		pixbuf = open_pixbuf = outbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, QUEUE_DIR) ? _("Queue") :
				item->name);
		break;
	case F_TRASH:
		pixbuf = open_pixbuf = trash_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, TRASH_DIR) ? _("Trash") :
				item->name);
		break;
	case F_DRAFT:
		pixbuf = folder_pixbuf;
		open_pixbuf = folderopen_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, DRAFT_DIR) ? _("Drafts") :
				item->name);
		break;
	default:
		if (item->no_select) {
			pixbuf = open_pixbuf = foldernoselect_pixbuf;
		} else {
			pixbuf = folder_pixbuf;
			open_pixbuf = folderopen_pixbuf;
		}

		if (!item->parent) {
			switch (FOLDER_TYPE(item->folder)) {
			case F_MH:
				name = " (MH)"; break;
			case F_IMAP:
				name = " (IMAP4)"; break;
			case F_NEWS:
				name = " (News)"; break;
			default:
				name = "";
			}
			name = g_strconcat(item->name, name, NULL);
		} else {
			if (FOLDER_TYPE(item->folder) == F_NEWS &&
			    item->path &&
			    !strcmp2(item->name, item->path))
				name = get_abbrev_newsgroup_name
					(item->path,
					 prefs_common.ng_abbrev_len);
			else
				name = g_strdup(item->name);
		}
	}

	path = gtk_tree_model_get_path(model, iter);
	if (!gtk_tree_view_row_expanded
		(GTK_TREE_VIEW(folderview->treeview), path) &&
	    folderview_have_unread_children(folderview, iter))
		add_unread_mark = TRUE;
	else
		add_unread_mark = FALSE;
	gtk_tree_path_free(path);

	if (item->stype == F_QUEUE && item->total > 0 &&
	    prefs_common.display_folder_unread) {
		str = g_strdup_printf("%s (%d%s)", name, item->total,
				      add_unread_mark ? "+" : "");
		g_free(name);
		name = str;
	} else if ((item->unread > 0 || add_unread_mark) &&
		   prefs_common.display_folder_unread) {
		if (item->unread > 0)
			str = g_strdup_printf("%s (%d%s)", name, item->unread,
					      add_unread_mark ? "+" : "");
		else
			str = g_strdup_printf("%s (+)", name);
		g_free(name);
		name = str;
	}

	if (!item->parent) {
		strcpy(new_s, "-");
		strcpy(unread_s, "-");
		strcpy(total_s, "-");
	} else {
		itos_buf(new_s, item->new);
		itos_buf(unread_s, item->unread);
		itos_buf(total_s, item->total);
	}

	if (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	    item->stype == F_TRASH) {
		use_bold = use_color = FALSE;
	} else if (item->stype == F_QUEUE) {
		/* highlight queue folder if there are any messages */
		use_bold = use_color = (item->total > 0);
	} else {
		/* if unread messages exist, print with bold font */
		use_bold = (item->unread > 0) || add_unread_mark;
		/* if new messages exist, print with colored letter */
		use_color =
			(item->new > 0) ||
			(add_unread_mark &&
			 folderview_have_new_children(folderview, iter));
	}

	if (item->no_select)
		foreground = &folderview->color_noselect;
	else if (use_color)
		foreground = &folderview->color_new;

	gtk_tree_store_set(store, iter,
			   COL_FOLDER_NAME, name,
			   COL_NEW, new_s,
			   COL_UNREAD, unread_s,
			   COL_TOTAL, total_s,
			   COL_FOLDER_ITEM, item,
			   COL_PIXBUF, pixbuf,
			   COL_PIXBUF_OPEN, open_pixbuf,
			   COL_FOREGROUND, foreground,
			   COL_BOLD, use_bold,
			   -1);
	g_free(name);

	item->updated = FALSE;

	if (gtkut_tree_view_find_collapsed_parent
		(GTK_TREE_VIEW(folderview->treeview), &parent, iter))
		folderview_update_row(folderview, &parent);
}

void folderview_update_item(FolderItem *item, gboolean update_summary)
{
	FolderView *folderview;
	GtkTreeIter iter;

	g_return_if_fail(item != NULL);

	folderview = folderview_get();

	if (gtkut_tree_model_find_by_column_data
		(GTK_TREE_MODEL(folderview->store), &iter, NULL,
		 COL_FOLDER_ITEM, item)) {
		folderview_update_row(folderview, &iter);
		if (update_summary &&
		    folderview->summaryview->folder_item == item)
			summary_show(folderview->summaryview, item, FALSE);
	}
}

static void folderview_update_item_foreach_func(gpointer key, gpointer val,
						gpointer data)
{
	folderview_update_item((FolderItem *)key, GPOINTER_TO_INT(data));
}

void folderview_update_item_foreach(GHashTable *table, gboolean update_summary)
{
	g_hash_table_foreach(table, folderview_update_item_foreach_func,
			     GINT_TO_POINTER(update_summary));
}

static gboolean folderview_update_all_updated_func(GNode *node, gpointer data)
{
	FolderItem *item;

	item = FOLDER_ITEM(node->data);
	if (item->updated) {
		debug_print("folderview_update_all_updated(): '%s' is updated\n", item->path);
		folderview_update_item(item, GPOINTER_TO_INT(data));
	}

	return FALSE;
}

void folderview_update_all_updated(gboolean update_summary)
{
	GList *list;
	Folder *folder;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		folder = (Folder *)list->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				folderview_update_all_updated_func,
				GINT_TO_POINTER(update_summary));
	}
}

static void folderview_insert_item_recursive(FolderView *folderview,
					     FolderItem *item)
{
	GNode *node;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail(item != NULL);

	valid = folderview_append_item(folderview, &iter, item);
	g_return_if_fail(valid == TRUE);

	for (node = item->node->children; node != NULL; node = node->next) {
		FolderItem *child_item = FOLDER_ITEM(node->data);
		folderview_insert_item_recursive(folderview, child_item);
	}

	if (item->node->children && !item->collapsed) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path
			(GTK_TREE_MODEL(folderview->store), &iter);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(folderview->treeview),
					 path, FALSE);
		gtk_tree_path_free(path);
	}
}

static void folderview_append_folder(FolderView *folderview, Folder *folder)
{
	g_return_if_fail(folder != NULL);

	folderview_insert_item_recursive
		(folderview, FOLDER_ITEM(folder->node->data));
}

void folderview_new_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_new_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_new_news_group_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_rename_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_rename_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
	default:
		break;
	}
}

void folderview_delete_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_delete_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_rm_news_group_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_check_new_selected(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_check_new(item->folder);
}

void folderview_remove_mailbox(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
		folderview_remove_mailbox_cb(folderview, 0, NULL);
		break;
	case F_IMAP:
		folderview_rm_imap_server_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_rm_news_server_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_rebuild_tree(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_rescan_tree(folderview, item->folder);
}

static gboolean folderview_menu_popup(FolderView *folderview,
				      GdkEventButton *event)
{
	FolderItem *item = NULL;
	Folder *folder;
	GtkWidget *popup;
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter;
	gboolean new_folder      = FALSE;
	gboolean rename_folder   = FALSE;
	gboolean delete_folder   = FALSE;
	gboolean empty_trash     = FALSE;
	gboolean download_msg    = FALSE;
	gboolean update_tree     = FALSE;
	gboolean rescan_tree     = FALSE;
	gboolean remove_tree     = FALSE;
	gboolean search_folder   = FALSE;
	gboolean folder_property = FALSE;

	if (!event) return FALSE;

	if (event->button != 3)
		return FALSE;

	if (!gtk_tree_selection_get_selected
		(folderview->selection, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);
	folder = item->folder;

	if (folderview->mainwin->lock_count == 0) {
		new_folder = TRUE;
		if (item->parent == NULL) {
			update_tree = remove_tree = TRUE;
			if (folder->account)
				folder_property = TRUE;
		} else {
			folder_property = TRUE;
			if (gtkut_tree_row_reference_equal(folderview->selected,
							   folderview->opened))
				search_folder = TRUE;
		}
		if (FOLDER_IS_LOCAL(folder) || FOLDER_TYPE(folder) == F_IMAP) {
			if (item->parent == NULL)
				update_tree = rescan_tree = TRUE;
			else if (item->stype == F_NORMAL)
				rename_folder = delete_folder = TRUE;
			else if (item->stype == F_TRASH)
				empty_trash = TRUE;
		} else if (FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL)
				delete_folder = TRUE;
		}
		if (FOLDER_TYPE(folder) == F_IMAP ||
		    FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL && item->no_select == FALSE)
				download_msg = TRUE;
		}
	}

#define SET_SENS(factory, name, sens) \
	menu_set_sensitive(folderview->factory, name, sens)

	if (FOLDER_IS_LOCAL(folder)) {
		popup = folderview->mail_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(mail_factory, "/Create new folder...", new_folder);
		SET_SENS(mail_factory, "/Rename folder...", rename_folder);
		SET_SENS(mail_factory, "/Delete folder", delete_folder);
		SET_SENS(mail_factory, "/Empty trash", empty_trash);
		SET_SENS(mail_factory, "/Check for new messages", update_tree);
		SET_SENS(mail_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(mail_factory, "/Search messages...", search_folder);
		SET_SENS(mail_factory, "/Properties...", folder_property);
	} else if (FOLDER_TYPE(folder) == F_IMAP) {
		popup = folderview->imap_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(imap_factory, "/Create new folder...", new_folder);
		SET_SENS(imap_factory, "/Rename folder...", rename_folder);
		SET_SENS(imap_factory, "/Delete folder", delete_folder);
		SET_SENS(imap_factory, "/Empty trash", empty_trash);
		SET_SENS(imap_factory, "/Download", download_msg);
		SET_SENS(imap_factory, "/Check for new messages", update_tree);
		SET_SENS(imap_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(imap_factory, "/Search messages...", search_folder);
		SET_SENS(imap_factory, "/Properties...", folder_property);
	} else if (FOLDER_TYPE(folder) == F_NEWS) {
		popup = folderview->news_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(news_factory, "/Subscribe to newsgroup...", new_folder);
		SET_SENS(news_factory, "/Remove newsgroup", delete_folder);
		SET_SENS(news_factory, "/Download", download_msg);
		SET_SENS(news_factory, "/Check for new messages", update_tree);
		SET_SENS(news_factory, "/Search messages...", search_folder);
		SET_SENS(news_factory, "/Properties...", folder_property);
	} else
		return FALSE;

#undef SET_SENS

	gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);

	return FALSE;
}


/* callback functions */

static gboolean folderview_button_pressed(GtkWidget *widget,
					  GdkEventButton *event,
					  FolderView *folderview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos(treeview, event->x, event->y,
					   &path, NULL, NULL, NULL))
		return TRUE;

	if (event->button == 1 || event->button == 2) {
		folderview->open_folder = TRUE;
	} else if (event->button == 3) {
		if (folderview->selected) {
			folderview->prev_selected =
				gtk_tree_row_reference_copy
					(folderview->selected);
		}
		gtk_tree_selection_select_path(folderview->selection, path);
		folderview_menu_popup(folderview, event);
		gtk_tree_path_free(path);
		return TRUE;
	}

	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean folderview_button_released(GtkWidget *treeview,
					   GdkEventButton *event,
					   FolderView *folderview)
{
	folderview->open_folder = FALSE;
	return FALSE;
}

static gboolean folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       FolderView *folderview)
{
	GtkTreePath *opened = NULL, *selected = NULL;

	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Return:
		if (folderview->selected) {
			folderview_select_row_ref(folderview,
						  folderview->selected);
		}
		return TRUE;
		break;
	case GDK_space:
		if (folderview->selected) {
			if (folderview->opened)
				opened = gtk_tree_row_reference_get_path
					(folderview->opened);
			selected = gtk_tree_row_reference_get_path
				(folderview->selected);
			if (opened && selected &&
			    gtk_tree_path_compare(opened, selected) == 0 &&
			    (!folderview->summaryview->folder_item ||
			     folderview->summaryview->folder_item->total == 0))
				folderview_select_next_unread(folderview);
			else
				folderview_select_row_ref(folderview,
							  folderview->selected);
			gtk_tree_path_free(selected);
			gtk_tree_path_free(opened);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static gboolean folderview_focus_idle_func(gpointer data)
{
	FolderView *folderview = (FolderView *)data;

	GTK_WIDGET_SET_FLAGS(folderview->treeview, GTK_CAN_FOCUS);

	return FALSE;
}

static void folderview_selection_changed(GtkTreeSelection *selection,
					 FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	FolderItem *item = NULL;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean opened;

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if (folderview->selected) {
			gtk_tree_row_reference_free(folderview->selected);
			folderview->selected = NULL;
		}
		return;
	}

	path = gtk_tree_model_get_path(model, &iter);

	gtk_tree_row_reference_free(folderview->selected);
	folderview->selected = gtk_tree_row_reference_new(model, path);

	main_window_set_menu_sensitive(folderview->mainwin);

	if (!folderview->open_folder) {
		gtk_tree_path_free(path);
		return;
	}
	folderview->open_folder = FALSE;

	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	if (!item) {
		gtk_tree_path_free(path);
		return;
	}

	if (item->path)
		debug_print(_("Folder %s is selected\n"), item->path);

	if (summary_is_locked(folderview->summaryview)) {
		gtk_tree_path_free(path);
		return;
	}

	if (folderview->opened) {
		GtkTreePath *open_path = NULL;

		open_path = gtk_tree_row_reference_get_path(folderview->opened);
		if (open_path && gtk_tree_path_compare(open_path, path) == 0) {
			gtk_tree_path_free(open_path);
			gtk_tree_path_free(path);
			return;
		}
		gtk_tree_path_free(open_path);
	}

	GTK_EVENTS_FLUSH();
	opened = summary_show(folderview->summaryview, item, FALSE);

	if (opened) {
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = gtk_tree_row_reference_new(model, path);
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(folderview->treeview), path, NULL, FALSE,
			 0.0, 0.0);
		if (item->total > 0) {
			/* don't let GtkTreeView::gtk_tree_view_button_press()
			 * grab focus */
			GTK_WIDGET_UNSET_FLAGS(folderview->treeview,
					       GTK_CAN_FOCUS);
			g_idle_add(folderview_focus_idle_func, folderview);
		}
	} else
		folderview_select_row_ref(folderview, folderview->opened);

	gtk_tree_path_free(path);
}

static void folderview_row_expanded(GtkTreeView *treeview, GtkTreeIter *iter,
				    GtkTreePath *path, FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	folderview->open_folder = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), iter,
			   COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);
	item->collapsed = FALSE;
	folderview_update_row(folderview, iter);

	valid = gtk_tree_model_iter_children(model, &iter_, iter);

	while (valid) {
		FolderItem *child_item = NULL;

		gtk_tree_model_get(model, &iter_, COL_FOLDER_ITEM, &child_item,
				   -1);
		if (child_item && child_item->node->children &&
		    !child_item->collapsed) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(model, &iter_);
			gtk_tree_view_expand_row
				(GTK_TREE_VIEW(folderview->treeview),
				 path, FALSE);
			gtk_tree_path_free(path);
		}
		valid = gtk_tree_model_iter_next(model, &iter_);
	}
}

static void folderview_row_collapsed(GtkTreeView *treeview, GtkTreeIter *iter,
				     GtkTreePath *path, FolderView *folderview)
{
	FolderItem *item = NULL;

	folderview->open_folder = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), iter,
			   COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);
	item->collapsed = TRUE;
	folderview_update_row(folderview, iter);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	GtkTreePath *path;

	if (!folderview->prev_selected) return;

	path = gtk_tree_row_reference_get_path(folderview->prev_selected);
	gtk_tree_row_reference_free(folderview->prev_selected);
	folderview->prev_selected = NULL;
	if (!path)
		return;
	gtk_tree_selection_select_path(folderview->selection, path);
	gtk_tree_path_free(path);
}

static void folderview_col_resized(GtkWidget *widget, GtkAllocation *allocation,
				   FolderView *folderview)
{
	GtkTreeViewColumn *column;
	gint type;
	gint width = allocation->width;

	for (type = 0; type <= COL_TOTAL; type++) {
		column = gtk_tree_view_get_column
			(GTK_TREE_VIEW(folderview->treeview), type);
		if (column && column->button == widget) {
			switch (type) {
			case COL_FOLDER_NAME:
				prefs_common.folder_col_folder = width;
				break;
			case COL_NEW:
				prefs_common.folder_col_new = width;
				break;
			case COL_UNREAD:
				prefs_common.folder_col_unread = width;
				break;
			case COL_TOTAL:
				prefs_common.folder_col_total = width;
				break;
			default:
				break;
			}
			break;
		}
	}
}

static void folderview_download_func(Folder *folder, FolderItem *item,
				     gpointer data)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;

		str = g_strdup_printf
			(_("Downloading messages in %s ..."), item->path);
		main_window_progress_set(mainwin,
					 GPOINTER_TO_INT(data), item->total);
		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

static void folderview_download_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	MainWindow *mainwin = folderview->mainwin;
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	main_window_cursor_wait(mainwin);
	inc_lock();
	main_window_lock(mainwin);
	gtk_widget_set_sensitive(folderview->treeview, FALSE);
	main_window_progress_on(mainwin);
	GTK_EVENTS_FLUSH();
	folder_set_ui_func(item->folder, folderview_download_func, NULL);
	if (folder_item_fetch_all_msg(item) < 0) {
		gchar *name;

		name = trim_string(item->name, 32);
		alertpanel_error(_("Error occurred while downloading messages in `%s'."), name);
		g_free(name);
	}
	folder_set_ui_func(item->folder, NULL, NULL);
	main_window_progress_off(mainwin);
	gtk_widget_set_sensitive(folderview->treeview, TRUE);
	main_window_unlock(mainwin);
	inc_unlock();
	main_window_cursor_normal(mainwin);
	statusbar_pop_all();
}

static void folderview_update_tree_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (action == 0)
		folderview_check_new(item->folder);
	else
		folderview_rescan_tree(folderview, item->folder);
}

static void folderview_new_folder_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (FOLDER_TYPE(item->folder) == F_IMAP)
		g_return_if_fail(item->folder->account != NULL);

	if (FOLDER_TYPE(item->folder) == F_IMAP) {
		new_folder = input_dialog
			(_("New folder"),
			 _("Input the name of new folder:\n"
			   "(if you want to create a folder to store subfolders,\n"
			   " append `/' at the end of the name)"),
			 _("NewFolder"));
	} else {
		new_folder = input_dialog(_("New folder"),
					  _("Input the name of new folder:"),
					  _("NewFolder"));
	}
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if ((p && FOLDER_TYPE(item->folder) != F_IMAP) ||
	    (p && FOLDER_TYPE(item->folder) == F_IMAP && *(p + 1) != '\0')) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(item, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."), name);
		return;
	}

	new_item = item->folder->klass->create_folder(item->folder, item,
						      new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder `%s'."), name);
		return;
	}

	folderview_append_item(folderview, NULL, new_item);
	folder_write_list();
}

static void folderview_rename_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;
	gchar *old_path;
	gchar *old_id;
	gchar *new_id;
	GtkTreePath *sel_path;
	GtkTreePath *open_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for `%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
	g_free(message);
	g_free(name);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	if (folder_find_child_item_by_name(item->parent, new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(name);
		return;
	}

	Xstrdup_a(old_path, item->path, {g_free(new_folder); return;});
	old_id = folder_item_get_identifier(item);

	if (item->folder->klass->rename_folder(item->folder, item,
					       new_folder) < 0) {
		g_free(old_id);
		return;
	}

	if (folder_get_default_folder() == item->folder)
		prefs_filter_rename_path(old_path, item->path);
	new_id = folder_item_get_identifier(item);
	prefs_filter_rename_path(old_id, new_id);
	g_free(old_id);
	g_free(new_id);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	open_path = gtk_tree_row_reference_get_path(folderview->opened);
	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		folderview_update_row(folderview, &iter);
	}
	if (sel_path && open_path &&
	    (gtk_tree_path_compare(open_path, sel_path) == 0 ||
	     gtk_tree_path_is_ancestor(sel_path, open_path))) {
		GtkTreeRowReference *row;

		row = gtk_tree_row_reference_copy(folderview->opened);
		folderview_unselect(folderview);
		folderview_select_row_ref(folderview, row);
		gtk_tree_row_reference_free(row);
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	folder_write_list();
}

static void folderview_delete_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	Folder *folder;
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;
	GtkTreePath *sel_path, *open_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under `%s' will be permanently deleted.\n"
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel(_("Delete folder"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(sel_path != NULL);
	open_path = gtk_tree_row_reference_get_path(folderview->opened);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter,
				sel_path);
	if (sel_path && open_path &&
	    (gtk_tree_path_compare(open_path, sel_path) == 0 ||
	     gtk_tree_path_is_ancestor(sel_path, open_path))) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	if (folder->klass->remove_folder(folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder `%s'."), name);
		g_free(old_id);
		return;
	}

	if (folder_get_default_folder() == folder)
		prefs_filter_delete_path(old_path);
	prefs_filter_delete_path(old_id);
	g_free(old_id);

	gtk_tree_store_remove(folderview->store, &iter);

	folder_write_list();
}

static void folderview_empty_trash_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	FolderItem *item;
	Folder *folder;
	GtkTreePath *sel_path, *open_path;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	if (folder->trash != item) return;
	if (item->stype != F_TRASH) return;

	if (alertpanel(_("Empty trash"), _("Empty all messages in trash?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	procmsg_empty_trash(folder->trash);
	statusbar_pop_all();
	folderview_update_item(folder->trash, TRUE);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	open_path = gtk_tree_row_reference_get_path(folderview->opened);
	if (open_path && sel_path &&
	    gtk_tree_path_compare(open_path, sel_path) == 0)
		gtk_widget_grab_focus(folderview->treeview);
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);
}

static void folderview_remove_mailbox_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent) return;

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf
		(_("Really remove the mailbox `%s' ?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel(_("Remove mailbox"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	folder_destroy(item->folder);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	folder_write_list();
}

static void folderview_rm_imap_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete IMAP4 account `%s'?"), name);
	avalue = alertpanel(_("Delete IMAP4 account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_new_news_group_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	Folder *folder;
	FolderItem *item;
	FolderItem *rootitem = NULL;
	FolderItem *newitem;
	GSList *new_subscr;
	GSList *cur;
	GNode *gnode;
	GtkTreePath *server_path;
	GtkTreeIter iter, root;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	folder = item->folder;
	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_TYPE(folder) == F_NEWS);
	g_return_if_fail(folder->account != NULL);

	server_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(server_path != NULL);
	gtk_tree_model_get_iter(model, &iter, server_path);
	gtk_tree_path_free(server_path);

	if (!gtk_tree_model_iter_parent(model, &root, &iter))
		root = iter;

	gtk_tree_model_get(model, &root, COL_FOLDER_ITEM, &rootitem, -1);

	new_subscr = grouplist_dialog(folder);

	/* remove unsubscribed newsgroups */
	for (gnode = folder->node->children; gnode != NULL; ) {
		GNode *next = gnode->next;
		GtkTreeIter found;

		item = FOLDER_ITEM(gnode->data);
		if (g_slist_find_custom(new_subscr, item->path,
					(GCompareFunc)g_ascii_strcasecmp)
		    != NULL) {
			gnode = next;
			continue;
		}

		if (!gtkut_tree_model_find_by_column_data
			(model, &found, &root, COL_FOLDER_ITEM, item)) {
			gnode = next;
			continue;
		}

		if (folderview->summaryview->folder_item == item) {
			summary_clear_all(folderview->summaryview);
			gtk_tree_row_reference_free(folderview->opened);
			folderview->opened = NULL;
		}

		folder_item_remove(item);
		gtk_tree_store_remove(folderview->store, &found);

		gnode = next;
	}

	/* add subscribed newsgroups */
	for (cur = new_subscr; cur != NULL; cur = cur->next) {
		gchar *name = (gchar *)cur->data;

		if (folder_find_child_item_by_name(rootitem, name) != NULL)
			continue;

		newitem = folder_item_new(name, name);
		folder_item_append(rootitem, newitem);
		folderview_append_item(folderview, NULL, newitem);
	}

	if (new_subscr) {
		server_path = gtk_tree_model_get_path(model, &root);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(folderview->treeview),
					 server_path, FALSE);
		gtk_tree_path_free(server_path);
	}

	slist_free_strings(new_subscr);
	g_slist_free(new_subscr);

	folder_write_list();
}

static void folderview_rm_news_group_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path, *open_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string_before(item->path, 32);
	message = g_strdup_printf(_("Really delete newsgroup `%s'?"), name);
	avalue = alertpanel(_("Delete newsgroup"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(sel_path != NULL);
	open_path = gtk_tree_row_reference_get_path(folderview->opened);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter,
				sel_path);
	if (open_path && sel_path &&
	    gtk_tree_path_compare(open_path, sel_path) == 0) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	folder_item_remove(item);
	gtk_tree_store_remove(folderview->store, &iter);
	folder_write_list();
}

static void folderview_rm_news_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete news account `%s'?"), name);
	avalue = alertpanel(_("Delete news account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_search_cb(FolderView *folderview, guint action,
				 GtkWidget *widget)
{
	summary_search(folderview->summaryview);
}

static void folderview_property_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (item->parent == NULL && item->folder->account)
		account_open(item->folder->account);
	else
		prefs_folder_item_open(item);
}

static gint auto_expand_timeout(gpointer data)
{
	FolderView *folderview = data;
	GtkTreeView *treeview = GTK_TREE_VIEW(folderview->treeview);
	GtkTreePath *path = NULL;

	gtk_tree_view_get_drag_dest_row(treeview, &path, NULL);

	if (path) {
		gtk_tree_view_expand_row(treeview, path, FALSE);
		gtk_tree_path_free(path);
		folderview->expand_timeout = 0;

		return FALSE;
	} else
		return TRUE;
}

static void remove_auto_expand_timeout(FolderView *folderview)
{
	if (folderview->expand_timeout != 0) {
		g_source_remove(folderview->expand_timeout);
		folderview->expand_timeout = 0;
	}
}

static gint auto_scroll_timeout(gpointer data)
{
	FolderView *folderview = data;

	gtkut_tree_view_vertical_autoscroll
		(GTK_TREE_VIEW(folderview->treeview));

	return TRUE;
}

static void remove_auto_scroll_timeout(FolderView *folderview)
{
	if (folderview->scroll_timeout != 0) {
		g_source_remove(folderview->scroll_timeout);
		folderview->scroll_timeout = 0;
	}
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path = NULL, *prev_path = NULL;
	GtkTreeIter iter;
	FolderItem *item = NULL, *src_item = NULL;
	gboolean acceptable = FALSE;

	if (gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL)) {
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
		src_item = folderview->summaryview->folder_item;
		if (src_item && src_item != item)
			acceptable = FOLDER_ITEM_CAN_ADD(item);
	} else
		remove_auto_expand_timeout(folderview);

	gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget),
					&prev_path, NULL);
	if (!path || (prev_path && gtk_tree_path_compare(path, prev_path) != 0))
		remove_auto_expand_timeout(folderview);
	if (prev_path)
		gtk_tree_path_free(prev_path);

	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), path,
					GTK_TREE_VIEW_DROP_INTO_OR_AFTER);

	if (path) {
		if (folderview->expand_timeout == 0) {
			folderview->expand_timeout =
				g_timeout_add(1000, auto_expand_timeout,
					      folderview);
		} else if (folderview->scroll_timeout == 0) {
			folderview->scroll_timeout =
				g_timeout_add(150, auto_scroll_timeout,
					      folderview);
		}
	}

	if (acceptable) {
		if ((context->actions & GDK_ACTION_MOVE) != 0 &&
		    FOLDER_ITEM_CAN_ADD(src_item))
			gdk_drag_status(context, GDK_ACTION_MOVE, time);
		else if ((context->actions & GDK_ACTION_COPY) != 0)
			gdk_drag_status(context, GDK_ACTION_COPY, time);
		else if ((context->actions & GDK_ACTION_LINK) != 0)
			gdk_drag_status(context, GDK_ACTION_LINK, time);
		else
			gdk_drag_status(context, 0, time);
	} else
		gdk_drag_status(context, 0, time);

	if (path)
		gtk_tree_path_free(path);

	return TRUE;
}

static void folderview_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     FolderView     *folderview)
{
	remove_auto_expand_timeout(folderview);
	remove_auto_scroll_timeout(folderview);

	gtk_tree_view_set_drag_dest_row
		(GTK_TREE_VIEW(widget), NULL, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

static void folderview_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					FolderView       *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	FolderItem *item = NULL, *src_item;

	remove_auto_expand_timeout(folderview);
	remove_auto_scroll_timeout(folderview);

	if (!gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL))
		return;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	src_item = folderview->summaryview->folder_item;

	if (FOLDER_ITEM_CAN_ADD(item) && src_item && src_item != item) {
		if ((context->actions & GDK_ACTION_MOVE) != 0 &&
		    FOLDER_ITEM_CAN_ADD(src_item)) {
			summary_move_selected_to(folderview->summaryview, item);
			context->action = 0;
			gtk_drag_finish(context, TRUE, FALSE, time);
		} else if ((context->actions & GDK_ACTION_COPY) != 0) {
			summary_copy_selected_to(folderview->summaryview, item);
			gtk_drag_finish(context, TRUE, FALSE, time);
		} else
			gtk_drag_finish(context, FALSE, FALSE, time);
	} else
		gtk_drag_finish(context, FALSE, FALSE, time);

	gtk_tree_path_free(path);
}

static gint folderview_folder_name_compare(GtkTreeModel *model,
					   GtkTreeIter *a, GtkTreeIter *b,
					   gpointer data)
{
	FolderItem *item_a = NULL, *item_b = NULL;

	gtk_tree_model_get(model, a, COL_FOLDER_ITEM, &item_a, -1);
	gtk_tree_model_get(model, b, COL_FOLDER_ITEM, &item_b, -1);

	return folder_item_compare(item_a, item_b);
}
