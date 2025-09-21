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
