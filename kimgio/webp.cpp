/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// WebP images with libwebp: still images (lossy or lossless, with or
// without alpha); of an animated image nothing is read.

#include "webp.h"

#include <qimage.h>
#include <qiodevice.h>

#include <kdebug.h>
#include <kglobal.h>

#include <webp/decode.h>
#include <webp/encode.h>

KDE_EXPORT void kimgio_webp_read( QImageIO *io )
{
	io->setImage( QImage() );
	io->setStatus( -1 );

	const QByteArray data = io->ioDevice()->readAll();
	const uint8_t *bytes = reinterpret_cast<const uint8_t *>( data.data() );

	WebPBitstreamFeatures features;
	if ( WebPGetFeatures( bytes, data.size(), &features ) != VP8_STATUS_OK ) {
		kdDebug(399) << "kimgio_webp_read: not a WebP image" << endl;
		return;
	}
	if ( features.has_animation ) {
		kdDebug(399) << "kimgio_webp_read: animated WebP is not supported" << endl;
		return;
	}

	QImage img( features.width, features.height, 32 );
	if ( img.isNull() ) {
		kdDebug(399) << "kimgio_webp_read: cannot allocate the image" << endl;
		return;
	}

	int width, height;
	uint8_t *rgba = WebPDecodeRGBA( bytes, data.size(), &width, &height );
	if ( !rgba || width != img.width() || height != img.height() ) {
		kdDebug(399) << "kimgio_webp_read: cannot decode the image" << endl;
		WebPFree( rgba );
		return;
	}

	// QRgb is 0xAARRGGBB in host byte order; libwebp gives R, G, B, A bytes
	const uint8_t *src = rgba;
	for ( int y = 0; y < height; ++y ) {
		QRgb *dst = reinterpret_cast<QRgb *>( img.scanLine( y ) );
		for ( int x = 0; x < width; ++x, src += 4 )
			dst[x] = qRgba( src[0], src[1], src[2], src[3] );
	}
	WebPFree( rgba );

	img.setAlphaBuffer( features.has_alpha );
	io->setImage( img );
	io->setStatus( 0 );
}

// The quality (QImageIO::setQuality, 0-100) of the lossy encoding: -1
// gives libwebp's default of 75, 100 a lossless image.
KDE_EXPORT void kimgio_webp_write( QImageIO *io )
{
	io->setStatus( -1 );

	QImage img = io->image();
	if ( img.isNull() )
		return;
	if ( img.depth() != 32 )
		img = img.convertDepth( 32 );
	if ( img.isNull() )
		return;

	const int width = img.width();
	const int height = img.height();
	const bool alpha = img.hasAlphaBuffer();

	QByteArray rgba( width * height * 4 );
	uint8_t *dst = reinterpret_cast<uint8_t *>( rgba.data() );
	for ( int y = 0; y < height; ++y ) {
		const QRgb *src = reinterpret_cast<const QRgb *>( img.scanLine( y ) );
		for ( int x = 0; x < width; ++x, dst += 4 ) {
			dst[0] = qRed( src[x] );
			dst[1] = qGreen( src[x] );
			dst[2] = qBlue( src[x] );
			dst[3] = alpha ? qAlpha( src[x] ) : 255;
		}
	}

	const uint8_t *pixels = reinterpret_cast<const uint8_t *>( rgba.data() );
	uint8_t *output = 0;
	size_t size;
	if ( io->quality() >= 100 )
		size = WebPEncodeLosslessRGBA( pixels, width, height, width * 4, &output );
	else
		size = WebPEncodeRGBA( pixels, width, height, width * 4,
			io->quality() < 0 ? 75.0f : float( io->quality() ), &output );
	if ( size == 0 ) {
		kdDebug(399) << "kimgio_webp_write: cannot encode the image" << endl;
		WebPFree( output );
		return;
	}

	const bool ok = io->ioDevice()->writeBlock( reinterpret_cast<const char *>( output ), size )
		== Q_LONG( size );
	WebPFree( output );
	if ( ok )
		io->setStatus( 0 );
}
