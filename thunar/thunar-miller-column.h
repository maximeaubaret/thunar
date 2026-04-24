/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2026 Thunar Miller Columns Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef __THUNAR_MILLER_COLUMN_H__
#define __THUNAR_MILLER_COLUMN_H__

#include "thunar/thunar-enum-types.h"
#include "thunar/thunar-file.h"
#include "thunar/thunar-folder.h"
#include "thunar/thunar-tree-view-model.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarMillerColumnClass ThunarMillerColumnClass;
typedef struct _ThunarMillerColumn      ThunarMillerColumn;

#define THUNAR_TYPE_MILLER_COLUMN (thunar_miller_column_get_type ())
#define THUNAR_MILLER_COLUMN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_MILLER_COLUMN, ThunarMillerColumn))
#define THUNAR_MILLER_COLUMN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_MILLER_COLUMN, ThunarMillerColumnClass))
#define THUNAR_IS_MILLER_COLUMN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_MILLER_COLUMN))
#define THUNAR_IS_MILLER_COLUMN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_MILLER_COLUMN))
#define THUNAR_MILLER_COLUMN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MILLER_COLUMN, ThunarMillerColumnClass))

GType
thunar_miller_column_get_type (void) G_GNUC_CONST;

GtkWidget *
thunar_miller_column_new (void);

ThunarFile *
thunar_miller_column_get_directory (ThunarMillerColumn *column);
void
thunar_miller_column_set_directory (ThunarMillerColumn *column,
                                    ThunarFile         *directory);

ThunarFile *
thunar_miller_column_get_selected_file (ThunarMillerColumn *column);
void
thunar_miller_column_set_selected_file (ThunarMillerColumn *column,
                                        ThunarFile         *file);

GList *
thunar_miller_column_get_selected_files (ThunarMillerColumn *column);
void
thunar_miller_column_set_selected_files (ThunarMillerColumn *column,
                                         GList              *files);

gboolean
thunar_miller_column_get_show_hidden (ThunarMillerColumn *column);
void
thunar_miller_column_set_show_hidden (ThunarMillerColumn *column,
                                      gboolean            show_hidden);

void
thunar_miller_column_select_first (ThunarMillerColumn *column);
void
thunar_miller_column_grab_focus (ThunarMillerColumn *column);

void
thunar_miller_column_set_active (ThunarMillerColumn *column,
                                 gboolean            active);
gboolean
thunar_miller_column_get_active (ThunarMillerColumn *column);

ThunarFolder *
thunar_miller_column_get_folder (ThunarMillerColumn *column);

void
thunar_miller_column_set_zoom_level (ThunarMillerColumn *column,
                                     ThunarZoomLevel     zoom_level);

G_END_DECLS;

#endif /* !__THUNAR_MILLER_COLUMN_H__ */
