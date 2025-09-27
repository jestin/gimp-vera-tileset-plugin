#include "vera-lib.h"

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
		gboolean	tiled_file,
		gboolean	bmp_file,
		gboolean	pal_file,
		GError      **error)
{
	guchar		*tile_buf;
	gint32		t_width = image_width / tile_width;
	gint32		t_height = image_height / tile_height;
	gint32		tile_buf_length = ((image_width * image_height * image_bpp) / (8 / tile_bpp));
	gint32		tile_buf_index = 0;
	FILE		*fp = NULL;
	gboolean	ret = FALSE;

	if(header)
	{
		tile_buf_length += 2;
		tile_buf_index += 2;
	}

	tile_buf = g_new (guchar, tile_buf_length);

	if(header)
	{
		// 2 byte header
		tile_buf[0] = 0;
		tile_buf[1] = 0;
	}

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
					int buf_index = ((yoff + ty) * image_width) + xoff + tx;
					guchar color = image_buffer[buf_index];

					switch(tile_bpp)
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
							switch(buf_index % 2)
							{
								case 0:
									tile_buf[tile_buf_index] = color << 4;
									break;
								case 1:
									tile_buf[tile_buf_index] |= color;
									tile_buf_index++;
									break;
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

gboolean save_bitmap(
		const gchar *filename,
		guchar		*image_buffer,
		guint32		image_bpp,
		gint		image_width,
		gint		image_height,
		TileBpp		bitmap_bpp,
		gboolean	header,
		gboolean	bmp_file,
		gboolean	pal_file,
		GError      **error)
{
	guchar		*bitmap_buf;
	gint32		bitmap_buf_length = ((image_width * image_height * image_bpp) / (8 / bitmap_bpp));
	gint32		bitmap_buf_index = 0;
	FILE		*fp = NULL;
	gboolean	ret = FALSE;

	if(header)
	{
		bitmap_buf_length += 2;
		bitmap_buf_index += 2;
	}

	bitmap_buf = g_new (guchar, bitmap_buf_length);

	if(header)
	{
		// 2 byte header
		bitmap_buf[0] = 0;
		bitmap_buf[1] = 0;
	}

	// write the whole file
	for(int y = 0; y < image_height; y++)
	{
		for(int x = 0; x < image_width; x++)
		{
			// get the color from the buffer
			int buf_index = (image_width * y) + x;
			guchar color = image_buffer[buf_index];

			switch(bitmap_bpp)
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
