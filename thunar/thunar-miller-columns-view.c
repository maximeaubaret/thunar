/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2026 Thunar Miller Columns Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "thunar/thunar-miller-columns-view.h"

#include "thunar/thunar-action-manager.h"
#include "thunar/thunar-component.h"
#include "thunar/thunar-dnd.h"
#include "thunar/thunar-folder.h"
#include "thunar/thunar-gio-extensions.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-gtk-extensions.h"
#include "thunar/thunar-history.h"
#include "thunar/thunar-menu.h"
#include "thunar/thunar-miller-column.h"
#include "thunar/thunar-navigator.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-util.h"
#include "thunar/thunar-view.h"
#include "thunar/thunar-window.h"

#include <libxfce4ui/libxfce4ui.h>

static void thunar_miller_columns_view_navigator_init (ThunarNavigatorIface *iface);
static void thunar_miller_columns_view_component_init (ThunarComponentIface *iface);
static void thunar_miller_columns_view_view_init (ThunarViewIface *iface);
static void thunar_miller_columns_view_connect_column (ThunarMillerColumnsView *view,
                                                       GtkWidget               *column_widget);
static void thunar_miller_columns_view_sync_action_directory (ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_update_file_drag_mode (ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_drag_leave (GtkWidget               *widget,
                                                   GdkDragContext          *context,
                                                   guint                    timestamp,
                                                   ThunarMillerColumnsView *view);

enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_ZOOM_LEVEL,
  PROP_LOADING,
  PROP_SORT_COLUMN_DEFAULT,
  PROP_SORT_ORDER_DEFAULT,
  PROP_PRELOAD_PREVIEW_IMAGES,
  PROP_DISPLAY_NAME,
  PROP_FULL_PARSED_PATH,
  PROP_SEARCHING,
  PROP_SEARCH_MODE_ACTIVE,
  PROP_ACCEL_GROUP,
  PROP_STATUSBAR_TEXT,
};

enum
{
  START_OPEN_LOCATION,
  LAST_SIGNAL,
};

enum
{
  TARGET_TEXT_URI_LIST,
};

static guint miller_columns_view_signals[LAST_SIGNAL];

static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};

static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};

struct _ThunarMillerColumnsViewClass
{
  GtkScrolledWindowClass __parent__;

  void (*start_open_location) (ThunarMillerColumnsView *view,
                               const gchar             *initial_text);
};

struct _ThunarMillerColumnsView
{
  GtkScrolledWindow __parent__;

  ThunarPreferences *preferences;
  ThunarFile        *current_directory;
  GList             *path_ancestors;
  GtkWidget         *viewport;
  GtkWidget         *columns_box;
  GList             *columns;
  gint               active_column_index;
  gboolean           show_hidden;
  gboolean           loading;
  gboolean           rebuilding;
  gboolean           preload_preview_images;
  ThunarZoomLevel    zoom_level;
  GtkAccelGroup     *accel_group;
  ThunarHistory     *history;
  ThunarFile        *pending_directory;
  GList             *drag_g_file_list;
  GList             *drop_file_list;
  GtkWidget         *drop_highlight_column;
  guint              pending_directory_source_id;
  gint               pending_directory_column_index;
  gint               applying_directory_change_column_index;
  gboolean           pending_grab_focus;
  gboolean           drop_data_ready;
  gboolean           drop_occurred;
  gchar             *statusbar_text;
};

G_DEFINE_TYPE_WITH_CODE (ThunarMillerColumnsView, thunar_miller_columns_view, GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_miller_columns_view_navigator_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_miller_columns_view_component_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW, thunar_miller_columns_view_view_init))

static GList *
thunar_miller_columns_view_collect_ancestors (ThunarFile *directory)
{
  GList      *ancestors = NULL;
  ThunarFile *parent;

  parent = thunar_file_get_parent (directory, NULL);
  while (parent != NULL)
    {
      ancestors = g_list_prepend (ancestors, parent);
      parent = thunar_file_get_parent (parent, NULL);
    }

  return ancestors;
}

static void
thunar_miller_columns_view_cancel_pending_directory_change (ThunarMillerColumnsView *view)
{
  if (view->pending_directory_source_id != 0)
    {
      g_source_remove (view->pending_directory_source_id);
      view->pending_directory_source_id = 0;
    }

  g_clear_object (&view->pending_directory);
  view->pending_directory_column_index = -1;
  view->pending_grab_focus = FALSE;
}

static gboolean
thunar_miller_columns_view_emit_pending_directory_change (gpointer user_data)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (user_data);
  ThunarFile              *directory;
  gboolean                 grab_focus;

  view->pending_directory_source_id = 0;

  if (view->pending_directory == NULL)
    return G_SOURCE_REMOVE;

  directory = view->pending_directory;
  view->applying_directory_change_column_index = view->pending_directory_column_index;
  grab_focus = view->pending_grab_focus;

  view->pending_directory = NULL;
  view->pending_directory_column_index = -1;
  view->pending_grab_focus = FALSE;

  thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), directory, grab_focus);
  view->applying_directory_change_column_index = -1;
  g_object_unref (directory);

  return G_SOURCE_REMOVE;
}

static void
thunar_miller_columns_view_queue_directory_change (ThunarMillerColumnsView *view,
                                                   ThunarFile              *directory,
                                                   gint                     column_index,
                                                   gboolean                 grab_focus)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMNS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_FILE (directory));

  if (view->pending_directory != NULL && view->pending_directory == directory)
    {
      view->pending_directory_column_index = column_index;
      view->pending_grab_focus = view->pending_grab_focus || grab_focus;
      return;
    }

  g_set_object (&view->pending_directory, directory);
  view->pending_directory_column_index = column_index;
  view->pending_grab_focus = grab_focus;

  if (view->pending_directory_source_id == 0)
    view->pending_directory_source_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                                         thunar_miller_columns_view_emit_pending_directory_change,
                                                         g_object_ref (view),
                                                         g_object_unref);
}

static void
thunar_miller_columns_view_set_active_column (ThunarMillerColumnsView *view,
                                              gint                     active_column_index,
                                              gboolean                 grab_focus)
{
  GList *lp;
  gint   i;

  view->active_column_index = active_column_index;

  for (lp = view->columns, i = 0; lp != NULL; lp = lp->next, ++i)
    {
      thunar_miller_column_set_active (THUNAR_MILLER_COLUMN (lp->data),
                                       i == active_column_index && grab_focus);
      if (i != active_column_index)
        thunar_miller_column_set_selected_file (THUNAR_MILLER_COLUMN (lp->data), NULL);
    }

  thunar_miller_columns_view_sync_action_directory (view);

  if (grab_focus)
    {
      ThunarMillerColumn *active_column;

      active_column = g_list_nth_data (view->columns, active_column_index);
      if (active_column != NULL)
        thunar_miller_column_grab_focus (active_column);
    }
}

static void
thunar_miller_columns_view_sync_action_directory (ThunarMillerColumnsView *view)
{
  ThunarActionManager *action_mgr;
  ThunarMillerColumn  *active_column;
  ThunarFile          *directory;
  GtkWidget           *window;

  active_column = g_list_nth_data (view->columns, view->active_column_index);
  if (active_column == NULL)
    return;

  directory = thunar_miller_column_get_directory (active_column);
  if (directory == NULL)
    return;

  window = gtk_widget_get_toplevel (GTK_WIDGET (view));
  if (!THUNAR_IS_WINDOW (window))
    return;

  action_mgr = thunar_window_get_action_manager (THUNAR_WINDOW (window));
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (action_mgr), directory, FALSE);
}

static ThunarMillerColumn *
thunar_miller_columns_view_get_column_for_drag_widget (GtkWidget *widget)
{
  gpointer column;

  column = g_object_get_data (G_OBJECT (widget), "thunar-miller-column");
  return THUNAR_IS_MILLER_COLUMN (column) ? THUNAR_MILLER_COLUMN (column) : NULL;
}

static void
thunar_miller_columns_view_set_drop_highlight_column (ThunarMillerColumnsView *view,
                                                      ThunarMillerColumn      *column)
{
  GtkWidget *widget = column != NULL ? GTK_WIDGET (column) : NULL;
  GtkWidget *tree_view;

  if (view->drop_highlight_column == widget)
    return;

  if (view->drop_highlight_column != NULL)
    {
      gtk_style_context_remove_class (gtk_widget_get_style_context (view->drop_highlight_column),
                                      "miller-column-drop-target");
      tree_view = thunar_miller_column_get_tree_view (THUNAR_MILLER_COLUMN (view->drop_highlight_column));
      if (tree_view != NULL)
        gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree_view), NULL, 0);
    }

  view->drop_highlight_column = widget;

  if (view->drop_highlight_column != NULL)
    gtk_style_context_add_class (gtk_widget_get_style_context (view->drop_highlight_column),
                                 "miller-column-drop-target");
}

static void
thunar_miller_columns_view_update_opened_files (ThunarMillerColumnsView *view)
{
  GList      *lp;
  ThunarFile *opened_file;

  for (lp = view->columns; lp != NULL; lp = lp->next)
    {
      opened_file = lp->next != NULL
                  ? thunar_miller_column_get_directory (THUNAR_MILLER_COLUMN (lp->next->data))
                  : NULL;
      thunar_miller_column_set_opened_file (THUNAR_MILLER_COLUMN (lp->data), opened_file);
    }
}

static GdkDragAction
thunar_miller_columns_view_get_dest_actions (ThunarMillerColumnsView *view,
                                             ThunarMillerColumn      *column,
                                             GdkDragContext          *context,
                                             gint                     x,
                                             gint                     y,
                                             guint                    timestamp,
                                             ThunarFile             **file_return)
{
  GdkDragAction  actions = 0;
  GdkDragAction  action = 0;
  GtkTreePath   *path = NULL;
  GtkWidget     *tree_view;
  ThunarFile    *file;

  file = thunar_miller_column_get_drop_file (column, x, y, &path);
  if (file != NULL)
    {
      actions = thunar_file_accepts_drop (file, view->drop_file_list, context, &action);
      if (actions != 0 && file_return != NULL)
        *file_return = g_object_ref (file);
    }

  if (action == 0 && path != NULL)
    {
      gtk_tree_path_free (path);
      path = NULL;
    }

  thunar_miller_column_set_drop_file (column, action != 0 ? file : NULL);
  thunar_miller_columns_view_set_drop_highlight_column (view, action != 0 && path == NULL ? column : NULL);
  tree_view = thunar_miller_column_get_tree_view (column);
  if (tree_view != NULL)
    gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree_view),
                                     action != 0 ? path : NULL,
                                     GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
  gdk_drag_status (context, action, timestamp);

  if (file != NULL)
    g_object_unref (file);
  if (path != NULL)
    gtk_tree_path_free (path);

  return actions;
}

static void
thunar_miller_columns_view_drag_data_get (GtkWidget               *widget,
                                          GdkDragContext          *context,
                                          GtkSelectionData        *selection_data,
                                          guint                    info,
                                          guint                    timestamp,
                                          ThunarMillerColumnsView *view)
{
  gchar **uris;

  if (info != TARGET_TEXT_URI_LIST || view->drag_g_file_list == NULL)
    return;

  uris = thunar_g_file_list_to_stringv (view->drag_g_file_list);
  gtk_selection_data_set_uris (selection_data, uris);
  g_strfreev (uris);
}

static void
thunar_miller_columns_view_drag_begin (GtkWidget               *widget,
                                       GdkDragContext          *context,
                                       ThunarMillerColumnsView *view)
{
  ThunarMillerColumn *column;
  GList              *selected_files;

  thunar_g_list_free_full (view->drag_g_file_list);
  view->drag_g_file_list = NULL;

  column = thunar_miller_columns_view_get_column_for_drag_widget (widget);
  if (column == NULL)
    return;

  selected_files = thunar_miller_column_get_selected_files (column);
  view->drag_g_file_list = thunar_file_list_to_thunar_g_file_list (selected_files);
  g_list_free_full (selected_files, g_object_unref);
}

static void
thunar_miller_columns_view_drag_data_delete (GtkWidget               *widget,
                                             GdkDragContext          *context,
                                             ThunarMillerColumnsView *view)
{
  g_signal_stop_emission_by_name (G_OBJECT (widget), "drag-data-delete");
}

static void
thunar_miller_columns_view_drag_end (GtkWidget               *widget,
                                     GdkDragContext          *context,
                                     ThunarMillerColumnsView *view)
{
  thunar_g_list_free_full (view->drag_g_file_list);
  view->drag_g_file_list = NULL;
}

static gboolean
thunar_miller_columns_view_drag_motion (GtkWidget               *widget,
                                        GdkDragContext          *context,
                                        gint                     x,
                                        gint                     y,
                                        guint                    timestamp,
                                        ThunarMillerColumnsView *view)
{
  ThunarMillerColumn *column;
  GdkAtom             target;

  column = thunar_miller_columns_view_get_column_for_drag_widget (widget);
  if (column == NULL)
    return FALSE;

  if (!view->drop_data_ready)
    {
      target = gtk_drag_dest_find_target (widget, context, NULL);
      if (target == gdk_atom_intern_static_string ("text/uri-list"))
        gtk_drag_get_data (widget, context, target, timestamp);

      gdk_drag_status (context, 0, timestamp);
      return TRUE;
    }

  thunar_miller_columns_view_get_dest_actions (view, column, context, x, y, timestamp, NULL);
  return TRUE;
}

static gboolean
thunar_miller_columns_view_receive_text_uri_list (GtkWidget               *widget,
                                                  GdkDragContext          *context,
                                                  gint                     x,
                                                  gint                     y,
                                                  guint                    timestamp,
                                                  ThunarMillerColumnsView *view)
{
  ThunarMillerColumn *column;
  GdkDragAction       actions;
  GdkDragAction       action;
  ThunarFile         *file = NULL;
  gint                column_index;
  gboolean            succeed = FALSE;

  column = thunar_miller_columns_view_get_column_for_drag_widget (widget);
  if (column == NULL)
    return FALSE;

  actions = thunar_miller_columns_view_get_dest_actions (view, column, context, x, y, timestamp, &file);
  if ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0 && file != NULL)
    {
      action = (gdk_drag_context_get_selected_action (context) == GDK_ACTION_ASK)
             ? thunar_dnd_ask (GTK_WIDGET (view), file, view->drop_file_list, actions)
             : gdk_drag_context_get_selected_action (context);

      if (action != 0)
        {
          column_index = g_list_index (view->columns, column);
          if (column_index >= 0)
            thunar_miller_columns_view_set_active_column (view, column_index, FALSE);

          succeed = thunar_dnd_perform (GTK_WIDGET (view), file, view->drop_file_list, action, NULL);
        }
    }

  if (file != NULL)
    g_object_unref (file);

  return succeed;
}

static gboolean
thunar_miller_columns_view_drag_drop (GtkWidget               *widget,
                                      GdkDragContext          *context,
                                      gint                     x,
                                      gint                     y,
                                      guint                    timestamp,
                                      ThunarMillerColumnsView *view)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target == GDK_NONE)
    return FALSE;

  view->drop_occurred = TRUE;
  gtk_drag_get_data (widget, context, target, timestamp);

  return TRUE;
}

static void
thunar_miller_columns_view_drag_data_received (GtkWidget               *widget,
                                               GdkDragContext          *context,
                                               gint                     x,
                                               gint                     y,
                                               GtkSelectionData        *selection_data,
                                               guint                    info,
                                               guint                    timestamp,
                                               ThunarMillerColumnsView *view)
{
  gboolean succeed = FALSE;

  if (!view->drop_data_ready)
    {
      if (info == TARGET_TEXT_URI_LIST
          && gtk_selection_data_get_format (selection_data) == 8
          && gtk_selection_data_get_length (selection_data) > 0)
        {
          view->drop_file_list = thunar_g_file_list_new_from_string ((const gchar *) gtk_selection_data_get_data (selection_data));
          view->drop_data_ready = TRUE;
        }
    }

  if (view->drop_occurred)
    {
      view->drop_occurred = FALSE;
      if (info == TARGET_TEXT_URI_LIST)
        succeed = thunar_miller_columns_view_receive_text_uri_list (widget, context, x, y, timestamp, view);

      gtk_drag_finish (context, succeed, FALSE, timestamp);
      thunar_miller_columns_view_drag_leave (widget, context, timestamp, view);
    }
}

static void
thunar_miller_columns_view_drag_leave (GtkWidget               *widget,
                                       GdkDragContext          *context,
                                       guint                    timestamp,
                                       ThunarMillerColumnsView *view)
{
  ThunarMillerColumn *column;

  column = thunar_miller_columns_view_get_column_for_drag_widget (widget);
  if (column != NULL)
    thunar_miller_column_set_drop_file (column, NULL);
  thunar_miller_columns_view_set_drop_highlight_column (view, NULL);

  if (view->drop_data_ready)
    {
      thunar_g_list_free_full (view->drop_file_list);
      view->drop_file_list = NULL;
      view->drop_data_ready = FALSE;
    }

  view->drop_occurred = FALSE;
}

static void
thunar_miller_columns_view_update_file_drag_mode_for_column (ThunarMillerColumnsView *view,
                                                             ThunarMillerColumn      *column)
{
  ThunarFileDragMode drag_mode;
  GtkWidget         *tree_view;

  tree_view = thunar_miller_column_get_tree_view (column);
  if (tree_view == NULL)
    return;

  g_object_get (G_OBJECT (view->preferences), "misc-file-drag-mode", &drag_mode, NULL);
  if (drag_mode == THUNAR_FILE_DRAG_MODE_MENU_ALWAYS)
    gtk_drag_source_set (tree_view, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets),
                         GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
  else if (drag_mode == THUNAR_FILE_DRAG_MODE_MENU_CONDITIONAL)
    gtk_drag_source_set (tree_view, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets),
                         GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
  else if (drag_mode == THUNAR_FILE_DRAG_MODE_DISABLED)
    gtk_drag_source_unset (tree_view);
  else
    g_warning ("Unsupported value received for thunar property misc-file-drag-mode");
}

static void
thunar_miller_columns_view_update_file_drag_mode (ThunarMillerColumnsView *view)
{
  GList *lp;

  for (lp = view->columns; lp != NULL; lp = lp->next)
    thunar_miller_columns_view_update_file_drag_mode_for_column (view, THUNAR_MILLER_COLUMN (lp->data));
}

static void
thunar_miller_columns_view_scroll_to_active_column (ThunarMillerColumnsView *view)
{
  GtkAdjustment *hadjustment;
  GtkWidget     *column;
  GtkAllocation  allocation;
  gdouble        value;

  if (view->active_column_index < 0)
    return;

  column = g_list_nth_data (view->columns, view->active_column_index);
  if (column == NULL)
    return;

  hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (view));
  gtk_widget_get_allocation (column, &allocation);

  value = allocation.x + allocation.width - gtk_adjustment_get_page_size (hadjustment);
  gtk_adjustment_set_value (hadjustment, MAX (value, 0.0));
}

static void
thunar_miller_columns_view_update_statusbar_text_internal (ThunarMillerColumnsView *view)
{
  ThunarMillerColumn *active_column;
  ThunarFolder       *folder;
  GHashTable         *files_table;
  GHashTable         *g_files;
  GHashTableIter      iter;
  gpointer            key;
  GList              *selected_files;
  gchar              *text = NULL;
  gchar              *date_custom_style = NULL;
  ThunarDateStyle     date_style;
  guint               active_info;
  gboolean            file_size_binary;

  if (view->loading)
    {
      text = g_strdup (_("Loading folder contents..."));
      goto done;
    }

  selected_files = thunar_view_get_selected_files (THUNAR_VIEW (view));
  if (g_list_length (selected_files) == 1)
    {
      text = thunar_util_get_statusbar_text_for_single_file (selected_files->data);
      g_list_free_full (selected_files, g_object_unref);
      goto done;
    }
  else if (selected_files != NULL)
    {
      text = g_strdup_printf (ngettext ("%u item selected", "%u items selected", g_list_length (selected_files)),
                              g_list_length (selected_files));
      g_list_free_full (selected_files, g_object_unref);
      goto done;
    }

  g_list_free_full (selected_files, g_object_unref);

  active_column = g_list_nth_data (view->columns, view->active_column_index);
  if (active_column == NULL)
    goto done;

  folder = thunar_miller_column_get_folder (active_column);
  if (folder == NULL)
    goto done;

  files_table = thunar_folder_get_files (folder);
  g_files = g_hash_table_new (g_direct_hash, NULL);

  g_hash_table_iter_init (&iter, files_table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_hash_table_add (g_files, thunar_file_get_file (THUNAR_FILE (key)));

  g_object_get (G_OBJECT (view->preferences),
                "misc-date-style", &date_style,
                "misc-date-custom-style", &date_custom_style,
                "misc-file-size-binary", &file_size_binary,
                "misc-status-bar-active-info", &active_info,
                NULL);

  text = thunar_util_get_statusbar_text_for_files (g_files,
                                                   view->show_hidden,
                                                   file_size_binary,
                                                   date_style,
                                                   date_custom_style,
                                                   active_info);
  g_hash_table_destroy (g_files);
  g_free (date_custom_style);

done:
  g_free (view->statusbar_text);
  view->statusbar_text = text;
  g_object_notify (G_OBJECT (view), "statusbar-text");
}

static void
thunar_miller_columns_view_update_loading (ThunarMillerColumnsView *view)
{
  GList    *lp;
  gboolean  loading = FALSE;
  gboolean  column_loading;

  for (lp = view->columns; lp != NULL; lp = lp->next)
    {
      g_object_get (lp->data, "loading", &column_loading, NULL);
      if (column_loading)
        {
          loading = TRUE;
          break;
        }
    }

  if (view->loading != loading)
    {
      view->loading = loading;
      g_object_notify (G_OBJECT (view), "loading");
    }
}

static void
thunar_miller_columns_view_column_notify_loading (ThunarMillerColumn      *column,
                                                  GParamSpec              *pspec,
                                                  ThunarMillerColumnsView *view)
{
  thunar_miller_columns_view_update_loading (view);
  thunar_miller_columns_view_update_statusbar_text_internal (view);
}

static void
thunar_miller_columns_view_clear_columns (ThunarMillerColumnsView *view)
{
  GList *columns;
  GList *lp;

  columns = view->columns;
  view->columns = NULL;
  view->active_column_index = -1;

  for (lp = columns; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_data (lp->data, view);
      gtk_container_remove (GTK_CONTAINER (view->columns_box), GTK_WIDGET (lp->data));
    }

  g_list_free (columns);
}

static void
thunar_miller_columns_view_remove_columns_after (ThunarMillerColumnsView *view,
                                                 gint                     column_index)
{
  GList *link;
  GList *tail;
  GList *lp;

  link = g_list_nth (view->columns, column_index);
  if (link == NULL)
    return;

  tail = link->next;
  link->next = NULL;

  if (tail != NULL)
    tail->prev = NULL;

  for (lp = tail; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_data (lp->data, view);
      gtk_container_remove (GTK_CONTAINER (view->columns_box), GTK_WIDGET (lp->data));
    }

  g_list_free (tail);
}

static GtkWidget *
thunar_miller_columns_view_append_column (ThunarMillerColumnsView *view,
                                          ThunarFile              *directory)
{
  GtkWidget *column_widget;

  column_widget = thunar_miller_column_new ();
  thunar_miller_column_set_show_hidden (THUNAR_MILLER_COLUMN (column_widget), view->show_hidden);
  thunar_miller_column_set_zoom_level (THUNAR_MILLER_COLUMN (column_widget), view->zoom_level);
  thunar_miller_column_set_directory (THUNAR_MILLER_COLUMN (column_widget), directory);
  thunar_miller_columns_view_connect_column (view, column_widget);
  gtk_box_pack_start (GTK_BOX (view->columns_box), column_widget, FALSE, FALSE, 0);
  gtk_widget_show (column_widget);
  view->columns = g_list_append (view->columns, column_widget);

  return column_widget;
}

static gboolean
thunar_miller_columns_view_update_columns_incremental (ThunarMillerColumnsView *view,
                                                       ThunarFile              *directory,
                                                       gint                     source_column_index,
                                                       gboolean                 grab_focus)
{
  ThunarFile *source_directory;
  GtkWidget  *source_column;

  if (directory == NULL || source_column_index < 0)
    return FALSE;

  source_column = g_list_nth_data (view->columns, source_column_index);
  if (source_column == NULL)
    return FALSE;

  source_directory = thunar_miller_column_get_directory (THUNAR_MILLER_COLUMN (source_column));

  view->rebuilding = TRUE;
  thunar_miller_columns_view_remove_columns_after (view, source_column_index);
  if (source_directory == directory)
    thunar_miller_columns_view_set_active_column (view, source_column_index, grab_focus);
  else
    {
      thunar_miller_columns_view_append_column (view, directory);
      thunar_miller_columns_view_set_active_column (view, source_column_index, grab_focus);
    }
  thunar_miller_columns_view_update_opened_files (view);
  view->rebuilding = FALSE;

  thunar_miller_columns_view_scroll_to_active_column (view);
  thunar_miller_columns_view_update_loading (view);
  thunar_miller_columns_view_update_statusbar_text_internal (view);

  return TRUE;
}

static void
thunar_miller_columns_view_column_file_activated (ThunarMillerColumn      *column,
                                                  ThunarFile              *file,
                                                  ThunarMillerColumnsView *view)
{
  ThunarActionManager *action_mgr;
  GtkWidget           *window;
  gint                 column_index;

  if (thunar_file_is_directory (file))
    {
      column_index = g_list_index (view->columns, column);
      thunar_miller_columns_view_queue_directory_change (view, file, column_index, TRUE);
      return;
    }

  window = gtk_widget_get_toplevel (GTK_WIDGET (view));
  action_mgr = thunar_window_get_action_manager (THUNAR_WINDOW (window));
  thunar_action_manager_activate_selected_files (action_mgr, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY, NULL, TRUE);
}

static void
thunar_miller_columns_view_column_context_menu (ThunarMillerColumn      *column,
                                                ThunarMillerColumnsView *view)
{
  GtkWidget  *window;
  ThunarMenu *menu;
  GList      *selected_files;

  window = gtk_widget_get_toplevel (GTK_WIDGET (view));
  selected_files = thunar_view_get_selected_files (THUNAR_VIEW (view));

  menu = g_object_new (THUNAR_TYPE_MENU,
                       "menu-type", THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW,
                       "action_mgr", thunar_window_get_action_manager (THUNAR_WINDOW (window)),
                       NULL);

  if (selected_files != NULL)
    {
      thunar_menu_add_sections (menu, THUNAR_MENU_SECTION_OPEN
                                      | THUNAR_MENU_SECTION_SENDTO
                                      | THUNAR_MENU_SECTION_CUT
                                      | THUNAR_MENU_SECTION_COPY_PASTE
                                      | THUNAR_MENU_SECTION_TRASH_DELETE
                                      | THUNAR_MENU_SECTION_RENAME
                                      | THUNAR_MENU_SECTION_RESTORE
                                      | THUNAR_MENU_SECTION_REMOVE_FROM_RECENT
                                      | THUNAR_MENU_SECTION_CUSTOM_ACTIONS
                                      | THUNAR_MENU_SECTION_PROPERTIES);
    }
  else
    {
      thunar_menu_add_sections (menu, THUNAR_MENU_SECTION_CREATE_NEW_FILES
                                      | THUNAR_MENU_SECTION_COPY_PASTE
                                      | THUNAR_MENU_SECTION_EMPTY_TRASH
                                      | THUNAR_MENU_SECTION_CUSTOM_ACTIONS
                                      | THUNAR_MENU_SECTION_ZOOM
                                      | THUNAR_MENU_SECTION_PROPERTIES);
    }

  thunar_gtk_menu_hide_accel_labels (GTK_MENU (menu));
  gtk_widget_show_all (GTK_WIDGET (menu));
  thunar_window_redirect_menu_tooltips_to_statusbar (THUNAR_WINDOW (window), GTK_MENU (menu));
  thunar_gtk_menu_run (GTK_MENU (menu));

  g_list_free_full (selected_files, g_object_unref);
}

static void
thunar_miller_columns_view_column_navigate_left (ThunarMillerColumn      *column,
                                                 ThunarMillerColumnsView *view)
{
  gint previous_index;

  previous_index = g_list_index (view->columns, column) - 1;
  if (previous_index < 0)
    return;

  thunar_miller_columns_view_set_active_column (view, previous_index, TRUE);
  thunar_miller_columns_view_scroll_to_active_column (view);
  thunar_miller_columns_view_update_statusbar_text_internal (view);
}

static void
thunar_miller_columns_view_column_navigate_right (ThunarMillerColumn      *column,
                                                  ThunarMillerColumnsView *view)
{
  gint        column_index;
  GtkWidget  *next_column;
  ThunarFile *selected_file = NULL;

  column_index = g_list_index (view->columns, column);
  next_column = g_list_nth_data (view->columns, column_index + 1);

  if (next_column != NULL)
    {
      thunar_miller_columns_view_set_active_column (view, column_index + 1, TRUE);
      thunar_miller_columns_view_scroll_to_active_column (view);
      thunar_miller_columns_view_update_statusbar_text_internal (view);
      return;
    }

  selected_file = thunar_miller_column_get_selected_file (column);
  if (selected_file != NULL && thunar_file_is_directory (selected_file))
    thunar_miller_columns_view_queue_directory_change (view, selected_file, column_index, TRUE);
  if (selected_file != NULL)
    g_object_unref (selected_file);
}

static void
thunar_miller_columns_view_column_focus_in (ThunarMillerColumn      *column,
                                            ThunarMillerColumnsView *view)
{
  gint column_index;

  column_index = g_list_index (view->columns, column);
  if (column_index < 0 || column_index == view->active_column_index)
    return;

  thunar_miller_columns_view_set_active_column (view, column_index, FALSE);
  g_object_notify (G_OBJECT (view), "selected-files");
  thunar_miller_columns_view_update_statusbar_text_internal (view);
}

static void
thunar_miller_columns_view_column_selection_changed (ThunarMillerColumn      *column,
                                                     ThunarMillerColumnsView *view)
{
  ThunarFile *next_directory = NULL;
  ThunarFile *selected_file;
  GtkWidget  *next_column;
  gint        column_index;

  if (view->rebuilding)
    return;

  column_index = g_list_index (view->columns, column);
  if (column_index < 0)
    return;

  thunar_miller_columns_view_set_active_column (view, column_index, FALSE);

  selected_file = thunar_miller_column_get_selected_file (column);

  if (selected_file != NULL && thunar_file_is_directory (selected_file))
    {
      next_column = g_list_nth_data (view->columns, column_index + 1);
      if (next_column != NULL)
        next_directory = thunar_miller_column_get_directory (THUNAR_MILLER_COLUMN (next_column));

      if (selected_file != view->current_directory && selected_file != next_directory)
        thunar_miller_columns_view_queue_directory_change (view, selected_file, column_index, TRUE);
    }

  if (selected_file != NULL)
    g_object_unref (selected_file);

  g_object_notify (G_OBJECT (view), "selected-files");
  thunar_miller_columns_view_update_statusbar_text_internal (view);
}

static void
thunar_miller_columns_view_connect_column (ThunarMillerColumnsView *view,
                                           GtkWidget               *column_widget)
{
  GtkWidget *tree_view;

  g_signal_connect (column_widget, "file-activated",
                    G_CALLBACK (thunar_miller_columns_view_column_file_activated), view);
  g_signal_connect (column_widget, "selection-changed",
                    G_CALLBACK (thunar_miller_columns_view_column_selection_changed), view);
  g_signal_connect (column_widget, "context-menu",
                    G_CALLBACK (thunar_miller_columns_view_column_context_menu), view);
  g_signal_connect (column_widget, "navigate-left",
                    G_CALLBACK (thunar_miller_columns_view_column_navigate_left), view);
  g_signal_connect (column_widget, "navigate-right",
                    G_CALLBACK (thunar_miller_columns_view_column_navigate_right), view);
  g_signal_connect (column_widget, "focus-in",
                    G_CALLBACK (thunar_miller_columns_view_column_focus_in), view);
  g_signal_connect (column_widget, "notify::loading",
                    G_CALLBACK (thunar_miller_columns_view_column_notify_loading), view);

  tree_view = thunar_miller_column_get_tree_view (THUNAR_MILLER_COLUMN (column_widget));
  g_object_set_data (G_OBJECT (tree_view), "thunar-miller-column", column_widget);

  gtk_drag_dest_set (tree_view, 0, drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (tree_view), "drag-drop",
                    G_CALLBACK (thunar_miller_columns_view_drag_drop), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-data-received",
                    G_CALLBACK (thunar_miller_columns_view_drag_data_received), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-leave",
                    G_CALLBACK (thunar_miller_columns_view_drag_leave), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-motion",
                    G_CALLBACK (thunar_miller_columns_view_drag_motion), view);

  thunar_miller_columns_view_update_file_drag_mode_for_column (view, THUNAR_MILLER_COLUMN (column_widget));
  g_signal_connect (G_OBJECT (tree_view), "drag-begin",
                    G_CALLBACK (thunar_miller_columns_view_drag_begin), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-data-get",
                    G_CALLBACK (thunar_miller_columns_view_drag_data_get), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-data-delete",
                    G_CALLBACK (thunar_miller_columns_view_drag_data_delete), view);
  g_signal_connect (G_OBJECT (tree_view), "drag-end",
                    G_CALLBACK (thunar_miller_columns_view_drag_end), view);
}

static void
thunar_miller_columns_view_update_columns (ThunarMillerColumnsView *view,
                                           gboolean                 grab_focus)
{
  view->rebuilding = TRUE;
  thunar_miller_columns_view_clear_columns (view);

  if (view->current_directory == NULL)
    {
      view->rebuilding = FALSE;
      thunar_miller_columns_view_update_loading (view);
      thunar_miller_columns_view_update_statusbar_text_internal (view);
      return;
    }

  thunar_miller_columns_view_append_column (view, view->current_directory);
  thunar_miller_columns_view_update_opened_files (view);

  thunar_miller_columns_view_set_active_column (view, g_list_length (view->columns) - 1, grab_focus);
  view->rebuilding = FALSE;

  thunar_miller_columns_view_scroll_to_active_column (view);
  thunar_miller_columns_view_update_loading (view);
  thunar_miller_columns_view_update_statusbar_text_internal (view);
}

static void
thunar_miller_columns_view_dispose (GObject *object)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (object);

  thunar_miller_columns_view_set_drop_highlight_column (view, NULL);
  thunar_miller_columns_view_clear_columns (view);
  thunar_miller_columns_view_cancel_pending_directory_change (view);

  if (view->history != NULL)
    g_signal_handlers_disconnect_by_func (view->history,
                                          thunar_navigator_change_directory,
                                          view);
  if (view->preferences != NULL)
    g_signal_handlers_disconnect_by_func (view->preferences,
                                          thunar_miller_columns_view_update_file_drag_mode,
                                          view);

  (*G_OBJECT_CLASS (thunar_miller_columns_view_parent_class)->dispose) (object);
}

static void
thunar_miller_columns_view_finalize (GObject *object)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (object);

  if (view->current_directory != NULL)
    g_object_unref (view->current_directory);
  g_clear_object (&view->pending_directory);
  thunar_g_list_free_full (view->drag_g_file_list);
  thunar_g_list_free_full (view->drop_file_list);
  if (view->accel_group != NULL)
    g_object_unref (view->accel_group);
  if (view->preferences != NULL)
    g_object_unref (view->preferences);
  if (view->history != NULL)
    g_object_unref (view->history);

  g_list_free_full (view->path_ancestors, g_object_unref);
  g_list_free (view->columns);
  g_free (view->statusbar_text);

  (*G_OBJECT_CLASS (thunar_miller_columns_view_parent_class)->finalize) (object);
}

static void
thunar_miller_columns_view_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, view->current_directory);
      break;

    case PROP_SELECTED_FILES:
      g_value_take_boxed (value, thunar_view_get_selected_files (THUNAR_VIEW (view)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, view->show_hidden);
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_enum (value, view->zoom_level);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, view->loading);
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value,
                          view->current_directory != NULL
                          ? thunar_file_get_display_name (view->current_directory)
                          : NULL);
      break;

    case PROP_FULL_PARSED_PATH:
      if (view->current_directory != NULL)
        g_value_take_string (value, g_file_get_parse_name (thunar_file_get_file (view->current_directory)));
      else
        g_value_set_string (value, NULL);
      break;

    case PROP_SEARCHING:
    case PROP_SEARCH_MODE_ACTIVE:
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_ACCEL_GROUP:
      g_value_set_object (value, view->accel_group);
      break;

    case PROP_STATUSBAR_TEXT:
      g_value_set_string (value, view->statusbar_text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_miller_columns_view_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view), g_value_get_object (value), FALSE);
      break;

    case PROP_SELECTED_FILES:
      thunar_view_set_selected_files (THUNAR_VIEW (view), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_view_set_show_hidden (THUNAR_VIEW (view), g_value_get_boolean (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_view_set_zoom_level (THUNAR_VIEW (view), g_value_get_enum (value));
      break;

    case PROP_SORT_COLUMN_DEFAULT:
    case PROP_SORT_ORDER_DEFAULT:
      break;

    case PROP_PRELOAD_PREVIEW_IMAGES:
      view->preload_preview_images = g_value_get_boolean (value);
      break;

    case PROP_ACCEL_GROUP:
      if (view->accel_group != NULL)
        g_object_unref (view->accel_group);
      view->accel_group = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static ThunarFile *
thunar_miller_columns_view_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_MILLER_COLUMNS_VIEW (navigator)->current_directory;
}

static void
thunar_miller_columns_view_set_current_directory (ThunarNavigator *navigator,
                                                  ThunarFile      *current_directory,
                                                  gboolean         grab_focus)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (navigator);

  if (view->current_directory == current_directory)
    {
      if (grab_focus)
        thunar_miller_columns_view_set_active_column (view, view->active_column_index, TRUE);
      return;
    }

  if (view->current_directory != NULL)
    g_object_unref (view->current_directory);
  g_list_free_full (view->path_ancestors, g_object_unref);

  view->current_directory = current_directory != NULL ? g_object_ref (current_directory) : NULL;
  view->path_ancestors = current_directory != NULL
                       ? thunar_miller_columns_view_collect_ancestors (current_directory)
                       : NULL;

  if (!thunar_miller_columns_view_update_columns_incremental (view,
                                                              view->current_directory,
                                                              view->applying_directory_change_column_index,
                                                              grab_focus))
    thunar_miller_columns_view_update_columns (view, grab_focus);

  if (view->history != NULL)
    thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view->history), current_directory, FALSE);

  g_object_notify (G_OBJECT (view), "current-directory");
  g_object_notify (G_OBJECT (view), "display-name");
  g_object_notify (G_OBJECT (view), "full-parsed-path");
}

static GList *
thunar_miller_columns_view_get_selected_files_component (ThunarComponent *component)
{
  return thunar_view_get_selected_files (THUNAR_VIEW (component));
}

static void
thunar_miller_columns_view_set_selected_files_component (ThunarComponent *component,
                                                         GList           *selected_files)
{
  thunar_view_set_selected_files (THUNAR_VIEW (component), selected_files);
}

static gboolean
thunar_miller_columns_view_get_loading (ThunarView *view)
{
  return THUNAR_MILLER_COLUMNS_VIEW (view)->loading;
}

static gboolean
thunar_miller_columns_view_get_show_hidden (ThunarView *view)
{
  return THUNAR_MILLER_COLUMNS_VIEW (view)->show_hidden;
}

static void
thunar_miller_columns_view_set_show_hidden (ThunarView *view,
                                            gboolean    show_hidden)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  GList                   *lp;

  if (miller_view->show_hidden == show_hidden)
    return;

  miller_view->show_hidden = show_hidden;
  for (lp = miller_view->columns; lp != NULL; lp = lp->next)
    thunar_miller_column_set_show_hidden (THUNAR_MILLER_COLUMN (lp->data), show_hidden);

  g_object_notify (G_OBJECT (view), "show-hidden");
  thunar_miller_columns_view_update_statusbar_text_internal (miller_view);
}

static ThunarZoomLevel
thunar_miller_columns_view_get_zoom_level (ThunarView *view)
{
  return THUNAR_MILLER_COLUMNS_VIEW (view)->zoom_level;
}

static void
thunar_miller_columns_view_set_zoom_level (ThunarView     *view,
                                           ThunarZoomLevel zoom_level)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  GList                   *lp;

  if (miller_view->zoom_level == zoom_level)
    return;

  miller_view->zoom_level = zoom_level;
  for (lp = miller_view->columns; lp != NULL; lp = lp->next)
    thunar_miller_column_set_zoom_level (THUNAR_MILLER_COLUMN (lp->data), zoom_level);

  g_object_notify (G_OBJECT (view), "zoom-level");
}

static void
thunar_miller_columns_view_reset_zoom_level (ThunarView *view)
{
  thunar_miller_columns_view_set_zoom_level (view, THUNAR_ZOOM_LEVEL_100_PERCENT);
}

static void
thunar_miller_columns_view_reload (ThunarView *view,
                                   gboolean    reload_info)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);

  if (miller_view->current_directory != NULL)
    thunar_miller_columns_view_update_columns (miller_view, FALSE);
}

static gboolean
thunar_miller_columns_view_get_visible_range (ThunarView  *view,
                                              ThunarFile **start_file,
                                              ThunarFile **end_file)
{
  if (start_file != NULL)
    *start_file = NULL;
  if (end_file != NULL)
    *end_file = NULL;

  return FALSE;
}

static void
thunar_miller_columns_view_scroll_to_file (ThunarView *view,
                                           ThunarFile *file,
                                           gboolean    select,
                                           gboolean    use_align,
                                           gfloat      row_align,
                                           gfloat      col_align)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  ThunarMillerColumn      *column;

  column = g_list_last (miller_view->columns) != NULL
         ? THUNAR_MILLER_COLUMN (g_list_last (miller_view->columns)->data)
         : NULL;

  if (column != NULL && select)
    {
      thunar_miller_column_set_selected_file (column, file);
      g_object_notify (G_OBJECT (view), "selected-files");
      thunar_miller_columns_view_update_statusbar_text_internal (miller_view);
    }
}

static GList *
thunar_miller_columns_view_get_selected_files_view (ThunarView *view)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  ThunarMillerColumn      *column;

  column = g_list_nth_data (miller_view->columns, miller_view->active_column_index);
  return column != NULL ? thunar_miller_column_get_selected_files (column) : NULL;
}

static void
thunar_miller_columns_view_set_selected_files_view (ThunarView *view,
                                                    GList      *selected_files)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  ThunarMillerColumn      *column;

  column = g_list_nth_data (miller_view->columns, miller_view->active_column_index);
  if (column == NULL)
    column = g_list_last (miller_view->columns) != NULL
           ? THUNAR_MILLER_COLUMN (g_list_last (miller_view->columns)->data)
           : NULL;

  if (column != NULL)
    {
      thunar_miller_column_set_selected_files (column, selected_files);
      g_object_notify (G_OBJECT (view), "selected-files");
      thunar_miller_columns_view_update_statusbar_text_internal (miller_view);
    }
}

static void
thunar_miller_columns_view_set_history_view (ThunarView    *view,
                                             ThunarHistory *history)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);

  _thunar_return_if_fail (history == NULL || THUNAR_IS_HISTORY (history));

  if (miller_view->history != NULL)
    {
      g_signal_handlers_disconnect_by_func (miller_view->history,
                                            thunar_navigator_change_directory,
                                            miller_view);
      g_object_unref (miller_view->history);
    }

  miller_view->history = history != NULL ? g_object_ref (history) : g_object_new (THUNAR_TYPE_HISTORY, NULL);
  g_signal_connect_swapped (miller_view->history, "change-directory",
                            G_CALLBACK (thunar_navigator_change_directory), miller_view);

  if (miller_view->current_directory != NULL)
    thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (miller_view->history), miller_view->current_directory, FALSE);
}

static ThunarHistory *
thunar_miller_columns_view_get_history_view (ThunarView *view)
{
  return THUNAR_MILLER_COLUMNS_VIEW (view)->history;
}

static ThunarHistory *
thunar_miller_columns_view_copy_history_view (ThunarView *view)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  return miller_view->history != NULL ? thunar_history_copy (miller_view->history) : NULL;
}

static void
thunar_miller_columns_view_update_statusbar_text_view (ThunarView *view)
{
  thunar_miller_columns_view_update_statusbar_text_internal (THUNAR_MILLER_COLUMNS_VIEW (view));
}

static void
thunar_miller_columns_view_queue_redraw_view (ThunarView *view)
{
  gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
thunar_miller_columns_view_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_miller_columns_view_get_current_directory;
  iface->set_current_directory = thunar_miller_columns_view_set_current_directory;
}

static void
thunar_miller_columns_view_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_miller_columns_view_get_selected_files_component;
  iface->set_selected_files = thunar_miller_columns_view_set_selected_files_component;
}

static void
thunar_miller_columns_view_view_init (ThunarViewIface *iface)
{
  iface->get_loading = thunar_miller_columns_view_get_loading;
  iface->get_show_hidden = thunar_miller_columns_view_get_show_hidden;
  iface->set_show_hidden = thunar_miller_columns_view_set_show_hidden;
  iface->get_zoom_level = thunar_miller_columns_view_get_zoom_level;
  iface->set_zoom_level = thunar_miller_columns_view_set_zoom_level;
  iface->reset_zoom_level = thunar_miller_columns_view_reset_zoom_level;
  iface->reload = thunar_miller_columns_view_reload;
  iface->get_visible_range = thunar_miller_columns_view_get_visible_range;
  iface->scroll_to_file = thunar_miller_columns_view_scroll_to_file;
  iface->get_selected_files = thunar_miller_columns_view_get_selected_files_view;
  iface->set_selected_files = thunar_miller_columns_view_set_selected_files_view;
  iface->set_history = thunar_miller_columns_view_set_history_view;
  iface->get_history = thunar_miller_columns_view_get_history_view;
  iface->copy_history = thunar_miller_columns_view_copy_history_view;
  iface->update_statusbar_text = thunar_miller_columns_view_update_statusbar_text_view;
  iface->queue_redraw = thunar_miller_columns_view_queue_redraw_view;
}

static void
thunar_miller_columns_view_class_init (ThunarMillerColumnsViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = thunar_miller_columns_view_dispose;
  gobject_class->finalize = thunar_miller_columns_view_finalize;
  gobject_class->get_property = thunar_miller_columns_view_get_property;
  gobject_class->set_property = thunar_miller_columns_view_set_property;

  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
  g_object_class_override_property (gobject_class, PROP_ZOOM_LEVEL, "zoom-level");

  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         "loading",
                                                         "loading",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SORT_COLUMN_DEFAULT,
                                   g_param_spec_enum ("sort-column-default",
                                                      "sort-column-default",
                                                      "sort-column-default",
                                                      THUNAR_TYPE_COLUMN,
                                                      THUNAR_COLUMN_NAME,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SORT_ORDER_DEFAULT,
                                   g_param_spec_enum ("sort-order-default",
                                                      "sort-order-default",
                                                      "sort-order-default",
                                                      GTK_TYPE_SORT_TYPE,
                                                      GTK_SORT_ASCENDING,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_PRELOAD_PREVIEW_IMAGES,
                                   g_param_spec_boolean ("preload-preview-images",
                                                         "preload-preview-images",
                                                         "preload-preview-images",
                                                         FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY_NAME,
                                   g_param_spec_string ("display-name",
                                                        "display-name",
                                                        "display-name",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_FULL_PARSED_PATH,
                                   g_param_spec_string ("full-parsed-path",
                                                        "full-parsed-path",
                                                        "full-parsed-path",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SEARCHING,
                                   g_param_spec_boolean ("searching",
                                                         "searching",
                                                         "searching",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_MODE_ACTIVE,
                                   g_param_spec_boolean ("search-mode-active",
                                                         "search-mode-active",
                                                         "search-mode-active",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_GROUP,
                                   g_param_spec_object ("accel-group",
                                                        "accel-group",
                                                        "accel-group",
                                                        GTK_TYPE_ACCEL_GROUP,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_STATUSBAR_TEXT,
                                   g_param_spec_string ("statusbar-text",
                                                        "statusbar-text",
                                                        "statusbar-text",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  miller_columns_view_signals[START_OPEN_LOCATION] =
    g_signal_new (I_ ("start-open-location"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarMillerColumnsViewClass, start_open_location),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
thunar_miller_columns_view_init (ThunarMillerColumnsView *view)
{
  ThunarZoomLevel zoom_level;

  view->preferences = thunar_preferences_get ();
  view->pending_directory_column_index = -1;
  view->applying_directory_change_column_index = -1;
  view->history = g_object_new (THUNAR_TYPE_HISTORY, NULL);
  g_signal_connect_swapped (view->history, "change-directory",
                            G_CALLBACK (thunar_navigator_change_directory), view);
  g_signal_connect_swapped (view->preferences, "notify::misc-file-drag-mode",
                            G_CALLBACK (thunar_miller_columns_view_update_file_drag_mode), view);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view), GTK_SHADOW_NONE);

  view->viewport = gtk_viewport_new (NULL, NULL);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (view->viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (view), view->viewport);
  gtk_widget_show (view->viewport);

  view->columns_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_container_add (GTK_CONTAINER (view->viewport), view->columns_box);
  gtk_widget_show (view->columns_box);

  g_object_bind_property (view->preferences, "last-show-hidden",
                          view, "show-hidden",
                          G_BINDING_SYNC_CREATE);
  g_object_get (view->preferences, "last-icon-view-zoom-level", &zoom_level, NULL);
  thunar_view_set_zoom_level (THUNAR_VIEW (view), zoom_level);
  g_object_bind_property (view, "zoom-level",
                          view->preferences, "last-icon-view-zoom-level",
                          G_BINDING_DEFAULT);
}
