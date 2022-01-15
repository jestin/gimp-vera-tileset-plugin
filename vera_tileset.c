#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <errno.h>
#include <string.h>

#include <stdio.h>

#define SAVE_PROC	"file-vera-save"
#define PLUG_IN_BINARY   "file-vera"

static void query (void);
static void run (const gchar      *name,
		gint              nparams,
		const GimpParam  *param,
		gint             *nreturn_vals,
		GimpParam       **return_vals);

static gboolean save_image (const gchar  *filename,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error);

// static gboolean save_dialog (void);

typedef enum
{
	RAW_RGB,          /* RGB Image */
	RAW_RGBA,         /* RGB Image with an Alpha channel */
	RAW_RGB565_BE,    /* RGB Image 16bit, 5,6,5 bits per channel, big-endian */
	RAW_RGB565_LE,    /* RGB Image 16bit, 5,6,5 bits per channel, little-endian */
	RAW_BGR565_BE,    /* RGB Image 16bit, 5,6,5 bits per channel, big-endian, red and blue swapped */
	RAW_BGR565_LE,    /* RGB Image 16bit, 5,6,5 bits per channel, little-endian, red and blue swapped */
	RAW_PLANAR,       /* Planar RGB */
	RAW_GRAY_1BPP,
	RAW_GRAY_2BPP,
	RAW_GRAY_4BPP,
	RAW_GRAY_8BPP,
	RAW_INDEXED,      /* Indexed image */
	RAW_INDEXEDA,     /* Indexed image with an Alpha channel */
	RAW_GRAY_16BPP_BE,
	RAW_GRAY_16BPP_LE,
	RAW_GRAY_16BPP_SBE,
	RAW_GRAY_16BPP_SLE,
} RawType;

typedef enum
{
	RAW_PALETTE_RGB,  /* standard RGB */
	RAW_PALETTE_BGR   /* Windows BGRX */
} RawPaletteType;

typedef struct
{
	RawType        image_type;     /* type of image (RGB, PLANAR) */
	RawPaletteType palette_type;   /* type of palette (RGB/BGR)   */
} RawSaveVals;

typedef struct
{
	gboolean   run;

	GtkWidget *image_type_standard;
	GtkWidget *image_type_planar;
	GtkWidget *palette_type_normal;
	GtkWidget *palette_type_bmp;
} RawSaveGui;

typedef struct
{
	gint32         file_offset;    /* offset to beginning of image in raw data */
	gint32         image_width;    /* width of the raw image                   */
	gint32         image_height;   /* height of the raw image                  */
	RawType        image_type;     /* type of image (RGB, INDEXED, etc)        */
	gint32         palette_offset; /* offset inside the palette file, if any   */
	RawPaletteType palette_type;   /* type of palette (RGB/BGR)                */
} RawConfig;

typedef struct
{
	FILE         *fp;        /* pointer to the already open file */
	GeglBuffer   *buffer;    /* gimp drawable buffer             */
	gint32        image_id;  /* gimp image id                    */
	guchar        cmap[768]; /* color map for indexed images     */
} RawGimpData;

GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,
	NULL,
	query,
	run
};

static const RawSaveVals defaults =
{
	RAW_RGB,
	RAW_PALETTE_RGB
};

static RawSaveVals rawvals;

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

		// load_defaults ();

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
				gimp_get_data (SAVE_PROC, &rawvals);

				/*
				 * Then acquire information with a dialog...
				 */
				// if (! save_dialog (image_id))
				// 	status = GIMP_PDB_CANCEL;
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
						rawvals.image_type   = param[5].data.d_int32;
						rawvals.palette_type = param[6].data.d_int32;

						if (((rawvals.image_type != RAW_RGB) && (rawvals.image_type != RAW_PLANAR)) ||
								((rawvals.palette_type != RAW_PALETTE_RGB) && (rawvals.palette_type != RAW_PALETTE_BGR)))
						{
							status = GIMP_PDB_CALLING_ERROR;
						}
					}
				}
				break;

			case GIMP_RUN_WITH_LAST_VALS:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &rawvals);
				break;

			default:
				break;
		}

		if (status == GIMP_PDB_SUCCESS)
		{
			if (save_image (param[3].data.d_string,
						image_id, drawable_id, &error))
			{
				gimp_set_data (SAVE_PROC, &rawvals, sizeof (rawvals));
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

static gboolean save_image (const gchar  *filename,
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

	// write out per tile, not per pixel
	// for now assuming 4bbp and 8x8 tiles

	tile_buf = g_new (guchar, (width * height * bpp) / 2) + 2;

	int tile_width = 8;
	int tile_height = 8;
	int t_width = width / tile_width;
	int t_height = height / tile_height;
	int tile_buf_index = 2;
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

	switch (rawvals.image_type)
	{
		case RAW_RGB:
			if (! fwrite (tile_buf, (width * height * bpp) / 2, 1, fp))  // segfault here
			{
				return FALSE;
			}

			fclose (fp);


			if (cmap)
			{
				/* we have colormap, too.write it into filename+pal */
				gchar *newfile = g_strconcat (filename, ".pal", NULL);
				gchar *temp;

				fp = fopen (newfile, "wb");

				if (! fp)
				{
					g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
							"Could not open '%s' for writing: %s",
							gimp_filename_to_utf8 (newfile), g_strerror (errno));
					return FALSE;
				}

				switch (rawvals.palette_type)
				{
					case RAW_PALETTE_RGB:
						if (!fwrite (cmap, palsize * 3, 1, fp))
							ret = FALSE;
						fclose (fp);
						break;

					case RAW_PALETTE_BGR:
						temp = g_malloc0 (palsize * 4);
						for (i = 0, j = 0; i < palsize * 3; i += 3)
						{
							temp[j++] = cmap[i + 2];
							temp[j++] = cmap[i + 1];
							temp[j++] = cmap[i + 0];
							temp[j++] = 0;
						}
						if (!fwrite (temp, palsize * 4, 1, fp))
							ret = FALSE;
						fclose (fp);
						g_free (temp);
						break;
				}
			}
			break;

		default:
			break;
	}

	return ret;
}

// static gboolean
// save_dialog (void)
// {
// 	GtkWidget *dialog;
// 	GtkWidget *frame;
// 	gboolean   run;
// 
// 	dialog = gimp_export_dialog_new ("VERA", PLUG_IN_BINARY, SAVE_PROC);
// 
// 	// frame = gimp_int_radio_group_new (TRUE, _("Compression type"),
// 	//                                   G_CALLBACK (gimp_radio_button_update),
// 	//                                   &compression, compression,
// 
// 	//                                   _("No compression"),
// 	//                                   SGI_COMP_NONE, NULL,
// 	//                                   _("RLE compression"),
// 	//                                   SGI_COMP_RLE, NULL,
// 	//                                   _("Aggressive RLE\n(not supported by SGI)"),
// 	//                                   SGI_COMP_ARLE, NULL,
// 
// 	//                                   NULL);
// 
// 	gtk_container_set_border_width (GTK_CONTAINER (frame), 12);
// 	gtk_box_pack_start (GTK_BOX (gimp_export_dialog_get_content_area (dialog)),
// 			frame, TRUE, TRUE, 0);
// 	gtk_widget_show (frame);
// 
// 	gtk_widget_show (dialog);
// 
// 	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);
// 
// 	gtk_widget_destroy (dialog);
// 
// 	return run;
// }
