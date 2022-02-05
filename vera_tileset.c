#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#define SAVE_PROC	"file-vera-save"
#define PLUG_IN_BINARY   "file-vera"
#define VERA_DEFAULTS_PARASITE  "vera-save-defaults"

static void query(void);
static void run(const gchar      *name,
		gint              nparams,
		const GimpParam  *param,
		gint             *nreturn_vals,
		GimpParam       **return_vals);

static gboolean save_tile_set(const gchar  *filename,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error);

static gboolean save_tsx(const gchar *filename,
		const gchar *bmp_filename,
		GimpRunMode   run_mode,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error);


typedef enum
{
	TILE_2BPP = 2,
	TILE_4BPP = 4,
	TILE_8BPP = 8
} TileBpp;

typedef enum
{
	TILE_WIDTH_8 = 8,
	TILE_WIDTH_16 = 16
} TileWidth;

typedef enum
{
	TILE_HEIGHT_8 = 8,
	TILE_HEIGHT_16 = 16
} TileHeight;

typedef struct
{
	TileBpp        tile_bpp;     /* Bits per pixel format for tiles */
	TileWidth      tile_width;
	TileHeight     tile_height;
	gboolean       tiled_file;
	gboolean       bmp_file;
	gboolean       pal_file;
} VeraTileSaveVals;

typedef struct
{
	gboolean   run;

	GtkWidget *tile_2bpp;
	GtkWidget *tile_4bpp;
	GtkWidget *tile_8bpp;
	GtkWidget *tile_width_8;
	GtkWidget *tile_width_16;
	GtkWidget *tile_height_8;
	GtkWidget *tile_height_16;
	GtkWidget *no_tiled_file;
	GtkWidget *tiled_file;
	GtkWidget *bmp_file;
	GtkWidget *pal_file;
} VeraTileSaveGui;

GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,
	NULL,
	query,
	run
};

static const VeraTileSaveVals defaults =
{
	TILE_4BPP,
	TILE_WIDTH_8,
	TILE_HEIGHT_8,
	TRUE,
	TRUE,
	TRUE,
};

static VeraTileSaveVals veravals;
static gboolean save_tiles_dialog(gint32 image_id);
static void save_dialog_response(GtkWidget *widget,
		gint response_id,
		gpointer data);
static void load_defaults(void);
static void save_defaults(void);
static void load_gui_defaults(VeraTileSaveGui *vg);

MAIN()

static void query (void)
{
	static const GimpParamDef save_args[] =
	{
		{ GIMP_PDB_INT32,    "run-mode",     "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
		{ GIMP_PDB_IMAGE,    "image",        "Input image" },
		{ GIMP_PDB_DRAWABLE, "drawable",     "Drawable to export" },
		{ GIMP_PDB_STRING,   "filename",     "The name of the file to export the image in" },
		{ GIMP_PDB_STRING,   "raw-filename", "The name of the file to export the image in" }
	};

	gimp_install_procedure (SAVE_PROC,
			"Exports files in VERA compatible binaries",
			"This plug-in exports binary files for VERA chips.",
			"Jestin Stoffel <jestin.stoffel@gmail.com>",
			"Copyright 2021-2022 by Jestin Stoffel",
			"0.0.1 - 2021",
			"VERA tile set",
			"INDEXED*",
			GIMP_PLUGIN,
			G_N_ELEMENTS (save_args),
			0,
			save_args,
			NULL);

	gimp_register_file_handler_mime (SAVE_PROC, "application/octet-stream");
	gimp_register_save_handler (SAVE_PROC, "BIN", "");
}

static void run (const gchar      *name,
		gint              nparams,
		const GimpParam  *param,
		gint             *nreturn_vals,
		GimpParam       **return_vals)
{
	static GimpParam   values[3];
	GimpRunMode        run_mode;
	GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
	GError            *error  = NULL;
	gint32             image_id;
	gint32             drawable_id;
	gchar*             filename;
	GimpExportReturn   export = GIMP_EXPORT_CANCEL;

	// INIT_I18N ();
	gegl_init (NULL, NULL);

	*nreturn_vals = 1;
	*return_vals  = values;

	values[0].type          = GIMP_PDB_STATUS;
	values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	if (strcmp (name, SAVE_PROC) == 0)
	{
		run_mode    = param[0].data.d_int32;
		image_id    = param[1].data.d_int32;
		drawable_id = param[2].data.d_int32;
		filename = param[3].data.d_string;

		load_defaults ();

		/* export the image */
		export = gimp_export_image (&image_id, &drawable_id, "VERA",
				GIMP_EXPORT_CAN_HANDLE_INDEXED);

		if (export == GIMP_EXPORT_CANCEL)
		{
			*nreturn_vals = 1;
			values[0].data.d_status = GIMP_PDB_CANCEL;
			return;
		}

		switch (run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &veravals);

				/*
				 * Then acquire information with a dialog...
				 */
				if (! save_tiles_dialog (image_id))
					status = GIMP_PDB_CANCEL;
				break;

			case GIMP_RUN_NONINTERACTIVE:
				/*
				 * Make sure all the arguments are there!
				 */
				if (nparams != 5)
				{
					if (nparams != 7)
					{
						status = GIMP_PDB_CALLING_ERROR;
					}
					else
					{
						veravals.tile_bpp   = param[5].data.d_int32;
					}
				}
				break;

			case GIMP_RUN_WITH_LAST_VALS:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &veravals);
				break;

			default:
				break;
		}

		if (status == GIMP_PDB_SUCCESS)
		{
			gchar *bmp_filename = g_strconcat (filename, ".bmp", NULL);
			if (veravals.bmp_file)
			{
				// write out a bitmap to be used with the .tsx file
				gimp_file_save(run_mode, image_id, drawable_id, bmp_filename, bmp_filename);
			}

			if(veravals.tiled_file)
			{
				if(!save_tsx(filename, bmp_filename,  GIMP_RUN_NONINTERACTIVE, image_id, drawable_id, &error))
				{
					status = GIMP_PDB_EXECUTION_ERROR;
				}
			}

			if (save_tile_set (filename, image_id, drawable_id, &error))
			{
				gimp_set_data (SAVE_PROC, &veravals, sizeof (veravals));
			}
			else
			{
				status = GIMP_PDB_EXECUTION_ERROR;
			}
		}

		if (export == GIMP_EXPORT_EXPORT)
			gimp_image_delete (image_id);
	}
	else
	{
		status = GIMP_PDB_CALLING_ERROR;
	}

	if (status != GIMP_PDB_SUCCESS && error)
	{
		*nreturn_vals = 2;
		values[1].type          = GIMP_PDB_STRING;
		values[1].data.d_string = error->message;
	}

	values[0].data.d_status = status;
}

static gboolean save_tsx (const gchar  *filename,
		const gchar  *bmp_filename,
		GimpRunMode   run_mode,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error)
{
	// write out the tsx file
	gchar *tsx_filename = g_strconcat (filename, ".tsx", NULL);

	int rc;
	xmlTextWriterPtr writer;
	xmlChar* tmp;

	writer = xmlNewTextWriterFilename(tsx_filename, 0);
	if(writer == NULL)
	{
		printf("could not create writer\n");
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				"Could not open '%s' for writing: %s",
				gimp_filename_to_utf8 (filename), g_strerror (errno));
		return FALSE;
	}

	rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	if (rc < 0)
	{
		printf("could not create document\n");
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				"Error starting document '%s': %s",
				gimp_filename_to_utf8 (filename), g_strerror (errno));
		return FALSE;
	}

	GeglBuffer       *buffer;
	gint32            width, height, tile_count, columns;

	/* get info about the current image */
	buffer = gimp_drawable_get_buffer (drawable_id);
	width  = gegl_buffer_get_width  (buffer);
	height = gegl_buffer_get_height (buffer);
	
	tile_count = (width * height) / (veravals.tile_width * veravals.tile_height);
	columns = width / veravals.tile_width;

	printf("writing tsx document\n");
	gchar* val_string;

	xmlTextWriterStartElement(writer, BAD_CAST "tileset");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1.5");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "tiledversion", BAD_CAST "1.8.0");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "name", BAD_CAST filename);
	val_string = g_strdup_printf("%d", veravals.tile_width);
	xmlTextWriterWriteAttribute(writer,BAD_CAST "tilewidth", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", veravals.tile_height);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "tileheight", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", tile_count);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "tilecount", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", columns);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "columns", BAD_CAST val_string);
	g_free(val_string);

	xmlTextWriterStartElement(writer, BAD_CAST "image");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "source", BAD_CAST bmp_filename);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "trans", BAD_CAST "000000");
	val_string = g_strdup_printf("%d", width);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "width", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", height);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "height", BAD_CAST val_string);
	g_free(val_string);
	xmlTextWriterEndElement(writer); // image

	xmlTextWriterEndElement(writer); // tileset

	xmlTextWriterEndDocument(writer);

	xmlFreeTextWriter(writer);

	printf("finished writing tsx document\n");
}

static gboolean save_tile_set (const gchar  *filename,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error)
{
	GeglBuffer       *buffer;
	const Babl       *format = NULL;
	guchar           *cmap   = NULL;  /* colormap for indexed images */
	guchar           *buf;
	guchar           *tile_buf;
	guchar           *components[4] = { 0, };
	gint              n_components;
	gint32            width, height, bpp;
	FILE             *fp = NULL;
	gint              i, j, c;
	gint              palsize = 0;
	gboolean          ret = FALSE;

	/* get info about the current image */
	buffer = gimp_drawable_get_buffer (drawable_id);

	switch (gimp_drawable_type (drawable_id))
	{
		case GIMP_INDEXED_IMAGE:
		case GIMP_INDEXEDA_IMAGE:
			format = gimp_drawable_get_format (drawable_id);
			break;
	}

	n_components = babl_format_get_n_components (format);
	bpp          = babl_format_get_bytes_per_pixel (format);

	if (gimp_drawable_is_indexed (drawable_id))
	{
		cmap = gimp_image_get_colormap (image_id, &palsize);
	}

	width  = gegl_buffer_get_width  (buffer);
	height = gegl_buffer_get_height (buffer);

	buf = g_new (guchar, width * height * bpp);

	gegl_buffer_get (buffer, GEGL_RECTANGLE (0, 0, width, height), 1.0,
			format, buf,
			GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

	g_object_unref (buffer);

	int tile_width = veravals.tile_width;
	int tile_height = veravals.tile_height;
	int t_width = width / tile_width;
	int t_height = height / tile_height;

	int tile_buf_length = ((width * height * bpp) / (8 / veravals.tile_bpp)) + 2;
	tile_buf = g_new (guchar, tile_buf_length);

	int tile_buf_index = 2;

	// 2 byte header
	tile_buf[0] = 0;
	tile_buf[1] = 0;

	for(int y = 0; y < t_height; y++)
	{
		int yoff = y * tile_height;

		for(int x = 0; x < t_width; x++)
		{
			int xoff = x * tile_width;

			// write out a single tile
			for(int ty = 0; ty < tile_width; ty++)
			{
				for(int tx = 0; tx < tile_height; tx++)
				{
					// get the color from the buffer
					int buf_index = ((yoff + ty) * width) + xoff + tx;
					guchar color = buf[buf_index];

					switch(veravals.tile_bpp)
					{
						case TILE_2BPP:
							switch(buf_index % 4)
							{
								case 0:
									tile_buf[tile_buf_index] = color << 6;
									break;
								case 1:
									tile_buf[tile_buf_index] |= color << 4;
									break;
								case 2:
									tile_buf[tile_buf_index] |= color << 2;
									break;
								case 3:
									tile_buf[tile_buf_index] |= color;
									tile_buf_index++;
									break;
							}
							break;
						case TILE_4BPP:
							if (buf_index % 2)
							{
								// odd byte
								tile_buf[tile_buf_index] |= color;
								tile_buf_index++;
							}
							else
							{
								// even byte
								tile_buf[tile_buf_index] = color << 4;
							}
							break;
						case TILE_8BPP:
							tile_buf[tile_buf_index] = color;
							tile_buf_index++;
							break;
					}

				}
			}
		}
	}

	fp = fopen (filename, "wb");

	if (! fp)
	{
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				"Could not open '%s' for writing: %s",
				gimp_filename_to_utf8 (filename), g_strerror (errno));
		return FALSE;
	}

	ret = TRUE;

	if (! fwrite (tile_buf, tile_buf_length, 1, fp))
	{
		return FALSE;
	}

	fclose (fp);
	g_free(tile_buf);


	if (cmap && veravals.pal_file)
	{
		guchar *pal_buf;
		pal_buf = g_new (guchar, (palsize * 2) + 2); // 2 bytes per color, 2 byte header

		// 2 byte header
		pal_buf[0] = 0;
		pal_buf[1] = 0;

		int pal_buf_index = 2; // start past the 2 byte header

		for(int i = 0; i < palsize*3; i+=3)
		{
			// read rgb values from colormap
			guchar r = cmap[i];
			guchar g = cmap[i+1];
			guchar b = cmap[i+2];
			
			// write out packed g and b values
			pal_buf[pal_buf_index] = (g & 0xf0) | ((b & 0xf0) >> 4);

			// write out r value in lower nibble
			pal_buf[pal_buf_index+1] = (r & 0xf0) >> 4;

			pal_buf_index += 2;
		}

		/* we have colormap too, write it into filename+PAL.BIN */
		gchar *newfile = g_strconcat (filename, ".PAL", NULL);
		gchar *temp;

		fp = fopen (newfile, "wb");

		if (! fp)
		{
			g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
					"Could not open '%s' for writing: %s",
					gimp_filename_to_utf8 (newfile), g_strerror (errno));
			g_free(pal_buf);
			return FALSE;
		}

		if (!fwrite (pal_buf, (palsize * 2) + 2, 1, fp))
			ret = FALSE;
		fclose (fp);
		g_free(pal_buf);
	}

	return ret;
}

static GtkWidget * radio_button_init (GtkBuilder  *builder,
		const gchar *name,
		gint         item_data,
		gint         initial_value,
		gpointer     value_pointer)
{
	GtkWidget *radio = NULL;

	radio = GTK_WIDGET (gtk_builder_get_object (builder, name));
	if (item_data)
		g_object_set_data (G_OBJECT (radio), "gimp-item-data", GINT_TO_POINTER (item_data));
	if (initial_value == item_data)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	g_signal_connect (radio, "toggled",
			G_CALLBACK (gimp_radio_button_update),
			value_pointer);

	return radio;
}

static GtkWidget * check_button_init (GtkBuilder  *builder,
		const gchar *name,
		gint         item_data,
		gint         initial_value,
		gpointer     value_pointer)
{
	GtkWidget *radio = NULL;

	radio = GTK_WIDGET (gtk_builder_get_object (builder, name));
	if (item_data)
		g_object_set_data (G_OBJECT (radio), "gimp-item-data", GINT_TO_POINTER (item_data));
	if (initial_value == item_data)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	g_signal_connect (radio, "toggled",
			G_CALLBACK (gimp_toggle_button_update),
			value_pointer);

	return radio;
}

static gboolean save_tiles_dialog (gint32 image_id)
{
	VeraTileSaveGui  vg;
	GtkWidget  *dialog;
	GtkBuilder *builder;
	gchar      *ui_file;
	GError     *error = NULL;

	gimp_ui_init (PLUG_IN_BINARY, TRUE);

	/* Dialog init */
	dialog = gimp_export_dialog_new ("VERA Tile Data", PLUG_IN_BINARY, SAVE_PROC);
	g_signal_connect (dialog, "response",
			G_CALLBACK (save_dialog_response),
			&vg);
	g_signal_connect (dialog, "destroy",
			G_CALLBACK (gtk_main_quit),
			NULL);

	/* GtkBuilder init */
	builder = gtk_builder_new ();
	ui_file = g_build_filename (gimp_data_directory (),
			"ui/plug-ins/plug-in-file-vera-tiles.ui",
			NULL);
	if (! gtk_builder_add_from_file (builder, ui_file, &error))
	{
		gchar *display_name = g_filename_display_name (ui_file);
		g_printerr ("Error loading UI file '%s': %s",
				display_name, error ? error->message : "Unknown error");
		g_free (display_name);
	}

	g_free (ui_file);

	/* VBox */
	gtk_box_pack_start (GTK_BOX (gimp_export_dialog_get_content_area (dialog)),
			GTK_WIDGET (gtk_builder_get_object (builder, "vbox")),
			FALSE, FALSE, 0);

	/* Radios */
	vg.tile_2bpp = radio_button_init (builder, "tile-bpp-2",
			TILE_2BPP,
			veravals.tile_bpp,
			&veravals.tile_bpp);
	vg.tile_4bpp = radio_button_init (builder, "tile-bpp-4",
			TILE_4BPP,
			veravals.tile_bpp,
			&veravals.tile_bpp);
	vg.tile_8bpp = radio_button_init (builder, "tile-bpp-8",
			TILE_8BPP,
			veravals.tile_bpp,
			&veravals.tile_bpp);

	vg.tile_width_8 = radio_button_init (builder, "tile-width-8",
			TILE_WIDTH_8,
			veravals.tile_width,
			&veravals.tile_width);
	vg.tile_width_16 = radio_button_init (builder, "tile-width-16",
			TILE_WIDTH_16,
			veravals.tile_width,
			&veravals.tile_width);

	vg.tile_height_8 = radio_button_init (builder, "tile-height-8",
			TILE_HEIGHT_8,
			veravals.tile_height,
			&veravals.tile_height);
	vg.tile_height_16 = radio_button_init (builder, "tile-height-16",
			TILE_HEIGHT_16,
			veravals.tile_height,
			&veravals.tile_height);

	vg.tiled_file = check_button_init (builder, "tiled-file",
			TRUE,
			veravals.tiled_file,
			&veravals.tiled_file);

	vg.no_tiled_file = check_button_init (builder, "bmp-file",
			TRUE,
			veravals.bmp_file,
			&veravals.bmp_file);

	vg.no_tiled_file = check_button_init (builder, "pal-file",
			TRUE,
			veravals.pal_file,
			&veravals.pal_file);

	/* Load/save defaults buttons */
	g_signal_connect_swapped (gtk_builder_get_object (builder, "load-defaults"),
			"clicked",
			G_CALLBACK (load_gui_defaults),
			&vg);

	g_signal_connect_swapped (gtk_builder_get_object (builder, "save-defaults"),
			"clicked",
			G_CALLBACK (save_defaults),
			&vg);

	/* Show dialog and run */
	gtk_widget_show (dialog);

	vg.run = FALSE;

	gtk_main ();

	return vg.run;
}

static void save_dialog_response (GtkWidget *widget,
		gint       response_id,
		gpointer   data)
{
	VeraTileSaveGui *vg = data;

	switch (response_id)
	{
		case GTK_RESPONSE_OK:
			vg->run = TRUE;

		default:
			gtk_widget_destroy (widget);
			break;
	}
}

static void load_gui_defaults (VeraTileSaveGui *vg)
{
	load_defaults ();

#define SET_ACTIVE(field, datafield) \
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (vg->field), "gimp-item-data")) == veravals.datafield) \
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vg->field), TRUE)

	SET_ACTIVE (tile_2bpp, tile_bpp);
	SET_ACTIVE (tile_4bpp, tile_bpp);
	SET_ACTIVE (tile_8bpp, tile_bpp);

	SET_ACTIVE (tile_width_8, tile_width);
	SET_ACTIVE (tile_width_16, tile_width);

	SET_ACTIVE (tile_height_8, tile_height);
	SET_ACTIVE (tile_height_16, tile_height);

	SET_ACTIVE (tiled_file, tiled_file);
	SET_ACTIVE (bmp_file, bmp_file);
	SET_ACTIVE (pal_file, pal_file);

#undef SET_ACTIVE
}

static void load_defaults (void) {
	GimpParasite *parasite;

	/* initialize with hardcoded defaults */
	veravals = defaults;

	parasite = gimp_get_parasite (VERA_DEFAULTS_PARASITE);

	if (parasite)
	{
		gchar        *def_str;
		VeraTileSaveVals   tmpvals = defaults;
		gint          num_fields;

		def_str = g_strndup (gimp_parasite_data (parasite),
				gimp_parasite_data_size (parasite));

		gimp_parasite_free (parasite);

		num_fields = sscanf (def_str, "%d", (int *) &tmpvals.tile_bpp);

		g_free (def_str);

		if (num_fields == 2)
			veravals = tmpvals;
	}
}

static void save_defaults (void)
{
	GimpParasite *parasite;
	gchar        *def_str;

	def_str = g_strdup_printf ("%d", veravals.tile_bpp);

	parasite = gimp_parasite_new (VERA_DEFAULTS_PARASITE,
			GIMP_PARASITE_PERSISTENT,
			strlen (def_str), def_str);

	gimp_attach_parasite (parasite);

	gimp_parasite_free (parasite);
	g_free (def_str);
}
