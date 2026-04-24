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

GType
thunar_miller_columns_view_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_MILLER_COLUMNS_VIEW_H__ */
