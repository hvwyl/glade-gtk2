/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2005 The GNOME Foundation.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include "glade-builtins.h"
#include "glade-displayable-values.h"


struct _GladeParamSpecObjects {
	GParamSpec    parent_instance;
	
	GType         type; /* Object or interface type accepted
			     * in this object list.
			     */
};

typedef struct _GladeStockItem {
	gchar *value_name;
	gchar *value_nick;
	gchar *clean_name;
	gint  value;
} GladeStockItem;


/************************************************************
 *      Auto-generate the enum type for stock properties    *
 ************************************************************/
 
/* Hard-coded list of stock images (and displayable translations) from gtk+ that are not stock "items" */
static const gchar *builtin_stock_images[] = 
{
	"gtk-dialog-authentication", /* GTK_STOCK_DIALOG_AUTHENTICATION */
	"gtk-dnd",                   /* GTK_STOCK_DND */
	"gtk-dnd-multiple",          /* GTK_STOCK_DND_MULTIPLE */
	"gtk-color-picker",          /* GTK_STOCK_COLOR_PICKER */
	"gtk-directory",             /* GTK_STOCK_DIRECTORY */
	"gtk-file",                  /* GTK_STOCK_FILE */
	"gtk-missing-image"          /* GTK_STOCK_MISSING_IMAGE */
};

static const gchar *builtin_stock_displayables[] = 
{
	N_("Authentication"), /* GTK_STOCK_DIALOG_AUTHENTICATION */
	N_("DnD"),            /* GTK_STOCK_DND */
	N_("DnD Multiple"),   /* GTK_STOCK_DND_MULTIPLE */
	N_("Color Picker"),   /* GTK_STOCK_COLOR_PICKER */
	N_("Directory"),      /* GTK_STOCK_DIRECTORY */
	N_("File"),           /* GTK_STOCK_FILE */
	N_("Missing Image")   /* GTK_STOCK_MISSING_IMAGE */
};

static GSList *stock_prefixs = NULL;
static gboolean stock_prefixs_done = FALSE;

/* FIXME: func needs documentation
 */
void
glade_standard_stock_append_prefix (const gchar *prefix)
{
	if (stock_prefixs_done)
	{
		g_warning ("glade_standard_stock_append_prefix should be used in catalog init-function");
		return;
	}
	
	stock_prefixs = g_slist_append (stock_prefixs, g_strdup (prefix));
}

static GladeStockItem *
new_from_values (const gchar *name, const gchar *nick, gint value)
{
	GladeStockItem *new_gsi = NULL;
	gchar *clean_name;
	size_t len = 0;
	guint i = 0;
	guint j = 0;
	
	new_gsi = (GladeStockItem *) g_malloc0 (sizeof(GladeStockItem));

	new_gsi->value_name = g_strdup (name);
	new_gsi->value_nick = g_strdup (nick);
	new_gsi->value = value;


	clean_name = g_strdup (name);
	len = strlen (clean_name);

	while (i+j <= len)
		{
			if (clean_name[i+j] == '_')
				j++;
			
			clean_name[i] = clean_name[i+j];
			i++;				
		}

	new_gsi->clean_name = g_utf8_collate_key (clean_name, i - j);
	
	g_free (clean_name);

	return new_gsi;
}


static gint
compare_two_gsi (gconstpointer a, gconstpointer b)
{
	GladeStockItem *gsi1 = (GladeStockItem *) a;
	GladeStockItem *gsi2 = (GladeStockItem *) b;

	return strcmp (gsi1->clean_name, gsi2->clean_name);
}


static GArray *
list_stock_items (gboolean include_images)
{
	GtkStockItem  item;
	GSList       *l = NULL, *stock_list = NULL, *p = NULL;
	gchar        *stock_id = NULL, *prefix = NULL;
	gint          stock_enum = 0, i = 0;
	GEnumValue    value;
	GArray       *values = NULL;
	GladeStockItem *gsi;
	GSList         *gsi_list = NULL;
	GSList         *gsi_list_list = NULL;

	stock_list = g_slist_reverse (gtk_stock_list_ids ());

	values = g_array_sized_new (TRUE, TRUE, sizeof (GEnumValue),
				    g_slist_length (stock_list));

	/* We want gtk+ stock items to appear first */
	if ((stock_prefixs && strcmp (stock_prefixs->data, "gtk-")) || 
	    stock_prefixs == NULL)
		stock_prefixs = g_slist_prepend (stock_prefixs, g_strdup ("gtk-")); 
	
	for (p = stock_prefixs; p; p = g_slist_next (p))
	{
		prefix = p->data;

		for (l = stock_list; l; l = g_slist_next (l))
		{
			stock_id = l->data;
			if (g_str_has_prefix (stock_id, prefix) == FALSE ||
			    gtk_stock_lookup (stock_id, &item) == FALSE )
				continue;

			gsi = new_from_values (item.label, stock_id, stock_enum++ );
			gsi_list = g_slist_insert_sorted (gsi_list, gsi, (GCompareFunc) compare_two_gsi);
		}

		gsi_list_list = g_slist_append (gsi_list_list, gsi_list);
		gsi_list = NULL;

		/* Images are appended after the gtk+ group of items */
		if (include_images && !strcmp (prefix, "gtk-"))
		{
			for (i = 0; i < G_N_ELEMENTS (builtin_stock_images); i++)
			{
				gsi = new_from_values (builtin_stock_images[i], builtin_stock_images[i], stock_enum++);
				gsi_list = g_slist_insert_sorted (gsi_list, gsi, (GCompareFunc) compare_two_gsi);	
			}
			gsi_list_list = g_slist_append (gsi_list_list, gsi_list);
			gsi_list = NULL;
		}	
	}

	for (p = gsi_list_list; p; p = g_slist_next (p))
	{
		
		for (l = (GSList *) p->data; l; l = g_slist_next (l))
		{
			gsi = (GladeStockItem *) l->data;
			value.value = gsi->value;
			value.value_name = g_strdup (gsi->value_name);
			value.value_nick = g_strdup (gsi->value_nick);
			values = g_array_append_val (values, value);

			g_free (gsi->value_nick);
			g_free (gsi->value_name);
			g_free (gsi->clean_name);
			g_free (gsi);
		}
		g_slist_free ((GSList *) p->data);	
	}

	g_slist_free (gsi_list_list);

	stock_prefixs_done = TRUE;
	g_slist_free (stock_list);

	return values;
}

static gchar *
clean_stock_name (const gchar *name)
{
	gchar *clean_name, *str;
	size_t len = 0;
	guint i = 0;
	guint j = 0;
	
	g_assert (name && name[0]);

	str = g_strdup (name);
	len = strlen (str);

	while (i+j <= len)
		{
			if (str[i+j] == '_')
				j++;
			
			str[i] = str[i+j];
			i++;				
		}
	clean_name = g_strndup (str, i - j);
	g_free (str);

	return clean_name;
}

GType
glade_standard_stock_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		GArray *values = list_stock_items (FALSE);
		gint i, n_values = values->len;
		GEnumValue *enum_values = (GEnumValue *)values->data;
		GtkStockItem item;

		etype = g_enum_register_static ("GladeStock", 
						(GEnumValue *)g_array_free (values, FALSE));

		/* Register displayable by GType, i.e. after the types been created. */
		for (i = 0; i < n_values; i++)
		{
			if (gtk_stock_lookup (enum_values[i].value_nick, &item))
			{
				gchar *clean_name = clean_stock_name (item.label);
				glade_register_translated_value (etype, enum_values[i].value_nick, clean_name);
				g_free (clean_name);
			}
		}		
	}
	return etype;
}


GType
glade_standard_stock_image_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		GArray *values = list_stock_items (TRUE);
		gint i, n_values = values->len;
		GEnumValue *enum_values = (GEnumValue *)values->data;
		GtkStockItem item;

		etype = g_enum_register_static ("GladeStockImage", 
						(GEnumValue *)g_array_free (values, FALSE));

		/* Register displayable by GType, i.e. after the types been created. */
		for (i = 0; i < n_values; i++)
		{
			if (gtk_stock_lookup (enum_values[i].value_nick, &item))
			{
				gchar *clean_name = clean_stock_name (item.label);

				/* These are translated, we just cut out the mnemonic underscores */
				glade_register_translated_value (etype, enum_values[i].value_nick, clean_name);
				g_free (clean_name);
			}
		}		

		for (i = 0; i < G_N_ELEMENTS (builtin_stock_images); i++)
		{
			/* these ones are translated from glade-gtk2 */
			glade_register_displayable_value (etype,
							  builtin_stock_images[i], GETTEXT_PACKAGE, 
							  builtin_stock_displayables[i]);
		}
	}
	return etype;
}

GParamSpec *
glade_standard_stock_spec (void)
{
	return g_param_spec_enum ("stock", _("Stock"), 
				  _("A builtin stock item"),
				  GLADE_TYPE_STOCK,
				  0, G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_stock_image_spec (void)
{
	return g_param_spec_enum ("stock-image", _("Stock Image"), 
				  _("A builtin stock image"),
				  GLADE_TYPE_STOCK_IMAGE,
				  0, G_PARAM_READWRITE);
}

/****************************************************************
 *  A GList boxed type used by GladeParamSpecObjects and        *
 *  GladeParamSpecAccel (which is now in the glade-gtk backend) *
 ****************************************************************/
GType
glade_glist_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeGList", 
			 (GBoxedCopyFunc) g_list_copy,
			 (GBoxedFreeFunc) g_list_free);
	return type_id;
}

/****************************************************************
 *  Built-in GladeParamSpecObjects for object list properties   *
 *  (Used as a pspec to desctibe an AtkRelationSet, but can     *
 *  for any object list property)                               *
 ****************************************************************/
static void
param_objects_init (GParamSpec *pspec)
{
	GladeParamSpecObjects *ospec = GLADE_PARAM_SPEC_OBJECTS (pspec);
	ospec->type = G_TYPE_OBJECT;
}

static void
param_objects_set_default (GParamSpec *pspec,
			   GValue     *value)
{
	if (value->data[0].v_pointer != NULL)
	{
		g_free (value->data[0].v_pointer);
	}
	value->data[0].v_pointer = NULL;
}

static gboolean
param_objects_validate (GParamSpec *pspec,
			GValue     *value)
{
	GladeParamSpecObjects *ospec = GLADE_PARAM_SPEC_OBJECTS (pspec);
	GList                 *objects, *list, *toremove = NULL;
	GObject               *object;

	objects = value->data[0].v_pointer;

	for (list = objects; list; list = list->next)
	{
		object = list->data;

		if (G_TYPE_IS_INTERFACE (ospec->type) &&
		    glade_util_class_implements_interface
		    (G_OBJECT_TYPE (object), ospec->type) == FALSE)
			toremove = g_list_prepend (toremove, object);
		else if (G_TYPE_IS_INTERFACE (ospec->type) == FALSE &&
			 g_type_is_a (G_OBJECT_TYPE (object), 
				      ospec->type) == FALSE)
			toremove = g_list_prepend (toremove, object);
		

	}

	for (list = toremove; list; list = list->next)
	{
		object = list->data;
		objects = g_list_remove (objects, object);
	}
	if (toremove) g_list_free (toremove);
 
	value->data[0].v_pointer = objects;

	return toremove != NULL;
}

static gint
param_objects_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

GType
glade_param_objects_get_type (void)
{
	static GType objects_type = 0;

	if (objects_type == 0)
	{
		static /* const */ GParamSpecTypeInfo pspec_info = {
			sizeof (GladeParamSpecObjects),  /* instance_size */
			16,                         /* n_preallocs */
			param_objects_init,         /* instance_init */
			0xdeadbeef,                 /* value_type, assigned further down */
			NULL,                       /* finalize */
			param_objects_set_default,  /* value_set_default */
			param_objects_validate,     /* value_validate */
			param_objects_values_cmp,   /* values_cmp */
		};
		pspec_info.value_type = GLADE_TYPE_GLIST;

		objects_type = g_param_type_register_static
			("GladeParamObjects", &pspec_info);
	}
	return objects_type;
}

GParamSpec *
glade_param_spec_objects (const gchar   *name,
			  const gchar   *nick,
			  const gchar   *blurb,
			  GType          accepted_type,
			  GParamFlags    flags)
{
  GladeParamSpecObjects *pspec;

  pspec = g_param_spec_internal (GLADE_TYPE_PARAM_OBJECTS,
				 name, nick, blurb, flags);
  
  pspec->type = accepted_type;
  return G_PARAM_SPEC (pspec);
}

void
glade_param_spec_objects_set_type (GladeParamSpecObjects *pspec,
				   GType                  type)
{
	pspec->type = type;
}

GType
glade_param_spec_objects_get_type  (GladeParamSpecObjects *pspec)
{
	return pspec->type;
}

/* This was developed for the purpose of holding a list
 * of 'targets' in an AtkRelation (we are setting it up
 * as a property)
 */
GParamSpec *
glade_standard_objects_spec (void)
{
	return glade_param_spec_objects ("objects", _("Objects"), 
					 _("A list of objects"),
					 G_TYPE_OBJECT,
					 G_PARAM_READWRITE);
}

/* Pixbuf Type */
GParamSpec *
glade_standard_pixbuf_spec (void)
{
	return g_param_spec_object ("pixbuf", _("Image File Name"),
				     _("Enter a filename, relative path or full path to "
				       "load the image"), GDK_TYPE_PIXBUF,
				     G_PARAM_READWRITE);
}

/* GdkColor */
GParamSpec *
glade_standard_gdkcolor_spec (void)
{
	return g_param_spec_boxed ("gdkcolor", _("GdkColor"),
				     _("A GDK color value"), GDK_TYPE_COLOR,
				     G_PARAM_READWRITE);
}

/****************************************************************
 *                    Basic types follow                        *
 ****************************************************************/
GParamSpec *
glade_standard_int_spec (void)
{
	return g_param_spec_int ("int", _("Integer"), 
				 _("An integer value"),
				 G_MININT, G_MAXINT,
				 0, G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_uint_spec (void)
{
	return g_param_spec_uint ("uint", _("Unsigned Integer"), 
				  _("An unsigned integer value"),
				  0, G_MAXUINT, 0, G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_string_spec (void)
{
	return g_param_spec_string ("string", _("String"), 
				    _("An entry"), "", 
				    G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_strv_spec (void)
{
	return g_param_spec_boxed ("strv", _("Strv"),
				   _("String array"),
				   G_TYPE_STRV,
				   G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_float_spec (void)
{
	return g_param_spec_float ("float", _("Float"), 
				   _("A floating point entry"),
				   0.0F, G_MAXFLOAT, 0.0F,
				   G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_boolean_spec (void)
{
	return g_param_spec_boolean ("boolean", _("Boolean"),
				     _("A boolean value"), FALSE,
				     G_PARAM_READWRITE);
}

GType
glade_item_appearance_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] = {
			{ GLADE_ITEM_ICON_AND_LABEL, "GLADE_ITEM_ICON_AND_LABEL", "icon-and-label" },
			{ GLADE_ITEM_ICON_ONLY,      "GLADE_ITEM_ICON_ONLY",      "icon-only" },
			{ GLADE_ITEM_LABEL_ONLY,     "GLADE_ITEM_LABEL_ONLY",     "label-only" },
			{ 0, NULL, NULL }
		};

	etype = g_enum_register_static ("GladeItemAppearance", values);

	}

	return etype;
}
