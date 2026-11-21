/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// SVG and gzip-compressed SVG (SVGZ) images, rendered with librsvg into a
// cairo image surface at the size of the document, with alpha.
//
// The glib, cairo and librsvg headers come before every Qt header: Qt 3
// #defines signals, slots and emit, which glib uses as identifiers.
#include <cairo.h>
#include <librsvg/rsvg.h>

#include "svg.h"

#include <qimage.h>
#include <qiodevice.h>

#include <kdebug.h>
#include <kglobal.h>

// The size of a document that does not say how large it is (no width and
// height in absolute units, no viewBox).
static const double defaultSize = 512;
// A larger document is scaled down to fit into maxSize x maxSize pixels.
static const double maxSize = 8192;

// The size of the image of the document, in pixels, with its aspect ratio.
static void documentSize( RsvgHandle *handle, int *width, int *height )
{
	gdouble w = 0, h = 0;
	if ( !rsvg_handle_get_intrinsic_size_in_pixels( handle, &w, &h ) ) {
		// width and height missing or relative (100%): the viewBox,
		// in user units, which are pixels
		gboolean hasViewBox = FALSE;
		RsvgRectangle viewBox;
		rsvg_handle_get_intrinsic_dimensions( handle, 0, 0, 0, 0,
			&hasViewBox, &viewBox );
		if ( hasViewBox ) {
			w = viewBox.width;
			h = viewBox.height;
		}
	}
	if ( !( w > 0 && h > 0 ) ) {	// also NaN
		w = defaultSize;
		h = defaultSize;
	}
	const double larger = w > h ? w : h;
	if ( larger > maxSize ) {	// also infinity
		w = w * ( maxSize / larger );
		h = h * ( maxSize / larger );
	}
	*width = w < 1 ? 1 : int( w + 0.5 );
	*height = h < 1 ? 1 : int( h + 0.5 );
}

// cairo's ARGB32 holds native-endian 0xAARRGGBB words like QRgb, but with
// premultiplied colour components; QImage wants them not premultiplied.
static void unpremultiply( cairo_surface_t *surface, QImage *img )
{
	const unsigned char *data = cairo_image_surface_get_data( surface );
	const int stride = cairo_image_surface_get_stride( surface );
	for ( int y = 0; y < img->height(); ++y ) {
		const Q_UINT32 *src = reinterpret_cast<const Q_UINT32 *>( data + y * stride );
		QRgb *dst = reinterpret_cast<QRgb *>( img->scanLine( y ) );
		for ( int x = 0; x < img->width(); ++x ) {
			const Q_UINT32 p = src[x];
			const uint a = p >> 24;
			if ( a == 0 )
				dst[x] = 0;
			else if ( a == 255 )
				dst[x] = p;
			else {
				// Premultiplied components are <= a, so these are <= 255.
				const uint r = ( ( ( p >> 16 ) & 0xff ) * 255 + a / 2 ) / a;
				const uint g = ( ( ( p >> 8 ) & 0xff ) * 255 + a / 2 ) / a;
				const uint b = ( ( p & 0xff ) * 255 + a / 2 ) / a;
				dst[x] = qRgba( r, g, b, a );
			}
		}
	}
}

KDE_EXPORT void kimgio_svg_read( QImageIO *io )
{
	io->setImage( QImage() );
	io->setStatus( -1 );

	// The document is given to librsvg as data, without a base URI: it
	// does not load other files (<image href="x.png">, <use
	// href="other.svg#id">), and GIO does not load its VFS modules (see
	// kdecore's ksvgiconrender.cpp).  librsvg recognises gzip-compressed
	// data (.svgz) itself.
	const QByteArray data = io->ioDevice()->readAll();
	GError *error = 0;
	RsvgHandle *handle = rsvg_handle_new_from_data(
		reinterpret_cast<const guint8 *>( data.data() ), data.size(), &error );
	if ( !handle ) {
		kdDebug(399) << "kimgio_svg_read: "
			<< ( error ? error->message : "cannot read the document" ) << endl;
		if ( error )
			g_error_free( error );
		return;
	}

	int width, height;
	documentSize( handle, &width, &height );

	cairo_surface_t *surface =
		cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width, height );
	cairo_t *cr = cairo_create( surface );
	if ( cairo_status( cr ) == CAIRO_STATUS_SUCCESS ) {
		// The document is scaled to the viewport, keeping its aspect ratio.
		RsvgRectangle viewport = { 0.0, 0.0, double( width ), double( height ) };
		const gboolean rendered =
			rsvg_handle_render_document( handle, cr, &viewport, &error );
		cairo_surface_flush( surface );
		if ( rendered && cairo_status( cr ) == CAIRO_STATUS_SUCCESS ) {
			QImage img( width, height, 32 );
			if ( img.isNull() )
				kdDebug(399) << "kimgio_svg_read: cannot allocate the image" << endl;
			else {
				img.setAlphaBuffer( true );
				unpremultiply( surface, &img );
				io->setImage( img );
				io->setStatus( 0 );
			}
		} else
			kdDebug(399) << "kimgio_svg_read: "
				<< ( error ? error->message : "cannot render the document" ) << endl;
		if ( error )
			g_error_free( error );
	} else
		kdDebug(399) << "kimgio_svg_read: cannot allocate the image" << endl;
	cairo_destroy( cr );
	cairo_surface_destroy( surface );
	g_object_unref( handle );
}
