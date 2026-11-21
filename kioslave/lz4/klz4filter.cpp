/* This file is part of the KDE libraries

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


#include "klz4filter.h"

#include <string.h>
#include <lz4frame.h>

#include <qmemarray.h>

#include <kdebug.h>
#include <klibloader.h>

// liblz4 compresses the input given in pieces of at most this size, into
// a buffer of LZ4F_compressBound() of it (its output can't go straight
// into the output buffer, which may be too small).
static const size_t s_chunkSize = 64 * 1024;

class KLz4FilterFactory : public KLibFactory
{
public:
    KLz4FilterFactory() : KLibFactory() {}
    virtual ~KLz4FilterFactory() {}
    QObject *createObject( QObject *, const char *, const char *, const QStringList & )
    {
        return new KLz4Filter;
    }
};

K_EXPORT_COMPONENT_FACTORY( klz4filter, KLz4FilterFactory )

class KLz4Filter::KLz4FilterPrivate
{
public:
    KLz4FilterPrivate() : dctx( 0 ), cctx( 0 ), in( 0 ), inAvail( 0 ), out( 0 ), outAvail( 0 ),
                          pendingPos( 0 ), pendingLen( 0 ), headerWritten( false ),
                          ended( false ), frameDone( false ), endOfInput( false ) {}
    void free()
    {
        if ( dctx )
            LZ4F_freeDecompressionContext( dctx );
        if ( cctx )
            LZ4F_freeCompressionContext( cctx );
        dctx = 0;
        cctx = 0;
    }
    LZ4F_dctx *dctx;
    LZ4F_cctx *cctx;
    LZ4F_preferences_t prefs;
    const char *in;
    size_t inAvail;
    char *out;
    size_t outAvail;
    // compressing: output of liblz4 not yet copied into the output buffer
    QByteArray pending;
    size_t pendingPos;
    size_t pendingLen;
    bool headerWritten;
    bool ended;         // compressing: the frame end was made
    bool frameDone;     // uncompressing: a frame ended, no other one started yet
    bool endOfInput;
};

KLz4Filter::KLz4Filter()
    : m_mode( 0 )
{
    d = new KLz4FilterPrivate;
}

KLz4Filter::~KLz4Filter()
{
    d->free();
    delete d;
}

void KLz4Filter::init( int mode )
{
    d->free();
    d->in = 0;
    d->inAvail = 0;
    d->pendingPos = d->pendingLen = 0;
    d->headerWritten = d->ended = d->frameDone = d->endOfInput = false;
    m_mode = mode;

    if ( mode == IO_ReadOnly )
    {
        if ( LZ4F_isError( LZ4F_createDecompressionContext( &d->dctx, LZ4F_VERSION ) ) )
            d->dctx = 0;
    }
    else if ( mode == IO_WriteOnly )
    {
        memset( &d->prefs, 0, sizeof( d->prefs ) );
        d->prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
        if ( LZ4F_isError( LZ4F_createCompressionContext( &d->cctx, LZ4F_VERSION ) ) )
            d->cctx = 0;
        else
        {
            // big enough for the header, one chunk and the end of the frame
            d->pending.resize( LZ4F_compressBound( s_chunkSize, &d->prefs ) + LZ4F_HEADER_SIZE_MAX );
        }
    }
    else
    {
        kdWarning(7005) << "Unsupported mode " << mode << ". Only IO_ReadOnly and IO_WriteOnly supported" << endl;
        return;
    }

    if ( !d->dctx && !d->cctx )
        kdWarning(7005) << "KLz4Filter: could not initialize liblz4" << endl;
}

void KLz4Filter::terminate()
{
    d->free();
    d->pending.resize( 0 );
}

void KLz4Filter::reset()
{
    init( m_mode );
}

void KLz4Filter::setOutBuffer( char * data, uint maxlen )
{
    d->out = data;
    d->outAvail = maxlen;
}

void KLz4Filter::setInBuffer( const char * data, uint size )
{
    d->in = data;
    d->inAvail = size;
}

int KLz4Filter::inBufferAvailable() const
{
    return d->inAvail;
}

int KLz4Filter::outBufferAvailable() const
{
    return d->outAvail;
}

KLz4Filter::Result KLz4Filter::uncompress()
{
    if ( !d->dctx )
        return ERROR;

    // Between two frames (or after the last one)
    if ( d->frameDone && d->inAvail == 0 )
        return d->endOfInput ? END : OK;

    size_t outSize = d->outAvail;
    size_t inSize = d->inAvail;
    size_t ret = LZ4F_decompress( d->dctx, d->out, &outSize, d->in, &inSize, 0 );
    if ( LZ4F_isError( ret ) )
    {
        kdWarning(7005) << "KLz4Filter: " << LZ4F_getErrorName( ret ) << endl;
        return ERROR;
    }
    d->in += inSize;
    d->inAvail -= inSize;
    d->out += outSize;
    d->outAvail -= outSize;

    if ( ret == 0 )
    {
        // A frame is complete and all of it given out. Another one may
        // follow (lz4 reads concatenated frames as one stream).
        d->frameDone = true;
        return ( d->endOfInput && d->inAvail == 0 ) ? END : OK;
    }

    d->frameDone = false;
    // With all of the input given and room left for the output, the
    // frame can only be incomplete: the data was cut off.
    if ( d->endOfInput && d->outAvail > 0 )
    {
        kdWarning(7005) << "KLz4Filter: unexpected end of the data" << endl;
        return ERROR;
    }
    return OK;
}

KLz4Filter::Result KLz4Filter::compress( bool finish )
{
    if ( !d->cctx )
        return ERROR;

    const size_t capacity = d->pending.size();
    while ( true )
    {
        // Give out what liblz4 made before
        if ( d->pendingLen > 0 )
        {
            size_t n = QMIN( d->pendingLen, d->outAvail );
            memcpy( d->out, d->pending.data() + d->pendingPos, n );
            d->out += n;
            d->outAvail -= n;
            d->pendingPos += n;
            d->pendingLen -= n;
            if ( d->pendingLen > 0 )
                return OK; // output buffer full
        }
        if ( d->ended )
            return END;

        size_t ret;
        if ( !d->headerWritten )
        {
            ret = LZ4F_compressBegin( d->cctx, d->pending.data(), capacity, &d->prefs );
            d->headerWritten = true;
        }
        else if ( d->inAvail > 0 )
        {
            size_t n = QMIN( d->inAvail, s_chunkSize );
            ret = LZ4F_compressUpdate( d->cctx, d->pending.data(), capacity, d->in, n, 0 );
            if ( !LZ4F_isError( ret ) )
            {
                d->in += n;
                d->inAvail -= n;
            }
        }
        else if ( finish )
        {
            ret = LZ4F_compressEnd( d->cctx, d->pending.data(), capacity, 0 );
            d->ended = true;
        }
        else
            return OK; // all input taken

        if ( LZ4F_isError( ret ) )
        {
            kdWarning(7005) << "KLz4Filter: " << LZ4F_getErrorName( ret ) << endl;
            return ERROR;
        }
        d->pendingPos = 0;
        d->pendingLen = ret;
    }
}

void KLz4Filter::virtual_hook( int id, void* data )
{
    if ( id == VirtualHookEndOfInput )
    {
        d->endOfInput = true;
        *static_cast<bool *>( data ) = true;
    }
    else
        KFilterBase::virtual_hook( id, data );
}
