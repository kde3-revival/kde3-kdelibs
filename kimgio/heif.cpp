/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// HEIF images (HEIC and the other HEIF brands) with libheif: the primary
// image of the file, with alpha, rotated and mirrored as the file says
// (irot, imir), and of 8 bits per channel or more, the latter reduced to
// 8 bits.  Which codecs can be read depends on the decoder plugins of
// libheif (libde265 or FFmpeg for HEVC, dav1d or aom for AV1, ...).

#include "heif.h"

#include <stdint.h>

#include <qimage.h>
#include <qiodevice.h>

#include <kdebug.h>
#include <kglobal.h>

#include <libheif/heif.h>

// The largest image read, in pixels (16384 x 16384: 1 GB as a QImage).
static const uint64_t maxPixels = uint64_t( 16384 ) * 16384;

// The heif_context, image handle and decoded image of one read.
struct HeifRead
{
	heif_context *ctx;
	heif_image_handle *handle;
	heif_image *image;

	HeifRead() : ctx( 0 ), handle( 0 ), image( 0 ) {}
	~HeifRead()
	{
		if ( image )
			heif_image_release( image );
		if ( handle )
			heif_image_handle_release( handle );
		if ( ctx )
			heif_context_free( ctx );
	}
};

KDE_EXPORT void kimgio_heif_read( QImageIO *io )
{
	io->setImage( QImage() );
	io->setStatus( -1 );

	// heif_init() loads libheif's decoder plugins.  It is reference
	// counted; the reference taken here is never given back, so that the
	// plugins are not loaded again for every image.
	static bool initialized = false;
	if ( !initialized ) {
		heif_error err = heif_init( 0 );
		if ( err.code != heif_error_Ok ) {
			kdDebug(399) << "kimgio_heif_read: heif_init: " << err.message << endl;
			return;
		}
		initialized = true;
	}

	const QByteArray data = io->ioDevice()->readAll();

	HeifRead r;
	r.ctx = heif_context_alloc();
	if ( !r.ctx )
		return;
	heif_security_limits *limits = heif_context_get_security_limits( r.ctx );
	if ( limits && ( limits->max_image_size_pixels == 0
			|| limits->max_image_size_pixels > maxPixels ) )
		limits->max_image_size_pixels = maxPixels;

	heif_error err = heif_context_read_from_memory_without_copy( r.ctx,
		data.data(), data.size(), 0 );
	if ( err.code != heif_error_Ok ) {
		kdDebug(399) << "kimgio_heif_read: " << err.message << endl;
		return;
	}
	err = heif_context_get_primary_image_handle( r.ctx, &r.handle );
	if ( err.code != heif_error_Ok ) {
		kdDebug(399) << "kimgio_heif_read: " << err.message << endl;
		return;
	}

	const bool alpha = heif_image_handle_has_alpha_channel( r.handle );

	// The width and height after the transformations (rotation, crop).
	const int width = heif_image_handle_get_width( r.handle );
	const int height = heif_image_handle_get_height( r.handle );
	if ( width <= 0 || height <= 0
			|| uint64_t( width ) * uint64_t( height ) > maxPixels ) {
		kdDebug(399) << "kimgio_heif_read: unsupported image size "
			<< width << "x" << height << endl;
		return;
	}

	// libheif applies the transformations (irot, imir, clap) and reduces
	// more than 8 bits per channel to 8 bits.
	heif_decoding_options *options = heif_decoding_options_alloc();
	if ( !options )
		return;
	options->convert_hdr_to_8bit = 1;
	err = heif_decode_image( r.handle, &r.image, heif_colorspace_RGB,
		heif_chroma_interleaved_RGBA, options );
	heif_decoding_options_free( options );
	if ( err.code != heif_error_Ok ) {
		kdDebug(399) << "kimgio_heif_read: " << err.message << endl;
		return;
	}

	// The size of the decoded plane, which need not be the size of the
	// handle; the row is read with the stride libheif returns.
	const int w = heif_image_get_width( r.image, heif_channel_interleaved );
	const int h = heif_image_get_height( r.image, heif_channel_interleaved );
	size_t stride = 0;
	const uint8_t *pixels = heif_image_get_plane_readonly2( r.image,
		heif_channel_interleaved, &stride );
	if ( !pixels || w <= 0 || h <= 0 || uint64_t( w ) * uint64_t( h ) > maxPixels
			|| stride < size_t( w ) * 4
			|| heif_image_get_bits_per_pixel_range( r.image,
				heif_channel_interleaved ) != 8 ) {
		kdDebug(399) << "kimgio_heif_read: unexpected decoded image" << endl;
		return;
	}

	QImage img( w, h, 32 );
	if ( img.isNull() ) {
		kdDebug(399) << "kimgio_heif_read: cannot allocate the image" << endl;
		return;
	}

	// QRgb is 0xAARRGGBB in host byte order; libheif gives R, G, B, A bytes
	for ( int y = 0; y < h; ++y ) {
		const uint8_t *src = pixels + size_t( y ) * stride;
		QRgb *dst = reinterpret_cast<QRgb *>( img.scanLine( y ) );
		for ( int x = 0; x < w; ++x, src += 4 )
			dst[x] = qRgba( src[0], src[1], src[2], src[3] );
	}

	img.setAlphaBuffer( alpha );
	io->setImage( img );
	io->setStatus( 0 );
}
