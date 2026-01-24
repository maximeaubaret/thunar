/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Thunar Miller Columns Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "thunar/thunar-miller-column.h"

#include <gdk/gdkkeysyms.h>

#include "thunar/thunar-folder.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-icon-renderer.h"
#include "thunar/thunar-list-model.h"
#include "thunar/thunar-standard-view-model.h"
#include "thunar/thunar-private.h"

#include <libxfce4ui/libxfce4ui.h>

enum
{
  PROP_0,
  PROP_DIRECTORY,
  PROP_SHOW_HIDDEN,
  PROP_ACTIVE,
  PROP_LOADING,
};

enum
{
  SIGNAL_FILE_ACTIVATED,
  SIGNAL_SELECTION_CHANGED,
  SIGNAL_CONTEXT_MENU,
  SIGNAL_NAVIGATE_LEFT,
  SIGNAL_NAVIGATE_RIGHT,
  SIGNAL_FOCUS_IN,
  LAST_SIGNAL
};

static guint miller_column_signals[LAST_SIGNAL];

struct _ThunarMillerColumnClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarMillerColumn
{
  GtkScrolledWindow __parent__;

  ThunarFile              *directory;
  ThunarStandardViewModel *model;
  GtkWidget               *tree_view;
  GtkCellRenderer         *icon_renderer;
  GtkCellRenderer         *name_renderer;

  gboolean                 show_hidden;
  gboolean                 active;
  gboolean                 loading;
};

G_DEFINE_TYPE (ThunarMillerColumn, thunar_miller_column, GTK_TYPE_SCROLLED_WINDOW)

static void
thunar_miller_column_finalize (GObject *object)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (object);

  if (column->directory != NULL)
    g_object_unref (column->directory);

  if (column->model != NULL)
    g_object_unref (column->model);

  (*G_OBJECT_CLASS (thunar_miller_column_parent_class)->finalize) (object);
}

static void
thunar_miller_column_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, column->directory);
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, column->show_hidden);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, column->active);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, column->loading);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_miller_column_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      thunar_miller_column_set_directory (column, g_value_get_object (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_miller_column_set_show_hidden (column, g_value_get_boolean (value));
      break;

    case PROP_ACTIVE:
      thunar_miller_column_set_active (column, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_miller_column_selection_changed (GtkTreeSelection   *selection,
                                        ThunarMillerColumn *column)
{
  g_signal_emit (column, miller_column_signals[SIGNAL_SELECTION_CHANGED], 0);
}

static void
thunar_miller_column_row_activated (GtkTreeView        *tree_view,
                                    GtkTreePath        *path,
                                    GtkTreeViewColumn  *tree_column,
                                    ThunarMillerColumn *column)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ThunarFile   *file;

  model = gtk_tree_view_get_model (tree_view);
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);
      if (file != NULL)
        {
          g_signal_emit (column, miller_column_signals[SIGNAL_FILE_ACTIVATED], 0, file);
          g_object_unref (file);
        }
    }
}

static gboolean
thunar_miller_column_button_press (GtkWidget          *widget,
                                   GdkEventButton     *event,
                                   ThunarMillerColumn *column)
{
  GtkTreePath      *path;
  GtkTreeSelection *selection;

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                         (gint) event->x, (gint) event->y,
                                         &path, NULL, NULL, NULL))
        {
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
          if (!gtk_tree_selection_path_is_selected (selection, path))
            {
              gtk_tree_selection_unselect_all (selection);
              gtk_tree_selection_select_path (selection, path);
            }
          gtk_tree_path_free (path);
        }
    }
  else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                         (gint) event->x, (gint) event->y,
                                         &path, NULL, NULL, NULL))
        {
          if (!gtk_tree_selection_path_is_selected (selection, path))
            {
              gtk_tree_selection_unselect_all (selection);
              gtk_tree_selection_select_path (selection, path);
            }
          gtk_tree_path_free (path);
        }
      else
        {
          gtk_tree_selection_unselect_all (selection);
        }

      g_signal_emit (column, miller_column_signals[SIGNAL_CONTEXT_MENU], 0);
      return TRUE;
    }

  return FALSE;
}

static gboolean
thunar_miller_column_focus_in (GtkWidget          *widget,
                               GdkEventFocus      *event,
                               ThunarMillerColumn *column)
{
  g_signal_emit (column, miller_column_signals[SIGNAL_FOCUS_IN], 0);
  return FALSE;
}

static gboolean
thunar_miller_column_select_first_or_last (ThunarMillerColumn *column,
                                           gboolean            select_first)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  gint              n_children;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (column->tree_view));
  if (model == NULL)
    return FALSE;

  n_children = gtk_tree_model_iter_n_children (model, NULL);
  if (n_children == 0)
    return FALSE;

  if (select_first)
    path = gtk_tree_path_new_first ();
  else
    path = gtk_tree_path_new_from_indices (n_children - 1, -1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (column->tree_view), path, NULL, FALSE, 0, 0);
  gtk_tree_path_free (path);
  return TRUE;
}

static gboolean
thunar_miller_column_key_press (GtkWidget          *widget,
                                GdkEventKey        *event,
                                ThunarMillerColumn *column)
{
  switch (event->keyval)
    {
    case GDK_KEY_Left:
      g_signal_emit (column, miller_column_signals[SIGNAL_NAVIGATE_LEFT], 0);
      return TRUE;

    case GDK_KEY_Right:
      g_signal_emit (column, miller_column_signals[SIGNAL_NAVIGATE_RIGHT], 0);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static void
thunar_miller_column_class_init (ThunarMillerColumnClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_miller_column_finalize;
  gobject_class->get_property = thunar_miller_column_get_property;
  gobject_class->set_property = thunar_miller_column_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTORY,
                                   g_param_spec_object ("directory",
                                                        "directory",
                                                        "directory",
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HIDDEN,
                                   g_param_spec_boolean ("show-hidden",
                                                         "show-hidden",
                                                         "show-hidden",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "active",
                                                         "active",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         "loading",
                                                         "loading",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  miller_column_signals[SIGNAL_FILE_ACTIVATED] =
    g_signal_new ("file-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_TYPE_FILE);

  miller_column_signals[SIGNAL_SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  miller_column_signals[SIGNAL_CONTEXT_MENU] =
    g_signal_new ("context-menu",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  miller_column_signals[SIGNAL_NAVIGATE_LEFT] =
    g_signal_new ("navigate-left",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  miller_column_signals[SIGNAL_NAVIGATE_RIGHT] =
    g_signal_new ("navigate-right",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  miller_column_signals[SIGNAL_FOCUS_IN] =
    g_signal_new ("focus-in",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
thunar_miller_column_init (ThunarMillerColumn *column)
{
  GtkTreeViewColumn *tree_column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *arrow_renderer;

  column->directory = NULL;
  column->model = NULL;
  column->show_hidden = FALSE;
  column->active = FALSE;
  column->loading = FALSE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (column),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (column),
                                       GTK_SHADOW_NONE);

  column->tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (column->tree_view), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (column->tree_view), TRUE);
  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (column->tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (column), column->tree_view);
  gtk_widget_show (column->tree_view);

  g_signal_connect (column->tree_view, "row-activated",
                    G_CALLBACK (thunar_miller_column_row_activated), column);
  g_signal_connect (column->tree_view, "button-press-event",
                    G_CALLBACK (thunar_miller_column_button_press), column);
  g_signal_connect (column->tree_view, "key-press-event",
                    G_CALLBACK (thunar_miller_column_key_press), column);
  g_signal_connect (column->tree_view, "focus-in-event",
                    G_CALLBACK (thunar_miller_column_focus_in), column);

  tree_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_sizing (tree_column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_expand (tree_column, TRUE);

  column->icon_renderer = thunar_icon_renderer_new ();
  gtk_tree_view_column_pack_start (tree_column, column->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (tree_column, column->icon_renderer,
                                       "file", THUNAR_COLUMN_FILE,
                                       NULL);

  column->name_renderer = gtk_cell_renderer_text_new ();
  g_object_set (column->name_renderer,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  gtk_tree_view_column_pack_start (tree_column, column->name_renderer, TRUE);
  gtk_tree_view_column_set_attributes (tree_column, column->name_renderer,
                                       "text", THUNAR_COLUMN_NAME,
                                       NULL);

  arrow_renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (arrow_renderer,
                "icon-name", "go-next-symbolic",
                "stock-size", GTK_ICON_SIZE_MENU,
                NULL);
  gtk_tree_view_column_pack_end (tree_column, arrow_renderer, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (column->tree_view), tree_column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (thunar_miller_column_selection_changed), column);

  gtk_widget_set_size_request (GTK_WIDGET (column), 200, -1);
}

static void
thunar_miller_column_model_notify_loading (ThunarStandardViewModel *model,
                                           GParamSpec              *pspec,
                                           ThunarMillerColumn      *column)
{
  gboolean loading;

  g_object_get (model, "loading", &loading, NULL);

  if (column->loading != loading)
    {
      column->loading = loading;
      g_object_notify (G_OBJECT (column), "loading");
    }
}

GtkWidget *
thunar_miller_column_new (void)
{
  return g_object_new (THUNAR_TYPE_MILLER_COLUMN, NULL);
}

ThunarFile *
thunar_miller_column_get_directory (ThunarMillerColumn *column)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);
  return column->directory;
}

void
thunar_miller_column_set_directory (ThunarMillerColumn *column,
                                    ThunarFile         *directory)
{
  ThunarFolder *folder;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  _thunar_return_if_fail (directory == NULL || THUNAR_IS_FILE (directory));

  if (column->directory == directory)
    return;

  if (column->directory != NULL)
    g_object_unref (column->directory);

  column->directory = directory;

  if (directory != NULL)
    g_object_ref (directory);

  if (column->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (column->model,
                                            thunar_miller_column_model_notify_loading,
                                            column);
      g_object_unref (column->model);
      column->model = NULL;
    }

  if (directory != NULL)
    {
      folder = thunar_folder_get_for_file (directory);
      column->model = thunar_list_model_new ();
      g_signal_connect (column->model, "notify::loading",
                        G_CALLBACK (thunar_miller_column_model_notify_loading), column);
      thunar_standard_view_model_set_folder (column->model, folder, NULL);
      thunar_standard_view_model_set_show_hidden (column->model, column->show_hidden);
      gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view),
                               GTK_TREE_MODEL (column->model));
      g_object_unref (folder);
    }
  else
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view), NULL);
    }

  g_object_notify (G_OBJECT (column), "directory");
}

ThunarFile *
thunar_miller_column_get_selected_file (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file = NULL;
  GList            *rows;

  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  if (rows != NULL)
    {
      if (gtk_tree_model_get_iter (model, &iter, rows->data))
        file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);

      g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);
    }

  return file;
}

void
thunar_miller_column_set_selected_file (ThunarMillerColumn *column,
                                        ThunarFile         *file)
{
  GtkTreeSelection *selection;
  GtkTreePath      *path;
  GList             file_list;
  GList            *paths;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));

  if (file == NULL)
    {
      gtk_tree_selection_unselect_all (selection);
      return;
    }

  if (column->model != NULL)
    {
      /* create a single-element list for the file */
      file_list.data = file;
      file_list.next = NULL;
      file_list.prev = NULL;

      paths = thunar_standard_view_model_get_paths_for_files (column->model, &file_list);
      if (paths != NULL)
        {
          path = paths->data;

          gtk_tree_selection_unselect_all (selection);
          gtk_tree_selection_select_path (selection, path);

          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (column->tree_view),
                                        path, NULL, TRUE, 0.5, 0.0);

          g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
    }
}

GList *
thunar_miller_column_get_selected_files (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file;
  GList            *files = NULL;
  GList            *rows;
  GList            *lp;

  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (lp = rows; lp != NULL; lp = lp->next)
    {
      if (gtk_tree_model_get_iter (model, &iter, lp->data))
        {
          file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);
          if (file != NULL)
            files = g_list_prepend (files, file);
        }
    }

  g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

  return g_list_reverse (files);
}

void
thunar_miller_column_set_selected_files (ThunarMillerColumn *column,
                                         GList              *files)
{
  GtkTreeSelection *selection;
  GList            *paths;
  GList            *lp;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_unselect_all (selection);

  if (column->model == NULL || files == NULL)
    return;

  paths = thunar_standard_view_model_get_paths_for_files (column->model, files);
  for (lp = paths; lp != NULL; lp = lp->next)
    gtk_tree_selection_select_path (selection, lp->data);

  g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
}

gboolean
thunar_miller_column_get_show_hidden (ThunarMillerColumn *column)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), FALSE);
  return column->show_hidden;
}

void
thunar_miller_column_set_show_hidden (ThunarMillerColumn *column,
                                      gboolean            show_hidden)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  if (column->show_hidden == show_hidden)
    return;

  column->show_hidden = show_hidden;

  if (column->model != NULL)
    thunar_standard_view_model_set_show_hidden (column->model, show_hidden);

  g_object_notify (G_OBJECT (column), "show-hidden");
}

void
thunar_miller_column_select_all (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_select_all (selection);
}

void
thunar_miller_column_unselect_all (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_unselect_all (selection);
}

void
thunar_miller_column_set_active (ThunarMillerColumn *column,
                                 gboolean            active)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  if (column->active == active)
    return;

  column->active = active;

  if (active)
    gtk_widget_grab_focus (column->tree_view);

  g_object_notify (G_OBJECT (column), "active");
}

gboolean
thunar_miller_column_get_active (ThunarMillerColumn *column)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), FALSE);
  return column->active;
}

ThunarFolder *
thunar_miller_column_get_folder (ThunarMillerColumn *column)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  if (column->model == NULL)
    return NULL;

  return thunar_standard_view_model_get_folder (column->model);
}

void
thunar_miller_column_select_first (ThunarMillerColumn *column)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  thunar_miller_column_select_first_or_last (column, TRUE);
}
