/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2026 Thunar Miller Columns Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef __THUNAR_MILLER_COLUMNS_VIEW_H__
#define __THUNAR_MILLER_COLUMNS_VIEW_H__

#include "thunar/thunar-history.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarMillerColumnsViewClass ThunarMillerColumnsViewClass;
typedef struct _ThunarMillerColumnsView      ThunarMillerColumnsView;

#define THUNAR_TYPE_MILLER_COLUMNS_VIEW (thunar_miller_columns_view_get_type ())
#define THUNAR_MILLER_COLUMNS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsView))
#define THUNAR_MILLER_COLUMNS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsViewClass))
#define THUNAR_IS_MILLER_COLUMNS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW))
#define THUNAR_IS_MILLER_COLUMNS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_MILLER_COLUMNS_VIEW))
#define THUNAR_MILLER_COLUMNS_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsViewClass))

typedef enum
{
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SELECT_ALL_FILES,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SELECT_BY_PATTERN,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_INVERT_SELECTION,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_UNSELECT_ALL_FILES,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_ARRANGE_ITEMS_MENU,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_BY_NAME,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_BY_SIZE,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_BY_TYPE,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_BY_MTIME,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_BY_DTIME,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_ASCENDING,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_DESCENDING,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_ORDER_TOGGLE,
  THUNAR_MILLER_COLUMNS_VIEW_ACTION_SORT_FOLDERS_FIRST,

  THUNAR_MILLER_COLUMNS_VIEW_N_ACTIONS
} ThunarMillerColumnsViewAction;

GType
thunar_miller_columns_view_get_type (void) G_GNUC_CONST;

GtkWidget *
thunar_miller_columns_view_append_menu_item (ThunarMillerColumnsView       *view,
                                             GtkMenu                       *menu,
                                             ThunarMillerColumnsViewAction  action);
void
thunar_miller_columns_view_append_menu_items (ThunarMillerColumnsView *view,
                                              GtkMenu                 *menu,
                                              GtkAccelGroup           *accel_group);

G_END_DECLS;

#endif /* !__THUNAR_MILLER_COLUMNS_VIEW_H__ */
