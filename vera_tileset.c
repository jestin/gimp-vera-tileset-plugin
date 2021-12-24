#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#define SAVE_PROC	"file-vera-save"
#define PLUG_IN_BINARY   "file-vera"

static void query (void);
static void run   (const gchar      *name,
		gint              nparams,
		const GimpParam  *param,
		gint             *nreturn_vals,
		GimpParam       **return_vals);

static gint save_image  (const gchar      *filename,
		gint32            image_ID,
		gint32            drawable_ID,
		GError          **error);

static gboolean save_dialog (void);

GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,
	NULL,
	query,
	run
};

static gint  compression = 0;

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

static void run (
		const gchar *name,
		gint nparams,
		const GimpParam *param,
		gint *nreturn_vals,
		GimpParam **return_vals)
{
	static GimpParam   values[2];
	GimpRunMode        run_mode;
	GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
	gint32             image_ID;
	gint32             drawable_ID;
	GimpExportReturn   export = GIMP_EXPORT_CANCEL;
	GError            *error  = NULL;

	// INIT_I18N ();
	gegl_init (NULL, NULL);

	run_mode = param[0].data.d_int32;

	*nreturn_vals = 1;
	*return_vals  = values;

	values[0].type          = GIMP_PDB_STATUS;
	values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	if (strcmp (name, SAVE_PROC) == 0)
	{
		image_ID    = param[1].data.d_int32;
		drawable_ID = param[2].data.d_int32;

		/*  eventually export the image */
		switch (run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
			case GIMP_RUN_WITH_LAST_VALS:
				gimp_ui_init (PLUG_IN_BINARY, FALSE);

				export = gimp_export_image (&image_ID, &drawable_ID, "VERA",
						GIMP_EXPORT_CAN_HANDLE_INDEXED);

				if (export == GIMP_EXPORT_CANCEL)
				{
					values[0].data.d_status = GIMP_PDB_CANCEL;
					return;
				}
				break;
			default:
				break;
		}

		switch (run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &compression);

				/*
				 * Then acquire information with a dialog...
				 */
				if (!save_dialog ())
					status = GIMP_PDB_CANCEL;
				break;

			case GIMP_RUN_NONINTERACTIVE:
				/*
				 * Make sure all the arguments are there!
				 */
				if (nparams != 6)
				{
					status = GIMP_PDB_CALLING_ERROR;
				}
				else
				{
					compression = param[5].data.d_int32;

					if (compression < 0 || compression > 2)
						status = GIMP_PDB_CALLING_ERROR;
				};
				break;

			case GIMP_RUN_WITH_LAST_VALS:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &compression);
				break;

			default:
				break;
		};

		if (status == GIMP_PDB_SUCCESS)
		{
			if (save_image (param[3].data.d_string, image_ID, drawable_ID,
						&error))
			{
				gimp_set_data (SAVE_PROC, &compression, sizeof (compression));
			}
			else
			{
				status = GIMP_PDB_EXECUTION_ERROR;
			}
		}

		if (export == GIMP_EXPORT_EXPORT)
			gimp_image_delete (image_ID);
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

	static gint
save_image (const gchar  *filename,
		gint32        image_ID,
		gint32        drawable_ID,
		GError      **error)
{
	gint         i, j,        /* Looping var */
				 x,           /* Current X coordinate */
				 y,           /* Current Y coordinate */
				 width,       /* Drawable width */
				 height,      /* Drawable height */
				 tile_height, /* Height of tile in GIMP */
				 count,       /* Count of rows to put in image */
				 zsize;       /* Number of channels in file */
	// sgi_t       *sgip;        /* File pointer */
	GeglBuffer  *buffer;      /* Buffer for layer */
	const Babl  *format;
	guchar     **pixels,      /* Pixel rows */
			   *pptr;        /* Current pixel */
	gushort    **rows;        /* SGI image data */

	/*
	 * Get the drawable for the current image...
	 */

	width  = gimp_drawable_width  (drawable_ID);
	height = gimp_drawable_height (drawable_ID);

	buffer = gimp_drawable_get_buffer (drawable_ID);

	switch (gimp_drawable_type (drawable_ID))
	{
		case GIMP_INDEXED_IMAGE:
			zsize = 3;
			format = babl_format ("R'G'B' u8");
			break;

		case GIMP_INDEXEDA_IMAGE:
			format = babl_format ("R'G'B'A u8");
			zsize = 4;
			break;

		default:
			return FALSE;
	}

	/*
	 * Open the file for writing...
	 */

	gimp_progress_init_printf ("Exporting '%s'",
			gimp_filename_to_utf8 (filename));

	// sgip = sgiOpen (filename, SGI_WRITE, compression, 1,
	// 		width, height, zsize);
	// if (sgip == NULL)
	// {
	// 	g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
	// 			_("Could not open '%s' for writing."),
	// 			gimp_filename_to_utf8 (filename));
	// 	return FALSE;
	// };

	/*
	 * Allocate memory for "tile_height" rows...
	 */

	tile_height = gimp_tile_height ();
	pixels      = g_new (guchar *, tile_height);
	pixels[0]   = g_new (guchar, ((gsize) tile_height) * width * zsize);

	for (i = 1; i < tile_height; i ++)
		pixels[i]= pixels[0] + width * zsize * i;

// 	rows    = g_new (gushort *, sgip->zsize);
// 	rows[0] = g_new (gushort, ((gsize) sgip->xsize) * sgip->zsize);
// 
// 	for (i = 1; i < sgip->zsize; i ++)
// 		rows[i] = rows[0] + i * sgip->xsize;

	/*
	 * Save the image...
	 */

	for (y = 0; y < height; y += count)
	{
		/*
		 * Grab more pixel data...
		 */

		if ((y + tile_height) >= height)
			count = height - y;
		else
			count = tile_height;

		gegl_buffer_get (buffer, GEGL_RECTANGLE (0, y, width, count), 1.0,
				format, pixels[0],
				GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

		/*
		 * Convert to shorts and write each color plane separately...
		 */

		for (i = 0, pptr = pixels[0]; i < count; i ++)
		{
			for (x = 0; x < width; x ++)
				for (j = 0; j < zsize; j ++, pptr ++)
					rows[j][x] = *pptr;

			// for (j = 0; j < zsize; j ++)
			// 	sgiPutRow (sgip, rows[j], height - 1 - y - i, j);
		};

		gimp_progress_update ((double) y / (double) height);
	}

	/*
	 * Done with the file...
	 */

	// sgiClose (sgip);

	g_free (pixels[0]);
	g_free (pixels);
	g_free (rows[0]);
	g_free (rows);

	g_object_unref (buffer);

	gimp_progress_update (1.0);

	return TRUE;
}

	static gboolean
save_dialog (void)
{
	GtkWidget *dialog;
	GtkWidget *frame;
	gboolean   run;

	dialog = gimp_export_dialog_new ("VERA", PLUG_IN_BINARY, SAVE_PROC);

	// frame = gimp_int_radio_group_new (TRUE, _("Compression type"),
	//                                   G_CALLBACK (gimp_radio_button_update),
	//                                   &compression, compression,

	//                                   _("No compression"),
	//                                   SGI_COMP_NONE, NULL,
	//                                   _("RLE compression"),
	//                                   SGI_COMP_RLE, NULL,
	//                                   _("Aggressive RLE\n(not supported by SGI)"),
	//                                   SGI_COMP_ARLE, NULL,

	//                                   NULL);

	gtk_container_set_border_width (GTK_CONTAINER (frame), 12);
	gtk_box_pack_start (GTK_BOX (gimp_export_dialog_get_content_area (dialog)),
			frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);

	gtk_widget_show (dialog);

	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

	gtk_widget_destroy (dialog);

	return run;
}
