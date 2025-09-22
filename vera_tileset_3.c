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

static gint             export_image         (GFile                 *file,
                                              GimpImage             *image,
                                              GimpDrawable          *drawable,
                                              GObject               *config,
                                              GError               **error);

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
                                      "This plug-in exports binary files for VERA chips.",
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

    gimp_procedure_add_choice_argument (procedure, "exporttype",
                                        "Export _type",
                                        "Export type",
                                        gimp_choice_new_with_values ("tileset", TILESET, "VERA Tileset", NULL,
																	 "bitmap",  BITMAP,  "VERA Bitmap",  NULL,
																	 NULL),
										"tileset",
                                        G_PARAM_READWRITE);

    gimp_procedure_add_choice_argument (procedure, "bpp",
                                        "Bits Per Pixel",
                                        "Bits Per Pixel",
                                        gimp_choice_new_with_values ("1bpp", TILE_1BPP, "1 Bit Per Pixel", NULL,
																	 "2bpp",  TILE_2BPP,  "2 Bits Per Pixel",  NULL,
																	 "4bpp",  TILE_4BPP,  "4 Bits Per Pixel",  NULL,
																	 "8bpp",  TILE_8BPP,  "8 Bits Per Pixel",  NULL,
																	 NULL),
										"4bpp",
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
	GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
	GimpExportReturn   export = GIMP_EXPORT_IGNORE;
	GList             *drawables;
	GError            *error  = NULL;

	gegl_init (NULL, NULL);

	if (run_mode == GIMP_RUN_INTERACTIVE)
	{
		gimp_ui_init (PLUG_IN_BINARY);

		if (! save_dialog (procedure, G_OBJECT (config), image))
			status = GIMP_PDB_CANCEL;
	}

	export = gimp_export_options_get_image (options, &image);
	drawables = gimp_image_list_layers (image);

	if (status == GIMP_PDB_SUCCESS)
	{
		if (! export_image (file, image, drawables->data,
					G_OBJECT (config), &error))
		{
			status = GIMP_PDB_EXECUTION_ERROR;
		}
	}

	if (export == GIMP_EXPORT_EXPORT)
		gimp_image_delete (image);

	g_list_free (drawables);
	return gimp_procedure_new_return_values (procedure, status, error);
}


static gint
export_image (GFile        *file,
              GimpImage    *image,
              GimpDrawable *drawable,
              GObject      *config,
              GError      **error)
{
  gint         type;
  gint         bpp;
  gint         width;       /* Drawable width */
  gint         height;      /* Drawable height */
  GeglBuffer  *buffer;      /* Buffer for layer */
  const Babl  *format;

  type = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "exporttype");
  bpp = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config), "bpp");

  /*
   * Get the drawable for the current image...
   */

  width  = gimp_drawable_get_width  (drawable);
  height = gimp_drawable_get_height (drawable);

  buffer = gimp_drawable_get_buffer (drawable);

  switch (gimp_drawable_type (drawable))
    {
    case GIMP_INDEXED_IMAGE:
      format = babl_format ("R'G'B' u8");
      break;

    case GIMP_INDEXEDA_IMAGE:
      format = babl_format ("R'G'B'A u8");
      break;

    default:
      return FALSE;
    }

  /*
   * Open the file for writing...
   */

  gimp_progress_init_printf ("Exporting '%s'",
                             gimp_file_get_utf8_name (file));

  gimp_message("Exporting to VERA format");


  g_object_unref (buffer);

  gimp_progress_update (1.0);

  return TRUE;
}

static gboolean
save_dialog (GimpProcedure *procedure,
             GObject       *config,
             GimpImage     *image)
{
  GtkWidget *dialog;
  GtkWidget *exporttype_vbox;
  GtkWidget *bpp_vbox;
  gboolean   run;

  dialog = gimp_export_procedure_dialog_new (GIMP_EXPORT_PROCEDURE (procedure),
                                             GIMP_PROCEDURE_CONFIG (config),
                                             image);

  exporttype_vbox = gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                         "exporttype-vbox", "exporttype", NULL);
  gtk_container_set_border_width (GTK_CONTAINER (exporttype_vbox), 12);

  bpp_vbox = gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                         "bpp-vbox", "bpp", NULL);
  gtk_container_set_border_width (GTK_CONTAINER (bpp_vbox), 12);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dialog),
                              "exporttype-vbox",
                              "bpp-vbox",
							  NULL);

  gtk_widget_show (dialog);

  run = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return run;
}

