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

static gboolean save_bitmap(const gchar  *filename,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error);

static gboolean save_tsx(const gchar *filename,
		const gchar *bmp_filename,
		GimpRunMode   run_mode,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error);

static gboolean save_palette(const gchar *filename,
		const guchar      *cmap,
		const gint        palsize,
		GError      **error);

static void shift_color_map(guchar* orig,
		guchar** shifted,
		gint palsize,
		gint offset);

typedef enum
{
	TILESET = 0,
	BITMAP = 1
} VeraExport;

typedef enum
{
	TILE_1BPP = 1,
	TILE_2BPP = 2,
	TILE_4BPP = 4,
	TILE_8BPP = 8
} TileBpp;

typedef enum
{
	TILE_WIDTH_8 = 8,
	TILE_WIDTH_16 = 16,
	TILE_WIDTH_32 = 32,
	TILE_WIDTH_64 = 64
} TileWidth;

typedef enum
{
	TILE_HEIGHT_8 = 8,
	TILE_HEIGHT_16 = 16,
	TILE_HEIGHT_32 = 32,
	TILE_HEIGHT_64 = 64
} TileHeight;

typedef struct
{
	VeraExport     export_type;	 /* tileset or bitmap */
	TileBpp        tile_bpp;     /* Bits per pixel format for tiles */
	TileWidth      tile_width;
	TileHeight     tile_height;
	gboolean       tiled_file;
	gboolean       bmp_file;
	gboolean       pal_file;
} VeraSaveVals;

typedef struct
{
	gboolean   run;

	// tile dialog
	GtkWidget *tile_1bpp;
	GtkWidget *tile_2bpp;
	GtkWidget *tile_4bpp;
	GtkWidget *tile_8bpp;
	GtkWidget *tile_width_8;
	GtkWidget *tile_width_16;
	GtkWidget *tile_width_32;
	GtkWidget *tile_width_64;
	GtkWidget *tile_height_8;
	GtkWidget *tile_height_16;
	GtkWidget *tile_height_32;
	GtkWidget *tile_height_64;
	GtkWidget *no_tiled_file;
	GtkWidget *tiled_file;
	GtkWidget *bmp_file;
	GtkWidget *pal_file;

	// selector dialog
	GtkWidget *tileset_export;
	GtkWidget *bitmap_export;
} VeraSaveGui;

GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,
	NULL,
	query,
	run
};

static const VeraSaveVals defaults =
{
	TILESET,
	TILE_4BPP,
	TILE_WIDTH_8,
	TILE_HEIGHT_8,
	TRUE,
	TRUE,
	TRUE
};

static VeraSaveVals veravals;
static gboolean save_tiles_dialog(gint32 image_id);
static gboolean save_bitmap_dialog(gint32 image_id);
static gboolean save_selector_dialog(gint32 image_id);
static void save_dialog_response(GtkWidget *widget,
		gint response_id,
		gpointer data);
static void load_defaults(void);
static void save_defaults(void);
static void load_gui_defaults(VeraSaveGui *vg);

MAIN()

static void query (void)
{
	static const GimpParamDef save_args[] =
	{
		{ GIMP_PDB_INT32,    "run-mode",	"The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
		{ GIMP_PDB_IMAGE,    "image",		"Input image" },
		{ GIMP_PDB_DRAWABLE, "drawable",	"Drawable to export" },
		{ GIMP_PDB_STRING,   "filename",	"The name of the file to export the image to" },
		{ GIMP_PDB_STRING,   "raw-filename",	"The name of the file to export the image to" },
		{ GIMP_PDB_INT32,   "export-type",	"0 - Tileset, 1 - Bitmap" },
		{ GIMP_PDB_INT32,   "tile-bpp",		"Bits per pixel" },
		{ GIMP_PDB_INT32,   "tile-width",	"Tile width" },
		{ GIMP_PDB_INT32,   "tile-height",	"Tile height" },
		{ GIMP_PDB_INT32,   "Tiled-file",	"Create a Tiled tile set file" },
		{ GIMP_PDB_INT32,   "BMP-file",		"Create a BMP output file" },
		{ GIMP_PDB_INT32,   "PAL-file",		"Create a PAL palette file" }
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

		switch(run_mode) 
		{
			case GIMP_RUN_INTERACTIVE:
				/*
				 * Possibly retrieve data...
				 */
				gimp_get_data (SAVE_PROC, &veravals);

				/*
				 * Then acquire information with a dialog...
				 */
				if (!save_selector_dialog (image_id))
				{
					values[0].data.d_status = GIMP_PDB_CANCEL;
					return;
				}
				break;
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

				switch(veravals.export_type)
				{
					case TILESET:
						if (!save_tiles_dialog (image_id))
							status = GIMP_PDB_CANCEL;
						break;
					case BITMAP:
						if (!save_bitmap_dialog (image_id))
							status = GIMP_PDB_CANCEL;
						break;
				}

				break;

			case GIMP_RUN_NONINTERACTIVE:
				/*
				 * Make sure all the arguments are there!
				 */
				if (nparams != 10)
				{
					if (nparams != 12)
					{
						status = GIMP_PDB_CALLING_ERROR;
					}
					else
					{
						veravals.export_type = param[5].data.d_int32;
						veravals.tile_bpp    = param[6].data.d_int32;
						veravals.tile_width  = param[7].data.d_int32;
						veravals.tile_height = param[8].data.d_int32;
						veravals.tiled_file  = param[9].data.d_int32;
						veravals.bmp_file    = param[10].data.d_int32;
						veravals.pal_file    = param[11].data.d_int32;
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
			gint palsize;
			guchar *cmap = gimp_image_get_colormap (image_id, &palsize);

			if (cmap && veravals.pal_file)
			{
				if(!save_palette(filename, cmap, palsize, &error))
				{
					status = GIMP_PDB_EXECUTION_ERROR;
				}
			}

			// test to modify the colormap

			gchar *bmp_filename = g_strconcat (filename, ".bmp", NULL);
			if (veravals.bmp_file && veravals.tile_bpp != TILE_4BPP)
			{
				// write out a bitmap to be used with the .tsx file
				gimp_file_save(run_mode, image_id, drawable_id, bmp_filename, bmp_filename);
			}

			if(veravals.tiled_file)
			{
				if(veravals.tile_bpp == TILE_4BPP)
				{
					guchar* shifted_map = (guchar*)malloc(sizeof(guchar) * palsize);
					gchar * numbered_filename;

					for (int i = 0; i < 16; i++)
					{

						shift_color_map(cmap, &shifted_map, palsize, i*16);
						gimp_image_set_colormap(image_id, shifted_map, palsize);
						sprintf(numbered_filename, "%s.%d", filename, i);
						bmp_filename = g_strconcat (numbered_filename, ".bmp", NULL);

						if (veravals.bmp_file)
						{
							// write out a bitmap to be used with the .tsx file
							gimp_file_save(run_mode, image_id, drawable_id, bmp_filename, bmp_filename);
						}

						if(!save_tsx(numbered_filename, bmp_filename,  GIMP_RUN_NONINTERACTIVE, image_id, drawable_id, &error))
						{
							status = GIMP_PDB_EXECUTION_ERROR;
						}
					}

					gimp_image_set_colormap(image_id, cmap, palsize);
					free(shifted_map);
				}

				if(!save_tsx(filename, bmp_filename,  GIMP_RUN_NONINTERACTIVE, image_id, drawable_id, &error))
				{
					status = GIMP_PDB_EXECUTION_ERROR;
				}
			}

			switch(veravals.export_type)
			{
				case TILESET:
					if (save_tile_set (filename, image_id, drawable_id, &error))
					{
						gimp_set_data (SAVE_PROC, &veravals, sizeof (veravals));
					}
					else
					{
						status = GIMP_PDB_EXECUTION_ERROR;
					}
					break;
				case BITMAP:
					if (save_bitmap (filename, image_id, drawable_id, &error))
					{
						gimp_set_data (SAVE_PROC, &veravals, sizeof (veravals));
					}
					else
					{
						status = GIMP_PDB_EXECUTION_ERROR;
					}
					break;
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

static void shift_color_map(guchar* orig,
		guchar** shifted,
		gint palsize,
		gint offset)
{
	gint offset_base = offset * 3;
	for(int i = 0; i < palsize*3; i+=3)
	{
		if(i + offset_base >= palsize * 3)
		{
			// this is the last <offset> values in the color map
			(*shifted)[i] = orig[(i - offset_base) % palsize];
			(*shifted)[i+1] = orig[(i+1 - offset_base) % palsize];
			(*shifted)[i+2] = orig[(i+2 - offset_base) % palsize];
			continue;
		}

		(*shifted)[i] = orig[i+offset_base];
		(*shifted)[i+1] = orig[i+1+offset_base];
		(*shifted)[i+2] = orig[i+2+offset_base];
	}
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
			for(int ty = 0; ty < tile_height; ty++)
			{
				for(int tx = 0; tx < tile_width; tx++)
				{
					// get the color from the buffer
					int buf_index = ((yoff + ty) * width) + xoff + tx;
					guchar color = buf[buf_index];

					switch(veravals.tile_bpp)
					{
						case TILE_1BPP:
							switch(buf_index % 8)
							{
								case 0:
									tile_buf[tile_buf_index] = color << 7;
									break;
								case 1:
									tile_buf[tile_buf_index] |= color << 6;
									break;
								case 2:
									tile_buf[tile_buf_index] |= color << 5;
									break;
								case 3:
									tile_buf[tile_buf_index] |= color << 4;
									break;
								case 4:
									tile_buf[tile_buf_index] |= color << 3;
									break;
								case 5:
									tile_buf[tile_buf_index] |= color << 2;
									break;
								case 6:
									tile_buf[tile_buf_index] |= color << 1;
									break;
								case 7:
									tile_buf[tile_buf_index] |= color;
									tile_buf_index++;
									break;
							}
							break;

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

	return ret;
}

static gboolean save_bitmap (const gchar  *filename,
		gint32        image_id,
		gint32        drawable_id,
		GError      **error)
{
	GeglBuffer       *buffer;
	const Babl       *format = NULL;
	guchar           *cmap   = NULL;  /* colormap for indexed images */
	guchar           *buf;
	guchar           *bitmap_buf;
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

	int bitmap_buf_length = ((width * height * bpp) / (8 / veravals.tile_bpp)) + 2;
	bitmap_buf = g_new (guchar, bitmap_buf_length);

	int bitmap_buf_index = 2;

	// 2 byte header
	bitmap_buf[0] = 0;
	bitmap_buf[1] = 0;

	// TODO: populate bitmap_buf
	// write the whole file
	for(int y = 0; y < height; y++)
	{
		for(int x = 0; x < width; x++)
		{
			// get the color from the buffer
			int buf_index = (width * y) + x;
			guchar color = buf[buf_index];

			switch(veravals.tile_bpp)
			{
				case TILE_1BPP:
					switch(buf_index % 8)
					{
						case 0:
							bitmap_buf[bitmap_buf_index] = color << 7;
							break;
						case 1:
							bitmap_buf[bitmap_buf_index] |= color << 6;
							break;
						case 2:
							bitmap_buf[bitmap_buf_index] |= color << 5;
							break;
						case 3:
							bitmap_buf[bitmap_buf_index] |= color << 4;
							break;
						case 4:
							bitmap_buf[bitmap_buf_index] |= color << 3;
							break;
						case 5:
							bitmap_buf[bitmap_buf_index] |= color << 2;
							break;
						case 6:
							bitmap_buf[bitmap_buf_index] |= color << 1;
							break;
						case 7:
							bitmap_buf[bitmap_buf_index] |= color;
							bitmap_buf_index++;
							break;
					}
					break;

				case TILE_2BPP:
					switch(buf_index % 4)
					{
						case 0:
							bitmap_buf[bitmap_buf_index] = color << 6;
							break;
						case 1:
							bitmap_buf[bitmap_buf_index] |= color << 4;
							break;
						case 2:
							bitmap_buf[bitmap_buf_index] |= color << 2;
							break;
						case 3:
							bitmap_buf[bitmap_buf_index] |= color;
							bitmap_buf_index++;
							break;
					}
					break;
				case TILE_4BPP:
					if (buf_index % 2)
					{
						// odd byte
						bitmap_buf[bitmap_buf_index] |= color;
						bitmap_buf_index++;
					}
					else
					{
						// even byte
						bitmap_buf[bitmap_buf_index] = color << 4;
					}
					break;
				case TILE_8BPP:
					bitmap_buf[bitmap_buf_index] = color;
					bitmap_buf_index++;
					break;
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

	if (! fwrite (bitmap_buf, bitmap_buf_length, 1, fp))
	{
		return FALSE;
	}

	fclose (fp);
	g_free(bitmap_buf);

	return ret;
}

static gboolean save_palette(const gchar *filename,
		const guchar      *cmap,
		const gint        palsize,
		GError            **error)
{
	FILE       *fp = NULL;
	gboolean   ret = FALSE;
	guchar     *pal_buf;
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

	return TRUE;
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
	VeraSaveGui  vg;
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
	vg.tile_1bpp = radio_button_init (builder, "tile-bpp-1",
			TILE_1BPP,
			veravals.tile_bpp,
			&veravals.tile_bpp);
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
	vg.tile_width_32 = radio_button_init (builder, "tile-width-32",
			TILE_WIDTH_32,
			veravals.tile_width,
			&veravals.tile_width);
	vg.tile_width_64 = radio_button_init (builder, "tile-width-64",
			TILE_WIDTH_64,
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
	vg.tile_height_32 = radio_button_init (builder, "tile-height-32",
			TILE_HEIGHT_32,
			veravals.tile_height,
			&veravals.tile_height);
	vg.tile_height_64 = radio_button_init (builder, "tile-height-64",
			TILE_HEIGHT_64,
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

static gboolean save_bitmap_dialog (gint32 image_id)
{
	VeraSaveGui  vg;
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
			"ui/plug-ins/plug-in-file-vera-bitmap.ui",
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
	vg.tile_1bpp = radio_button_init (builder, "tile-bpp-1",
			TILE_1BPP,
			veravals.tile_bpp,
			&veravals.tile_bpp);
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

static gboolean save_selector_dialog (gint32 image_id)
{
	VeraSaveGui  vg;
	GtkWidget  *dialog;
	GtkBuilder *builder;
	gchar      *ui_file;
	GError     *error = NULL;

	gimp_ui_init (PLUG_IN_BINARY, TRUE);

	/* Dialog init */
	dialog = gimp_export_dialog_new ("VERA Image Data", PLUG_IN_BINARY, SAVE_PROC);
	g_signal_connect (dialog, "response",
			G_CALLBACK (save_dialog_response),
			&vg);
	g_signal_connect (dialog, "destroy",
			G_CALLBACK (gtk_main_quit),
			NULL);

	/* GtkBuilder init */
	builder = gtk_builder_new ();
	ui_file = g_build_filename (gimp_data_directory (),
			"ui/plug-ins/plug-in-file-vera-selector.ui",
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
	vg.tileset_export = radio_button_init (builder, "vera-tileset",
			TILESET,
			veravals.export_type,
			&veravals.export_type);
	vg.bitmap_export = radio_button_init (builder, "vera-bitmap",
			BITMAP,
			veravals.export_type,
			&veravals.export_type);

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
	VeraSaveGui *vg = data;

	switch (response_id)
	{
		case GTK_RESPONSE_OK:
			vg->run = TRUE;

		default:
			gtk_widget_destroy (widget);
			break;
	}
}

static void load_gui_defaults (VeraSaveGui *vg)
{
	load_defaults ();

#define SET_ACTIVE(field, datafield) \
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (vg->field), "gimp-item-data")) == veravals.datafield) \
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vg->field), TRUE)

	// tile dialog
	SET_ACTIVE (tile_1bpp, tile_bpp);
	SET_ACTIVE (tile_2bpp, tile_bpp);
	SET_ACTIVE (tile_4bpp, tile_bpp);
	SET_ACTIVE (tile_8bpp, tile_bpp);

	SET_ACTIVE (tile_width_8, tile_width);
	SET_ACTIVE (tile_width_16, tile_width);
	SET_ACTIVE (tile_width_32, tile_width);
	SET_ACTIVE (tile_width_64, tile_width);

	SET_ACTIVE (tile_height_8, tile_height);
	SET_ACTIVE (tile_height_16, tile_height);
	SET_ACTIVE (tile_height_32, tile_height);
	SET_ACTIVE (tile_height_64, tile_height);

	SET_ACTIVE (tiled_file, tiled_file);
	SET_ACTIVE (bmp_file, bmp_file);
	SET_ACTIVE (pal_file, pal_file);

	// selector dialog
	SET_ACTIVE (tileset_export, export_type);
	SET_ACTIVE (bitmap_export, export_type);

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
		VeraSaveVals   tmpvals = defaults;
		gint          num_fields;

		def_str = g_strndup (gimp_parasite_data (parasite),
				gimp_parasite_data_size (parasite));

		gimp_parasite_free (parasite);

		num_fields = sscanf (def_str, "%d %d %d %d %d %d %d",
				(int *) &tmpvals.export_type,
				(int *) &tmpvals.tile_bpp,
				(int *) &tmpvals.tile_width,
				(int *) &tmpvals.tile_height,
				(int *) &tmpvals.tiled_file,
				(int *) &tmpvals.bmp_file,
				(int *) &tmpvals.pal_file);

		g_free (def_str);

		if (num_fields == 7)
			veravals = tmpvals;
	}
}

static void save_defaults (void)
{
	GimpParasite *parasite;
	gchar        *def_str;

	def_str = g_strdup_printf ("%d %d %d %d %d %d %d",
			veravals.export_type,
			veravals.tile_bpp,
			veravals.tile_width,
			veravals.tile_height,
			veravals.tiled_file,
			veravals.bmp_file,
			veravals.pal_file);

	parasite = gimp_parasite_new (VERA_DEFAULTS_PARASITE,
			GIMP_PARASITE_PERSISTENT,
			strlen (def_str), def_str);

	gimp_attach_parasite (parasite);

	gimp_parasite_free (parasite);
	g_free (def_str);
}
