/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
  Gpredict: Real-time satellite tracking and orbit prediction program

  Copyright (C)  2001-2007  Alexandru Csete, OZ9AEC.

  Authors: Alexandru Csete <oz9aec@gmail.com>

  Comments, questions and bugreports should be submitted via
  http://sourceforge.net/projects/groundstation/
  More details can be found at the project home page:

  http://groundstation.sourceforge.net/
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, visit http://www.fsf.org/
*/
/** \brief ROTOR control window.
 *  \ingroup widgets
 *
 * The master rotator control UI is implemented as a Gtk+ Widget in order
 * to allow multiple instances. The widget is created from the module
 * popup menu and each module can have several rotator control windows
 * attached to it.
 * 
 */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>
#include "sat-log.h"
#include "gtk-rot-ctrl.h"
#ifdef HAVE_CONFIG_H
#  include <build-config.h>
#endif


#define FMTSTR "%7.2f\302\260"


static void gtk_rot_ctrl_class_init (GtkRotCtrlClass *class);
static void gtk_rot_ctrl_init       (GtkRotCtrl      *list);
static void gtk_rot_ctrl_destroy    (GtkObject       *object);


static GtkWidget *create_az_widgets (GtkRotCtrl *ctrl);
static GtkWidget *create_el_widgets (GtkRotCtrl *ctrl);
static GtkWidget *create_target_widgets (GtkRotCtrl *ctrl);
static GtkWidget *create_conf_widgets (GtkRotCtrl *ctrl);
static GtkWidget *create_plot_widget (GtkRotCtrl *ctrl);

static void store_sats (gpointer key, gpointer value, gpointer user_data);

static void sat_selected_cb (GtkComboBox *satsel, gpointer data);
static void track_toggle_cb (GtkToggleButton *button, gpointer data);
static void delay_changed_cb (GtkSpinButton *spin, gpointer data);
static void toler_changed_cb (GtkSpinButton *spin, gpointer data);
static gboolean rot_ctrl_timeout_cb (gpointer data);


static GtkVBoxClass *parent_class = NULL;

static GdkColor ColBlack = { 0, 0, 0, 0};
static GdkColor ColWhite = { 0, 0xFFFF, 0xFFFF, 0xFFFF};
static GdkColor ColRed =   { 0, 0xFFFF, 0, 0};
static GdkColor ColGreen = {0, 0, 0xFFFF, 0};


GType
gtk_rot_ctrl_get_type ()
{
	static GType gtk_rot_ctrl_type = 0;

	if (!gtk_rot_ctrl_type) {

		static const GTypeInfo gtk_rot_ctrl_info = {
			sizeof (GtkRotCtrlClass),
			NULL,  /* base_init */
			NULL,  /* base_finalize */
			(GClassInitFunc) gtk_rot_ctrl_class_init,
			NULL,  /* class_finalize */
			NULL,  /* class_data */
			sizeof (GtkRotCtrl),
			5,     /* n_preallocs */
			(GInstanceInitFunc) gtk_rot_ctrl_init,
		};

		gtk_rot_ctrl_type = g_type_register_static (GTK_TYPE_VBOX,
												    "GtkRotCtrl",
													&gtk_rot_ctrl_info,
													0);
	}

	return gtk_rot_ctrl_type;
}


static void
gtk_rot_ctrl_class_init (GtkRotCtrlClass *class)
{
	GObjectClass      *gobject_class;
	GtkObjectClass    *object_class;
	GtkWidgetClass    *widget_class;
	GtkContainerClass *container_class;

	gobject_class   = G_OBJECT_CLASS (class);
	object_class    = (GtkObjectClass*) class;
	widget_class    = (GtkWidgetClass*) class;
	container_class = (GtkContainerClass*) class;

	parent_class = g_type_class_peek_parent (class);

	object_class->destroy = gtk_rot_ctrl_destroy;
 
}



static void
gtk_rot_ctrl_init (GtkRotCtrl *ctrl)
{
    ctrl->sats = NULL;
    ctrl->target = NULL;
    
    ctrl->tracking = FALSE;
    ctrl->busy = FALSE;
    ctrl->delay = 1000;
    ctrl->timerid = 0;
    ctrl->tolerance = 1.0;
}

static void
gtk_rot_ctrl_destroy (GtkObject *object)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (object);
    
    
    /* stop timer */
    if (ctrl->timerid > 0) 
        g_source_remove (ctrl->timerid);

    
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/** \brief Create a new rotor control widget.
 * \return A new rotor control window.
 * 
 */
GtkWidget *
gtk_rot_ctrl_new (GtkSatModule *module)
{
    GtkWidget *widget;
    GtkWidget *table;

	widget = g_object_new (GTK_TYPE_ROT_CTRL, NULL);
    
    /* store satellites */
    g_hash_table_foreach (module->satellites, store_sats, widget);
    
    GTK_ROT_CTRL (widget)->target = SAT (g_slist_nth_data (GTK_ROT_CTRL (widget)->sats, 0));
    
    /* initialise custom colors */
    gdk_rgb_find_color (gtk_widget_get_colormap (widget), &ColBlack);
    gdk_rgb_find_color (gtk_widget_get_colormap (widget), &ColWhite);
    gdk_rgb_find_color (gtk_widget_get_colormap (widget), &ColRed);
    gdk_rgb_find_color (gtk_widget_get_colormap (widget), &ColGreen);

    /* create contents */
    table = gtk_table_new (3, 2, TRUE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 5);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    gtk_container_set_border_width (GTK_CONTAINER (table), 10);
    gtk_table_attach (GTK_TABLE (table), create_az_widgets (GTK_ROT_CTRL (widget)),
                      0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE (table), create_el_widgets (GTK_ROT_CTRL (widget)),
                      1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE (table), create_target_widgets (GTK_ROT_CTRL (widget)),
                      0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE (table), create_conf_widgets (GTK_ROT_CTRL (widget)),
                      0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE (table), create_plot_widget (GTK_ROT_CTRL (widget)),
                      1, 2, 1, 3, GTK_FILL, GTK_FILL, 0, 0);

    gtk_container_add (GTK_CONTAINER (widget), table);
    
    GTK_ROT_CTRL (widget)->timerid = g_timeout_add (GTK_ROT_CTRL (widget)->delay,
                                                    rot_ctrl_timeout_cb,
                                                    GTK_ROT_CTRL (widget));
    
	return widget;
}


/** \brief Update rotator control state.
 * \param ctrl Pointer to the GtkRotCtrl.
 * 
 * This function is called by the parent, i.e. GtkSatModule, indicating that
 * the satellite data has been updated. The function updates the internal state
 * of the controller and the rotator.
 */
void
gtk_rot_ctrl_update   (GtkRotCtrl *ctrl, gdouble t)
{
    gchar *buff;
    
    if (ctrl->target) {
        /* update target displays */
        buff = g_strdup_printf (FMTSTR, ctrl->target->az);
        gtk_label_set_text (GTK_LABEL (ctrl->AzSat), buff);
        g_free (buff);
        buff = g_strdup_printf (FMTSTR, ctrl->target->el);
        gtk_label_set_text (GTK_LABEL (ctrl->ElSat), buff);
        g_free (buff);
    }
}


/** \brief Create azimuth control widgets.
 * \param ctrl Pointer to the GtkRotCtrl widget.
 * 
 * This function creates and initialises the widgets for controlling the
 * azimuth of the the rotator.
 */
static
GtkWidget *create_az_widgets (GtkRotCtrl *ctrl)
{
    GtkWidget *frame;
    
    frame = gtk_frame_new (_("Azimuth"));
    
    ctrl->AzSet = gtk_rot_knob_new (0.0, 360.0, 180.0);
    gtk_container_add (GTK_CONTAINER (frame), ctrl->AzSet);
    
    return frame;
}


/** \brief Create elevation control widgets.
 * \param ctrl Pointer to the GtkRotCtrl widget.
 * 
 * This function creates and initialises the widgets for controlling the
 * elevation of the the rotator.
 */
static
GtkWidget *create_el_widgets (GtkRotCtrl *ctrl)
{
    GtkWidget *frame;
    
    frame = gtk_frame_new (_("Elevation"));
    
    return frame;
}

/** \brief Create target widgets.
 * \param ctrl Pointer to the GtkRotCtrl widget.
 */
static
GtkWidget *create_target_widgets (GtkRotCtrl *ctrl)
{
    GtkWidget *frame,*table,*label,*satsel,*track;
    gchar *buff;
    guint i, n;
    sat_t *sat = NULL;
    
    
    buff = g_strdup_printf (FMTSTR, 0.0);
    
    table = gtk_table_new (4, 3, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    
    /* sat selector */
    satsel = gtk_combo_box_new_text ();
    n = g_slist_length (ctrl->sats);
    for (i = 0; i < n; i++) {
        sat = SAT (g_slist_nth_data (ctrl->sats, i));
        if (sat) {
            gtk_combo_box_append_text (GTK_COMBO_BOX (satsel), sat->tle.sat_name);
        }
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (satsel), 0);
    gtk_widget_set_tooltip_text (satsel, _("Select target object"));
    g_signal_connect (satsel, "changed", G_CALLBACK (sat_selected_cb), ctrl);
    gtk_table_attach (GTK_TABLE (table), satsel, 0, 2, 0, 1,
                     GTK_FILL, GTK_FILL, 5, 5);
    
    /* tracking button */
    track = gtk_toggle_button_new_with_label (_("Track"));
    gtk_widget_set_tooltip_text (track, _("Track the satellite when it is within range"));
    gtk_table_attach (GTK_TABLE (table), track, 2, 3, 0, 1,
                     GTK_SHRINK, GTK_SHRINK, 5, 0);
    g_signal_connect (track, "toggled", G_CALLBACK (track_toggle_cb), ctrl);
    
    /* Azimuth */
    label = gtk_label_new (_("Az:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
    
    ctrl->AzSat = gtk_label_new (buff);
    gtk_misc_set_alignment (GTK_MISC (ctrl->AzSat), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), ctrl->AzSat, 1, 2, 1, 2);
    
    
    /* Elevation */
    label = gtk_label_new (_("El:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
    
    ctrl->ElSat = gtk_label_new (buff);
    gtk_misc_set_alignment (GTK_MISC (ctrl->ElSat), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), ctrl->ElSat, 1, 2, 2, 3);
    
    /* count down */
    label = gtk_label_new (_("Time:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
    
    frame = gtk_frame_new (_("Target"));
    //gtk_container_set_border_width (GTK_CONTAINER (frame), 5);
    gtk_container_add (GTK_CONTAINER (frame), table);
    
    g_free (buff);
    
    return frame;
}


static GtkWidget *
create_conf_widgets (GtkRotCtrl *ctrl)
{
    GtkWidget *frame,*table,*label,*timer,*toler;
    
    table = gtk_table_new (2, 3, TRUE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    
    /* Timeout */
    label = gtk_label_new (_("Cycle:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
    
    timer = gtk_spin_button_new_with_range (100, 5000, 10);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (timer), 0);
    gtk_widget_set_tooltip_text (timer,
                                 _("This parameter controls the delay between "\
                                   "commands sent to the rotator."));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (timer), ctrl->delay);
    g_signal_connect (timer, "value-changed", G_CALLBACK (delay_changed_cb), ctrl);
    gtk_table_attach (GTK_TABLE (table), timer, 1, 2, 0, 1,
                      GTK_FILL, GTK_FILL, 0, 0);
    
    label = gtk_label_new (_("msec"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 2, 3, 0, 1);

    /* Tolerance */
    label = gtk_label_new (_("Tolerance:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
    
    toler = gtk_spin_button_new_with_range (0.0, 10.0, 0.1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (toler), 1);
    gtk_widget_set_tooltip_text (toler,
                                 _("This parameter controls the tolerance between "\
                                   "the target and rotator values for the rotator.\n"\
                                   "If the difference between the target and rotator values "\
                                   "is smaller than the tolerance, no new commands are sent"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (toler), ctrl->tolerance);
    g_signal_connect (toler, "value-changed", G_CALLBACK (toler_changed_cb), ctrl);
    gtk_table_attach (GTK_TABLE (table), toler, 1, 2, 1, 2,
                      GTK_FILL, GTK_FILL, 0, 0);
    
    
    label = gtk_label_new (_("deg"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 2, 3, 1, 2);
    
    
    frame = gtk_frame_new (_("Settings"));
    gtk_container_add (GTK_CONTAINER (frame), table);
    
    return frame;
}


/** \brief Create target widgets.
 * \param ctrl Pointer to the GtkRotCtrl widget.
 */
static
GtkWidget *create_plot_widget (GtkRotCtrl *ctrl)
{
    GtkWidget *frame;
    
    frame = gtk_frame_new (NULL);
    
    return frame;
}


/** \brief Copy satellite from hash table to singly linked list.
 */
static void
store_sats (gpointer key, gpointer value, gpointer user_data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL( user_data);
    sat_t        *sat = SAT (value);

    ctrl->sats = g_slist_append (ctrl->sats, sat);
}


/** \brief Manage satellite selections
 * \param satsel Pointer to the GtkComboBox.
 * \param data Pointer to the GtkRotCtrl widget.
 * 
 * This function is called when the user selects a new satellite.
 */
static void
sat_selected_cb (GtkComboBox *satsel, gpointer data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (data);
    gint i;
    
    i = gtk_combo_box_get_active (satsel);
    if (i >= 0) {
        ctrl->target = SAT (g_slist_nth_data (ctrl->sats, i));
    }
    else {
        sat_log_log (SAT_LOG_LEVEL_ERROR,
                     _("%s:%s: Invalid satellite selection: %d"),
                     __FILE__, __FUNCTION__, i);
    }
}


/** \brief Manage toggle signals (tracking)
 * \param button Pointer to the GtkToggle button.
 * \param data Pointer to the GtkRotCtrl widget.
 */
static void
track_toggle_cb (GtkToggleButton *button, gpointer data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (data);
    
    ctrl->tracking = gtk_toggle_button_get_active (button);
}


/** \brief Manage cycle delay changes.
 * \param spin Pointer to the spin button.
 * \param data Pointer to the GtkRotCtrl widget.
 * 
 * This function is called when the user changes the value of the
 * cycle delay.
 */
static void
delay_changed_cb (GtkSpinButton *spin, gpointer data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (data);
    
    
    ctrl->delay = (guint) gtk_spin_button_get_value (spin);

    if (ctrl->timerid > 0) 
        g_source_remove (ctrl->timerid);

    ctrl->timerid = g_timeout_add (ctrl->delay, rot_ctrl_timeout_cb, ctrl);
}



/** \brief Manage tolerance changes.
 * \param spin Pointer to the spin button.
 * \param data Pointer to the GtkRotCtrl widget.
 * 
 * This function is called when the user changes the value of the
 * tolerance.
 */
static void
toler_changed_cb (GtkSpinButton *spin, gpointer data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (data);
    
    ctrl->tolerance = gtk_spin_button_get_value (spin);
}




/** \brief Rotator controller timeout function
 * \param data Pointer to the GtkRotCtrl widget.
 * \return Always TRUE to let the timer continue.
 */
static gboolean
rot_ctrl_timeout_cb (gpointer data)
{
    GtkRotCtrl *ctrl = GTK_ROT_CTRL (data);
    
    if (ctrl->busy) {
        sat_log_log (SAT_LOG_LEVEL_ERROR,_("%s missed the deadline"),__FUNCTION__);
        return TRUE;
    }
    
    ctrl->busy = TRUE;
    
    /* do something */
    
    ctrl->busy = FALSE;
    
    return TRUE;
}



