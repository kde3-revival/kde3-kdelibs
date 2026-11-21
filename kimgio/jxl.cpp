/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// JPEG XL images with libjxl: the image, or the first frame of an
// animation, with alpha, turned as its orientation says, and of 8 bits
// per channel or more, the latter reduced to 8 bits.

#include "jxl.h"

#include <stdint.h>

#include <qimage.h>
#include <qiodevice.h>

#include <kdebug.h>
#include <kglobal.h>

#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/resizable_parallel_runner.h>

// The largest image read, in pixels (16384 x 16384: 1 GB as a QImage).
static const uint64_t maxPixels = uint64_t( 16384 ) * 16384;

// The decoder and its thread pool of one read.
struct JxlRead
{
	JxlDecoder *decoder;
	void *runner;

	JxlRead() : decoder( 0 ), runner( 0 ) {}
	~JxlRead()
	{
		if ( decoder )
			JxlDecoderDestroy( decoder );
		if ( runner )
			JxlResizableParallelRunnerDestroy( runner );
	}
};

KDE_EXPORT void kimgio_jxl_read( QImageIO *io )
{
	io->setImage( QImage() );
	io->setStatus( -1 );

	const QByteArray data = io->ioDevice()->readAll();

	JxlRead r;
	r.decoder = JxlDecoderCreate( 0 );
	r.runner = JxlResizableParallelRunnerCreate( 0 );
	if ( !r.decoder || !r.runner )
		return;
	JxlDecoder *dec = r.decoder;
	// libjxl turns the image as its orientation says (the default) and
	// gives colours that are not premultiplied by alpha, as Qt expects.
	if ( JxlDecoderSubscribeEvents( dec, JXL_DEC_BASIC_INFO
			| JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE ) != JXL_DEC_SUCCESS
			|| JxlDecoderSetParallelRunner( dec, JxlResizableParallelRunner,
				r.runner ) != JXL_DEC_SUCCESS
			|| JxlDecoderSetUnpremultiplyAlpha( dec, JXL_TRUE ) != JXL_DEC_SUCCESS
			|| JxlDecoderSetInput( dec, reinterpret_cast<const uint8_t *>( data.data() ),
				data.size() ) != JXL_DEC_SUCCESS )
		return;
	JxlDecoderCloseInput( dec );

	// Red, green, blue and alpha bytes; a grey image is given as RGB too.
	const JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
	JxlBasicInfo info;
	bool haveInfo = false;
	QImage img;

	for ( ;; ) {
		const JxlDecoderStatus status = JxlDecoderProcessInput( dec );
		if ( status == JXL_DEC_BASIC_INFO ) {
			if ( JxlDecoderGetBasicInfo( dec, &info ) != JXL_DEC_SUCCESS )
				break;
			if ( info.xsize == 0 || info.ysize == 0
					|| uint64_t( info.xsize ) * info.ysize > maxPixels ) {
				kdDebug(399) << "kimgio_jxl_read: unsupported image size "
					<< info.xsize << "x" << info.ysize << endl;
				return;
			}
			JxlResizableParallelRunnerSetThreads( r.runner,
				JxlResizableParallelRunnerSuggestThreads( info.xsize, info.ysize ) );
			haveInfo = true;
		} else if ( status == JXL_DEC_COLOR_ENCODING ) {
			// An image in libjxl's own colour space (XYB, lossy) is
			// given in sRGB; one in another colour space is given as it
			// is, Qt 3 does not manage colours.
			JxlColorEncoding srgb;
			JxlColorEncodingSetToSRGB( &srgb, JXL_FALSE );
			JxlDecoderSetPreferredColorProfile( dec, &srgb );
		} else if ( status == JXL_DEC_NEED_IMAGE_OUT_BUFFER ) {
			if ( !haveInfo || !img.isNull() )
				break;
			// The orientations 5 to 8 exchange the width and the height.
			const bool transposed = info.orientation >= JXL_ORIENT_TRANSPOSE;
			const int width = transposed ? info.ysize : info.xsize;
			const int height = transposed ? info.xsize : info.ysize;
			size_t size = 0;
			if ( JxlDecoderImageOutBufferSize( dec, &format, &size ) != JXL_DEC_SUCCESS
					|| size != size_t( width ) * size_t( height ) * 4 )
				break;
			// The rows of a 32 bit QImage have no padding and follow
			// each other: libjxl writes into the image, whose pixels are
			// converted below, in place.
			if ( !img.create( width, height, 32 )
					|| size_t( img.numBytes() ) != size
					|| img.bytesPerLine() != width * 4 ) {
				kdDebug(399) << "kimgio_jxl_read: cannot allocate the image" << endl;
				break;
			}
			if ( JxlDecoderSetImageOutBuffer( dec, &format, img.bits(), size )
					!= JXL_DEC_SUCCESS )
				break;
		} else if ( status == JXL_DEC_FULL_IMAGE ) {
			// The first frame of an animation: the rest is not read.
			if ( img.isNull() )
				break;
			for ( int y = 0; y < img.height(); ++y ) {
				QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( y ) );
				const uint8_t *src = reinterpret_cast<const uint8_t *>( line );
				for ( int x = 0; x < img.width(); ++x, src += 4 )
					line[x] = qRgba( src[0], src[1], src[2], src[3] );
			}
			img.setAlphaBuffer( info.alpha_bits != 0 );
			io->setImage( img );
			io->setStatus( 0 );
			return;
		} else {
			// JXL_DEC_ERROR, JXL_DEC_NEED_MORE_INPUT (a truncated file,
			// the input is complete) or JXL_DEC_SUCCESS without an image
			break;
		}
	}
	kdDebug(399) << "kimgio_jxl_read: cannot decode the image" << endl;
}
