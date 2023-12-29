#define _DEFAULT_SOURCE
#include <assert.h>
#include "background-image.h"
#include "cairo.h"
#include "log.h"
#include "swaylock.h"

#ifdef __FreeBSD__
#	include <sys/endian.h>
#else
#	include <endian.h>
#endif

// Cairo RGB24 uses 32 bits per pixel, as XRGB, in native endianness.
// xrgb32_le uses 32 bits per pixel, as XRGB, little endian (BGRX big endian).
void cairo_rgb24_from_xrgb32_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			unsigned char *pix = buf + y * stride + x * 4;
			*(uint32_t *)pix = 0 |
				(uint32_t)pix[2] << 16 |
				(uint32_t)pix[1] << 8 |
				(uint32_t)pix[0];
		}
	}
}

// Cairo RGB24 uses 32 bits per pixel, as XRGB, in native endianness.
// xbgr32_le uses 32 bits per pixel, as XBGR, little endian (RGBX big endian).
void cairo_rgb24_from_xbgr32_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			unsigned char *pix = buf + y * stride + x * 4;
			*(uint32_t *)pix = 0 |
				(uint32_t)pix[0] << 16 |
				(uint32_t)pix[1] << 8 |
				(uint32_t)pix[2];
		}
	}
}

void cairo_rgb24_from_xrgb2101010_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t *pix = (uint32_t *) (buf + y * stride + x * 4);
			uint32_t color = le32toh(*pix);
			*pix = 0 |
				((color >> 22) & 0xFF) << 16 |
				((color >> 12) & 0xFF) << 8 |
				((color >> 2) & 0xFF);
		}
	}
}

void cairo_rgb24_from_xbgr2101010_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t *pix = (uint32_t *) (buf + y * stride + x * 4);
			uint32_t color = le32toh(*pix);
			*pix = 0 |
				((color >> 2) & 0xFF) << 16 |
				((color >> 12) & 0xFF) << 8 |
				((color >> 22) & 0xFF);
		}
	}
}

void cairo_rgb24_from_rgbx1010102_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t *pix = (uint32_t *) (buf + y * stride + x * 4);
			uint32_t color = le32toh(*pix);
			*pix = 0 |
				((color >> 24) & 0xFF) << 16 |
				((color >> 14) & 0xFF) << 8 |
				((color >> 4) & 0xFF);
		}
	}
}

void cairo_rgb24_from_bgrx1010102_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t *pix = (uint32_t *) (buf + y * stride + x * 4);
			uint32_t color = le32toh(*pix);
			*pix = 0 |
				((color >> 4) & 0xFF) << 16 |
				((color >> 14) & 0xFF) << 8 |
				((color >> 24) & 0xFF);
		}
	}
}

// Cairo RGB24 uses 32 bits per pixel, as XRGB, in native endianness.
// BGR888 uses 24 bits per pixel, as BGR, little endian.
// 24-bit BGR format, [23:0] B:G:R little endian (From wayland-client-protocol.h)
void cairo_rgb24_from_bgr888_le(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		// Row from back to front to avoid overwriting data.
		for (int x = width-1; x >= 0; --x) {
			// 24 bits = 3 bytes, 32 bits = 4 bytes
			unsigned char *srcpix = buf + y * stride + x * 3;
			unsigned char *dstpix = buf + y * stride + x * 4;

			*(uint32_t *)dstpix = 0 |
				(uint32_t)srcpix[0] << 16 |
				(uint32_t)srcpix[1] << 8 |
				(uint32_t)srcpix[2];
		}
	}
}

// Swap red and blue values in Cairo RGB24
void cairo_rgb24_swap_rb(unsigned char *buf, int width, int height, int stride) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			unsigned char *pix = buf + y * stride + x * 4;

			*(uint32_t *)pix = 0 |
				(uint32_t)pix[0] << 16 |
				(uint32_t)pix[1] << 8 |
				(uint32_t)pix[2];
		}
	}
}

enum background_mode parse_background_mode(const char *mode) {
	if (strcmp(mode, "stretch") == 0) {
		return BACKGROUND_MODE_STRETCH;
	} else if (strcmp(mode, "fill") == 0) {
		return BACKGROUND_MODE_FILL;
	} else if (strcmp(mode, "fit") == 0) {
		return BACKGROUND_MODE_FIT;
	} else if (strcmp(mode, "center") == 0) {
		return BACKGROUND_MODE_CENTER;
	} else if (strcmp(mode, "tile") == 0) {
		return BACKGROUND_MODE_TILE;
	} else if (strcmp(mode, "solid_color") == 0) {
		return BACKGROUND_MODE_SOLID_COLOR;
	}
	swaylock_log(LOG_ERROR, "Unsupported background mode: %s", mode);
	return BACKGROUND_MODE_INVALID;
}

cairo_surface_t *load_background_from_buffer(void *buf, uint32_t format,
		uint32_t width, uint32_t height, uint32_t stride, enum wl_output_transform transform) {
	bool rotated =
		transform == WL_OUTPUT_TRANSFORM_90 ||
		transform == WL_OUTPUT_TRANSFORM_270 ||
		transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
		transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;

	cairo_surface_t *image;
	if (rotated) {
		image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, height, width);
	} else {
		image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
	}
	if (image == NULL) {
		swaylock_log(LOG_ERROR, "Failed to create image..");
		return NULL;
	}

	unsigned char *destbuf = cairo_image_surface_get_data(image);
	size_t destwidth = cairo_image_surface_get_width(image);
	size_t destheight = cairo_image_surface_get_height(image);
	size_t deststride = cairo_image_surface_get_stride(image);
	unsigned char *srcbuf = buf;
	size_t srcstride = stride;
	size_t minstride = srcstride < deststride ? srcstride : deststride;

	// Lots of these are mostly-copy-and-pasted, with a lot of boilerplate
	// for each case.
	// The only interesting differencess between a lot of these cases are
	// the definitions of srcx and srcy.
	// I don't think it's worth adding a macro to make this "cleaner" though,
	// as that would obfuscate what's actually going on.
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		// In most cases, the transform is probably normal. Luckily, it can be
		// done with just one big memcpy.
		if (srcstride == deststride) {
			memcpy(destbuf, srcbuf, destheight * deststride);
		} else {
			for (size_t y = 0; y < destheight; ++y) {
				memcpy(destbuf + y * deststride, srcbuf + y * srcstride, minstride);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_90:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcx = desty;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcy = destwidth - destx - 1;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_180:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcy = destheight - desty - 1;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcx = destwidth - destx - 1;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_270:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcx = destheight - desty - 1;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcy = destx;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcy = desty;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcx = destwidth - destx - 1;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcx = desty;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcy = destx;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcy = destheight - desty - 1;
			memcpy(destbuf + desty * deststride, srcbuf + srcy * srcstride, minstride);
		}
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		for (size_t desty = 0; desty < destheight; ++desty) {
			size_t srcx = destheight - desty - 1;
			for (size_t destx = 0; destx < destwidth; ++destx) {
				size_t srcy = destwidth - destx - 1;
				*((uint32_t *)(destbuf + desty * deststride) + destx) =
					*((uint32_t *)(srcbuf + srcy * srcstride) + srcx);
			}
		}
		break;
	}

	switch (format) {
	case WL_SHM_FORMAT_XBGR8888:
	case WL_SHM_FORMAT_ABGR8888:
		cairo_rgb24_from_xbgr32_le(
				cairo_image_surface_get_data(image),
				cairo_image_surface_get_width(image),
				cairo_image_surface_get_height(image),
				cairo_image_surface_get_stride(image));
		break;
	case WL_SHM_FORMAT_XRGB2101010:
	case WL_SHM_FORMAT_ARGB2101010:
		cairo_rgb24_from_xrgb2101010_le(
				cairo_image_surface_get_data(image),
				cairo_image_surface_get_width(image),
				cairo_image_surface_get_height(image),
				cairo_image_surface_get_stride(image));
		break;
	case WL_SHM_FORMAT_XBGR2101010:
	case WL_SHM_FORMAT_ABGR2101010:
		cairo_rgb24_from_xbgr2101010_le(
				cairo_image_surface_get_data(image),
				cairo_image_surface_get_width(image),
				cairo_image_surface_get_height(image),
				cairo_image_surface_get_stride(image));
		break;
	case WL_SHM_FORMAT_RGBX1010102:
	case WL_SHM_FORMAT_RGBA1010102:
		cairo_rgb24_from_rgbx1010102_le(
				cairo_image_surface_get_data(image),
				cairo_image_surface_get_width(image),
				cairo_image_surface_get_height(image),
				cairo_image_surface_get_stride(image));
		break;
	case WL_SHM_FORMAT_BGRX1010102:
	case WL_SHM_FORMAT_BGRA1010102:
		cairo_rgb24_from_bgrx1010102_le(
				cairo_image_surface_get_data(image),
				cairo_image_surface_get_width(image),
				cairo_image_surface_get_height(image),
				cairo_image_surface_get_stride(image));
		break;
	case WL_SHM_FORMAT_BGR888:
	case WL_SHM_FORMAT_RGB888:
			cairo_rgb24_from_bgr888_le(
					cairo_image_surface_get_data(image),
					cairo_image_surface_get_width(image),
					cairo_image_surface_get_height(image),
					cairo_image_surface_get_stride(image)
					);
			if (format == WL_SHM_FORMAT_RGB888) {
				cairo_rgb24_swap_rb(
						cairo_image_surface_get_data(image),
						cairo_image_surface_get_width(image),
						cairo_image_surface_get_height(image),
						cairo_image_surface_get_stride(image)
						);
			}
		break;
	default:
		swaylock_log(LOG_ERROR,
				"Unknown pixel format: %u. Assuming XRGB32. Colors may look wrong.",
				format);
		// fallthrough
	case WL_SHM_FORMAT_XRGB8888:
	case WL_SHM_FORMAT_ARGB8888: {
		// If we're little endian, we don't have to do anything
		int test = 1;
		bool is_little_endian = *(char *)&test == 1;
		if (!is_little_endian) {
			cairo_rgb24_from_xrgb32_le(
					cairo_image_surface_get_data(image),
					cairo_image_surface_get_width(image),
					cairo_image_surface_get_height(image),
					cairo_image_surface_get_stride(image));
		}
	}
	}

	return image;
}

cairo_surface_t *load_background_image(const char *path) {
	cairo_surface_t *image;
#if HAVE_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		swaylock_log(LOG_ERROR, "Failed to load background image (%s).",
				err->message);
		return NULL;
	}
	image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
#else
	image = cairo_image_surface_create_from_png(path);
#endif // HAVE_GDK_PIXBUF
	if (!image) {
		swaylock_log(LOG_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		swaylock_log(LOG_ERROR, "Failed to read background image: %s."
#if !HAVE_GDK_PIXBUF
				"\nSway was compiled without gdk_pixbuf support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif // !HAVE_GDK_PIXBUF
				, cairo_status_to_string(cairo_surface_status(image)));
		return NULL;
	}
	return image;
}

cairo_surface_t *scale_background_image(cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height) {
	cairo_surface_t *target = cairo_image_surface_create(CAIRO_FORMAT_RGB24, buffer_width, buffer_height);
	cairo_t *cairo = cairo_create(target);
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	switch (mode) {
	case BACKGROUND_MODE_STRETCH:
		cairo_scale(cairo,
				(double)buffer_width / width,
				(double)buffer_height / height);
		cairo_set_source_surface(cairo, image, 0, 0);
		break;
	case BACKGROUND_MODE_FILL: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		} else {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		}
		break;
	}
	case BACKGROUND_MODE_FIT: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		} else {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		}
		break;
	}
	case BACKGROUND_MODE_CENTER:
		/*
		 * Align the unscaled image to integer pixel boundaries
		 * in order to prevent loss of clarity (this only matters
		 * for odd-sized images).
		 */
		cairo_set_source_surface(cairo, image,
				(int)((double)buffer_width / 2 - width / 2),
				(int)((double)buffer_height / 2 - height / 2));
		break;
	case BACKGROUND_MODE_TILE: {
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
		cairo_set_source(cairo, pattern);
		break;
	}
	case BACKGROUND_MODE_SOLID_COLOR:
	case BACKGROUND_MODE_INVALID:
		assert(0);
		break;
	}

	cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_BILINEAR);
	cairo_paint(cairo);
	cairo_destroy(cairo);
	return target;
}

void render_background_image(cairo_t *cairo, cairo_surface_t *image, double alpha) {
	cairo_save(cairo);
	cairo_set_source_surface(cairo, image, 0, 0);
	cairo_paint_with_alpha(cairo, alpha);
	cairo_restore(cairo);
}
