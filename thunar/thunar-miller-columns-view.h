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

#ifndef __THUNAR_MILLER_COLUMNS_VIEW_H__
#define __THUNAR_MILLER_COLUMNS_VIEW_H__

#include <gtk/gtk.h>

#include "thunar/thunar-history.h"

G_BEGIN_DECLS;

typedef struct _ThunarMillerColumnsViewClass ThunarMillerColumnsViewClass;
typedef struct _ThunarMillerColumnsView      ThunarMillerColumnsView;

#define THUNAR_TYPE_MILLER_COLUMNS_VIEW             (thunar_miller_columns_view_get_type ())
#define THUNAR_MILLER_COLUMNS_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsView))
#define THUNAR_MILLER_COLUMNS_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsViewClass))
#define THUNAR_IS_MILLER_COLUMNS_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW))
#define THUNAR_IS_MILLER_COLUMNS_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_MILLER_COLUMNS_VIEW))
#define THUNAR_MILLER_COLUMNS_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MILLER_COLUMNS_VIEW, ThunarMillerColumnsViewClass))

GType
thunar_miller_columns_view_get_type (void) G_GNUC_CONST;

ThunarHistory *
thunar_miller_columns_view_get_history (ThunarMillerColumnsView *view);

void
thunar_miller_columns_view_set_history (ThunarMillerColumnsView *view,
                                        ThunarHistory           *history);

ThunarHistory *
thunar_miller_columns_view_copy_history (ThunarMillerColumnsView *view);

G_END_DECLS;

#endif /* !__THUNAR_MILLER_COLUMNS_VIEW_H__ */
