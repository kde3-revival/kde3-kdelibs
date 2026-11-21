/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// AVIF images with libavif: the primary image, or the first frame of an
// animated AVIF, with alpha, cropped, rotated and mirrored as the file
// says (clap, irot, imir), and of 8 bits per channel or more, the latter
// reduced to 8 bits.

#include "avif.h"

#include <stdint.h>

#include <qimage.h>
#include <qiodevice.h>

#include <kdebug.h>
#include <kglobal.h>

#include <avif/avif.h>

// The largest image read, in pixels (16384 x 16384: 1 GB as a QImage).
static const uint32_t maxPixels = 16384 * 16384;

// The decoder and the RGB pixels of one read.
struct AvifRead
{
	avifDecoder *decoder;
	avifRGBImage rgb;

	AvifRead() : decoder( 0 ) { rgb.pixels = 0; }
	~AvifRead()
	{
		if ( rgb.pixels )
			avifRGBImageFreePixels( &rgb );
		if ( decoder )
			avifDecoderDestroy( decoder );
	}
};

// @p img turned anti-clockwise by @p quarters x 90 degrees (irot).
static QImage rotated( const QImage &img, int quarters )
{
	const int w = img.width();
	const int h = img.height();
	if ( quarters == 2 )
		return img.mirror( true, true );

	QImage dst( h, w, 32 );
	if ( dst.isNull() )
		return dst;
	for ( int y = 0; y < h; ++y ) {
		const QRgb *src = reinterpret_cast<const QRgb *>( img.scanLine( y ) );
		for ( int x = 0; x < w; ++x ) {
			if ( quarters == 1 )	// (x, y) -> (y, w - 1 - x)
				reinterpret_cast<QRgb *>( dst.scanLine( w - 1 - x ) )[y] = src[x];
			else			// (x, y) -> (h - 1 - y, x)
				reinterpret_cast<QRgb *>( dst.scanLine( x ) )[h - 1 - y] = src[x];
		}
	}
	return dst;
}

KDE_EXPORT void kimgio_avif_read( QImageIO *io )
{
	io->setImage( QImage() );
	io->setStatus( -1 );

	const QByteArray data = io->ioDevice()->readAll();

	AvifRead r;
	r.decoder = avifDecoderCreate();
	if ( !r.decoder )
		return;
	avifDecoder *decoder = r.decoder;
	decoder->maxThreads = 4;
	decoder->ignoreExif = AVIF_TRUE;
	decoder->ignoreXMP = AVIF_TRUE;
	// As web browsers do: files with small violations of the
	// specification (a missing pixi box, ...) are read.
	decoder->strictFlags = AVIF_STRICT_DISABLED;
	if ( decoder->imageSizeLimit == 0 || decoder->imageSizeLimit > maxPixels )
		decoder->imageSizeLimit = maxPixels;

	avifResult result = avifDecoderSetIOMemory( decoder,
		reinterpret_cast<const uint8_t *>( data.data() ), data.size() );
	if ( result == AVIF_RESULT_OK )
		result = avifDecoderParse( decoder );
	if ( result == AVIF_RESULT_OK )
		result = avifDecoderNextImage( decoder );	// the first frame
	if ( result != AVIF_RESULT_OK ) {
		kdDebug(399) << "kimgio_avif_read: " << avifResultToString( result ) << endl;
		return;
	}

	const avifImage *image = decoder->image;
	if ( image->width == 0 || image->height == 0
			|| uint64_t( image->width ) * image->height > maxPixels ) {
		kdDebug(399) << "kimgio_avif_read: unsupported image size "
			<< image->width << "x" << image->height << endl;
		return;
	}

	// libavif reduces more than 8 bits per channel to 8 bits and gives
	// an opaque alpha to images without one.
	avifRGBImageSetDefaults( &r.rgb, image );
	r.rgb.depth = 8;
	r.rgb.format = AVIF_RGB_FORMAT_RGBA;
	r.rgb.maxThreads = decoder->maxThreads;
	result = avifRGBImageAllocatePixels( &r.rgb );
	if ( result == AVIF_RESULT_OK )
		result = avifImageYUVToRGB( image, &r.rgb );
	if ( result != AVIF_RESULT_OK ) {
		kdDebug(399) << "kimgio_avif_read: " << avifResultToString( result ) << endl;
		return;
	}

	// The clean aperture (clap) is the part of the image shown.
	avifCropRect crop = { 0, 0, r.rgb.width, r.rgb.height };
	if ( image->transformFlags & AVIF_TRANSFORM_CLAP ) {
		avifCropRect clap;
		if ( avifCropRectFromCleanApertureBox( &clap, &image->clap,
				r.rgb.width, r.rgb.height, &decoder->diag )
				&& clap.width > 0 && clap.height > 0
				&& clap.x <= r.rgb.width - clap.width
				&& clap.y <= r.rgb.height - clap.height )
			crop = clap;
		else
			kdDebug(399) << "kimgio_avif_read: invalid clean aperture, ignored" << endl;
	}

	QImage img( crop.width, crop.height, 32 );
	if ( img.isNull() ) {
		kdDebug(399) << "kimgio_avif_read: cannot allocate the image" << endl;
		return;
	}

	// QRgb is 0xAARRGGBB in host byte order; libavif gives R, G, B, A bytes
	for ( uint32_t y = 0; y < crop.height; ++y ) {
		const uint8_t *src = r.rgb.pixels + size_t( crop.y + y ) * r.rgb.rowBytes
			+ size_t( crop.x ) * 4;
		QRgb *dst = reinterpret_cast<QRgb *>( img.scanLine( y ) );
		for ( uint32_t x = 0; x < crop.width; ++x, src += 4 )
			dst[x] = qRgba( src[0], src[1], src[2], src[3] );
	}

	// After the crop, the rotation (irot) and then the mirroring (imir).
	if ( ( image->transformFlags & AVIF_TRANSFORM_IROT ) && ( image->irot.angle & 3 ) ) {
		img = rotated( img, image->irot.angle & 3 );
		if ( img.isNull() ) {
			kdDebug(399) << "kimgio_avif_read: cannot allocate the image" << endl;
			return;
		}
	}
	if ( image->transformFlags & AVIF_TRANSFORM_IMIR ) {
		// axis 0: top and bottom exchanged, 1: left and right
		img = img.mirror( image->imir.axis == 1, image->imir.axis == 0 );
		if ( img.isNull() ) {
			kdDebug(399) << "kimgio_avif_read: cannot allocate the image" << endl;
			return;
		}
	}

	img.setAlphaBuffer( image->alphaPlane != 0 );
	io->setImage( img );
	io->setStatus( 0 );
}
