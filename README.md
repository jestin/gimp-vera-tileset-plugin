# gimp-vera-tileset-plugin

A gimp plugin for creating tilesets for the VERA graphics chip.

![gimp-vera-plugin](gimp-vera-plugin.png)

The [VERA graphics chip](https://github.com/fvdhoef/vera-module) that is being
used by the Commander X16 modern retro computer can render graphics based on
the contents of its onboard video RAM, but those contents need to be in a
specific format.  This plugin is able to produce binary files in that format,
which can then by loaded into the VERA's video RAM for display.  This includes
the ability to export palette binaries that can also be loaded into the VERA.
Additionally, this plugin is able to produce simple tileset files that are
compatible with the [Tiled](https://github.com/mapeditor/tiled) tile mapping
application.  This can then be used in conjunction with
[tmx2vera](https://github.com/jestin/tmx2vera), a Tiled to VERA converter
application, which allows for game engine style development for Commander X16
games and applications.

## Compilation and Installation

To install this plugin, you need to make sure your system has `gimptool-2.0`
installed.  This GIMP-provided command line tool is used by the plugin's
`Makefile` in order to specify includes and library dependencies, as well as
facilitate installation of the plugin.  If all the appropriate GIMP development
dependencies are installed, you should be able to compile and install with:

```
$ make
$ make install
$ sudo make install-ui
```

## Usage

The plugin defines both interactive and non-interactive run modes, so you can
use it through a user interface from within GIMP, or from the command line with
scripts.  In either case, the plugin requires that you set your image to
_Indexed_ mode (using a palette to reference your colors).

### Interactive Mode

The plugin defines the `*.BIN` file export, so you first need to export your
image with the `.BIN` extension.  This will run the plugin to perform the
export.

![export](Export.png)

Hit "Export" to continue, and you will be prompted for whether you wish to
export a tile set or a bitmap.

![export type](Export_Type.png)

If you choose to export a tile set, you will be promoted for some details about
the tiles, and also about additional exports such as VERA-compatible palette
file, Tiled tileset file, and a BMP file to be used with Tiled tilesets.
Choose the correct settings for your needs.

![tile settings](Tile_Settings.png)

If you choose to export a bitmap instead of tiles, you will be shown a
different dialog with more limited settings.

![bitmap settings](Bitmap_Settings.png)

Both of these types of exports will result in files that can be loaded directly
into the VERA's VRAM using the `SETLFS`, `SETNAM`, and `LOAD` routines of the
Commander X16 Kernal.

### Non-Interactive Mode

Alternatively, you may want to export your graphics resources by way of a build
system such as GNU `make`.  For those situations, it is best to script the
export of your resources from your build system.  For this to be done, you will
need to create a script to call the plugin and put it in your GIMP scripts
directory.  This script should work for most use cases:

```
(define (export-vera filename
					 outfile
					 export-type
					 tile-bpp
					 tile-width
					 tile-height
					 tiled-file
					 bmp-file
					 pal-file)
  (let* ((image (car (gimp-file-load RUN-NONINTERACTIVE filename filename)))
		(drawable (car (gimp-image-get-active-layer image))))
  (file-vera-save RUN-NONINTERACTIVE
				  image drawable outfile outfile export-type tile-bpp tile-width tile-height tiled-file bmp-file pal-file)
  (gimp-image-delete image)))
```

Name this file something like `vera_export.scm` and place it in the GIMP
scripts directory for your particular installation.  On Ubuntu, the correct
directory is `~/.config/GIMP/2.10/scripts/`, replacing `2.10` for your version
of GIMP.

With that in place, you can now run both GIMP and the plugin non-interactively
from a `Makefile` or build script:

```
MYTILES.BIN: MyTiles.xcf
	gimp -i -b '(export-vera "MyTiles.xcf" "MYTILES.BIN" 0 8 16 16 0 1 0)' -b '(gimp-quit 0)'
```

This exports the tiles in MyTiles.xcf as a VERA tile set, at 8 bits per pixel,
16 pixel width, 16 pixel height, without created a Tiled tileset file, with
creating a corresponding BMP, and without writing out a VERA-compatible palette
file.  Whenever `make` detects a change in `MyTiles.xcf`, it will re-run this
command.  This makes it such that you only need to save your GIMP project, and
`make` will determine when your resources need to be exported and when they
don't.

![gimp compile](gimp_compile.gif)
