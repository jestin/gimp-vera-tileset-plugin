#include "vera-lib.h"

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

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

gboolean save_palette(
		const gchar		*filename,
		const guchar	*cmap,
		const gint		palsize,
		const gboolean  fileheader,
		GError			**error)
{
	FILE       *fp = NULL;
	guchar     *pal_buf;
	int pal_buf_length = palsize * 2;
	int pal_buf_index = 0; // start past the 2 byte header

	if(fileheader)
	{
		pal_buf_length += 2;
		pal_buf_index += 2;
	}

	pal_buf = g_new (guchar, pal_buf_length); // 2 bytes per color, 2 byte header

	if(fileheader)
	{
		// 2 byte header
		pal_buf[0] = 0;
		pal_buf[1] = 0;
	}


	for(int i = 0; i < palsize*3; i+=3)
	{
		// read rgb values from colormap
		gint r = ((cmap[i] * 15) + 135) >> 8;
		gint g = ((cmap[i+1] * 15) + 135) >> 8;
		gint b = ((cmap[i+2] * 15) + 135) >> 8;

		// write out packed g and b values
		pal_buf[pal_buf_index] = (g & 0x0f) << 4 | b;

		// write out r value in lower nibble
		pal_buf[pal_buf_index+1] = r & 0x0f;

		pal_buf_index += 2;
	}

	/* we have colormap too, write it into filename+PAL.BIN */
	gchar *newfile = g_strconcat (filename, ".PAL", NULL);

	fp = fopen (newfile, "wb");

	if (! fp)
	{
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				"Could not open '%s' for writing: %s",
				gimp_filename_to_utf8 (newfile), g_strerror (errno));
		g_free(pal_buf);
		return FALSE;
	}

	if (!fwrite (pal_buf, pal_buf_length, 1, fp))
		return FALSE;

	fclose (fp);
	g_free(pal_buf);
	g_free(newfile);

	return TRUE;
}

gboolean save_all_tsx(const gchar *filename,
        GimpImage	*image,
		guchar		*image_buffer,
		gint		image_width,
		gint		image_height,
		Babl		*format,
		TileBpp		tile_bpp,
		TileWidth	tile_width,
		TileHeight	tile_height,
		gboolean	bmp_file,
		GError		**error)
{
	gboolean		ret;
	gint			pal_size;
	gsize			pal_bytes;
	GimpPalette		*palette; 
	guchar			*cmap;

	ret = TRUE;

	gchar *bmp_filename = g_strconcat (filename, ".bmp", NULL);

	palette = gimp_image_get_palette(image); 

	cmap = gimp_palette_get_colormap(palette, format, &pal_size, &pal_bytes);

	// generate images for all palettes when using 2bpp or 4bpp
	if(tile_bpp == TILE_4BPP || tile_bpp == TILE_2BPP)
	{
		guchar* shifted_map = (guchar*)malloc(sizeof(guchar) * pal_size * 3);
		guchar num_palettes = pal_size / 16;

		for (int i = 0; i < num_palettes; i++)
		{

			shift_color_map(cmap, &shifted_map, pal_size, i*16);
			gimp_palette_set_colormap(palette, format, shifted_map, pal_bytes);
			gimp_image_set_palette(image, palette); // May be unecessary, since the palette is already owned by the image
			gchar* number_string = malloc(sizeof(gchar) * 3);
			sprintf(number_string, "%d", i);
			gchar *numbered_filename = g_strconcat(filename, ".", number_string, NULL);
			gchar *numbered_bmp_filename = g_strconcat (numbered_filename, ".bmp", NULL);

			if (bmp_file)
			{
				// write out a bitmap to be used with the .tsx file
				gimp_file_save(GIMP_RUN_NONINTERACTIVE,
						image,
						g_file_new_for_path(numbered_bmp_filename),
						NULL);
			}

			if(!save_tsx(numbered_filename,
						numbered_bmp_filename,
						image_buffer,
						image_width,
						image_height,
						tile_width,
						tile_height,
						&error))
			{
				ret = false;
			}

			g_free(number_string);
			g_free(numbered_filename);
			g_free(numbered_bmp_filename);
		}

		gimp_palette_set_colormap(palette, format, cmap, pal_bytes);
		gimp_image_set_palette(image, palette); // May be unecessary, since the palette is already owned by the image
		g_free(shifted_map);
	}
	else // NOT 4bpp or 2BPP
	{
		if (bmp_file)
		{
			// write out a bitmap to be used with the .tsx file
			gimp_file_save(GIMP_RUN_NONINTERACTIVE,
					image,
					g_file_new_for_path(bmp_filename),
					NULL);
		}

		if(!save_tsx(filename,
					bmp_filename,
					image_buffer,
					image_width,
					image_height,
					tile_width,
					tile_height,
					&error))
		{
			ret = FALSE;
		}
	}

	return ret;
}

gboolean save_tsx(const gchar *filename,
		const gchar	*bmp_filename,
		guchar		*image_buffer,
		gint		image_width,
		gint		image_height,
		TileWidth	tile_width,
		TileHeight	tile_height,
		GError		**error)
{
	// write out the tsx file
	gchar *tsx_filename = g_strconcat (filename, ".tsx", NULL);

	int rc;
	xmlTextWriterPtr writer;

	writer = xmlNewTextWriterFilename(tsx_filename, 0);

	g_free(tsx_filename);
	
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
	gint32            tile_count, columns;

	/* get info about the current image */

	tile_count = (image_width * image_height) / (tile_width * tile_height);
	columns = image_width / tile_width;

	gimp_message("writing tsx document\n");
	gchar* val_string;

	xmlTextWriterStartElement(writer, BAD_CAST "tileset");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1.5");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "tiledversion", BAD_CAST "1.8.0");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "name", BAD_CAST filename);
	val_string = g_strdup_printf("%d", tile_width);
	xmlTextWriterWriteAttribute(writer,BAD_CAST "tilewidth", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", tile_height);
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
	val_string = g_strdup_printf("%d", image_width);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "width", BAD_CAST val_string);
	g_free(val_string);
	val_string = g_strdup_printf("%d", image_height);
	xmlTextWriterWriteAttribute(writer, BAD_CAST "height", BAD_CAST val_string);
	g_free(val_string);
	xmlTextWriterEndElement(writer); // image

	xmlTextWriterEndElement(writer); // tileset

	xmlTextWriterEndDocument(writer);

	xmlFreeTextWriter(writer);

	gimp_message("finished writing tsx document\n");

	return TRUE;
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
