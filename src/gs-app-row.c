/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2014-2018 Kalev Lember <klember@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gs-app-row.h"
#include "gs-star-widget.h"
#include "gs-progress-button.h"
#include "gs-common.h"
#include "gs-folders.h"

typedef struct
{
	GsApp		*app;
	GtkWidget	*image;
	GtkWidget	*name_box;
	GtkWidget	*name_label;
	GtkWidget	*version_box;
	GtkWidget	*version_current_label;
	GtkWidget	*version_arrow_label;
	GtkWidget	*version_update_label;
	GtkWidget	*star;
	GtkWidget	*description_box;
	GtkWidget	*description_label;
	GtkWidget	*button_box;
	GtkWidget	*button;
	GtkWidget	*spinner;
	GtkWidget	*label;
	GtkWidget	*label_warning;
	GtkWidget	*label_origin;
	GtkWidget	*label_installed;
	GtkWidget	*label_app_size;
	gboolean	 colorful;
	gboolean	 show_buttons;
	gboolean	 show_rating;
	gboolean	 show_source;
	gboolean	 show_update;
	gboolean	 show_installed_size;
	guint		 pending_refresh_id;
} GsAppRowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GsAppRow, gs_app_row, GTK_TYPE_LIST_BOX_ROW)

enum {
	SIGNAL_BUTTON_CLICKED,
	SIGNAL_UNREVEALED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

/**
 * gs_app_row_get_description:
 *
 * Return value: PangoMarkup
 **/
static GString *
gs_app_row_get_description (GsAppRow *app_row)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	const gchar *tmp = NULL;

	/* convert the markdown update description into PangoMarkup */
	if (priv->show_update) {
		tmp = gs_app_get_update_details (priv->app);
		if (tmp != NULL && tmp[0] != '\0')
			return g_string_new (tmp);
	}

	/* if missing summary is set, return it without escaping in order to
	 * correctly show hyperlinks */
	if (gs_app_get_state (priv->app) == AS_APP_STATE_UNAVAILABLE) {
		tmp = gs_app_get_summary_missing (priv->app);
		if (tmp != NULL && tmp[0] != '\0')
			return g_string_new (tmp);
	}

	/* try all these things in order */
	if (tmp == NULL || (tmp != NULL && tmp[0] == '\0'))
		tmp = gs_app_get_summary (priv->app);
	if (tmp == NULL || (tmp != NULL && tmp[0] == '\0'))
		tmp = gs_app_get_description (priv->app);
	if (tmp == NULL || (tmp != NULL && tmp[0] == '\0'))
		tmp = gs_app_get_name (priv->app);
	if (tmp == NULL)
		return NULL;
	return g_string_new (tmp);
}

static void
gs_app_row_refresh_button (GsAppRow *app_row, gboolean missing_search_result)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	GtkStyleContext *context;

	/* disabled */
	if (!priv->show_buttons) {
		gtk_widget_set_visible (priv->button, FALSE);
		return;
	}

	/* label */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_UNAVAILABLE:
		gtk_widget_set_visible (priv->button, TRUE);
		if (missing_search_result) {
			/* TRANSLATORS: this is a button next to the search results that
			 * allows the application to be easily installed */
			gtk_button_set_label (GTK_BUTTON (priv->button), _("Visit website"));
		} else {
			/* TRANSLATORS: this is a button next to the search results that
			 * allows the application to be easily installed.
			 * The ellipsis indicates that further steps are required */
			gtk_button_set_label (GTK_BUTTON (priv->button), _("Install…"));
		}
		break;
	case AS_APP_STATE_QUEUED_FOR_INSTALL:
		gtk_widget_set_visible (priv->button, TRUE);
		/* TRANSLATORS: this is a button next to the search results that
		 * allows to cancel a queued install of the application */
		gtk_button_set_label (GTK_BUTTON (priv->button), _("Cancel"));
		break;
	case AS_APP_STATE_AVAILABLE:
	case AS_APP_STATE_AVAILABLE_LOCAL:
		gtk_widget_set_visible (priv->button, TRUE);
		/* TRANSLATORS: this is a button next to the search results that
		 * allows the application to be easily installed */
		gtk_button_set_label (GTK_BUTTON (priv->button), _("Install"));
		break;
	case AS_APP_STATE_UPDATABLE_LIVE:
		gtk_widget_set_visible (priv->button, TRUE);
		if (priv->show_update) {
			/* TRANSLATORS: this is a button in the updates panel
			 * that allows the app to be easily updated live */
			gtk_button_set_label (GTK_BUTTON (priv->button), _("Update"));
		} else {
			/* TRANSLATORS: this is a button next to the search results that
			 * allows the application to be easily removed */
			gtk_button_set_label (GTK_BUTTON (priv->button), _("Remove"));
		}
		break;
	case AS_APP_STATE_UPDATABLE:
	case AS_APP_STATE_INSTALLED:
		if (!gs_app_has_quirk (priv->app, GS_APP_QUIRK_COMPULSORY))
			gtk_widget_set_visible (priv->button, TRUE);
		/* TRANSLATORS: this is a button next to the search results that
		 * allows the application to be easily removed */
		gtk_button_set_label (GTK_BUTTON (priv->button), _("Remove"));
		break;
	case AS_APP_STATE_INSTALLING:
		gtk_widget_set_visible (priv->button, TRUE);
		/* TRANSLATORS: this is a button next to the search results that
		 * shows the status of an application being installed */
		gtk_button_set_label (GTK_BUTTON (priv->button), _("Installing"));
		break;
	case AS_APP_STATE_REMOVING:
		gtk_widget_set_visible (priv->button, TRUE);
		/* TRANSLATORS: this is a button next to the search results that
		 * shows the status of an application being erased */
		gtk_button_set_label (GTK_BUTTON (priv->button), _("Removing"));
		break;
	default:
		break;
	}

	/* visible */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_UNAVAILABLE:
	case AS_APP_STATE_QUEUED_FOR_INSTALL:
	case AS_APP_STATE_AVAILABLE:
	case AS_APP_STATE_AVAILABLE_LOCAL:
	case AS_APP_STATE_UPDATABLE_LIVE:
	case AS_APP_STATE_INSTALLING:
	case AS_APP_STATE_REMOVING:
		gtk_widget_set_visible (priv->button, TRUE);
		break;
	case AS_APP_STATE_UPDATABLE:
	case AS_APP_STATE_INSTALLED:
		gtk_widget_set_visible (priv->button,
					!gs_app_has_quirk (priv->app,
							   GS_APP_QUIRK_COMPULSORY));
		break;
	default:
		gtk_widget_set_visible (priv->button, FALSE);
		break;
	}

	/* colorful */
	context = gtk_widget_get_style_context (priv->button);
	if (!priv->colorful) {
		gtk_style_context_remove_class (context, "destructive-action");
	} else {
		switch (gs_app_get_state (priv->app)) {
		case AS_APP_STATE_UPDATABLE:
		case AS_APP_STATE_INSTALLED:
			gtk_style_context_add_class (context, "destructive-action");
			break;
		case AS_APP_STATE_UPDATABLE_LIVE:
			if (priv->show_update)
				gtk_style_context_remove_class (context, "destructive-action");
			else
				gtk_style_context_add_class (context, "destructive-action");
			break;
		default:
			gtk_style_context_remove_class (context, "destructive-action");
			break;
		}
	}

	/* always insensitive when in selection mode */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_INSTALLING:
	case AS_APP_STATE_REMOVING:
		gtk_widget_set_sensitive (priv->button, FALSE);
		break;
	default:
		gtk_widget_set_sensitive (priv->button, TRUE);
		break;
	}
}

void
gs_app_row_refresh (GsAppRow *app_row)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	GtkStyleContext *context;
	GString *str = NULL;
	const gchar *tmp;
	gboolean missing_search_result;
	guint64 size = 0;

	if (priv->app == NULL)
		return;

	/* is this a missing search result from the extras page? */
	missing_search_result = (gs_app_get_state (priv->app) == AS_APP_STATE_UNAVAILABLE &&
	                         gs_app_get_url (priv->app, AS_URL_KIND_MISSING) != NULL);

	/* do a fill bar for the current progress */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_INSTALLING:
		gs_progress_button_set_progress (GS_PROGRESS_BUTTON (priv->button),
		                                 gs_app_get_progress (priv->app));
		gs_progress_button_set_show_progress (GS_PROGRESS_BUTTON (priv->button), TRUE);
		break;
	default:
		gs_progress_button_set_show_progress (GS_PROGRESS_BUTTON (priv->button), FALSE);
		break;
	}

	/* join the description lines */
	str = gs_app_row_get_description (app_row);
	if (str != NULL) {
		as_utils_string_replace (str, "\n", " ");
		gtk_label_set_label (GTK_LABEL (priv->description_label), str->str);
		g_string_free (str, TRUE);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->description_label), NULL);
	}

	/* add warning */
	if (gs_app_has_quirk (priv->app, GS_APP_QUIRK_REMOVABLE_HARDWARE)) {
		gtk_label_set_text (GTK_LABEL (priv->label_warning),
				    /* TRANSLATORS: during the update the device
				     * will restart into a special update-only mode */
				    _("Device cannot be used during update."));
		gtk_widget_show (priv->label_warning);
	}

	/* where did this app come from */
	if (priv->show_source) {
		tmp = gs_app_get_origin_hostname (priv->app);
		if (tmp != NULL) {
			g_autofree gchar *origin_tmp = NULL;
			/* TRANSLATORS: this refers to where the app came from */
			origin_tmp = g_strdup_printf ("%s: %s", _("Source"), tmp);
			gtk_label_set_label (GTK_LABEL (priv->label_origin), origin_tmp);
		}
		gtk_widget_set_visible (priv->label_origin, tmp != NULL);
	} else {
		gtk_widget_set_visible (priv->label_origin, FALSE);
	}

	/* installed tag */
	if (!priv->show_buttons) {
		switch (gs_app_get_state (priv->app)) {
		case AS_APP_STATE_UPDATABLE:
		case AS_APP_STATE_UPDATABLE_LIVE:
		case AS_APP_STATE_INSTALLED:
			gtk_widget_set_visible (priv->label_installed, TRUE);
			break;
		default:
			gtk_widget_set_visible (priv->label_installed, FALSE);
			break;
		}
	} else {
		gtk_widget_set_visible (priv->label_installed, FALSE);
	}

	/* name */
	gtk_label_set_label (GTK_LABEL (priv->name_label),
	                     gs_app_get_name (priv->app));

	if (priv->show_update) {
		const gchar *version_current = NULL;
		const gchar *version_update = NULL;

		/* current version */
		tmp = gs_app_get_version_ui (priv->app);
		if (tmp != NULL && tmp[0] != '\0') {
			version_current = tmp;
			gtk_label_set_label (GTK_LABEL (priv->version_current_label),
			                     version_current);
			gtk_widget_show (priv->version_current_label);
		} else {
			gtk_widget_hide (priv->version_current_label);
		}

		/* update version */
		tmp = gs_app_get_update_version_ui (priv->app);
		if (tmp != NULL && tmp[0] != '\0' &&
		    g_strcmp0 (tmp, version_current) != 0) {
			version_update = tmp;
			gtk_label_set_label (GTK_LABEL (priv->version_update_label),
			                     version_update);
			gtk_widget_show (priv->version_update_label);
		} else {
			gtk_widget_hide (priv->version_update_label);
		}

		/* have both: show arrow */
		if (version_current != NULL && version_update != NULL &&
		    g_strcmp0 (version_current, version_update) != 0) {
			gtk_widget_show (priv->version_arrow_label);
		} else {
			gtk_widget_hide (priv->version_arrow_label);
		}

		/* show the box if we have either of the versions */
		if (version_current != NULL || version_update != NULL)
			gtk_widget_show (priv->version_box);
		else
			gtk_widget_hide (priv->version_box);

		gtk_widget_hide (priv->star);
	} else {
		gtk_widget_hide (priv->version_box);
		if (missing_search_result || gs_app_get_rating (priv->app) <= 0 || !priv->show_rating) {
			gtk_widget_hide (priv->star);
		} else {
			gtk_widget_show (priv->star);
			gtk_widget_set_sensitive (priv->star, FALSE);
			gs_star_widget_set_rating (GS_STAR_WIDGET (priv->star),
						   gs_app_get_rating (priv->app));
		}
	}

	/* pixbuf */
	if (gs_app_get_pixbuf (priv->app) == NULL) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "application-x-executable",
					      GTK_ICON_SIZE_DIALOG);
	} else {
		gs_image_set_from_pixbuf (GTK_IMAGE (priv->image),
					  gs_app_get_pixbuf (priv->app));
	}

	context = gtk_widget_get_style_context (priv->image);
	if (missing_search_result)
		gtk_style_context_add_class (context, "dimmer-label");
	else
		gtk_style_context_remove_class (context, "dimmer-label");

	if (gs_app_get_use_drop_shadow (priv->app))
		gtk_style_context_add_class (context, "icon-dropshadow");
	else
		gtk_style_context_remove_class (context, "icon-dropshadow");

	/* pending label */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_QUEUED_FOR_INSTALL:
		gtk_widget_set_visible (priv->label, TRUE);
		gtk_label_set_label (GTK_LABEL (priv->label), _("Pending"));
		break;
	default:
		gtk_widget_set_visible (priv->label, FALSE);
		break;
	}

	/* spinner */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_REMOVING:
		gtk_spinner_start (GTK_SPINNER (priv->spinner));
		gtk_widget_set_visible (priv->spinner, TRUE);
		break;
	default:
		gtk_widget_set_visible (priv->spinner, FALSE);
		break;
	}

	/* button */
	gs_app_row_refresh_button (app_row, missing_search_result);

	/* hide buttons in the update list, unless the app is live updatable */
	switch (gs_app_get_state (priv->app)) {
	case AS_APP_STATE_UPDATABLE_LIVE:
	case AS_APP_STATE_INSTALLING:
		gtk_widget_set_visible (priv->button_box, TRUE);
		break;
	default:
		gtk_widget_set_visible (priv->button_box, !priv->show_update);
		break;
	}

	/* show the right size */
	if (priv->show_installed_size) {
		size = gs_app_get_size_installed (priv->app);
	} else if (priv->show_update) {
		switch (gs_app_get_state (priv->app)) {
		case AS_APP_STATE_UPDATABLE_LIVE:
		case AS_APP_STATE_INSTALLING:
			size = gs_app_get_size_download (priv->app);
			break;
		default:
			break;
		}
	}
	if (size != GS_APP_SIZE_UNKNOWABLE && size != 0) {
		g_autofree gchar *sizestr = NULL;
		sizestr = g_format_size (size);
		gtk_label_set_label (GTK_LABEL (priv->label_app_size), sizestr);
		gtk_widget_show (priv->label_app_size);
	} else {
		gtk_widget_hide (priv->label_app_size);
	}

	/* add warning */
	if (priv->show_update &&
	    gs_app_has_quirk (priv->app, GS_APP_QUIRK_NEW_PERMISSIONS)) {
		gtk_label_set_text (GTK_LABEL (priv->label_warning),
		                    _("Requires additional permissions"));
		gtk_widget_show (priv->label_warning);
	}
}

static void
child_unrevealed (GObject *revealer, GParamSpec *pspec, gpointer user_data)
{
	GsAppRow *app_row = user_data;

	g_signal_emit (app_row, signals[SIGNAL_UNREVEALED], 0);
}

void
gs_app_row_unreveal (GsAppRow *app_row)
{
	GtkWidget *child;
	GtkWidget *revealer;

	g_return_if_fail (GS_IS_APP_ROW (app_row));

	child = gtk_bin_get_child (GTK_BIN (app_row));
	gtk_widget_set_sensitive (child, FALSE);

	revealer = gtk_revealer_new ();
	gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
	gtk_widget_show (revealer);

	g_object_ref (child);
	gtk_container_remove (GTK_CONTAINER (app_row), child);
	gtk_container_add (GTK_CONTAINER (revealer), child);
	g_object_unref (child);

	gtk_container_add (GTK_CONTAINER (app_row), revealer);
	g_signal_connect (revealer, "notify::child-revealed",
			  G_CALLBACK (child_unrevealed), app_row);
	gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
}

GsApp *
gs_app_row_get_app (GsAppRow *app_row)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	g_return_val_if_fail (GS_IS_APP_ROW (app_row), NULL);
	return priv->app;
}

static gboolean
gs_app_row_refresh_idle_cb (gpointer user_data)
{
	GsAppRow *app_row = GS_APP_ROW (user_data);
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	priv->pending_refresh_id = 0;
	gs_app_row_refresh (app_row);
	return FALSE;
}

static void
gs_app_row_notify_props_changed_cb (GsApp *app,
				    GParamSpec *pspec,
				    GsAppRow *app_row)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	if (priv->pending_refresh_id > 0)
		return;
	priv->pending_refresh_id = g_idle_add (gs_app_row_refresh_idle_cb, app_row);
}

static void
gs_app_row_set_app (GsAppRow *app_row, GsApp *app)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->app = g_object_ref (app);

	g_signal_connect_object (priv->app, "notify::state",
				 G_CALLBACK (gs_app_row_notify_props_changed_cb),
				 app_row, 0);
	g_signal_connect_object (priv->app, "notify::rating",
				 G_CALLBACK (gs_app_row_notify_props_changed_cb),
				 app_row, 0);
	g_signal_connect_object (priv->app, "notify::progress",
				 G_CALLBACK (gs_app_row_notify_props_changed_cb),
				 app_row, 0);
	g_signal_connect_object (priv->app, "notify::allow-cancel",
				 G_CALLBACK (gs_app_row_notify_props_changed_cb),
				 app_row, 0);
	gs_app_row_refresh (app_row);
}

static void
gs_app_row_destroy (GtkWidget *object)
{
	GsAppRow *app_row = GS_APP_ROW (object);
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	if (priv->app)
		g_signal_handlers_disconnect_by_func (priv->app, gs_app_row_notify_props_changed_cb, app_row);

	g_clear_object (&priv->app);
	if (priv->pending_refresh_id != 0) {
		g_source_remove (priv->pending_refresh_id);
		priv->pending_refresh_id = 0;
	}

	GTK_WIDGET_CLASS (gs_app_row_parent_class)->destroy (object);
}

static void
gs_app_row_class_init (GsAppRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->destroy = gs_app_row_destroy;

	signals [SIGNAL_BUTTON_CLICKED] =
		g_signal_new ("button-clicked",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsAppRowClass, button_clicked),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals [SIGNAL_UNREVEALED] =
		g_signal_new ("unrevealed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsAppRowClass, unrevealed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Software/gs-app-row.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, image);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, name_box);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, name_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, version_box);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, version_current_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, version_arrow_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, version_update_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, star);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, description_box);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, description_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, button_box);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, button);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, spinner);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, label);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, label_warning);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, label_origin);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, label_installed);
	gtk_widget_class_bind_template_child_private (widget_class, GsAppRow, label_app_size);
}

static void
button_clicked (GtkWidget *widget, GsAppRow *app_row)
{
	g_signal_emit (app_row, signals[SIGNAL_BUTTON_CLICKED], 0);
}

static void
gs_app_row_init (GsAppRow *app_row)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	gtk_widget_set_has_window (GTK_WIDGET (app_row), FALSE);
	gtk_widget_init_template (GTK_WIDGET (app_row));

	g_signal_connect (priv->button, "clicked",
			  G_CALLBACK (button_clicked), app_row);
}

void
gs_app_row_set_size_groups (GsAppRow *app_row,
			    GtkSizeGroup *image,
			    GtkSizeGroup *name,
			    GtkSizeGroup *desc,
			    GtkSizeGroup *button)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	gtk_size_group_add_widget (image, priv->image);
	gtk_size_group_add_widget (name, priv->name_box);
	gtk_size_group_add_widget (desc, priv->description_box);
	gtk_size_group_add_widget (button, priv->button);
}

void
gs_app_row_set_colorful (GsAppRow *app_row, gboolean colorful)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->colorful = colorful;
	gs_app_row_refresh (app_row);
}

void
gs_app_row_set_show_buttons (GsAppRow *app_row, gboolean show_buttons)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->show_buttons = show_buttons;
	gs_app_row_refresh (app_row);
}

void
gs_app_row_set_show_rating (GsAppRow *app_row, gboolean show_rating)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->show_rating = show_rating;
	gs_app_row_refresh (app_row);
}

void
gs_app_row_set_show_source (GsAppRow *app_row, gboolean show_source)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->show_source = show_source;
	gs_app_row_refresh (app_row);
}

void
gs_app_row_set_show_installed_size (GsAppRow *app_row, gboolean show_size)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);
	priv->show_installed_size = show_size;
	gs_app_row_refresh (app_row);
}

/**
 * gs_app_row_set_show_update:
 *
 * Only really useful for the update panel to call
 **/
void
gs_app_row_set_show_update (GsAppRow *app_row, gboolean show_update)
{
	GsAppRowPrivate *priv = gs_app_row_get_instance_private (app_row);

	priv->show_update = show_update;
	gs_app_row_refresh (app_row);
}

GtkWidget *
gs_app_row_new (GsApp *app)
{
	GtkWidget *app_row;

	g_return_val_if_fail (GS_IS_APP (app), NULL);

	app_row = g_object_new (GS_TYPE_APP_ROW, NULL);
	gs_app_row_set_app (GS_APP_ROW (app_row), app);
	return app_row;
}
