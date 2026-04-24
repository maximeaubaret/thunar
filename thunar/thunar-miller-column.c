/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2026 Thunar Miller Columns Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "thunar/thunar-miller-column.h"

#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-icon-renderer.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-text-renderer.h"

#include <gdk/gdkkeysyms.h>

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

static void thunar_miller_column_selection_changed (GtkTreeSelection   *selection,
                                                    ThunarMillerColumn *column);
static void thunar_miller_column_model_notify_loading (ThunarTreeViewModel *model,
                                                       GParamSpec          *pspec,
                                                       ThunarMillerColumn  *column);
static void thunar_miller_column_cell_data_func (GtkTreeViewColumn *tree_column,
                                                 GtkCellRenderer   *renderer,
                                                 GtkTreeModel      *model,
                                                 GtkTreeIter       *iter,
                                                 gpointer           user_data);
static gboolean thunar_miller_column_draw (GtkWidget *widget,
                                           cairo_t   *cr);

struct _ThunarMillerColumnClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarMillerColumn
{
  GtkScrolledWindow __parent__;

  ThunarFile          *directory;
  ThunarFile          *opened_file;
  ThunarTreeViewModel *model;
  GtkWidget           *tree_view;
  GtkCellRenderer     *icon_renderer;
  GtkCellRenderer     *name_renderer;
  ThunarColumn         sort_column;
  GtkSortType          sort_order;
  gboolean             show_hidden;
  gboolean             active;
  gboolean             loading;
  gboolean             folders_first;
};

G_DEFINE_TYPE (ThunarMillerColumn, thunar_miller_column, GTK_TYPE_SCROLLED_WINDOW)

static void
thunar_miller_column_get_drop_border_color (GtkWidget *widget,
                                            GdkRGBA   *color)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);
  if (gtk_style_context_lookup_color (context, "theme_selected_bg_color", color))
    return;

  color->red = 0.2;
  color->green = 0.45;
  color->blue = 0.85;
  color->alpha = 1.0;
}

static gboolean
thunar_miller_column_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkAllocation    allocation;
  GdkRGBA          color;
  gboolean         result;

  result = (*GTK_WIDGET_CLASS (thunar_miller_column_parent_class)->draw) (widget, cr);

  context = gtk_widget_get_style_context (widget);
  if (!gtk_style_context_has_class (context, "miller-column-drop-target"))
    return result;

  gtk_widget_get_allocation (widget, &allocation);
  thunar_miller_column_get_drop_border_color (widget, &color);

  cairo_save (cr);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_set_line_width (cr, 2.0);
  cairo_rectangle (cr, 1.0, 1.0,
                   MAX (1, allocation.width - 2),
                   MAX (1, allocation.height - 2));
  cairo_stroke (cr);
  cairo_restore (cr);

  return result;
}

static void
thunar_miller_column_get_opened_background (ThunarMillerColumn *column,
                                            GdkRGBA            *background)
{
  GtkStyleContext *context;
  GdkRGBA          base;
  GdkRGBA          foreground;

  context = gtk_widget_get_style_context (column->tree_view);

  if (!gtk_style_context_lookup_color (context, "theme_base_color", &base)
      && !gtk_style_context_lookup_color (context, "theme_bg_color", &base))
    {
      base.red = 1.0;
      base.green = 1.0;
      base.blue = 1.0;
      base.alpha = 1.0;
    }

  if (!gtk_style_context_lookup_color (context, "theme_fg_color", &foreground)
      && !gtk_style_context_lookup_color (context, "theme_text_color", &foreground))
    {
      foreground.red = 0.0;
      foreground.green = 0.0;
      foreground.blue = 0.0;
      foreground.alpha = 1.0;
    }

  background->red = base.red * 0.84 + foreground.red * 0.16;
  background->green = base.green * 0.84 + foreground.green * 0.16;
  background->blue = base.blue * 0.84 + foreground.blue * 0.16;
  background->alpha = 1.0;
}

static void
thunar_miller_column_cell_data_func (GtkTreeViewColumn *tree_column,
                                     GtkCellRenderer   *renderer,
                                     GtkTreeModel      *model,
                                     GtkTreeIter       *iter,
                                     gpointer           user_data)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (user_data);
  GdkRGBA             background;
  GtkTreePath        *path;
  GtkTreeSelection   *selection;
  ThunarFile         *file;
  gchar              *background_string = NULL;
  gboolean            opened;
  gboolean            selected;

  file = thunar_tree_view_model_get_file (THUNAR_TREE_VIEW_MODEL (model), iter);
  path = gtk_tree_model_get_path (model, iter);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  selected = path != NULL && gtk_tree_selection_path_is_selected (selection, path);
  opened = file != NULL && file == column->opened_file && !selected;

  if (opened)
    {
      thunar_miller_column_get_opened_background (column, &background);
      background_string = gdk_rgba_to_string (&background);
    }

  if (THUNAR_IS_TEXT_RENDERER (renderer))
    g_object_set (renderer,
                  "highlight-color", background_string,
                  "highlighting-enabled", opened,
                  "foreground-set", FALSE,
                  "weight", PANGO_WEIGHT_NORMAL,
                  "weight-set", FALSE,
                  NULL);
  else if (THUNAR_IS_ICON_RENDERER (renderer))
    g_object_set (renderer,
                  "highlight-color", background_string,
                  "highlighting-enabled", opened,
                  NULL);

  if (file != NULL)
    g_object_unref (file);
  if (path != NULL)
    gtk_tree_path_free (path);
  g_free (background_string);
}

static gboolean
thunar_miller_column_set_cursor_first_or_last (ThunarMillerColumn *column,
                                               gboolean            select_first)
{
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeSelection *selection;
  gint              n_children;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (column->tree_view));
  if (model == NULL)
    return FALSE;

  n_children = gtk_tree_model_iter_n_children (model, NULL);
  if (n_children <= 0)
    return FALSE;

  path = select_first ? gtk_tree_path_new_first ()
                      : gtk_tree_path_new_from_indices (n_children - 1, -1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (column->tree_view), path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (column->tree_view), path, NULL, FALSE, 0.0f, 0.0f);
  gtk_widget_grab_focus (column->tree_view);
  gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
thunar_miller_column_cursor_is_first_or_last (ThunarMillerColumn *column,
                                              gboolean            first)
{
  GtkTreeModel *model;
  GtkTreePath  *cursor_path = NULL;
  GtkTreePath  *boundary_path;
  gint          n_children;
  gboolean      result = FALSE;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (column->tree_view));
  if (model == NULL)
    return TRUE;

  n_children = gtk_tree_model_iter_n_children (model, NULL);
  if (n_children <= 0)
    return TRUE;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (column->tree_view), &cursor_path, NULL);
  if (cursor_path == NULL)
    return FALSE;

  boundary_path = first ? gtk_tree_path_new_first ()
                        : gtk_tree_path_new_from_indices (n_children - 1, -1);
  result = gtk_tree_path_compare (cursor_path, boundary_path) == 0;

  gtk_tree_path_free (boundary_path);
  gtk_tree_path_free (cursor_path);

  return result;
}

static gboolean
thunar_miller_column_ensure_cursor (ThunarMillerColumn *column)
{
  GtkTreePath *cursor_path = NULL;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (column->tree_view), &cursor_path, NULL);
  if (cursor_path != NULL)
    {
      gtk_tree_path_free (cursor_path);
      return TRUE;
    }

  return thunar_miller_column_set_cursor_first_or_last (column, TRUE);
}

static void
thunar_miller_column_block_selection_changed (ThunarMillerColumn *column,
                                              gboolean            block)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));

  if (block)
    g_signal_handlers_block_by_func (selection,
                                     thunar_miller_column_selection_changed,
                                     column);
  else
    g_signal_handlers_unblock_by_func (selection,
                                       thunar_miller_column_selection_changed,
                                       column);
}

static void
thunar_miller_column_dispose (GObject *object)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (object);

  if (column->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (column->model,
                                            thunar_miller_column_model_notify_loading,
                                            column);

      if (GTK_IS_TREE_VIEW (column->tree_view))
        gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view), NULL);

      g_clear_object (&column->model);
    }

  (*G_OBJECT_CLASS (thunar_miller_column_parent_class)->dispose) (object);
}

static void
thunar_miller_column_finalize (GObject *object)
{
  ThunarMillerColumn *column = THUNAR_MILLER_COLUMN (object);

  if (column->directory != NULL)
    g_object_unref (column->directory);
  if (column->opened_file != NULL)
    g_object_unref (column->opened_file);
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
thunar_miller_column_row_activated (GtkTreeView        *tree_view,
                                    GtkTreePath        *path,
                                    GtkTreeViewColumn  *tree_column,
                                    ThunarMillerColumn *column)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ThunarFile   *file = NULL;

  model = gtk_tree_view_get_model (tree_view);
  if (gtk_tree_model_get_iter (model, &iter, path))
    file = thunar_tree_view_model_get_file (THUNAR_TREE_VIEW_MODEL (model), &iter);

  if (file == NULL)
    return;

  g_signal_emit (column, miller_column_signals[SIGNAL_FILE_ACTIVATED], 0, file);
  g_object_unref (file);
}

static void
thunar_miller_column_selection_changed (GtkTreeSelection   *selection,
                                        ThunarMillerColumn *column)
{
  g_signal_emit (column, miller_column_signals[SIGNAL_SELECTION_CHANGED], 0);
}

static gboolean
thunar_miller_column_button_press (GtkWidget          *widget,
                                   GdkEventButton     *event,
                                   ThunarMillerColumn *column)
{
  GtkTreePath      *path = NULL;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                     (gint) event->x,
                                     (gint) event->y,
                                     &path, NULL, NULL, NULL))
    {
      if (!gtk_tree_selection_path_is_selected (selection, path))
        {
          gtk_tree_selection_unselect_all (selection);
          gtk_tree_selection_select_path (selection, path);
        }
      gtk_tree_path_free (path);
    }
  else if (event->button == 3)
    {
      gtk_tree_selection_unselect_all (selection);
    }

  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
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
thunar_miller_column_key_press (GtkWidget          *widget,
                                GdkEventKey        *event,
                                ThunarMillerColumn *column)
{
  GtkTreePath *cursor_path = NULL;

  switch (event->keyval)
    {
    case GDK_KEY_Left:
      g_signal_emit (column, miller_column_signals[SIGNAL_NAVIGATE_LEFT], 0);
      return TRUE;

    case GDK_KEY_Right:
      g_signal_emit (column, miller_column_signals[SIGNAL_NAVIGATE_RIGHT], 0);
      return TRUE;

    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      gtk_tree_view_get_cursor (GTK_TREE_VIEW (column->tree_view), &cursor_path, NULL);
      if (cursor_path == NULL)
        {
          thunar_miller_column_set_cursor_first_or_last (column, FALSE);
          return TRUE;
        }
      gtk_tree_path_free (cursor_path);
      return thunar_miller_column_cursor_is_first_or_last (column, TRUE);

    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      gtk_tree_view_get_cursor (GTK_TREE_VIEW (column->tree_view), &cursor_path, NULL);
      if (cursor_path == NULL)
        {
          thunar_miller_column_set_cursor_first_or_last (column, TRUE);
          return TRUE;
        }
      gtk_tree_path_free (cursor_path);
      return thunar_miller_column_cursor_is_first_or_last (column, FALSE);

    default:
      return FALSE;
    }
}

static void
thunar_miller_column_notify_model (GtkTreeView        *tree_view,
                                   GParamSpec         *pspec,
                                   ThunarMillerColumn *column)
{
  gtk_tree_view_set_search_column (tree_view, THUNAR_COLUMN_NAME);
}

static void
thunar_miller_column_model_notify_loading (ThunarTreeViewModel *model,
                                           GParamSpec          *pspec,
                                           ThunarMillerColumn  *column)
{
  gboolean loading;

  g_object_get (model, "loading", &loading, NULL);
  if (column->loading == loading)
    return;

  column->loading = loading;
  g_object_notify (G_OBJECT (column), "loading");
}

static void
thunar_miller_column_class_init (ThunarMillerColumnClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = thunar_miller_column_dispose;
  gobject_class->finalize = thunar_miller_column_finalize;
  gobject_class->get_property = thunar_miller_column_get_property;
  gobject_class->set_property = thunar_miller_column_set_property;
  gtkwidget_class->draw = thunar_miller_column_draw;

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

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (column),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (column), GTK_SHADOW_NONE);

  column->tree_view = gtk_tree_view_new ();
  column->sort_column = THUNAR_COLUMN_NAME;
  column->sort_order = GTK_SORT_ASCENDING;
  column->folders_first = TRUE;
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (column->tree_view), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (column->tree_view), FALSE);
  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (column->tree_view), FALSE);
  gtk_tree_view_set_level_indentation (GTK_TREE_VIEW (column->tree_view), 0);
  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (column->tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (column), column->tree_view);
  gtk_widget_show (column->tree_view);

  g_signal_connect (column->tree_view, "notify::model",
                    G_CALLBACK (thunar_miller_column_notify_model), column);
  g_signal_connect (column->tree_view, "row-activated",
                    G_CALLBACK (thunar_miller_column_row_activated), column);
  g_signal_connect (column->tree_view, "button-press-event",
                    G_CALLBACK (thunar_miller_column_button_press), column);
  g_signal_connect (column->tree_view, "focus-in-event",
                    G_CALLBACK (thunar_miller_column_focus_in), column);
  g_signal_connect (column->tree_view, "key-press-event",
                    G_CALLBACK (thunar_miller_column_key_press), column);

  tree_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (tree_column, TRUE);
  gtk_tree_view_column_set_sizing (tree_column, GTK_TREE_VIEW_COLUMN_FIXED);

  column->icon_renderer = thunar_icon_renderer_new ();
  gtk_tree_view_column_pack_start (tree_column, column->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (tree_column, column->icon_renderer,
                                       "file", THUNAR_COLUMN_FILE,
                                       NULL);
  gtk_tree_view_column_set_cell_data_func (tree_column,
                                           column->icon_renderer,
                                           thunar_miller_column_cell_data_func,
                                           column,
                                           NULL);

  column->name_renderer = thunar_text_renderer_new ();
  g_object_set (column->name_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_pack_start (tree_column, column->name_renderer, TRUE);
  gtk_tree_view_column_set_attributes (tree_column, column->name_renderer,
                                       "text", THUNAR_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_cell_data_func (tree_column,
                                           column->name_renderer,
                                           thunar_miller_column_cell_data_func,
                                           column,
                                           NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (column->tree_view), tree_column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (thunar_miller_column_selection_changed), column);

  gtk_widget_set_size_request (GTK_WIDGET (column), 220, -1);
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
  ThunarFolder *folder = NULL;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  _thunar_return_if_fail (directory == NULL || THUNAR_IS_FILE (directory));

  if (column->directory == directory)
    return;

  if (column->directory != NULL)
    g_object_unref (column->directory);
  if (column->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (column->model,
                                            thunar_miller_column_model_notify_loading,
                                            column);
      g_object_unref (column->model);
      column->model = NULL;
    }
  if (column->opened_file != NULL)
    {
      g_object_unref (column->opened_file);
      column->opened_file = NULL;
    }

  column->directory = directory != NULL ? g_object_ref (directory) : NULL;
  column->loading = FALSE;

  if (directory != NULL)
    {
      folder = thunar_folder_get_for_file (directory);
      if (folder == NULL)
        {
          gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view), NULL);
          g_object_notify (G_OBJECT (column), "directory");
          g_object_notify (G_OBJECT (column), "loading");
          return;
        }

      column->model = g_object_new (THUNAR_TYPE_TREE_VIEW_MODEL, NULL);
      g_signal_connect (column->model, "notify::loading",
                        G_CALLBACK (thunar_miller_column_model_notify_loading), column);
      g_object_set (G_OBJECT (column->model), "folders-first", column->folders_first, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (column->model), column->sort_column, column->sort_order);
      thunar_tree_view_model_set_folder (column->model, folder, NULL);
      thunar_tree_view_model_set_show_hidden (column->model, column->show_hidden);
      gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view), GTK_TREE_MODEL (column->model));
      g_object_unref (folder);
    }
  else
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (column->tree_view), NULL);
    }

  g_object_notify (G_OBJECT (column), "directory");
  g_object_notify (G_OBJECT (column), "loading");
}

ThunarFile *
thunar_miller_column_get_selected_file (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GList            *rows;
  ThunarFile       *file = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);
  if (rows != NULL && gtk_tree_model_get_iter (model, &iter, rows->data))
    file = thunar_tree_view_model_get_file (THUNAR_TREE_VIEW_MODEL (model), &iter);
  g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

  return file;
}

void
thunar_miller_column_set_opened_file (ThunarMillerColumn *column,
                                      ThunarFile         *file)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  if (column->opened_file == file)
    return;

  if (column->opened_file != NULL)
    g_object_unref (column->opened_file);
  column->opened_file = file != NULL ? g_object_ref (file) : NULL;

  gtk_widget_queue_draw (column->tree_view);
}

void
thunar_miller_column_set_sorting (ThunarMillerColumn *column,
                                  ThunarColumn        sort_column,
                                  GtkSortType         sort_order,
                                  gboolean            folders_first)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  column->sort_column = sort_column;
  column->sort_order = sort_order;
  column->folders_first = folders_first;

  if (column->model != NULL)
    {
      g_object_set (G_OBJECT (column->model), "folders-first", folders_first, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (column->model), sort_column, sort_order);
    }
}

GtkWidget *
thunar_miller_column_get_tree_view (ThunarMillerColumn *column)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);
  return column->tree_view;
}

ThunarFile *
thunar_miller_column_get_drop_file (ThunarMillerColumn *column,
                                    gint                x,
                                    gint                y,
                                    GtkTreePath       **path_return)
{
  GtkTreePath *path = NULL;
  GtkTreeIter  iter;
  ThunarFile  *file = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (column->tree_view),
                                     x, y, &path, NULL, NULL, NULL))
    {
      if (column->model != NULL && gtk_tree_model_get_iter (GTK_TREE_MODEL (column->model), &iter, path))
        file = thunar_tree_view_model_get_file (column->model, &iter);

      if (file != NULL && !thunar_file_is_directory (file) && !thunar_file_can_execute (file, NULL))
        {
          g_object_unref (file);
          file = NULL;
          gtk_tree_path_free (path);
          path = NULL;
        }
    }

  if (file == NULL && column->directory != NULL)
    file = g_object_ref (column->directory);

  if (path_return != NULL)
    *path_return = path;
  else if (path != NULL)
    gtk_tree_path_free (path);

  return file;
}

void
thunar_miller_column_set_drop_file (ThunarMillerColumn *column,
                                    ThunarFile         *file)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  g_object_set (G_OBJECT (column->icon_renderer), "drop-file", file, NULL);
}

void
thunar_miller_column_set_selected_file (ThunarMillerColumn *column,
                                        ThunarFile         *file)
{
  GtkTreeSelection *selection;
  GList             singleton;
  GList            *paths;
  GtkTreePath      *path;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  thunar_miller_column_block_selection_changed (column, TRUE);
  gtk_tree_selection_unselect_all (selection);

  if (file == NULL || column->model == NULL)
    {
      thunar_miller_column_block_selection_changed (column, FALSE);
      return;
    }

  singleton.data = file;
  singleton.prev = NULL;
  singleton.next = NULL;
  paths = thunar_tree_view_model_get_paths_for_files (column->model, &singleton);
  if (paths == NULL)
    {
      thunar_miller_column_block_selection_changed (column, FALSE);
      return;
    }

  path = paths->data;
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (column->tree_view), path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (column->tree_view), path, NULL, TRUE, 0.5f, 0.0f);
  g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
  thunar_miller_column_block_selection_changed (column, FALSE);
}

GList *
thunar_miller_column_get_selected_files (ThunarMillerColumn *column)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GList            *rows;
  GList            *lp;
  GList            *files = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMN (column), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column->tree_view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (lp = rows; lp != NULL; lp = lp->next)
    if (gtk_tree_model_get_iter (model, &iter, lp->data))
      files = g_list_prepend (files, thunar_tree_view_model_get_file (THUNAR_TREE_VIEW_MODEL (model), &iter));

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
  thunar_miller_column_block_selection_changed (column, TRUE);
  gtk_tree_selection_unselect_all (selection);

  if (column->model == NULL || files == NULL)
    {
      thunar_miller_column_block_selection_changed (column, FALSE);
      return;
    }

  paths = thunar_tree_view_model_get_paths_for_files (column->model, files);
  for (lp = paths; lp != NULL; lp = lp->next)
    gtk_tree_selection_select_path (selection, lp->data);
  g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
  thunar_miller_column_block_selection_changed (column, FALSE);
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
    thunar_tree_view_model_set_show_hidden (column->model, show_hidden);

  g_object_notify (G_OBJECT (column), "show-hidden");
}

void
thunar_miller_column_select_first (ThunarMillerColumn *column)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  thunar_miller_column_set_cursor_first_or_last (column, TRUE);
}

void
thunar_miller_column_grab_focus (ThunarMillerColumn *column)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));
  thunar_miller_column_ensure_cursor (column);
  gtk_widget_grab_focus (column->tree_view);
}

void
thunar_miller_column_set_active (ThunarMillerColumn *column,
                                 gboolean            active)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  if (column->active == active)
    return;

  column->active = active;
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (column->tree_view), active);

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
  return column->model != NULL ? thunar_tree_view_model_get_folder (column->model) : NULL;
}

void
thunar_miller_column_set_zoom_level (ThunarMillerColumn *column,
                                     ThunarZoomLevel     zoom_level)
{
  ThunarIconSize icon_size = THUNAR_ICON_SIZE_64;

  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMN (column));

  switch (zoom_level)
    {
    case THUNAR_ZOOM_LEVEL_25_PERCENT: icon_size = THUNAR_ICON_SIZE_16; break;
    case THUNAR_ZOOM_LEVEL_38_PERCENT: icon_size = THUNAR_ICON_SIZE_24; break;
    case THUNAR_ZOOM_LEVEL_50_PERCENT: icon_size = THUNAR_ICON_SIZE_32; break;
    case THUNAR_ZOOM_LEVEL_75_PERCENT: icon_size = THUNAR_ICON_SIZE_48; break;
    case THUNAR_ZOOM_LEVEL_100_PERCENT: icon_size = THUNAR_ICON_SIZE_64; break;
    case THUNAR_ZOOM_LEVEL_150_PERCENT: icon_size = THUNAR_ICON_SIZE_96; break;
    case THUNAR_ZOOM_LEVEL_200_PERCENT: icon_size = THUNAR_ICON_SIZE_128; break;
    case THUNAR_ZOOM_LEVEL_250_PERCENT: icon_size = THUNAR_ICON_SIZE_160; break;
    case THUNAR_ZOOM_LEVEL_300_PERCENT: icon_size = THUNAR_ICON_SIZE_192; break;
    case THUNAR_ZOOM_LEVEL_400_PERCENT: icon_size = THUNAR_ICON_SIZE_256; break;
    case THUNAR_ZOOM_LEVEL_800_PERCENT: icon_size = THUNAR_ICON_SIZE_512; break;
    case THUNAR_ZOOM_LEVEL_1600_PERCENT: icon_size = THUNAR_ICON_SIZE_1024; break;
    default: break;
    }

  g_object_set (column->icon_renderer, "size", icon_size, NULL);
  gtk_tree_view_column_queue_resize (gtk_tree_view_get_column (GTK_TREE_VIEW (column->tree_view), 0));
}
