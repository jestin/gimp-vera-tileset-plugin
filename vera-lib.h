#ifndef __VERA_LIB_H__
#define __VERA_LIB_H__

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

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
	gboolean       file_header;
	VeraExport     export_type;	 /* tileset or bitmap */
	TileBpp        tile_bpp;     /* Bits per pixel format for tiles */
	TileWidth      tile_width;
	TileHeight     tile_height;
	gboolean       tiled_file;
	gboolean       bmp_file;
	gboolean       pal_file;
} VeraSaveVals;

gboolean save_tile_set(
		const gchar *filename,
		guchar		*image_buffer,
		guint32		image_bpp,
		gint		image_width,
		gint		image_height,
		TileBpp		tile_bpp,
		TileWidth	tile_width,
		TileHeight	tile_height,
		gboolean	header,
		GError      **error);

gboolean save_bitmap(
		const gchar *filename,
		guchar		*image_buffer,
		guint32		image_bpp,
		gint		image_width,
		gint		image_height,
		TileBpp		bitmap_bpp,
		gboolean	header,
		GError      **error);

gboolean save_palette(
		const gchar		*filename,
		const guchar	*cmap,
		const gint		palsize,
		const gboolean  fileheader,
		GError			**error);

gboolean save_all_tsx(const gchar *filename,
        GimpImage	*image,
		guchar		*image_buffer,
		gint		image_width,
		gint		image_height,
		Babl		*format,
		TileBpp		tile_bpp,
		TileWidth	tile_width,
		TileHeight	tile_height,
		GError		**error);

gboolean save_tsx(const gchar *filename,
		const gchar *bmp_filename,
		guchar		*image_buffer,
		gint		image_width,
		gint		image_height,
		TileWidth	tile_width,
		TileHeight	tile_height,
		GError		**error);

static void shift_color_map(guchar* orig,
		guchar** shifted,
		gint palsize,
		gint offset);

#endif /* __VERA_LIB_H__ */
