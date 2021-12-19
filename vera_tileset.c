#include <libgimp/gimp.h>

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
	static GimpParamDef args[] =
	{
		{
			GIMP_PDB_INT32,
			"run-mode",
			"Run mode"
		},
		{
			GIMP_PDB_IMAGE,
			"image",
			"Input image"
		},
		{
			GIMP_PDB_DRAWABLE,
			"drawable",
			"Input drawable"
		}
	};

	gimp_install_procedure (
			"plug-in-vera",
			"VERA Tile Output",
			"Displays \"Hello, world!\" in a dialog",
			"Jestin Stoffel",
			"Copyright Jestin Stoffel",
			"2021",
			"_VERA Tiles...",
			"INDEXED",
			GIMP_PLUGIN,
			G_N_ELEMENTS (args), 0,
			args, NULL);

	gimp_plugin_menu_register (
			"plug-in-vera",
			"<Image>/File/Export");
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

