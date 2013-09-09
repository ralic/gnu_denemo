/*
 * palettes.h
 * 
 * Copyright 2013 Richard Shann 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#ifndef PALETTES_H
#define PALETTES_H
#include <gtk/gtk.h>
#include <denemo/denemo.h>
#include <string.h>
 

/* returns palette of given name, returns NULL if none */
DenemoPalette *get_palette (gchar *name);
DenemoPalette *create_palette (gchar *name, gboolean docked);
DenemoPalette *set_palate_shape (gchar *name, gboolean row_wise, gint limit);//creates palette if it does not exist
gboolean palette_add_button (DenemoPalette *pal, gchar *label, gchar *tooltip, gchar *script);
void palette_delete_button (DenemoPalette *pal, GtkWidget *button);
void repack_palette (DenemoPalette *pal);
void delete_palette (DenemoPalette *pal);
gchar *get_palette_name (void);
#endif
