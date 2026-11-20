/*
 * This file is part of the KDE libraries
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 */

// SVG icons rendered with librsvg into a cairo image surface.
//
// The glib, cairo and librsvg headers come before every Qt header: Qt 3
// #defines signals, slots and emit, which glib uses as identifiers.
#include <cairo.h>
#include <librsvg/rsvg.h>

#include <qfile.h>
#include <qimage.h>
#include <qstring.h>

#include "ksvgiconrender_p.h"

// Used when an icon is loaded at its own size and the document does not
// say how large it is (no width/height in absolute units, no viewBox).
static const int defaultSize = 32;
// Upper limit for an icon loaded at its own size.
static const int maxDocumentSize = 1024;

static int documentSize(RsvgHandle *handle)
{
    gdouble width = 0, height = 0;
    if (!rsvg_handle_get_intrinsic_size_in_pixels(handle, &width, &height))
    {
        gboolean hasViewBox = FALSE;
        RsvgRectangle viewBox;
        rsvg_handle_get_intrinsic_dimensions(handle, 0, 0, 0, 0,
                                             &hasViewBox, &viewBox);
        if (hasViewBox)
        {
            width = viewBox.width;
            height = viewBox.height;
        }
    }
    double size = width > height ? width : height;
    if (!(size >= 1.0))         // also NaN
        return defaultSize;
    if (size > maxDocumentSize)
        return maxDocumentSize;
    return int(size + 0.5);
}

// cairo's ARGB32 holds native-endian 0xAARRGGBB words like QRgb, but with
// premultiplied colour components; QImage wants them not premultiplied.
static void unpremultiply(cairo_surface_t *surface, QImage *img)
{
    const unsigned char *data = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < img->height(); y++)
    {
        const Q_UINT32 *src = reinterpret_cast<const Q_UINT32 *>(data + y * stride);
        QRgb *dst = reinterpret_cast<QRgb *>(img->scanLine(y));
        for (int x = 0; x < img->width(); x++)
        {
            const Q_UINT32 p = src[x];
            const uint a = p >> 24;
            if (a == 0)
                dst[x] = 0;
            else if (a == 255)
                dst[x] = p;
            else
            {
                // Premultiplied components are <= a, so these are <= 255.
                const uint r = (((p >> 16) & 0xff) * 255 + a / 2) / a;
                const uint g = (((p >> 8) & 0xff) * 255 + a / 2) / a;
                const uint b = ((p & 0xff) * 255 + a / 2) / a;
                dst[x] = qRgba(r, g, b, a);
            }
        }
    }
}

QImage *kdeRenderSvgIcon(const QString &path, int size)
{
    GError *error = 0;
    // The file is read here and given to librsvg as data, without a base
    // URI: a GFile, a file name or a base URI make librsvg create a GFile,
    // and that makes GIO load its VFS modules, e.g. gvfs, which starts
    // threads and connects to the session bus in every application that
    // shows an SVG icon.  Without a base URI, references to other files
    // (<image href="x.png">, <use href="other.svg#id">) are not loaded;
    // icons are self-contained.  librsvg recognises gzip-compressed data
    // (.svgz) itself.
    gchar *data = 0;
    gsize length = 0;
    if (!g_file_get_contents(QFile::encodeName(path).data(), &data, &length,
                             &error))
    {
        if (error)
            g_error_free(error);
        return 0;
    }
    RsvgHandle *handle = rsvg_handle_new_from_data(
        reinterpret_cast<const guint8 *>(data), length, &error);
    g_free(data);
    if (!handle)
    {
        if (error)
            g_error_free(error);
        return 0;
    }

    if (size <= 0)
        size = documentSize(handle);

    QImage *img = 0;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);
    if (cairo_status(cr) == CAIRO_STATUS_SUCCESS)
    {
        // The document is scaled to fit the viewport and centred in it.
        RsvgRectangle viewport = { 0.0, 0.0, double(size), double(size) };
        const gboolean rendered =
            rsvg_handle_render_document(handle, cr, &viewport, &error);
        cairo_surface_flush(surface);
        if (rendered && cairo_status(cr) == CAIRO_STATUS_SUCCESS)
        {
            img = new QImage(size, size, 32);
            if (img->isNull())
            {
                delete img;
                img = 0;
            }
            else
            {
                img->setAlphaBuffer(true);
                unpremultiply(surface, img);
            }
        }
        if (error)
            g_error_free(error);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);
    return img;
}
