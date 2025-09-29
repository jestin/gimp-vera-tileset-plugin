#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "vera-lib.h"

#define EXPORT_PROC	"file-vera-export"
#define PLUG_IN_BINARY   "file-vera"
#define PLUG_IN_VERSION  "1.0.0 - 21 September 2025"

typedef struct _Vera      Vera;
typedef struct _VeraClass VeraClass;

struct _Vera
{
  GimpPlugIn      parent_instance;
};

struct _VeraClass
{
  GimpPlugInClass parent_class;
};

#define VERA_TYPE  (vera_get_type ())
#define VERA(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), VERA_TYPE, Vera))

GType                   vera_get_type         (void) G_GNUC_CONST;

static GList          * vera_query_procedures (GimpPlugIn            *plug_in);
static GimpProcedure  * vera_create_procedure (GimpPlugIn            *plug_in,
                                              const gchar           *name);

static GimpValueArray * vera_export           (GimpProcedure         *procedure,
                                              GimpRunMode            run_mode,
                                              GimpImage             *image,
                                              GFile                 *file,
                                              GimpExportOptions     *options,
                                              GimpMetadata          *metadata,
                                              GimpProcedureConfig   *config,
                                              gpointer               run_data);

static gint             export_image         (const gchar           *filename,
                                              GimpImage             *image,
                                              GimpDrawable          *drawable,
											  const Babl            *format,
			  								  gboolean				report_prgress,
                                              GObject               *config,
                                              GError              	**error);

static gboolean         save_dialog          (GimpProcedure         *procedure,
                                              GObject               *config,
                                              GimpImage             *image);


G_DEFINE_TYPE (Vera, vera, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (VERA_TYPE)

static void
vera_class_init (VeraClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = vera_query_procedures;
  plug_in_class->create_procedure = vera_create_procedure;
}

static void
vera_init (Vera *vera)
{
}

static GList *
vera_query_procedures (GimpPlugIn *plug_in)
{
  GList *list = NULL;

  list = g_list_append (list, g_strdup (EXPORT_PROC));

  return list;
}

static GimpProcedure *
vera_create_procedure (GimpPlugIn  *plug_in,
                      const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (! strcmp (name, EXPORT_PROC))
  {
    procedure = gimp_export_procedure_new (plug_in, name,
                                           GIMP_PDB_PROC_TYPE_PLUGIN,
                                           FALSE, vera_export, NULL, NULL);

    gimp_procedure_set_image_types (procedure, "*");

    gimp_procedure_set_menu_label (procedure,
                                   "Versitle Embedded Retro Adapter");
    gimp_file_procedure_set_format_name (GIMP_FILE_PROCEDURE (procedure),
                                         "VERA Compatible");
    gimp_procedure_set_icon_name (procedure, GIMP_ICON_BRUSH);

    gimp_procedure_set_documentation (procedure,
                                      "Exports files in VERA compatible binaries",
                                      "This procedure exports binary files for VERA chips.",
                                      name);
    gimp_procedure_set_attribution (procedure,
                                    "Jestin Stoffel <jestin.stoffel@gmail.com>",
                                    "Copywrite 2022 by Jestin Stoffel",
                                    PLUG_IN_VERSION);

    gimp_file_procedure_set_extensions (GIMP_FILE_PROCEDURE (procedure),
                                        "BIN");

    gimp_export_procedure_set_capabilities (GIMP_EXPORT_PROCEDURE (procedure),
                                            GIMP_EXPORT_CAN_HANDLE_INDEXED,
                                            NULL, NULL, NULL);

    gimp_procedure_add_choice_argument (procedure, "export-type",
                                        "_Export _type",
                                        "Export type",
                                        gimp_choice_new_with_values ("tileset", TILESET, "VERA Tileset", NULL,
																	 "bitmap",  BITMAP,  "VERA Bitmap",  NULL,
																	 NULL),
										"tileset",
                                        G_PARAM_READWRITE);

    gimp_procedure_add_choice_argument (procedure, "tile-bpp",
                                        "_Bits Per Pixel",
                                        "Bits Per Pixel",
                                        gimp_choice_new_with_values ("1bpp", TILE_1BPP, "1 Bit Per Pixel", NULL,
																	 "2bpp",  TILE_2BPP,  "2 Bits Per Pixel",  NULL,
																	 "4bpp",  TILE_4BPP,  "4 Bits Per Pixel",  NULL,
																	 "8bpp",  TILE_8BPP,  "8 Bits Per Pixel",  NULL,
																	 NULL),
										"4bpp",
                                        G_PARAM_READWRITE);

    gimp_procedure_add_choice_argument (procedure, "tile-width",
                                        "Tile _Width",
                                        "Tile Width",
                                        gimp_choice_new_with_values ("8", TILE_WIDTH_8, "8", NULL,
																	 "16",  TILE_WIDTH_16,  "16",  NULL,
																	 "32",  TILE_WIDTH_32,  "32",  NULL,
																	 "64",  TILE_WIDTH_64,  "64",  NULL,
																	 NULL),
										"8",
                                        G_PARAM_READWRITE);

    gimp_procedure_add_choice_argument (procedure, "tile-height",
                                        "Tile _Height",
                                        "Tile Height",
                                        gimp_choice_new_with_values ("8", TILE_HEIGHT_8, "8", NULL,
																	 "16",  TILE_HEIGHT_16,  "16",  NULL,
																	 "32",  TILE_HEIGHT_32,  "32",  NULL,
																	 "64",  TILE_HEIGHT_64,  "64",  NULL,
																	 NULL),
										"8",
                                        G_PARAM_READWRITE);

      gimp_procedure_add_boolean_argument (procedure, "header",
                                           "2-byte H_eader",
                                           "Add 2-byte Header",
                                           TRUE,
                                           G_PARAM_READWRITE);

      gimp_procedure_add_boolean_argument (procedure, "pal-file",
                                           "Export VERA _Palette File",
                                           "Export VERA Palette File",
                                           TRUE,
                                           G_PARAM_READWRITE);

      gimp_procedure_add_boolean_argument (procedure, "tiled-file",
                                           "Export _Tiled Tileset File",
                                           "Export Tiled Tileset File",
                                           TRUE,
                                           G_PARAM_READWRITE);

      gimp_procedure_add_boolean_argument (procedure, "bmp-file",
                                           "Export Bitmap _File",
										   "Export Bimap File",
                                           TRUE,
                                           G_PARAM_READWRITE);
  }

  return procedure;
}


static GimpValueArray *
vera_export (GimpProcedure        *procedure,
            GimpRunMode           run_mode,
            GimpImage            *image,
            GFile                *file,
            GimpExportOptions    *options,
            GimpMetadata         *metadata,
            GimpProcedureConfig  *config,
			gpointer              run_data)
{
	GimpPDBStatusType	status = GIMP_PDB_SUCCESS;
	GimpExportReturn	export = GIMP_EXPORT_IGNORE;
	GList				*drawables;
	gboolean			pal_file;
	gboolean			tiled_file;
	gboolean			bmp_file;
	gboolean			header;
	Babl				*format;
	const gchar*		filename;
	GError				*error  = NULL;

	gegl_init (NULL, NULL);

	export = gimp_export_options_get_image (options, &image);
	drawables = gimp_image_list_layers (image);
	filename = gimp_file_get_utf8_name (file);

	switch (gimp_drawable_type (drawables->data))
	{
		case GIMP_INDEXED_IMAGE:
		case GIMP_INDEXEDA_IMAGE:
			format = gimp_drawable_get_format(drawables->data);
			break;
		default:
			status = GIMP_PDB_EXECUTION_ERROR;
	}

	if (run_mode == GIMP_RUN_INTERACTIVE)
	{
		gimp_ui_init (PLUG_IN_BINARY);

		if (! save_dialog (procedure, G_OBJECT (config), image))
			status = GIMP_PDB_CANCEL;
	}

	/* get user options */	
	g_object_get(config,
			"pal-file", &pal_file,
			"tiled-file", &tiled_file,
			"bmp-file", &bmp_file,
			"header", &header,
			NULL);


	if (status == GIMP_PDB_SUCCESS)
	{
		if (! export_image (filename, image, drawables->data, format,
					run_mode != GIMP_RUN_NONINTERACTIVE,
					G_OBJECT (config), &error))
		{
			status = GIMP_PDB_EXECUTION_ERROR;
		}
	}

	/* write palette file */
	if (status == GIMP_PDB_SUCCESS && pal_file)
	{
		gint			pal_size;
		gsize			pal_bytes;
		GimpPalette		*palette; 
		guchar			*cmap;

		palette = gimp_image_get_palette(image); 

		cmap = gimp_palette_get_colormap(palette, format, &pal_size, &pal_bytes);

		if (! save_palette (filename, cmap, pal_size, header, &error))
		{
			status = GIMP_PDB_EXECUTION_ERROR;
		}

		// free colormap
		g_free(cmap);
	}

	/* write bmp file */
	if (status == GIMP_PDB_SUCCESS && bmp_file)
	{
	}

	/* write Tiled tileset file */
	if (status == GIMP_PDB_SUCCESS && tiled_file)
	{
	}

	if (export == GIMP_EXPORT_EXPORT)
		gimp_image_delete (image);

	g_list_free (drawables);
	return gimp_procedure_new_return_values (procedure, status, error);
}


static gint
export_image (const gchar	*filename,
              GimpImage		*image,
              GimpDrawable	*drawable,
			  const Babl	*format,
			  gboolean		report_prgress,
              GObject		*config,
              GError		**error)
{
  gint			image_bpp;
  VeraExport	export_type;
  TileBpp		tile_bpp;
  TileWidth		tile_width;
  TileHeight	tile_height;
  gboolean		header;
  gint			width;
  gint			height;
  GeglBuffer	*buffer;
  gboolean		ret;

  export_type = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "export-type");
  tile_bpp = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "tile-bpp");
  tile_width = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "tile-width");
  tile_height = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "tile-height");

  g_object_get(config,
		  "header", &header,
		  NULL);

  guchar           *image_buf;
  /*
   * Get the drawable for the current image...
   */

  width  = gimp_drawable_get_width  (drawable);
  height = gimp_drawable_get_height (drawable);

  buffer = gimp_drawable_get_buffer (drawable);

  /*
   * Open the file for writing...
   */

  image_bpp          = babl_format_get_bytes_per_pixel (format);

  width  = gegl_buffer_get_width  (buffer);
  height = gegl_buffer_get_height (buffer);

  image_buf = g_new (guchar, width * height * image_bpp);

  gegl_buffer_get (buffer, GEGL_RECTANGLE (0, 0, width, height), 1.0,
		  format, image_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

  g_object_unref (buffer);

  if (report_prgress)
  {
	  gimp_progress_init_printf ("Exporting '%s'", filename);
  }

  ret = TRUE;

 switch(export_type)
 {
	 case TILESET:
		ret = save_tile_set(
				filename,
				image_buf,
				image_bpp,
				width,
				height,
				tile_bpp,
				tile_width,
				tile_height,
				header,
				error);
		 break;

	 case BITMAP:
		ret = save_bitmap(
				filename,
				image_buf,
				image_bpp,
				width,
				height,
				tile_bpp,
				header,
				error);
		break;
 } 

 if (report_prgress)
 {
	 gimp_progress_update (1.0);
 }

  return ret;
}

static gboolean
save_dialog (GimpProcedure *procedure,
             GObject       *config,
             GimpImage     *image)
{
  GtkWidget *dialog;
  GtkWidget *standard_options_vbox;
  GtkWidget *tile_vbox;
  gboolean   run;

  dialog = gimp_export_procedure_dialog_new (GIMP_EXPORT_PROCEDURE (procedure),
                                             GIMP_PROCEDURE_CONFIG (config),
                                             image);

  standard_options_vbox = gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                         "standard_options_vbox",
										 "export-type",
										 "tile-bpp",
										 "header",
										 "pal-file",
										 "bmp-file",
										 NULL);
  gtk_container_set_border_width (GTK_CONTAINER (standard_options_vbox), 12);

  tile_vbox = gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                         "tile_vbox",
										 "tile-width",
										 "tile-height",
										 "tiled-file",
										 NULL);
  gtk_container_set_border_width (GTK_CONTAINER (tile_vbox), 12);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dialog),
                              "standard_options_vbox",
                              "tile_vbox",
							  NULL);

  gtk_widget_show (dialog);

  run = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return run;
}

