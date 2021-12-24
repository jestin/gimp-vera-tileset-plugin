#include <libgimp/gimp.h>

#define SAVE_PROC	"file-vera-save"

static void query (void);
static void run   (const gchar      *name,
		gint              nparams,
		const GimpParam  *param,
		gint             *nreturn_vals,
		GimpParam       **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
	NULL,
	NULL,
	query,
	run
};

MAIN()

static void query (void)
{
	static const GimpParamDef save_args[] =
	{
		{ GIMP_PDB_INT32,    "run-mode",     "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
		{ GIMP_PDB_IMAGE,    "image",        "Input image" },
		{ GIMP_PDB_DRAWABLE, "drawable",     "Drawable to export" },
		{ GIMP_PDB_STRING,   "filename",     "The name of the file to export the image in" },
		{ GIMP_PDB_STRING,   "raw-filename", "The name of the file to export the image in" },
		{ GIMP_PDB_INT32,    "compression",  "Compression level (0 = none, 1 = RLE, 2 = ARLE)" }
	};

	gimp_install_procedure (SAVE_PROC,
			"Exports files in VERA compatible binaries",
			"This plug-in exports binary files for VERA chips.",
			"Jestin Stoffel <jestin.stoffel@gmail.com>",
			"Copyright 2021-2022 by Jestin Stoffel",
			"0.0.1 - 2021",
			"VERA binary",
			"INDEXED*",
			GIMP_PLUGIN,
			G_N_ELEMENTS (save_args),
			0,
			save_args,
			NULL);

	gimp_register_file_handler_mime (SAVE_PROC, "application/octet-stream");
	gimp_register_save_handler (SAVE_PROC, "bin", "");
}

static void run (
		const gchar *name,
		gint nparams,
		const GimpParam *param,
		gint *nreturn_vals,
		GimpParam **return_vals)
{
	static GimpParam  values[1];
	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	GimpRunMode       run_mode;

	/* Setting mandatory output values */
	*nreturn_vals = 1;
	*return_vals  = values;

	values[0].type = GIMP_PDB_STATUS;
	values[0].data.d_status = status;

	/* Getting run_mode - we won't display a dialog if 
	 * we are in NONINTERACTIVE mode */
	run_mode = param[0].data.d_int32;

	if (run_mode != GIMP_RUN_NONINTERACTIVE)
		g_message("Hello, world!\n");
}

