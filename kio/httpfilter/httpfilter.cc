/*
   This file is part of the KDE libraries
   Copyright (c) 2002 Waldo Bastian <bastian@kde.org>
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <kio/global.h>

#include <klocale.h>

#include "httpfilter.h"

#include <zstd.h>
#ifdef HAVE_LIBBROTLIDEC
#include <brotli/decode.h>
#endif

HTTPFilterBase::HTTPFilterBase()
 : last(0)
{
}

HTTPFilterBase::~HTTPFilterBase()
{
   delete last;
}

void
HTTPFilterBase::chain(HTTPFilterBase *previous)
{
   last = previous;
   connect(last, SIGNAL(output(const QByteArray &)),
           this, SLOT(slotInput(const QByteArray &)));
}

HTTPFilterChain::HTTPFilterChain()
 : first(0)
{
}

void
HTTPFilterChain::addFilter(HTTPFilterBase *filter)
{
   if (!last)
   {
      first = filter;
   }
   else
   {
      disconnect(last, SIGNAL(output(const QByteArray &)), 0, 0);
      filter->chain(last);
   }
   last = filter;
   connect(filter, SIGNAL(output(const QByteArray &)),
           this, SIGNAL(output(const QByteArray &)));
   connect(filter, SIGNAL(error(int, const QString &)),
           this, SIGNAL(error(int, const QString &)));
}

void
HTTPFilterChain::slotInput(const QByteArray &d)
{
   if (first)
      first->slotInput(d);
   else
      emit output(d);      
}

HTTPFilterMD5::HTTPFilterMD5()
{
}

QString 
HTTPFilterMD5::md5()
{
   return QString::fromLatin1(context.base64Digest());
}

void 
HTTPFilterMD5::slotInput(const QByteArray &d)
{
   context.update(d);
   emit output(d);
}


HTTPFilterGZip::HTTPFilterGZip()
{
#ifdef DO_GZIP
  bHasHeader = false;
  bHasFinished = false;
  bPlainText = false;
  bEatTrailer = false;
  bEof = false;
  zstr.next_in = (Bytef *) Z_NULL;
  zstr.avail_in = 0;
  zstr.zalloc = Z_NULL;
  zstr.zfree = Z_NULL;
  zstr.opaque = Z_NULL;

  inflateInit2(&zstr, -MAX_WBITS);

  iTrailer = 8;
#endif
}

HTTPFilterGZip::~HTTPFilterGZip()
{
#ifdef DO_GZIP
  inflateEnd(&zstr);
#endif
  
}

/* The get_byte() and checkHeader() functions are modified version from */
/* the correpsonding functions that can be found in zlib, the following */
/* copyright notice applies to these functions:                         */

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.1.3, July 9th, 1998

  Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

int
HTTPFilterGZip::get_byte()
{
#ifdef DO_GZIP
    if (bEof) return EOF;
    if (zstr.avail_in == 0)
    {
        bEof = true;
        return EOF;
    }
    zstr.avail_in--;
    zstr.total_in++;
    return *(zstr.next_in)++;
#else 
    return 0;
#endif
}

#ifdef DO_GZIP

static int gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */
#endif

// 0 : ok
// 1 : not gzip
// 2 : no header
int
HTTPFilterGZip::checkHeader()
{
#ifdef DO_GZIP
    uInt len;
    int c;

    /* Check the gzip magic header */
    for (len = 0; len < 2; len++) {
	c = get_byte();
	if (c != gz_magic[len]) {
	    if (len != 0) 
	    {
	       zstr.avail_in++;
	       zstr.next_in--;
	    }
	    if (c != EOF) {
		zstr.avail_in++;
		zstr.next_in--;
		return 1;
	    }
	    return 2;
	}
    }

    int method = get_byte(); /* method byte */
    int flags = get_byte(); /* flags byte */

    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
	return bEof ? 2 : 1;
    }

    /* Discard time, xflags and OS code: */
    for (len = 0; len < 6; len++) (void)get_byte();

    if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
	len  =  (uInt)get_byte();
	len += ((uInt)get_byte())<<8;
	/* len is garbage if EOF but the loop below will quit anyway */
	while (len-- != 0 && get_byte() != EOF) ;
    }
    if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
	while ((c = get_byte()) != 0 && c != EOF) ;
    }
    if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
	while ((c = get_byte()) != 0 && c != EOF) ;
    }
    if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
	for (len = 0; len < 2; len++) (void)get_byte();
    }
    
    return bEof ? 2 : 0;
#else
    return 0;
#endif
} 

void 
HTTPFilterGZip::slotInput(const QByteArray &d)
{
#ifdef DO_GZIP
  if (bPlainText)
  {
     emit output(d);
     return;
  }
  if (d.size() == 0)
  {
     if (bEatTrailer)
        bHasFinished = true;
     if (!bHasFinished)
     {
        // Make sure we get the last bytes still in the pipe.
        // Needed with "deflate".
        QByteArray flush(4);
        flush.fill(0);
        slotInput(flush);
        if (!bHasFinished && !bHasHeader)
        {
           // Send as-is
           emit output(headerData);
           bHasFinished = true;
           // End of data
           emit output(QByteArray());
        }
     }
     if (!bHasFinished)
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Unexpected end of data, some information may be lost."));
     return;
  }
  if (bHasFinished)
     return;

  if (bEatTrailer)
  {
     iTrailer -= d.size();
     if (iTrailer <= 0)
     {
        bHasFinished = true;
        // End of data
        emit output(QByteArray());
     }
     return;
  }

  if (!bHasHeader)
  {
     bEof = false;

     // Add data to header.
     int orig_size = headerData.size();
     headerData.resize(orig_size+d.size());
     memcpy(headerData.data()+orig_size, d.data(), d.size());

     zstr.avail_in = headerData.size();
     zstr.next_in = (Bytef *) headerData.data();     

     int result = checkHeader();
     if (result == 1)
     {
        bPlainText = true;
        output(headerData);
        return;
     }

     if (result != 0)
        return; // next time better

     bHasHeader = true;
  }
  else
  {
     zstr.avail_in = d.size();
     zstr.next_in = (Bytef *) d.data();
  }

  while( zstr.avail_in )
  {
     char buf[8192];
     zstr.next_out = (Bytef *) buf;
     zstr.avail_out = 8192;
     int result = inflate( &zstr, Z_NO_FLUSH );
     if ((result != Z_OK) && (result != Z_STREAM_END))
     {
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
        break;
     }
     int bytesOut = 8192 - zstr.avail_out;
     if (bytesOut)
     {
        QByteArray d;
        d.setRawData( buf, bytesOut );
        emit output(d);
        d.resetRawData( buf, bytesOut );
     }
     if (result == Z_STREAM_END)
     {
        if (iTrailer)
        {
           bEatTrailer = true;
        }
        else
        {
           bHasFinished = true;
           // End of data
           emit output(QByteArray());
        }
        return;
     }
  }  
#endif
}

HTTPFilterDeflate::HTTPFilterDeflate()
{
#ifdef DO_GZIP
  bHasHeader = true;
  iTrailer = 0;
#endif
}

// RFC 9659: a "zstd" content encoding must not need a window of more
// than 8 MB, and a decoder may refuse a larger one.
static const int s_zstdWindowLogMax = 23;

HTTPFilterZstd::HTTPFilterZstd()
 : bFrameDone(false), bFinished(false), bGotInput(false)
{
  dctx = ZSTD_createDCtx();
  if (dctx && ZSTD_isError(ZSTD_DCtx_setParameter(dctx, ZSTD_d_windowLogMax, s_zstdWindowLogMax)))
  {
    ZSTD_freeDCtx(dctx);
    dctx = 0;
  }
}

HTTPFilterZstd::~HTTPFilterZstd()
{
  ZSTD_freeDCtx(dctx);
}

void
HTTPFilterZstd::slotInput(const QByteArray &d)
{
  if (bFinished)
     return;
  if (d.size() == 0)
  {
     // End of data; an empty body is fine
     bFinished = true;
     if (bFrameDone || !bGotInput)
        emit output(QByteArray());
     else
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Unexpected end of data, some information may be lost."));
     return;
  }
  bGotInput = true;
  if (!dctx)
  {
     bFinished = true;
     emit error( KIO::ERR_OUT_OF_MEMORY, QString::null );
     return;
  }

  ZSTD_inBuffer in = { d.data(), d.size(), 0 };
  while (true)
  {
     char buf[8192];
     ZSTD_outBuffer out = { buf, sizeof(buf), 0 };
     size_t ret = ZSTD_decompressStream(dctx, &out, &in);
     if (ZSTD_isError(ret))
     {
        bFinished = true;
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
        return;
     }
     if (out.pos)
     {
        QByteArray o;
        o.setRawData( buf, out.pos );
        emit output(o);
        o.resetRawData( buf, out.pos );
     }
     // ret == 0: a frame is complete; another one may follow
     bFrameDone = (ret == 0);
     // All input taken and the decoder did not fill the output: it
     // waits for more input
     if (in.pos == in.size && out.pos < out.size)
        break;
  }
}

HTTPFilterBrotli::HTTPFilterBrotli()
 : state(0), bDone(false), bFinished(false), bGotInput(false)
{
#ifdef HAVE_LIBBROTLIDEC
  // The default (no large window) limits the window to 16 MiB
  state = BrotliDecoderCreateInstance(0, 0, 0);
#endif
}

HTTPFilterBrotli::~HTTPFilterBrotli()
{
#ifdef HAVE_LIBBROTLIDEC
  if (state)
     BrotliDecoderDestroyInstance(state);
#endif
}

#ifndef HAVE_LIBBROTLIDEC
void
HTTPFilterBrotli::slotInput(const QByteArray &)
{
  // kio_http neither asks for nor accepts "br" without libbrotlidec
  if (bFinished)
     return;
  bFinished = true;
  emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
}
#else

void
HTTPFilterBrotli::slotInput(const QByteArray &d)
{
  if (bFinished)
     return;
  if (d.size() == 0)
  {
     // End of data; an empty body is fine
     bFinished = true;
     if (bDone || !bGotInput)
        emit output(QByteArray());
     else
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Unexpected end of data, some information may be lost."));
     return;
  }
  bGotInput = true;
  if (bDone)
  {
     // Data after the end of the Brotli stream
     bFinished = true;
     emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
     return;
  }
  if (!state)
  {
     bFinished = true;
     emit error( KIO::ERR_OUT_OF_MEMORY, QString::null );
     return;
  }

  size_t availIn = d.size();
  const uint8_t *nextIn = reinterpret_cast<const uint8_t *>(d.data());
  while (true)
  {
     uint8_t buf[8192];
     size_t availOut = sizeof(buf);
     uint8_t *nextOut = buf;
     BrotliDecoderResult result =
        BrotliDecoderDecompressStream(state, &availIn, &nextIn, &availOut, &nextOut, 0);
     if (result == BROTLI_DECODER_RESULT_ERROR)
     {
        bFinished = true;
        emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
        return;
     }
     const size_t bytesOut = sizeof(buf) - availOut;
     if (bytesOut)
     {
        QByteArray o;
        o.setRawData( reinterpret_cast<char *>(buf), bytesOut );
        emit output(o);
        o.resetRawData( reinterpret_cast<char *>(buf), bytesOut );
     }
     if (result == BROTLI_DECODER_RESULT_SUCCESS)
     {
        bDone = true;
        if (availIn)
        {
           bFinished = true;
           emit error( KIO::ERR_SLAVE_DEFINED, i18n("Receiving corrupt data."));
        }
        return;
     }
     if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
        return;
     // BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: go on
  }
}
#endif

#include "httpfilter.moc"
