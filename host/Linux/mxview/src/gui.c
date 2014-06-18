/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
*
* Copyright (c) 2011 Maxim Integrated Products.
* All rights reserved.
*
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "videodev2.h"
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include "uvcext.h"
#include "audio.h"

/* Support older Gtk+ versions - dynamically */
static void gtk_scale_add_mark_dummy(GtkScale *scale,
		gdouble value,
		GtkPositionType position,
		const gchar *markup)
{
}

static void (*pgtk_scale_add_mark)(GtkScale *scale,
		gdouble value,
		GtkPositionType position,
		const gchar *markup) = gtk_scale_add_mark_dummy;

static void compat_init(void)
{
	void *gtk_scale_add_mark = dlsym(NULL, "gtk_scale_add_mark");
	if(gtk_scale_add_mark)
		pgtk_scale_add_mark = gtk_scale_add_mark;
}
#define gtk_scale_add_mark pgtk_scale_add_mark

static pthread_t        gui_thread;

GtkWidget *label_aud_frame, *label_aud_nzero;
GtkWidget *label_aud_nzero_perc, *label_aud_nzero_live_perc;
GtkWidget *label_aud_maxsize, *label_aud_nzero_maxseq;
GtkWidget *label_vid_framerate, *label_vid_bitrate;
GtkWidget *label_vid_framedrop;
GtkWidget *hBox_xu, *slider_framerate, *combo_res, *combo_res_xu;
GtkWidget *hBox_puxu_nf, *hBox_puxu_tf, *hBox_puxu_wdr;
GtkWidget *hBox_puxu_awb, *hBox_puxu_wbzone, *hBox_puxu_wbzone_ena;
GtkWidget *hBox_puxu_ae, *hBox_puxu_expzone, *hBox_puxu_expzone_ena;
GtkWidget *hBox_res_strp;
GtkWidget *combo_audiodev;
GtkWidget *button_startstop;
GtkWidget *button_snapshot;
GtkWidget *camera_mvmt_query;
int camera_mvmt_query_enable = 0;

struct {
	GtkWidget *enable;
	GtkWidget *width;
	GtkWidget *height;
	GtkWidget *x;
	GtkWidget *y;
} crop_gui_info;

/* Callback functions */
void destroy_cb(GtkWidget *widget, gpointer data)
{
	exit(0);
}
gint delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	/* TRUE won't destroy the window, FALSE will */
	return(FALSE);
}


static void set_range_cb(GtkWidget *widget, gpointer data)
{
	int value = gtk_range_get_value(GTK_RANGE(widget));
	set_v4l2_ctrl((ptrdiff_t)data, value);
}
static void set_button_cb(GtkWidget *widget, gpointer data)
{
	set_v4l2_ctrl((ptrdiff_t)data, 1);
}
static void set_toggle_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char puxu_anf_en;
	extern volatile char puxu_awdr_en;
	extern volatile char puxu_ae_en;
	extern volatile char puxu_awb_en;
	extern volatile char puxu_wbzone_en;
    extern volatile char puxu_expzone_en;

	int id = (ptrdiff_t)data;
	int value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	set_v4l2_ctrl(id, value);

	if(id == V4L2_CID_PU_XU_ANF_ENABLE) {
		puxu_anf_en = value ? 2 : 1;
	}
    else if(id == V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE) {
		puxu_awdr_en = value ? 2 : 1;
	}
    else if(id == V4L2_CID_PU_XU_AUTO_EXPOSURE) {
        puxu_ae_en = value ? 2 : 1;
    }
    else if(id == V4L2_CID_PU_XU_AUTO_WHITE_BAL) {
        puxu_awb_en = value ? 2 : 1;
    }
    else if(id == V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE) {
        puxu_wbzone_en = value ? 2 : 1;
    }
    else if (id == V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE) {
        puxu_expzone_en = value ? 2 : 1;
    }
}
static void set_combo_cb(GtkWidget *widget, gpointer data)
{
	int value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	set_v4l2_ctrl((ptrdiff_t)data, value);
}

void set_format_cb(GtkWidget *widget, gpointer data)
{
	/* This basically involves completely restarting the system in
	 * the main thread */
	extern volatile int change_fmt;
	int value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	change_fmt = value+1;
}

void set_resolution_cb(GtkWidget *widget, gpointer data)
{
	/* This basically involves completely restarting the system in
	 * the main thread */
	extern volatile int change_res;
	int value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	change_res = value+1;
}

void set_resolution_xu_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeModel *store;
	GtkTreeIter   iter;
	int res_id;

	if(gtk_combo_box_get_active_iter( GTK_COMBO_BOX(widget), &iter))
	{
		/* Obtain data model from combo box. */
		store = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
		/* Obtain resolution id from model. */
		gtk_tree_model_get( store, &iter, 1, &res_id, -1 );
		/* Set resolution. */
		set_v4l2_ctrl((ptrdiff_t)data, res_id);
	}
}

void gui_setbr(GtkWidget *widget, gpointer data)
{
	int step = (ptrdiff_t)data;
	int bps = gtk_range_get_value(GTK_RANGE(widget))*step;
	printf("setbr = %f %d\n", gtk_range_get_value(GTK_RANGE(widget)), bps);
	set_v4l2_ctrl(V4L2_CID_XU_BITRATE, bps);
}

void gui_setfr(GtkWidget *widget, gpointer data)
{
	extern volatile int change_fps;
	int value = gtk_range_get_value(GTK_RANGE(widget));
	printf("setfr = %f\n", gtk_range_get_value(GTK_RANGE(widget)));
	if(value != get_v4l2_framerate()) {
		value = set_v4l2_framerate(value);
		if(value < 0) {
			/* fallback to using main */
			change_fps = gtk_range_get_value(GTK_RANGE(widget));
			while(change_fps);
			value = get_v4l2_framerate();
		}
		printf("fps = %d\n", value);
		gtk_range_set_value(GTK_RANGE(widget), value);
	}
}
static gchar* format_value_callback (GtkScale *scale,
					gdouble   value, gpointer data)
{
	int step = (ptrdiff_t)data;
	gdouble bps = value*step;
	if(bps >= 1000000)
		return g_strdup_printf ("%.1f Mbps", bps/1.0e6);
	return g_strdup_printf ("%u Kbps", (unsigned)(bps/1.0e3));
}
void set_capture_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char start_capture, stop_capture;
	extern volatile char capture_state;
	if (capture_state == 1)
	{
		gtk_widget_set_sensitive(button_snapshot, FALSE);
		stop_capture=1;
	}
	else
	{
		gtk_widget_set_sensitive(button_snapshot, TRUE);
		start_capture=1;
	}
}

void set_snapshot_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char snapshot_clicked;
	snapshot_clicked = 1;

}

#ifndef NOAUDIO
void audio_startstop_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char toggle_audio;
	extern volatile char audio_state;
	if (audio_state == 1) {
		gtk_button_set_label(GTK_BUTTON(widget), "Start audio");
		gtk_widget_set_sensitive(combo_audiodev, TRUE);
	} else {
		gtk_button_set_label(GTK_BUTTON(widget), "Stop audio");
		gtk_widget_set_sensitive(combo_audiodev, FALSE);
	}
	toggle_audio=1;
}
void audio_reset_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char reset_stats;
	reset_stats=1;

	/* Update 'Audio frames' */
	gtk_label_set_text(GTK_LABEL(label_aud_frame), "0");

	/* Update 'Zero-size audio frames' */
	gtk_label_set_text(GTK_LABEL(label_aud_nzero), "0");

	/* Update 'Zero-size audio frames %' */
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_perc), "0.0%");

	/* Update 'Zero-size audio frames (LIVE) %' */
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_live_perc), "0%");

	/* Update 'Maximum consecutive zero-size audio frames' */
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_maxseq), "0");

	/* Update 'Maximum frane size' */
	gtk_label_set_text(GTK_LABEL(label_aud_maxsize), "0");
}
void set_audiodev_cb(GtkWidget *widget, gpointer data)
{
	extern volatile char *alsa_device_capture;
	GtkTreeModel *store;
	GtkTreeIter   iter;
	char *string = NULL;

	if(gtk_combo_box_get_active_iter( GTK_COMBO_BOX(widget), &iter))
	{
		/* Obtain data model from combo box. */
		store = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));

		/* Obtain string from model. */
		gtk_tree_model_get( store, &iter, 0, &string, -1 );
	}
	alsa_device_capture = string;
}
#endif

void set_crop_cb(GtkWidget *widget, gpointer data)
{
	struct crop_info info;
	info.enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(crop_gui_info.enable));
	info.width = atoi(gtk_entry_get_text(GTK_ENTRY(crop_gui_info.width)));
	info.height = atoi(gtk_entry_get_text(GTK_ENTRY(crop_gui_info.height)));
	info.x = atoi(gtk_entry_get_text(GTK_ENTRY(crop_gui_info.x)));
	info.y = atoi(gtk_entry_get_text(GTK_ENTRY(crop_gui_info.y)));

	set_v4l2_crop(&info);
}

/*
 * Generate the widgets for video controls
 */
static GtkWidget* gen_video_controls(void)
{
	GtkWidget *hBoxMain, *vBoxMain, *hBox;
	GtkWidget *label, *combo;
	GtkListStore *store;
	GtkTreeIter   iter;
	GtkCellRenderer  *cell;

	hBoxMain = gtk_hbox_new( FALSE, 10 );
	gtk_container_set_border_width( GTK_CONTAINER( hBoxMain ), 10 );

	/* create vertical box that will hold our settings, 5 px padding */
	vBoxMain = gtk_vbox_new( FALSE, 12 );

	/* Start/stop the video capture */
	extern volatile char capture_state;
	if (capture_state == 1)
		button_startstop = gtk_button_new_with_label("Stop video capture");
	else
		button_startstop = gtk_button_new_with_label("Start video capture");
	gtk_widget_set_size_request(button_startstop, 200, 30 );
	g_signal_connect( G_OBJECT(button_startstop), "clicked",
			G_CALLBACK(set_capture_cb), NULL);

	gtk_box_pack_start(GTK_BOX( vBoxMain ), button_startstop, FALSE, FALSE, 0);
	gtk_widget_show(button_startstop);

	/* Snapshot */
	button_snapshot = gtk_button_new_with_label("Snapshot");
	g_signal_connect( G_OBJECT(button_snapshot), "clicked",
			G_CALLBACK(set_snapshot_cb), NULL);
	gtk_box_pack_start(GTK_BOX( vBoxMain ), button_snapshot, FALSE, FALSE, 0);
	gtk_widget_show(button_snapshot);

	/* Display output file  */
	extern char *filename_video;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Output file");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label = gtk_label_new((filename_video == NULL) ? "none" : filename_video);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(vBoxMain), hBox, FALSE, FALSE, 0);
	gtk_widget_show(hBox);

	/* Format */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Format");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	combo = gtk_combo_box_new_text( );
	gtk_widget_set_size_request(combo, 200, 30);
	{
		extern struct v4l2_fmtdesc fmtd[50];
		extern int cfmt;
		extern int nfmt;
		int i;
		for(i = 0; i < nfmt; i++) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
					(gchar*)fmtd[i].description);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), cfmt);
		g_signal_connect(G_OBJECT(combo), "changed",
				G_CALLBACK(set_format_cb), NULL);
	}
	gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
	gtk_widget_show( combo );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Resolution */
	hBox_res_strp = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Resolution");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_res_strp ), label );
	gtk_widget_show( label );
	{
		extern struct v4l2_frmsize_discrete res[50];
		extern int nres;
		extern int cres;
		int i;

		/* Create a model for the resolutions */
		store = gtk_list_store_new( 1, G_TYPE_STRING );
		/* Populate the model with the resolutions */
		for(i = 0; i < nres; i++) {
			gchar * text = g_strdup_printf ("%ux%u", res[i].width,
					res[i].height);
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, text, -1);
		}

		/* Create a combo box with our model as data source. */
		combo_res = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
		gtk_widget_set_size_request(combo_res, 200, 30);
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_res), cres);
		g_signal_connect(G_OBJECT(combo_res), "changed",
				G_CALLBACK(set_resolution_cb), NULL);
		/* Remove our reference from store to avoid memory leak. */
		g_object_unref( G_OBJECT( store ) );

		/* Create a cell renderer. */
		cell = gtk_cell_renderer_text_new();
		/* Pack it to the combo box. */
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo_res ), cell,
				TRUE );
		/* Connect the renderer to the data source */
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_res),
				cell, "text", 0, NULL );
	}


	gtk_box_pack_start( GTK_BOX( hBox_res_strp ), combo_res, FALSE, TRUE, 0 );
	gtk_widget_show( combo_res );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_res_strp, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_res_strp );

	/* Framerate */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Framerate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider_framerate = gtk_hscale_new_with_range(1, 60, 1);
	gtk_widget_set_size_request(slider_framerate, 200, -1 );
	gtk_range_set_update_policy(GTK_RANGE(slider_framerate),
			GTK_UPDATE_DISCONTINUOUS);
	gtk_range_set_value(GTK_RANGE(slider_framerate), get_v4l2_framerate());
	gtk_scale_add_mark(GTK_SCALE(slider_framerate), 30, GTK_POS_BOTTOM, NULL);

	g_signal_connect(G_OBJECT(slider_framerate), "value-changed",
			G_CALLBACK(gui_setfr), NULL);
	gtk_box_pack_start(GTK_BOX(hBox), slider_framerate, FALSE, TRUE, 0);
	gtk_widget_show(slider_framerate);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Framerate display */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Current Framerate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_vid_framerate = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_vid_framerate ), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label_vid_framerate, FALSE, TRUE, 0);
	gtk_widget_show( label_vid_framerate );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Bitrate display */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Current Bitrate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_vid_bitrate = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_vid_bitrate ), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label_vid_bitrate, FALSE, TRUE, 0);
	gtk_widget_show( label_vid_bitrate );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Undisplayed frames */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Undisplayed Frames");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_vid_framedrop = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_vid_framedrop ), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label_vid_framedrop, FALSE, TRUE, 0);
	gtk_widget_show( label_vid_framedrop );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* display the panel */
	gtk_box_pack_start( GTK_BOX( hBoxMain ), vBoxMain, TRUE, TRUE, 0 );
	gtk_widget_show( vBoxMain );

	return hBoxMain;
}

/*
 * Generate the widgets for each extension control
 */
static GtkWidget* gen_xu_controls(void)
{
	int id, value;
	GtkWidget *hBoxMain, *vBoxMain, *hBox;
	GtkWidget *label, *slider, *combo, *button;
	GtkListStore *store;
	GtkTreeIter   iter;
	GtkCellRenderer  *cell;

	hBoxMain = gtk_hbox_new( FALSE, 10 );
	gtk_container_set_border_width( GTK_CONTAINER( hBoxMain ), 10 );

	/* create vertical box that will hold our settings, 5 px padding */
	vBoxMain = gtk_vbox_new( FALSE, 12 );

	/* Resolution */
	id = V4L2_CID_XU_RESOLUTION2;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Resolution");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	/* Get current resolution */
	if(get_v4l2_ctrl(id, &value)) {
		id = V4L2_CID_XU_RESOLUTION;
		if(get_v4l2_ctrl(id, &value))
			gtk_widget_set_sensitive(hBox, FALSE);
	}


	/* Create a model for the resolutions */
	store = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_INT );
	/* Populate the model with the resolutions */
	int i, res_combo_idx=0;
	for (i=0; i < NUM_XU_RES; i++) {
		int cur = (id == V4L2_CID_XU_RESOLUTION
				? xu_res_mapping[i].id
				: xu_res_mapping[i].id2);
		gtk_list_store_append( store, &iter );
		gtk_list_store_set(store, &iter,
					0, xu_res_mapping[i].name,
					1, cur,
					-1);
		if (cur == value)
			res_combo_idx = i;
	}

	/* Create a combo box with our model as data source. */
	combo_res_xu = gtk_combo_box_new_with_model( GTK_TREE_MODEL( store ) );
	gtk_widget_set_size_request(combo_res_xu, 200, 30);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_res_xu), res_combo_idx);
	g_signal_connect(G_OBJECT(combo_res_xu), "changed",
			G_CALLBACK(set_resolution_xu_cb),
			(gpointer)(ptrdiff_t)id);
	/* Remove our reference from store to avoid memory leak. */
	g_object_unref( G_OBJECT( store ) );

	/* Create a cell renderer. */
	cell = gtk_cell_renderer_text_new();
	/* Pack it to the combo box. */
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_res_xu), cell, TRUE);
	/* Connect the renderer to the data source */
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( combo_res_xu ), cell,
			"text", 0, NULL );

	gtk_box_pack_start( GTK_BOX( hBox ), combo_res_xu, FALSE, TRUE, 0 );
	gtk_widget_show( combo_res_xu );
	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Bitrate */
	id = V4L2_CID_XU_BITRATE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Bitrate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

/* This defines the maximum displayed bitrate */
#define MAX_BITRATE 10000000
/* This defines the default bitrate notch */
#define DEFAULT_BITRATE 2000000
/* This defines the granularity of each slider step */
#define BITRATE_STEP 25000
	slider = gtk_hscale_new_with_range(1, MAX_BITRATE/BITRATE_STEP, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value/BITRATE_STEP );
	gtk_scale_add_mark( GTK_SCALE(slider), DEFAULT_BITRATE/BITRATE_STEP, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(gui_setbr), (void*)BITRATE_STEP);
	g_signal_connect( G_OBJECT(slider), "format-value",
			G_CALLBACK(format_value_callback), (void*)BITRATE_STEP);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Profile */
	id = V4L2_CID_XU_AVC_PROFILE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Profile");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	combo = gtk_combo_box_new_text( );
	gtk_widget_set_size_request(combo, 200, 30);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Baseline");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Main");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "High");
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), value);

	g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(set_combo_cb),
			(gpointer)(ptrdiff_t)id);
	gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
	gtk_widget_show( combo );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Level */
	/* Picture Coding */
	id = V4L2_CID_XU_PICTURE_CODING;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Picture Coding");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	combo = gtk_combo_box_new_text( );
	gtk_widget_set_size_request(combo, 200, 30);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Frame");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Field");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "MBAFF");
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), value);

	g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(set_combo_cb),
			(gpointer)(ptrdiff_t)id);
	gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
	gtk_widget_show( combo );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* GOP Structure */
	id = V4L2_CID_XU_GOP_STRUCTURE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("GOP Structure");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	combo = gtk_combo_box_new_text( );
	gtk_widget_set_size_request(combo, 200, 30);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "IP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "IBP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "IBBP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "IBBRBP");
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), value);

	g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(set_combo_cb),
			(gpointer)(ptrdiff_t)id);
	gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
	gtk_widget_show( combo );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* GOP Length */
	id = V4L2_CID_XU_GOP_LENGTH;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("GOP Length");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 500, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 15, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Force I Frame */
	id = V4L2_CID_XU_FORCE_I_FRAME;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Force I Frame");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_button_new_with_label("I Frame");
	gtk_widget_set_size_request(slider, 200, 30 );
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	g_signal_connect( G_OBJECT(slider), "clicked",
			G_CALLBACK(set_button_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Max NAL */
	id = V4L2_CID_XU_MAX_NAL;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Max NAL Units");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 2000, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* VUI Enable */
	id = V4L2_CID_XU_VUI_ENABLE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Video Usability Information");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Picture Timing Enable */
	id = V4L2_CID_XU_PIC_TIMING_ENABLE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Picture Timing");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

    /* AV Mux Enable */
	id = V4L2_CID_XU_AV_MUX_ENABLE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("AAC TS Mux");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Max Frame Size */
	id = V4L2_CID_XU_MAX_FRAME_SIZE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Max Frame Size");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 64000, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 64000, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* display the panel */
	gtk_box_pack_start( GTK_BOX( hBoxMain ), vBoxMain, TRUE, TRUE, 0 );
	gtk_widget_show( vBoxMain );

	return hBoxMain;
}

static struct stream_widgets {
	GtkWidget *streamtype;
	GtkWidget *resolution;
	GtkWidget *frameinterval;
	GtkWidget *bitrate;
} stream_widgets;

extern int skypexu_active;

void skype_stream_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeModel *store;
	GtkTreeIter   iter;
	struct StreamFormat format;
	int streamtype = 3;
	int w = 640, h = 480;
	int frameinterval = 333333;
	int bitrate = 1000000;
	int commit = (ptrdiff_t)data;

	if(skypexu_active == 0){
		set_v4l2_ctrl(V4L2_CID_SKYPE_XU_STREAMID, 0);
	}

	/* Obtain data model from combo box. */
	store = gtk_combo_box_get_model(GTK_COMBO_BOX(stream_widgets.resolution));
	streamtype = gtk_combo_box_get_active( GTK_COMBO_BOX(stream_widgets.streamtype) );
	if(gtk_combo_box_get_active_iter( GTK_COMBO_BOX(stream_widgets.resolution), &iter))
	{
		/* Obtain resolution id from model. */
		gtk_tree_model_get( store, &iter, 1, &w, 2, &h, -1 );
	}
	frameinterval = gtk_range_get_value(GTK_RANGE(stream_widgets.frameinterval));
	bitrate = BITRATE_STEP*gtk_range_get_value(GTK_RANGE(stream_widgets.bitrate)); /* FIXME */

	format.bStreamType = streamtype;
	format.wWidth = w;
	format.wHeight = h;
	format.dwFrameInterval = frameinterval;
	format.dwBitrate = bitrate;

	extern int fd;
	printf("setting %d  %dx%d  %u  %u\n", format.bStreamType, format.wWidth, format.wHeight, format.dwFrameInterval, format.dwBitrate);
	set_skype_stream_control(fd, &format, commit);

	get_skype_stream_control(fd, &format, commit);
	printf("getting %d  %dx%d  %u  %u\n", format.bStreamType, format.wWidth, format.wHeight, format.dwFrameInterval, format.dwBitrate);

	skypexu_active = 1;

	if(format.bStreamType != streamtype)
		 gtk_combo_box_set_active( GTK_COMBO_BOX(stream_widgets.streamtype), format.bStreamType );
	if(format.wWidth != w || format.wHeight != h)
	{
		gboolean valid = gtk_tree_model_get_iter_first(store, &iter);
		while(valid)
		{
			gtk_tree_model_get(store, &iter, 1, &w, 2, &h, -1);
			if(format.wWidth == w && format.wHeight == h)
				break;
			valid = gtk_tree_model_iter_next(store, &iter);
		}
		if (valid)
			gtk_combo_box_set_active_iter( GTK_COMBO_BOX(stream_widgets.resolution), &iter );
	}
}
void skype_setbr(GtkWidget *widget, gpointer data)
{
	int step = (ptrdiff_t)data;
	int bps = gtk_range_get_value(GTK_RANGE(widget))*step;
	printf("setbr = %f %d\n", gtk_range_get_value(GTK_RANGE(widget)), bps);
	set_v4l2_ctrl(V4L2_CID_SKYPE_XU_BITRATE, bps);
}
void skype_resolution_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeModel *store;
	GtkTreeIter   iter;
	int w, h;

	if(gtk_combo_box_get_active_iter( GTK_COMBO_BOX(widget), &iter))
	{
		/* Obtain data model from combo box. */
		store = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
		/* Obtain resolution id from model. */
		gtk_tree_model_get( store, &iter, 1, &w, 2, &h, -1 );
		/* Set resolution. */
		printf("Set resolution %dx%d\n", w, h);
	}
}
static gchar* format_frameinterval_callback (GtkScale *scale,
					gdouble   value, gpointer data)
{
	return g_strdup_printf ("%.1f fps [%u]", 1e7/value, (unsigned)value);
}
static gchar* format_firmwaredays (gdouble   value)
{
	/* convert from days since millenium to date string */
	char ds[64];
	GDate *d = g_date_new_dmy(1,1,2000);
	g_date_add_days(d, value);
	g_date_strftime(ds, sizeof(ds), "%B %e, %Y", d);
	return g_strdup(ds);
}
/*
 * Generate the widgets for each extension control
 */
static GtkWidget* gen_skype_controls(void)
{
	int id, value;
	GtkWidget *frame;
	GtkWidget *hBoxMain, *vBoxMain, *hBox, *vBox;
	GtkWidget *label, *slider, *combo;
	GtkListStore *store;
	GtkTreeIter   iter;
	GtkCellRenderer  *cell;

	/* If we fail to read the version control, no page */
	id = V4L2_CID_SKYPE_XU_VERSION;
	if(get_v4l2_ctrl(id, &value))
		return NULL;

	hBoxMain = gtk_hbox_new( FALSE, 10 );
	gtk_container_set_border_width( GTK_CONTAINER( hBoxMain ), 10 );

	/* create vertical box that will hold our settings, 5 px padding */
	vBoxMain = gtk_vbox_new( FALSE, 12 );

	/* Create one frame holding global controls */
	frame = gtk_frame_new ("Global Controls");
	vBox = gtk_vbox_new( FALSE, 12 );
	gtk_container_set_border_width( GTK_CONTAINER( vBox ), 10 );

	/* Version */
	id = V4L2_CID_SKYPE_XU_VERSION;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Version");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	//if(get_v4l2_ctrl(id, &value))
	//	gtk_widget_set_sensitive(hBox, FALSE);

	label = gtk_label_new(g_strdup_printf ("%u.%u [0x%02x]", value >> 4, value & 0xf, value));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Last Error */
	id = V4L2_CID_SKYPE_XU_LASTERROR;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Last Error");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);

	label = gtk_label_new(g_strdup_printf ("%u [0x%02x]", value, value));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Firmware Days */
	id = V4L2_CID_SKYPE_XU_FIRMWAREDAYS;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Firmware Days");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);

	label = gtk_label_new(g_strdup_printf ("%s [0x%04x]", format_firmwaredays(value), value));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* StreamID */
	id = V4L2_CID_SKYPE_XU_STREAMID;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("StreamID");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	/*
	label = gtk_label_new(g_strdup_printf ("%u [0x%02x]", value, value));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	*/
	{
	extern int fd;
	static struct v4l2_queryctrl queryctrl;
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = id;
	ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
	slider = gtk_spin_button_new_with_range(queryctrl.minimum, queryctrl.maximum, queryctrl.step);
	}
	gtk_spin_button_set_value( GTK_SPIN_BUTTON(slider), value);
	//g_signal_connect(G_OBJECT(slider), "changed", G_CALLBACK(set_combo_cb),
	//		(gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Interface Type */
	id = V4L2_CID_SKYPE_XU_ENDPOINT_SETTING;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Endpoint Setting");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	const char * interfacetype[] = { "Single EP", "Dual EP, Main first", "Dual EP, Preview first" };
	label = gtk_label_new(g_strdup_printf ("%s [0x%02x]", interfacetype[value], value));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );


	gtk_container_add( GTK_CONTAINER( frame ), vBox );
	gtk_widget_show( vBox );
	gtk_box_pack_start( GTK_BOX( vBoxMain ), frame, FALSE, TRUE, 0);
	gtk_widget_show( frame );

	/* Create one frame holding stream controls */
	frame = gtk_frame_new ("Stream Controls");
	vBox = gtk_vbox_new( FALSE, 12 );
	gtk_container_set_border_width( GTK_CONTAINER( vBox ), 10 );

	/* bStreamType */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Stream Type");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	combo = gtk_combo_box_new_text( );
	stream_widgets.streamtype = combo;
	gtk_widget_set_size_request(combo, 200, 30);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "YUY2");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "NV12");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "MJPEG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "H.264");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 3); // H.264
	//g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(set_combo_cb),
	//		(gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), combo, FALSE, TRUE, 0);
	gtk_widget_show(combo);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Resolution */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Resolution");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	/* Get current resolution */

	/* Create a model for the resolutions */
	store = gtk_list_store_new( 4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING );
	/* Populate the model with the resolutions */
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "1280x720",
				1, 1280,
				2, 720,
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "640x480",
				1, 640,
				2, 480,
				3, "red",
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "640x360",
				1, 640,
				2, 360,
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "320x240",
				1, 320,
				2, 240,
				3, "blue",
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "320x180",
				1, 320,
				2, 180,
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "160x120",
				1, 160,
				2, 120,
				-1);
	gtk_list_store_append( store, &iter );
	gtk_list_store_set(store, &iter,
				0, "160x90",
				1, 160,
				2, 90,
				-1);

	/* Create a combo box with our model as data source. */
	combo = gtk_combo_box_new_with_model( GTK_TREE_MODEL( store ) );
	stream_widgets.resolution = combo;
	gtk_widget_set_size_request(combo, 200, 30);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	g_signal_connect(G_OBJECT(combo), "changed",
			G_CALLBACK(skype_resolution_cb),
			NULL);
	/* Remove our reference from store to avoid memory leak. */
	g_object_unref( G_OBJECT( store ) );

	/* Create a cell renderer. */
	cell = gtk_cell_renderer_text_new();
	/* Pack it to the combo box. */
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(combo), cell, TRUE);
	/* Connect the renderer to the data source */
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(combo), cell,
			"text", 0, NULL );
	/* Create a cell renderer. */
	cell = gtk_cell_renderer_text_new();
	/* Pack it to the combo box. */
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(combo), cell, TRUE);
	/* Connect the renderer to the data source */
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(combo), cell,
			"text", 1, NULL );
	/* Create a cell renderer. */
	cell = gtk_cell_renderer_text_new();
	/* Pack it to the combo box. */
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(combo), cell, TRUE);
	/* Connect the renderer to the data source */
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(combo), cell,
			"text", 2, "foreground", 3, NULL );

	gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
	gtk_widget_show( combo );
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );
#if 0
	/* dwFrameInterval */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Frame Interval");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label = gtk_label_new("FIXME - pulled from dynamic");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* dwBitrate */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Bitrate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label = gtk_label_new("FIXME - pulled from dynamic");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );
#endif
	/* Probe & Commit */
	hBox = gtk_hbox_new( FALSE, 5 );

	slider = gtk_button_new_with_label("Commit");
	gtk_widget_set_size_request(slider, 95, 30 );
	g_signal_connect( G_OBJECT(slider), "clicked",
			G_CALLBACK(skype_stream_cb), (gpointer)1);
	gtk_box_pack_end(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	slider = gtk_button_new_with_label("Probe");
	gtk_widget_set_size_request(slider, 95, 30 );
	g_signal_connect( G_OBJECT(slider), "clicked",
			G_CALLBACK(skype_stream_cb), (gpointer)0);
	gtk_box_pack_end(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX(vBox), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	gtk_container_add( GTK_CONTAINER( frame ), vBox );
	gtk_widget_show( vBox );
	gtk_box_pack_start( GTK_BOX( vBoxMain ), frame, FALSE, TRUE, 0);
	gtk_widget_show( frame );

	/* Create one frame holding dynamic controls */
	frame = gtk_frame_new ("Dynamic Controls");
	vBox = gtk_vbox_new( FALSE, 12 );
	gtk_container_set_border_width( GTK_CONTAINER( vBox ), 10 );

	/* Bitrate */
	id = V4L2_CID_SKYPE_XU_BITRATE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Bitrate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

/* This defines the maximum displayed bitrate */
#define MAX_BITRATE 10000000
/* This defines the default bitrate notch */
#define DEFAULT_BITRATE 2000000
/* This defines the granularity of each slider step */
#define BITRATE_STEP 25000
	slider = gtk_hscale_new_with_range(1, MAX_BITRATE/BITRATE_STEP, 1);
	stream_widgets.bitrate = slider; /* FIXME */
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value/BITRATE_STEP );
	gtk_scale_add_mark( GTK_SCALE(slider), DEFAULT_BITRATE/BITRATE_STEP, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(skype_setbr), (void*)BITRATE_STEP);
	g_signal_connect( G_OBJECT(slider), "format-value",
			G_CALLBACK(format_value_callback), (void*)BITRATE_STEP);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Frame Interval */
	id = V4L2_CID_SKYPE_XU_FRAMEINTERVAL;

	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Frame Interval");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	{
	extern int fd;
	static struct v4l2_queryctrl queryctrl;
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = id;
	ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
	slider = gtk_hscale_new_with_range(queryctrl.minimum,
			queryctrl.maximum, queryctrl.step);
	gtk_scale_add_mark(GTK_SCALE(slider), queryctrl.default_value,
			GTK_POS_BOTTOM, NULL);
	}

	stream_widgets.frameinterval = slider; /* FIXME */
	gtk_scale_add_mark(GTK_SCALE(slider), 1e7/20, GTK_POS_BOTTOM, NULL);
	gtk_scale_add_mark(GTK_SCALE(slider), 1e7/10, GTK_POS_BOTTOM, NULL);
	gtk_scale_add_mark(GTK_SCALE(slider), 1e7/5, GTK_POS_BOTTOM, NULL);

	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);

	g_signal_connect( G_OBJECT(slider), "format-value",
			G_CALLBACK(format_frameinterval_callback), (void*)BITRATE_STEP);
	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start( GTK_BOX( hBox ), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBox ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Generate Key Frame */
	id = V4L2_CID_SKYPE_XU_GENERATEKEYFRAME;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Generate Key Frame");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_button_new_with_label("I Frame");
	gtk_widget_set_size_request(slider, 200, 30 );
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	g_signal_connect( G_OBJECT(slider), "clicked",
			G_CALLBACK(set_button_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBox), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	gtk_container_add( GTK_CONTAINER( frame ), vBox );
	gtk_widget_show( vBox );
	gtk_box_pack_start( GTK_BOX( vBoxMain ), frame, FALSE, TRUE, 0);
	gtk_widget_show( frame );

	/* display the panel */
	gtk_box_pack_start( GTK_BOX( hBoxMain ), vBoxMain, TRUE, TRUE, 0 );
	gtk_widget_show( vBoxMain );

	return hBoxMain;
}

static GtkWidget* gen_puxu_controls(void)
{
	int id, value, anf_en, awdr_en, ae_en, awb_en, wbzone_en, expzone_en;
	GtkWidget *hBoxMain, *vBoxMain, *hBox;
	GtkWidget *label, *slider, *button;
	extern volatile char puxu_wbzone_en;
	extern volatile char puxu_expzone_en;

	hBoxMain = gtk_hbox_new( FALSE, 10 );
	gtk_container_set_border_width( GTK_CONTAINER( hBoxMain ), 10 );

	/* create vertical box that will hold our settings, 5 px padding */
	vBoxMain = gtk_vbox_new( FALSE, 12 );

	/* Auto Noise Filter Enable */
	id = V4L2_CID_PU_XU_ANF_ENABLE;
	anf_en = 0;
	if(get_v4l2_ctrl(id, &anf_en)) {
		/* If ANF_ENABLE is not available we totally hide the tab */
		return NULL;
	}
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Auto Noise Filter");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), anf_en);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );


	/* Noise Filter Strength */
	id = V4L2_CID_PU_XU_NF_STRENGTH;
	hBox_puxu_nf = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Noise Filter Strength");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_nf ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 100, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || anf_en)
		gtk_widget_set_sensitive(hBox_puxu_nf, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 50, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_nf), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_nf, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_nf );

	/* Temporal Filter Strength */
	id = V4L2_CID_PU_XU_TF_STRENGTH;
	hBox_puxu_tf = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Temporal Filter Strength");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_tf ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 7, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox_puxu_tf, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 5, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_tf), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_tf, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_tf );

	/* Adaptive WDR Enable */
	id = V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE;
    awdr_en = 0;
	get_v4l2_ctrl(id, &awdr_en);
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Adaptive Wide Dynamic Range");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), awdr_en);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/*  WDR Strength */
	id = V4L2_CID_PU_XU_WDR_STRENGTH;
	hBox_puxu_wdr = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Wide Dynamic Range Strength");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_wdr ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 255, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || awdr_en)
		gtk_widget_set_sensitive(hBox_puxu_wdr, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_wdr), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_wdr, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_wdr );

	/* Auto Exposure Enable */
	id = V4L2_CID_PU_XU_AUTO_EXPOSURE;
    ae_en = 0;
	get_v4l2_ctrl(id, &ae_en);
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Auto Exposure");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), ae_en);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/*  Exposure Time */
	id = V4L2_CID_PU_XU_EXPOSURE_TIME;
	hBox_puxu_ae = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Exposure Time");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_ae ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 255, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || ae_en)
		gtk_widget_set_sensitive(hBox_puxu_ae, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_ae), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_ae, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_ae );

    /* Exposure Balance Zone Select Enable */
	id = V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE;
    expzone_en = 0;
	get_v4l2_ctrl(id, &expzone_en);
    puxu_expzone_en = expzone_en ? 2 : 1;
	hBox_puxu_expzone_ena = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Exposure Zone Select");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_expzone_ena ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), expzone_en);
    if (ae_en)
		gtk_widget_set_sensitive(hBox_puxu_expzone_ena, FALSE);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_expzone_ena), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_expzone_ena, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_expzone_ena );

	/* Exposure Balance Zone */
	id = V4L2_CID_PU_XU_EXP_ZONE_SEL;
	hBox_puxu_expzone = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Exposure Zone");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_expzone ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 62, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || !expzone_en || ae_en)
		gtk_widget_set_sensitive(hBox_puxu_expzone, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 4000, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_expzone), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_expzone, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_expzone );

    /* Auto White Balance Enable */
	id = V4L2_CID_PU_XU_AUTO_WHITE_BAL;
    awb_en = 0;
	get_v4l2_ctrl(id, &awb_en);
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Auto White Balance");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), awb_en);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/*  White Balance Temperature */
	id = V4L2_CID_PU_XU_WHITE_BAL_TEMP;
	hBox_puxu_awb = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("White Balance Temperature");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_awb ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(2500, 8500, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || awb_en)
		gtk_widget_set_sensitive(hBox_puxu_awb, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 4000, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_awb), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_awb, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_awb );

    /* White Balance Zone Select Enable */
	id = V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE;
    wbzone_en = 0;
	get_v4l2_ctrl(id, &wbzone_en);
    puxu_wbzone_en = wbzone_en ? 2 : 1;
	hBox_puxu_wbzone_ena = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("White Balance Zone Select");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_wbzone_ena ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), wbzone_en);
    if (awb_en)
		gtk_widget_set_sensitive(hBox_puxu_wbzone_ena, FALSE);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_wbzone_ena), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_wbzone_ena, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_wbzone_ena );

	/*  White Balance Zone */
	id = V4L2_CID_PU_XU_WB_ZONE_SEL;
	hBox_puxu_wbzone = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("White Balance Zone");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox_puxu_wbzone ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 62, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value) || !wbzone_en || awb_en)
		gtk_widget_set_sensitive(hBox_puxu_wbzone, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 4000, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox_puxu_wbzone), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox_puxu_wbzone, FALSE, FALSE, 0 );
	gtk_widget_show( hBox_puxu_wbzone );

    /* Vertical Flip */
	id = V4L2_CID_PU_XU_VFLIP;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Vertical Flip");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
    value = 0;
    if (get_v4l2_ctrl(id, &value))
        gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

    /* Horizontal Flip */
	id = V4L2_CID_PU_XU_HFLIP;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Horizontal Flip");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
    value = 0;
    if (get_v4l2_ctrl(id, &value))
        gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Max Analog Gain */
	id = V4L2_CID_PU_XU_MAX_ANALOG_GAIN;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Max Analog Gain");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 15, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

    /* Historgram Equalization */
	id = V4L2_CID_PU_XU_HISTO_EQ;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Histogram Equalization");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, 30 );
    value = 0;
    if (get_v4l2_ctrl(id, &value))
        gtk_widget_set_sensitive(hBox, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);

	g_signal_connect( G_OBJECT(button),	"toggled",
			G_CALLBACK(set_toggle_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Sharpen Filter Level */
    id = V4L2_CID_PU_XU_SHARPEN_FILTER;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Sharpen Filter Level");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 2, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 1, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Gain Mulitplier */
    id = V4L2_CID_PU_XU_GAIN_MULTIPLIER;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Exposure Gain Multiplier");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 256, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Crop Mode */
	char text[10];
	GtkWidget *frame = gtk_frame_new(NULL);
	GtkWidget *vBox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(vBoxMain), frame, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vBox);
	gtk_container_set_border_width(GTK_CONTAINER(vBox), 5);
	struct crop_info info;
	if(get_v4l2_crop(&info))
		gtk_widget_set_sensitive(frame, FALSE);

	// Enable
	hBox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("Crop");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(hBox), label);
	button = gtk_check_button_new_with_label("Enable");
	gtk_widget_set_size_request(button, 200, -1 );
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vBox), hBox, FALSE, FALSE, 0);
	crop_gui_info.enable = button;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), info.enable);

	// Width x Height
	hBox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("Crop size");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(hBox), label);
	GtkWidget *rhbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(rhbox, 200, -1);
	gtk_box_pack_start(GTK_BOX(hBox), rhbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vBox), hBox, FALSE, FALSE, 0);
	// Width
	GtkWidget *entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rhbox), entry, TRUE, TRUE, 0);
	crop_gui_info.width = entry;
	sprintf(text, "%i", info.width);
	gtk_entry_set_text(GTK_ENTRY(entry), (const gchar*) text);
	// *
	label = gtk_label_new("  x  ");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(rhbox), label, FALSE, FALSE, 0);
	// Height
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rhbox), entry, TRUE, TRUE, 0);
	crop_gui_info.height = entry;
	sprintf(text, "%i", info.height);
	gtk_entry_set_text(GTK_ENTRY(entry), (const gchar*) text);

	// X x Y Offset
	hBox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("Crop offset");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(hBox), label);
	rhbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(rhbox, 200, -1);
	gtk_box_pack_start(GTK_BOX(hBox), rhbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vBox), hBox, FALSE, FALSE, 0);
	// X
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rhbox), entry, TRUE, TRUE, 0);
	crop_gui_info.x = entry;
	sprintf(text, "%i", info.x);
	gtk_entry_set_text(GTK_ENTRY(entry), (const gchar*) text);
	// *
	label = gtk_label_new("  x  ");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(rhbox), label, FALSE, FALSE, 0);
	// Y
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rhbox), entry, TRUE, TRUE, 0);
	crop_gui_info.y = entry;
	sprintf(text, "%i", info.y);
	gtk_entry_set_text(GTK_ENTRY(entry), (const gchar*) text);

	// Set
	hBox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("Set crop");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(hBox), label);
	button = gtk_button_new_with_label("Set");
	gtk_widget_set_size_request(button, 200, -1 );
	gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vBox), hBox, FALSE, FALSE, 0);
	g_signal_connect( G_OBJECT(button), "clicked",
			G_CALLBACK(set_crop_cb), NULL);

	gtk_widget_show_all(frame);

	/* Minimum Frame Rate */
    id = V4L2_CID_PU_XU_EXP_MIN_FR_RATE;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Minimum Frame Rate");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	slider = gtk_hscale_new_with_range(0, 30, 1);
	gtk_widget_set_size_request(slider, 200, -1 );
	gtk_range_set_update_policy( GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS );
	value = 0;
	if(get_v4l2_ctrl(id, &value))
		gtk_widget_set_sensitive(hBox, FALSE);
	gtk_range_set_value( GTK_RANGE(slider), value );
	gtk_scale_add_mark( GTK_SCALE(slider), 0, GTK_POS_BOTTOM, NULL);

	g_signal_connect( G_OBJECT(slider), "value-changed",
			G_CALLBACK(set_range_cb), (gpointer)(ptrdiff_t)id);
	gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
	gtk_widget_show(slider);

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/*  Camera Movement Query */
	id = V4L2_CID_PU_XU_MVMT_QUERY;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Camera Movement State");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	if(get_v4l2_ctrl(id, &value)){
		gtk_widget_set_sensitive(hBox, FALSE);
		camera_mvmt_query_enable = 0;	
	}else 
		camera_mvmt_query_enable = 1;

	camera_mvmt_query = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( camera_mvmt_query ), 0, 0.5 );
	gtk_box_pack_start( GTK_BOX( hBox ), camera_mvmt_query, FALSE, TRUE, 0 );
	gtk_widget_show( camera_mvmt_query );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );	

	/* display the panel */
	
	gtk_box_pack_start( GTK_BOX( hBoxMain ), vBoxMain, TRUE, TRUE, 0 );
	gtk_widget_show( vBoxMain );

	return hBoxMain;
}


/*
 * Generate the widgets for each camera control
 */
extern int fd;
static struct v4l2_control control;
static struct v4l2_queryctrl queryctrl;
static struct v4l2_querymenu querymenu;
static const char* type_names[] = {
	NULL,
	"V4L2_CTRL_TYPE_INTEGER",
	"V4L2_CTRL_TYPE_BOOLEAN",
	"V4L2_CTRL_TYPE_MENU",
	"V4L2_CTRL_TYPE_BUTTON",
	"V4L2_CTRL_TYPE_INTEGER64",
	"V4L2_CTRL_TYPE_CTRL_CLASS",
	"V4L2_CTRL_TYPE_STRING"
};

static void enumerate_menu(GtkWidget* combo)
{
        memset (&querymenu, 0, sizeof (querymenu));
        querymenu.id = queryctrl.id;

        for (querymenu.index = queryctrl.minimum;
             querymenu.index <= queryctrl.maximum;
              querymenu.index++) {
                if (0 == ioctl (fd, VIDIOC_QUERYMENU, &querymenu)) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
					(gchar*)querymenu.name);
                } else {
                        perror ("VIDIOC_QUERYMENU");
                        exit (EXIT_FAILURE);
                }
        }
}


static GtkWidget* gen_cam_control(struct v4l2_control *control,
				struct v4l2_queryctrl *queryctrl)
{
	GtkWidget *hBox;
	GtkWidget *label, *slider, *combo, *button;

	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new((gchar *)queryctrl->name);
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );
	printf("%s is a %s\n", queryctrl->name, type_names[queryctrl->type]);
	if(queryctrl->type == V4L2_CTRL_TYPE_INTEGER) {
		/* Abort widget creation if min >= max */
		/* We want to detect this incoherence before gtk does since gtk
		 * will abort the whole application in that case */
		if (queryctrl->minimum >= queryctrl->maximum) {
			printf("WARNING: %s: GET_MIN (%i) >= GET_MAX (%i). ",
					queryctrl->name,
					queryctrl->minimum, queryctrl->maximum);
			printf("Skipping this control.\n");
			return NULL;
		}
		slider = gtk_hscale_new_with_range(queryctrl->minimum,
				queryctrl->maximum, queryctrl->step);
		gtk_widget_set_size_request(slider, 200, -1 );
		gtk_range_set_update_policy(GTK_RANGE(slider),
				GTK_UPDATE_DISCONTINUOUS);
		gtk_range_set_value(GTK_RANGE(slider), control->value);
		gtk_scale_add_mark(GTK_SCALE(slider), queryctrl->default_value,
				GTK_POS_BOTTOM, NULL);
		g_signal_connect( G_OBJECT(slider),
				"value-changed",
				G_CALLBACK(set_range_cb),
				(gpointer)(ptrdiff_t)control->id);
		gtk_box_pack_start(GTK_BOX(hBox), slider, FALSE, TRUE, 0);
		gtk_widget_show(slider);

	} else if (queryctrl->type == V4L2_CTRL_TYPE_BOOLEAN) {
		button = gtk_check_button_new_with_label("Active");
		gtk_widget_set_size_request(button, 200, 30 );
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
				control->value);
		g_signal_connect( G_OBJECT(button),
				"toggled",
				G_CALLBACK(set_toggle_cb),
				(gpointer)(ptrdiff_t)control->id);
		gtk_box_pack_start(GTK_BOX(hBox), button, FALSE, TRUE, 0);
		gtk_widget_show(button);

	} else if (queryctrl->type == V4L2_CTRL_TYPE_MENU) {
		combo = gtk_combo_box_new_text();
		gtk_widget_set_size_request(combo, 200, 30);
		enumerate_menu(combo);
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), control->value);

		g_signal_connect(G_OBJECT(combo),
				"changed",
				G_CALLBACK(set_combo_cb),
				(gpointer)(ptrdiff_t)control->id);
		gtk_box_pack_start( GTK_BOX( hBox ), combo, FALSE, TRUE, 0 );
		gtk_widget_show( combo );

	} else {
		printf("Add support for me!!!\n");
	}


	return hBox;
}

static GtkWidget* gen_cam_controls(void)
{
	GtkWidget *vBox;
	GtkWidget *hBox;
	int ret;
	vBox = gtk_vbox_new( FALSE, 5 );
	gtk_container_set_border_width( GTK_CONTAINER( vBox ), 10 );

	memset(&queryctrl, 0, sizeof(queryctrl));
	memset(&control, 0, sizeof(control));
	queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (queryctrl.id >= V4L2_CID_PRIVATE_BASE)
			break;

		control.id = queryctrl.id;
		ret = ioctl(fd, VIDIOC_G_CTRL, &control);
		if(ret < 0) {
			fprintf(stderr, "VIDIOC_G_CTRL failed: %s\n",
					strerror(errno));
			queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
			continue;
		}

		hBox = gen_cam_control(&control, &queryctrl);
		if (hBox != NULL) {
			gtk_box_pack_start(GTK_BOX(vBox), hBox, FALSE, FALSE, 0);
			gtk_widget_show(hBox);

			if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				gtk_widget_set_sensitive(hBox, FALSE);
		}
		queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	return vBox;
}

#ifndef NOAUDIO
/*
 * Generate the widgets for audio statistics
 */
static GtkWidget* gen_audio_stats(void)
{
	GtkWidget *hBoxMain, *vBoxMain, *hBox;
	GtkWidget *label, *button;

	hBoxMain = gtk_hbox_new( FALSE, 10 );
	gtk_container_set_border_width( GTK_CONTAINER( hBoxMain ), 10 );

	/* create vertical box that will hold our settings, 5 px padding */
	vBoxMain = gtk_vbox_new( FALSE, 20 );

	/* Reset stats button */
	hBox = gtk_hbox_new( FALSE, 5 );

	extern volatile char audio_state;
	if (audio_state == 1)
		button = gtk_button_new_with_label("Stop audio");
	else
		button = gtk_button_new_with_label("Start audio");
	gtk_widget_set_size_request(button, 200, 30 );
	g_signal_connect( G_OBJECT(button), "clicked",
			G_CALLBACK(audio_startstop_cb), NULL);
	gtk_widget_show(button);
	gtk_box_pack_start( GTK_BOX( hBox ), button, TRUE, TRUE, 0 );

	button = gtk_button_new_with_label("Reset stats");
	gtk_widget_set_size_request(button, 200, 30 );
	g_signal_connect( G_OBJECT(button), "clicked",
			G_CALLBACK(audio_reset_cb), NULL);
	gtk_widget_show(button);
	gtk_box_pack_start( GTK_BOX( hBox ), button, TRUE, TRUE, 0 );


	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Device choice */
	extern volatile char *alsa_device_capture;
	GtkListStore *store;
	GtkTreeIter   iter;
	GtkCellRenderer  *cell;
	char *name, first = 1;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Device");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	/* Create a model for the devices */
	store = gtk_list_store_new( 1, G_TYPE_STRING );
	/* Populate the model with the resolutions */
	while(get_next_audioin_dev(&name)>=0) {
		if (strcmp("null", name) == 0)
			continue;
		if (strcmp("default", name) == 0)
			continue;
		if (strcmp("pulse", name) == 0)
			continue;
		if (strncmp("surround", name, 8) == 0)
			continue;
		if (strncmp("iec958", name, 6) == 0)
			continue;
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter, 0, name, -1 );
		if (first) {
			alsa_device_capture = name;
			first = 0;
		}
	}
	/* Create a combo box with our model as data source. */
	combo_audiodev = gtk_combo_box_new_with_model( GTK_TREE_MODEL( store ) );
	gtk_widget_set_size_request(combo_audiodev, 200, 30);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_audiodev), 0);
	g_signal_connect(G_OBJECT(combo_audiodev), "changed",
			G_CALLBACK(set_audiodev_cb), NULL);
	/* Remove our reference from store to avoid memory leak. */
	g_object_unref( G_OBJECT( store ) );

	/* Create a cell renderer. */
	cell = gtk_cell_renderer_text_new();
	/* Pack it to the combo box. */
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_audiodev), cell, TRUE);
	/* Connect the renderer to the data source */
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_audiodev), cell,
			"text", 0, NULL );
	gtk_box_pack_start( GTK_BOX( hBox ), combo_audiodev, FALSE, TRUE, 0 );
	gtk_widget_show( combo_audiodev );
	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );


	/* Display output file  */
	extern char *filename_audio;
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Output file");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label = gtk_label_new((filename_audio == NULL) ? "none" : filename_audio);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(vBoxMain), hBox, FALSE, FALSE, 0);
	gtk_widget_show(hBox);

	/* Number of audio frames */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Audio frames");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_frame = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_aud_frame ), 1, 0.5 );
	gtk_box_pack_start( GTK_BOX( hBox ), label_aud_frame, FALSE, TRUE, 0 );
	gtk_widget_show( label_aud_frame );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Number of zero-size audio frames */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Zero-size audio frames");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_nzero = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_aud_nzero ), 1, 0.5 );
	gtk_box_pack_start( GTK_BOX( hBox ), label_aud_nzero, FALSE, TRUE, 0 );
	gtk_widget_show( label_aud_nzero );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Percentage of zero-size audio frames */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Zero-size audio frames %");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_nzero_perc = gtk_label_new("0.0%");
	gtk_misc_set_alignment( GTK_MISC( label_aud_nzero_perc ), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox), label_aud_nzero_perc, FALSE, TRUE, 0);
	gtk_widget_show( label_aud_nzero_perc );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Percentage of zero-size audio frames LIVE */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Zero-size audio frames (live) %");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_nzero_live_perc = gtk_label_new("0%");
	gtk_misc_set_alignment(GTK_MISC(label_aud_nzero_live_perc), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hBox), label_aud_nzero_live_perc, FALSE,
			TRUE, 0);
	gtk_widget_show( label_aud_nzero_live_perc );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Max number of zero-size consecutive audio frames*/
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Maximum consecutive zero-size audio frames");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_nzero_maxseq = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_aud_nzero_maxseq ), 1, 0.5 );
	gtk_box_pack_start(GTK_BOX(hBox),label_aud_nzero_maxseq, FALSE, TRUE, 0);
	gtk_widget_show( label_aud_nzero_maxseq );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* Maximum frame size */
	hBox = gtk_hbox_new( FALSE, 5 );
	label = gtk_label_new("Maximum frame size");
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_container_add( GTK_CONTAINER( hBox ), label );
	gtk_widget_show( label );

	label_aud_maxsize = gtk_label_new("0");
	gtk_misc_set_alignment( GTK_MISC( label_aud_maxsize ), 1, 0.5 );
	gtk_box_pack_start( GTK_BOX( hBox ),label_aud_maxsize, FALSE, TRUE, 0 );
	gtk_widget_show( label_aud_maxsize );

	gtk_box_pack_start( GTK_BOX( vBoxMain ), hBox, FALSE, FALSE, 0 );
	gtk_widget_show( hBox );

	/* display the panel */
	gtk_box_pack_start( GTK_BOX( hBoxMain ), vBoxMain, TRUE, TRUE, 0 );
	gtk_widget_show( vBoxMain );

	return hBoxMain;
}


static void update_gui_audio()
{
	extern volatile long aud_frame, aud_frame_live;
	extern volatile long aud_nzero, aud_nzero_live;
	extern volatile int aud_nzero_maxseq;
	extern volatile int aud_maxsize;
	char string[128];
	/* Update audio statistic tab */
	/* Update 'Audio frames' */
	sprintf(string, "%li", aud_frame);
	gtk_label_set_text(GTK_LABEL(label_aud_frame), string);

	/* Update 'Zero-size audio frames' */
	sprintf(string, "%li", aud_nzero);
	gtk_label_set_text(GTK_LABEL(label_aud_nzero), string);

	/* Update 'Zero-size audio frames %' */
	sprintf(string, "%.1f%%", aud_frame == 0 ?
			0 : (100*(double) aud_nzero/ (double) aud_frame));
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_perc), string);

	/* Update 'Zero-size audio frames (LIVE) %' */
	sprintf(string, "%li%%", aud_frame_live == 0 ?
			0 : (100*aud_nzero_live/aud_frame_live));
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_live_perc), string);

	/* Update 'Maximum consecutive zero-size audio frames' */
	sprintf(string, "%i", aud_nzero_maxseq);
	gtk_label_set_text(GTK_LABEL(label_aud_nzero_maxseq), string);

	/* Update 'Maximum frane size' */
	sprintf(string, "%i", aud_maxsize);
	gtk_label_set_text(GTK_LABEL(label_aud_maxsize), string);
}
#endif

static gboolean update_gui(gpointer data)
{
	extern volatile char display_xu;
	extern volatile char puxu_anf_en;
	extern volatile char puxu_awdr_en;
	extern volatile char puxu_ae_en;
	extern volatile char puxu_awb_en;
	extern volatile char puxu_wbzone_en;
	extern volatile char puxu_expzone_en;
	extern volatile char reset_res;
	extern volatile char reset_fr;
	char string[128];
	int value;
	
	GtkListStore *store;
	GtkTreeIter   iter;

	/* Update the Video Controls tab */
	if (reset_fr) {
		gtk_range_set_value( GTK_RANGE(slider_framerate),
				get_v4l2_framerate() );
		reset_fr = 0;
	}

	if (reset_res) {
		extern struct v4l2_frmsize_discrete res[50];
		extern int nres;
		extern int cres;
		int i;

		/* Create a new model with the new resolutions */
		store = gtk_list_store_new( 1, G_TYPE_STRING );
		for(i = 0; i < nres; i++) {
			gchar * text = g_strdup_printf ("%ux%u",
					res[i].width, res[i].height);
			gtk_list_store_append( store, &iter );
			gtk_list_store_set( store, &iter, 0, text, -1 );
		}
		/* Link the combo box to the new model */
		gtk_combo_box_set_model(GTK_COMBO_BOX(combo_res),
				GTK_TREE_MODEL( store ));
		/* Remove our reference from store to avoid memory leak */
		g_object_unref( G_OBJECT( store ) );

		/* Set the active resolution */
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_res), cres);

		reset_res = 0;
	}

	/* Hide/show the extension controls tab */
	if (display_xu == 1) {
		/* Update the current resolution */
		int i, value;
		if(get_v4l2_ctrl(V4L2_CID_XU_RESOLUTION2, &value) == 0) {
			for (i=0; i < NUM_XU_RES; i++) {
				if (xu_res_mapping[i].id2 == value)
					break;
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo_res_xu),i);
		} else if(get_v4l2_ctrl(V4L2_CID_XU_RESOLUTION, &value) == 0) {
			for (i=0; i < NUM_XU_RES; i++) {
				if (xu_res_mapping[i].id == value)
					break;
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo_res_xu), i);
		} else {
			gtk_widget_set_sensitive(combo_res_xu, FALSE);
		}

		/* Disable resolution in Stream Parameter tab */
		gtk_widget_set_sensitive(hBox_res_strp, FALSE);

		/* Enable the tab */
		gtk_widget_set_sensitive(
			gtk_notebook_get_tab_label(
				GTK_NOTEBOOK(gtk_widget_get_parent(hBox_xu)),
				hBox_xu),
			TRUE);
		gtk_widget_set_sensitive( hBox_xu, TRUE );
		display_xu = 0;
	} else if (display_xu == 2) {
		/* Disable the tab */
		gtk_widget_set_sensitive(
			gtk_notebook_get_tab_label(
				GTK_NOTEBOOK(gtk_widget_get_parent(hBox_xu)),
				hBox_xu),
			FALSE);
		gtk_widget_set_sensitive( hBox_xu, FALSE );
		display_xu = 0;

		/* Enable resolution in Stream Parameter tab */
		gtk_widget_set_sensitive(hBox_res_strp, TRUE);
	}

	if (puxu_anf_en == 1)
		gtk_widget_set_sensitive(hBox_puxu_nf, TRUE);
	else if (puxu_anf_en == 2)
		gtk_widget_set_sensitive(hBox_puxu_nf, FALSE);

	if (puxu_awdr_en == 1)
		gtk_widget_set_sensitive(hBox_puxu_wdr, TRUE);
	else if (puxu_awdr_en == 2)
		gtk_widget_set_sensitive(hBox_puxu_wdr, FALSE);

	if (puxu_ae_en == 1) {
		gtk_widget_set_sensitive(hBox_puxu_ae, TRUE);
		gtk_widget_set_sensitive(hBox_puxu_expzone_ena, TRUE);
        if (puxu_expzone_en == 1)
            gtk_widget_set_sensitive(hBox_puxu_expzone, FALSE);
        else if (puxu_expzone_en == 2)
            gtk_widget_set_sensitive(hBox_puxu_expzone, TRUE);
    }
	else if (puxu_ae_en == 2) {
		gtk_widget_set_sensitive(hBox_puxu_ae, FALSE);
		gtk_widget_set_sensitive(hBox_puxu_expzone_ena, FALSE);
        gtk_widget_set_sensitive(hBox_puxu_expzone, FALSE);
    }

	if (puxu_awb_en == 1) {
		gtk_widget_set_sensitive(hBox_puxu_awb, TRUE);
		gtk_widget_set_sensitive(hBox_puxu_wbzone_ena, TRUE);
        if (puxu_wbzone_en == 1)
            gtk_widget_set_sensitive(hBox_puxu_wbzone, FALSE);
        else if (puxu_wbzone_en == 2)
            gtk_widget_set_sensitive(hBox_puxu_wbzone, TRUE);
    }
	else if (puxu_awb_en == 2) {
		gtk_widget_set_sensitive(hBox_puxu_awb, FALSE);
		gtk_widget_set_sensitive(hBox_puxu_wbzone_ena, FALSE);
		gtk_widget_set_sensitive(hBox_puxu_wbzone, FALSE);
    }

	/* Update framerate */
	extern double cur_framerate;
	sprintf(string, "%.2f", cur_framerate);
	gtk_label_set_text(GTK_LABEL(label_vid_framerate), string);

	/* Update bitrate */
	extern int videoElementaryStreamBitrate;
	float cur_bitrate = videoElementaryStreamBitrate/1000;
	sprintf(string, "%.2f Mbps", cur_bitrate/1000);
	gtk_label_set_text(GTK_LABEL(label_vid_bitrate), string);

	/* Update frame drops */
	extern volatile int framedrops;
	sprintf(string, "%d", framedrops);
	gtk_label_set_text(GTK_LABEL(label_vid_framedrop), string);

	/* Update Start/stop video button */
	extern volatile char capture_state;
	if (capture_state == 0) {
		gtk_button_set_label(GTK_BUTTON(button_startstop),
				"Start video capture");
	} else {
		gtk_button_set_label(GTK_BUTTON(button_startstop),
				"Stop video capture");
	}
#ifndef NOAUDIO
	update_gui_audio();
#endif

	if(camera_mvmt_query_enable == 1){
		if(get_v4l2_ctrl(V4L2_CID_PU_XU_MVMT_QUERY, &value) == 0){	
			sprintf(string, "%i", value);
			gtk_label_set_text(GTK_LABEL(camera_mvmt_query), string);		
		}
	}
	return TRUE;
}

static void * gtk_main_func(void *ptr)
{
	/* enter main event loop */
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return NULL;
}

/* GUI creation */
void gui_init(int *argc, char ***argv)
{
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *hBox;
	GtkWidget *label;

	/* Make GTK thread safe */
	g_thread_init(NULL);
	gdk_threads_init();
	/* Initialize GTK and start the control GUI in a thread */
	gtk_init( argc, argv );

	/* initialize function pointer for newer GTK+ versions */
	compat_init();

	/* create a new window */
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );

	/* set default window size */
	gtk_window_set_default_size( GTK_WINDOW( window ), 500, 10 );

	/* set window position to be centered */
	gtk_window_set_position( GTK_WINDOW( window ), GTK_WIN_POS_CENTER );

	/* set window title */
	gtk_window_set_title( GTK_WINDOW( window ), "mxview" );

	/* set window border */
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	/* set window icon from stock image */
	gtk_window_set_icon_name( GTK_WINDOW( window ), GTK_STOCK_ZOOM_FIT );

	/* connect exit signals */
	g_signal_connect( G_OBJECT( window ), "destroy",
	                  G_CALLBACK( destroy_cb ), NULL );
	g_signal_connect( G_OBJECT( window ), "delete_event",
	                  G_CALLBACK( delete_cb ), NULL );

	/*
	* Create GTK interface
	*/
	notebook = gtk_notebook_new();

	/* Streaming Parameters page */
	label = gtk_label_new("Streaming Parameters");
	hBox = gen_video_controls();
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook), hBox, label);
	gtk_widget_show( hBox );

	/* Standard Control page */
	label = gtk_label_new("Standard Controls");
	hBox = gen_cam_controls();
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook), hBox, label);
	gtk_widget_show( hBox );

	/* AVC extensions page */
	label = gtk_label_new("AVC Extensions");
	hBox_xu = gen_xu_controls();
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook), hBox_xu, label);
	gtk_widget_show( hBox_xu );

	/* Skype extensions page */
	hBox = gen_skype_controls();
	if(hBox) {
		label = gtk_label_new("Skype Extensions");
		gtk_notebook_append_page( GTK_NOTEBOOK(notebook), hBox, label);
		gtk_widget_show( hBox );
	}

	/* PU XU extension page */
	label = gtk_label_new("PU Extensions");
	hBox = gen_puxu_controls();
	if (hBox) {
		/* Show this page in a scrolled window */
		GtkWidget *swin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin),
				hBox);
		/* Remove the extra border added by the viewport */
		gtk_viewport_set_shadow_type(
				GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(swin))),
				GTK_SHADOW_NONE);
		gtk_notebook_append_page( GTK_NOTEBOOK(notebook), swin, label);
		gtk_widget_show( hBox );
		gtk_widget_show( swin );
	}

#ifndef NOAUDIO
	/* Audio stats page */
	label = gtk_label_new("Audio Statistics");
	hBox = gen_audio_stats();
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook), hBox, label);
	gtk_widget_show( hBox );
#endif

	/* Display the window */
	gtk_container_add( GTK_CONTAINER( window ), notebook );
	gtk_widget_show( notebook );
	gtk_widget_show( window );

	/* Setup GUI update function for every 500ms */
	g_timeout_add(500, update_gui, NULL);

	/* Start the event loop in a separate thread */
	pthread_create( &gui_thread, NULL, &gtk_main_func, NULL);
}
