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

#include "thunar/thunar-miller-columns-view.h"

#include "thunar/thunar-component.h"
#include "thunar/thunar-enum-types.h"
#include "thunar/thunar-file.h"
#include "thunar/thunar-folder.h"
#include "thunar/thunar-gio-extensions.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-history.h"
#include "thunar/thunar-miller-column.h"
#include "thunar/thunar-navigator.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-util.h"
#include "thunar/thunar-view.h"

#include <libxfce4ui/libxfce4ui.h>

static void thunar_miller_columns_view_navigator_init (ThunarNavigatorIface *iface);
static void thunar_miller_columns_view_component_init (ThunarComponentIface *iface);
static void thunar_miller_columns_view_view_init (ThunarViewIface *iface);

static void thunar_miller_columns_view_finalize (GObject *object);
static void thunar_miller_columns_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void thunar_miller_columns_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static ThunarFile *thunar_miller_columns_view_get_current_directory (ThunarNavigator *navigator);
static void thunar_miller_columns_view_set_current_directory (ThunarNavigator *navigator, ThunarFile *current_directory);

static GList *thunar_miller_columns_view_get_selected_files_component (ThunarComponent *component);
static void thunar_miller_columns_view_set_selected_files_component (ThunarComponent *component, GList *selected_files);

static gboolean thunar_miller_columns_view_get_loading (ThunarView *view);
static gboolean thunar_miller_columns_view_get_show_hidden (ThunarView *view);
static void thunar_miller_columns_view_set_show_hidden (ThunarView *view, gboolean show_hidden);
static ThunarZoomLevel thunar_miller_columns_view_get_zoom_level (ThunarView *view);
static void thunar_miller_columns_view_set_zoom_level (ThunarView *view, ThunarZoomLevel zoom_level);
static void thunar_miller_columns_view_reset_zoom_level (ThunarView *view);
static void thunar_miller_columns_view_reload (ThunarView *view, gboolean reload_info);
static gboolean thunar_miller_columns_view_get_visible_range (ThunarView *view, ThunarFile **start_file, ThunarFile **end_file);
static void thunar_miller_columns_view_scroll_to_file (ThunarView *view, ThunarFile *file, gboolean select, gboolean use_align, gfloat row_align, gfloat col_align);
static GList *thunar_miller_columns_view_get_selected_files (ThunarView *view);
static void thunar_miller_columns_view_set_selected_files (ThunarView *view, GList *selected_files);

static void thunar_miller_columns_view_column_file_activated (ThunarMillerColumn *column, ThunarFile *file, ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_column_selection_changed (ThunarMillerColumn *column, ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_update_columns (ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_scroll_to_active_column (ThunarMillerColumnsView *view);
static void thunar_miller_columns_view_update_statusbar_text (ThunarMillerColumnsView *view);

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

static guint miller_columns_view_signals[LAST_SIGNAL];

struct _ThunarMillerColumnsViewClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarMillerColumnsView
{
  GtkScrolledWindow __parent__;

  ThunarPreferences *preferences;
  ThunarFile        *current_directory;
  GList             *path_ancestors;
  GtkWidget         *columns_box;
  GList             *columns;
  gint               active_column_index;

  gboolean           show_hidden;
  ThunarZoomLevel    zoom_level;
  gboolean           loading;

  GtkAccelGroup     *accel_group;

  ThunarHistory     *history;

  gchar             *statusbar_text;
};

G_DEFINE_TYPE_WITH_CODE (ThunarMillerColumnsView, thunar_miller_columns_view, GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_miller_columns_view_navigator_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_miller_columns_view_component_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW, thunar_miller_columns_view_view_init))

static void
thunar_miller_columns_view_class_init (ThunarMillerColumnsViewClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
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
    g_signal_new ("start-open-location",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
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
  iface->get_selected_files = thunar_miller_columns_view_get_selected_files;
  iface->set_selected_files = thunar_miller_columns_view_set_selected_files;
}

static void
thunar_miller_columns_view_init (ThunarMillerColumnsView *view)
{
  GtkWidget *viewport;

  view->preferences = thunar_preferences_get ();
  view->current_directory = NULL;
  view->path_ancestors = NULL;
  view->columns = NULL;
  view->active_column_index = -1;
  view->show_hidden = FALSE;
  view->zoom_level = THUNAR_ZOOM_LEVEL_100_PERCENT;
  view->loading = FALSE;

  /* setup the history support */
  view->history = g_object_new (THUNAR_TYPE_HISTORY, NULL);
  g_signal_connect_swapped (G_OBJECT (view->history), "change-directory",
                            G_CALLBACK (thunar_navigator_change_directory), view);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);

  viewport = gtk_viewport_new (NULL, NULL);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (view), viewport);
  gtk_widget_show (viewport);

  view->columns_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_container_add (GTK_CONTAINER (viewport), view->columns_box);
  gtk_widget_show (view->columns_box);

  g_object_bind_property (view->preferences, "last-show-hidden",
                          view, "show-hidden",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (view->preferences, "last-icon-view-zoom-level",
                          view, "zoom-level",
                          G_BINDING_SYNC_CREATE);
}

static void
thunar_miller_columns_view_finalize (GObject *object)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (object);

  if (view->current_directory != NULL)
    g_object_unref (view->current_directory);

  g_list_free_full (view->path_ancestors, g_object_unref);
  g_list_free (view->columns);
  g_object_unref (view->preferences);
  g_object_unref (view->history);
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
      g_value_set_boxed (value, thunar_miller_columns_view_get_selected_files (THUNAR_VIEW (view)));
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
      if (view->current_directory != NULL)
        g_value_set_string (value, thunar_file_get_display_name (view->current_directory));
      else
        g_value_set_string (value, NULL);
      break;

    case PROP_FULL_PARSED_PATH:
      if (view->current_directory != NULL)
        g_value_take_string (value, g_file_get_parse_name (thunar_file_get_file (view->current_directory)));
      else
        g_value_set_string (value, NULL);
      break;

    case PROP_SEARCHING:
      g_value_set_boolean (value, FALSE);
      break;

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
      thunar_miller_columns_view_set_current_directory (THUNAR_NAVIGATOR (view), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_miller_columns_view_set_selected_files (THUNAR_VIEW (view), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_miller_columns_view_set_show_hidden (THUNAR_VIEW (view), g_value_get_boolean (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_miller_columns_view_set_zoom_level (THUNAR_VIEW (view), g_value_get_enum (value));
      break;

    case PROP_SORT_COLUMN_DEFAULT:
    case PROP_SORT_ORDER_DEFAULT:
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
                                                  ThunarFile      *current_directory)
{
  ThunarMillerColumnsView *view = THUNAR_MILLER_COLUMNS_VIEW (navigator);

  if (view->current_directory == current_directory)
    return;

  if (view->current_directory != NULL)
    g_object_unref (view->current_directory);

  view->current_directory = current_directory;

  if (current_directory != NULL)
    g_object_ref (current_directory);

  g_list_free_full (view->path_ancestors, g_object_unref);
  view->path_ancestors = NULL;

  thunar_miller_columns_view_update_columns (view);

  if (current_directory != NULL)
    thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view->history), current_directory);

  g_object_notify (G_OBJECT (view), "current-directory");
}

static GList *
thunar_miller_columns_view_get_selected_files_component (ThunarComponent *component)
{
  return thunar_miller_columns_view_get_selected_files (THUNAR_VIEW (component));
}

static void
thunar_miller_columns_view_set_selected_files_component (ThunarComponent *component,
                                                         GList           *selected_files)
{
  thunar_miller_columns_view_set_selected_files (THUNAR_VIEW (component), selected_files);
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
  ThunarMillerColumn      *column;

  if (miller_view->show_hidden == show_hidden)
    return;

  miller_view->show_hidden = show_hidden;

  for (lp = miller_view->columns; lp != NULL; lp = lp->next)
    {
      column = THUNAR_MILLER_COLUMN (lp->data);
      thunar_miller_column_set_show_hidden (column, show_hidden);
    }

  g_object_notify (G_OBJECT (view), "show-hidden");
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

  if (miller_view->zoom_level == zoom_level)
    return;

  miller_view->zoom_level = zoom_level;
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

  thunar_miller_columns_view_update_columns (miller_view);
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

  if (miller_view->columns == NULL)
    return;

  column = THUNAR_MILLER_COLUMN (g_list_last (miller_view->columns)->data);

  if (select)
    thunar_miller_column_set_selected_file (column, file);
}

static GList *
thunar_miller_columns_view_get_selected_files (ThunarView *view)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  ThunarMillerColumn      *column;

  if (miller_view->columns == NULL || miller_view->active_column_index < 0)
    return NULL;

  column = THUNAR_MILLER_COLUMN (g_list_nth_data (miller_view->columns, miller_view->active_column_index));
  if (column == NULL)
    return NULL;

  return thunar_miller_column_get_selected_files (column);
}

static void
thunar_miller_columns_view_set_selected_files (ThunarView *view,
                                               GList      *selected_files)
{
  ThunarMillerColumnsView *miller_view = THUNAR_MILLER_COLUMNS_VIEW (view);
  ThunarMillerColumn      *column;

  if (miller_view->columns == NULL)
    return;

  column = THUNAR_MILLER_COLUMN (g_list_last (miller_view->columns)->data);
  thunar_miller_column_set_selected_files (column, selected_files);
}

static void
thunar_miller_columns_view_column_file_activated (ThunarMillerColumn      *column,
                                                  ThunarFile              *file,
                                                  ThunarMillerColumnsView *view)
{
  if (thunar_file_is_directory (file))
    {
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), file);
    }
}

static void
thunar_miller_columns_view_column_selection_changed (ThunarMillerColumn      *column,
                                                     ThunarMillerColumnsView *view)
{
  ThunarFile *selected_file;
  gint        column_index;
  GList      *lp;
  gint        i;

  column_index = g_list_index (view->columns, column);
  if (column_index < 0)
    return;

  for (lp = view->columns, i = 0; lp != NULL; lp = lp->next, i++)
    {
      thunar_miller_column_set_active (THUNAR_MILLER_COLUMN (lp->data), i == column_index);
    }

  view->active_column_index = column_index;

  selected_file = thunar_miller_column_get_selected_file (column);

  if (selected_file != NULL && thunar_file_is_directory (selected_file))
    {
      while (g_list_length (view->columns) > (guint) (column_index + 2))
        {
          GtkWidget *last_column = g_list_last (view->columns)->data;
          view->columns = g_list_remove (view->columns, last_column);
          gtk_container_remove (GTK_CONTAINER (view->columns_box), last_column);
        }

      if (g_list_length (view->columns) == (guint) (column_index + 1))
        {
          GtkWidget *new_column = thunar_miller_column_new ();
          thunar_miller_column_set_show_hidden (THUNAR_MILLER_COLUMN (new_column), view->show_hidden);
          thunar_miller_column_set_directory (THUNAR_MILLER_COLUMN (new_column), selected_file);

          g_signal_connect (new_column, "file-activated",
                            G_CALLBACK (thunar_miller_columns_view_column_file_activated), view);
          g_signal_connect (new_column, "selection-changed",
                            G_CALLBACK (thunar_miller_columns_view_column_selection_changed), view);

          view->columns = g_list_append (view->columns, new_column);
          gtk_box_pack_start (GTK_BOX (view->columns_box), new_column, FALSE, FALSE, 0);
          gtk_widget_show (new_column);
        }
      else if (g_list_length (view->columns) > (guint) (column_index + 1))
        {
          ThunarMillerColumn *next_column = g_list_nth_data (view->columns, column_index + 1);
          thunar_miller_column_set_directory (next_column, selected_file);
        }

      thunar_miller_columns_view_scroll_to_active_column (view);
    }
  else if (selected_file != NULL)
    {
      while (g_list_length (view->columns) > (guint) (column_index + 1))
        {
          GtkWidget *last_column = g_list_last (view->columns)->data;
          view->columns = g_list_remove (view->columns, last_column);
          gtk_container_remove (GTK_CONTAINER (view->columns_box), last_column);
        }
    }

  if (selected_file != NULL)
    g_object_unref (selected_file);

  g_object_notify (G_OBJECT (view), "selected-files");

  thunar_miller_columns_view_update_statusbar_text (view);
}

static void
thunar_miller_columns_view_update_loading (ThunarMillerColumnsView *view)
{
  GList    *lp;
  gboolean  any_loading = FALSE;
  gboolean  column_loading;

  for (lp = view->columns; lp != NULL; lp = lp->next)
    {
      g_object_get (lp->data, "loading", &column_loading, NULL);
      if (column_loading)
        {
          any_loading = TRUE;
          break;
        }
    }

  if (view->loading != any_loading)
    {
      view->loading = any_loading;
      g_object_notify (G_OBJECT (view), "loading");
    }
}

static void
thunar_miller_columns_view_column_notify_loading (ThunarMillerColumn      *column,
                                                  GParamSpec              *pspec,
                                                  ThunarMillerColumnsView *view)
{
  thunar_miller_columns_view_update_loading (view);
  thunar_miller_columns_view_update_statusbar_text (view);
}

static void
thunar_miller_columns_view_update_columns (ThunarMillerColumnsView *view)
{
  GList      *lp;
  GtkWidget  *column_widget;
  ThunarFile *directory;
  gint        i;

  for (lp = view->columns; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_data (lp->data, view);
      gtk_container_remove (GTK_CONTAINER (view->columns_box), GTK_WIDGET (lp->data));
    }
  g_list_free (view->columns);
  view->columns = NULL;
  view->active_column_index = -1;

  if (view->current_directory == NULL)
    return;

  for (lp = view->path_ancestors, i = 0; lp != NULL; lp = lp->next, i++)
    {
      directory = THUNAR_FILE (lp->data);

      column_widget = thunar_miller_column_new ();
      thunar_miller_column_set_show_hidden (THUNAR_MILLER_COLUMN (column_widget), view->show_hidden);
      thunar_miller_column_set_directory (THUNAR_MILLER_COLUMN (column_widget), directory);

      g_signal_connect (column_widget, "file-activated",
                        G_CALLBACK (thunar_miller_columns_view_column_file_activated), view);
      g_signal_connect (column_widget, "selection-changed",
                        G_CALLBACK (thunar_miller_columns_view_column_selection_changed), view);
      g_signal_connect (column_widget, "notify::loading",
                        G_CALLBACK (thunar_miller_columns_view_column_notify_loading), view);

      view->columns = g_list_append (view->columns, column_widget);
      gtk_box_pack_start (GTK_BOX (view->columns_box), column_widget, FALSE, FALSE, 0);
      gtk_widget_show (column_widget);

      if (lp->next != NULL)
        {
          ThunarFile *next_dir = THUNAR_FILE (lp->next->data);
          thunar_miller_column_set_selected_file (THUNAR_MILLER_COLUMN (column_widget), next_dir);
        }
      else
        {
          thunar_miller_column_set_selected_file (THUNAR_MILLER_COLUMN (column_widget), view->current_directory);
        }
    }

  column_widget = thunar_miller_column_new ();
  thunar_miller_column_set_show_hidden (THUNAR_MILLER_COLUMN (column_widget), view->show_hidden);
  thunar_miller_column_set_directory (THUNAR_MILLER_COLUMN (column_widget), view->current_directory);
  thunar_miller_column_set_active (THUNAR_MILLER_COLUMN (column_widget), TRUE);

  g_signal_connect (column_widget, "file-activated",
                    G_CALLBACK (thunar_miller_columns_view_column_file_activated), view);
  g_signal_connect (column_widget, "selection-changed",
                    G_CALLBACK (thunar_miller_columns_view_column_selection_changed), view);
  g_signal_connect (column_widget, "notify::loading",
                    G_CALLBACK (thunar_miller_columns_view_column_notify_loading), view);

  view->columns = g_list_append (view->columns, column_widget);
  view->active_column_index = g_list_length (view->columns) - 1;
  gtk_box_pack_start (GTK_BOX (view->columns_box), column_widget, FALSE, FALSE, 0);
  gtk_widget_show (column_widget);

  thunar_miller_columns_view_scroll_to_active_column (view);

  thunar_miller_columns_view_update_loading (view);

  thunar_miller_columns_view_update_statusbar_text (view);
}

static void
thunar_miller_columns_view_scroll_to_active_column (ThunarMillerColumnsView *view)
{
  GtkWidget     *column;
  GtkAdjustment *hadjustment;
  GtkAllocation  allocation;
  gdouble        new_value;

  if (view->active_column_index < 0 || view->columns == NULL)
    return;

  column = g_list_nth_data (view->columns, view->active_column_index);
  if (column == NULL)
    return;

  hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (view));
  gtk_widget_get_allocation (column, &allocation);

  new_value = allocation.x + allocation.width - gtk_adjustment_get_page_size (hadjustment);
  if (new_value < 0)
    new_value = 0;

  gtk_adjustment_set_value (hadjustment, new_value);
}

static void
thunar_miller_columns_view_update_statusbar_text (ThunarMillerColumnsView *view)
{
  GList              *selected_files;
  ThunarFile         *file;
  ThunarFile         *folder_file;
  gchar              *text = NULL;
  gchar              *file_text;
  gchar              *size_string;
  guint               n_selected;
  ThunarMillerColumn *active_column;
  ThunarFolder       *folder;
  GHashTable         *files_table;
  GHashTable         *g_files;
  GHashTableIter      iter;
  gpointer            key;
  gboolean            show_file_size_binary_format;
  ThunarDateStyle     date_style;
  gchar              *date_custom_style;
  guint               status_bar_active_info;
  gboolean            column_loading;
  guint64             free_space;

  if (view->loading)
    {
      text = g_strdup (_("Loading folder contents..."));
      goto done;
    }

  selected_files = thunar_miller_columns_view_get_selected_files (THUNAR_VIEW (view));
  n_selected = g_list_length (selected_files);

  if (n_selected == 0)
    {
      if (view->active_column_index >= 0 && view->columns != NULL)
        {
          active_column = g_list_nth_data (view->columns, view->active_column_index);
          if (active_column != NULL)
            {
              g_object_get (active_column, "loading", &column_loading, NULL);
              if (column_loading)
                {
                  text = g_strdup (_("Loading folder contents..."));
                  g_list_free_full (selected_files, g_object_unref);
                  goto done;
                }

              folder = thunar_miller_column_get_folder (active_column);
              if (folder != NULL)
                {
                  files_table = thunar_folder_get_files (folder);
                  g_files = g_hash_table_new (g_direct_hash, NULL);

                  g_hash_table_iter_init (&iter, files_table);
                  while (g_hash_table_iter_next (&iter, &key, NULL))
                    g_hash_table_add (g_files, thunar_file_get_file (THUNAR_FILE (key)));

                  g_object_get (G_OBJECT (view->preferences),
                                "misc-date-style", &date_style,
                                "misc-date-custom-style", &date_custom_style,
                                "misc-file-size-binary", &show_file_size_binary_format,
                                "misc-status-bar-active-info", &status_bar_active_info, NULL);

                  file_text = thunar_util_get_statusbar_text_for_files (g_files,
                                                                        view->show_hidden,
                                                                        show_file_size_binary_format,
                                                                        date_style,
                                                                        date_custom_style,
                                                                        status_bar_active_info);

                  folder_file = thunar_folder_get_corresponding_file (folder);
                  if (folder_file != NULL &&
                      thunar_g_file_get_free_space (thunar_file_get_file (folder_file), &free_space, NULL))
                    {
                      size_string = g_format_size_full (free_space,
                                                        show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
                      text = g_strdup_printf ("%s  |  %s %s", file_text, _("Free space:"), size_string);
                      g_free (size_string);
                      g_free (file_text);
                    }
                  else
                    {
                      text = file_text;
                    }

                  g_free (date_custom_style);
                  g_hash_table_destroy (g_files);
                }
            }
        }
    }
  else if (n_selected == 1)
    {
      file = THUNAR_FILE (selected_files->data);
      text = thunar_util_get_statusbar_text_for_single_file (file);
    }
  else
    {
      text = g_strdup_printf (ngettext ("%u item selected", "%u items selected", n_selected), n_selected);
    }

  g_list_free_full (selected_files, g_object_unref);

done:
  g_free (view->statusbar_text);
  view->statusbar_text = text;
  g_object_notify (G_OBJECT (view), "statusbar-text");
}

ThunarHistory *
thunar_miller_columns_view_get_history (ThunarMillerColumnsView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMNS_VIEW (view), NULL);
  return view->history;
}

void
thunar_miller_columns_view_set_history (ThunarMillerColumnsView *view,
                                        ThunarHistory           *history)
{
  _thunar_return_if_fail (THUNAR_IS_MILLER_COLUMNS_VIEW (view));
  _thunar_return_if_fail (history == NULL || THUNAR_IS_HISTORY (history));

  g_object_unref (view->history);
  view->history = history;

  g_signal_connect_swapped (G_OBJECT (history), "change-directory",
                            G_CALLBACK (thunar_navigator_change_directory), view);
}

ThunarHistory *
thunar_miller_columns_view_copy_history (ThunarMillerColumnsView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_MILLER_COLUMNS_VIEW (view), NULL);
  return thunar_history_copy (view->history);
}
