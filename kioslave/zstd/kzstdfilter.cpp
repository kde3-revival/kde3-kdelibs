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


#include "kzstdfilter.h"

#include <zstd.h>

#include <kdebug.h>
#include <klibloader.h>

// The archives come from anywhere (web sites, the KDE Store, mail): refuse
// a frame with a larger window, it needs as much memory to decompress.
// 2^27 = 128 MiB is also the limit of the zstd tool without --memory.
static const int s_windowLogMax = 27;
// What the zstd tool uses by default
static const int s_level = 3;

class KZstdFilterFactory : public KLibFactory
{
public:
    KZstdFilterFactory() : KLibFactory() {}
    virtual ~KZstdFilterFactory() {}
    QObject *createObject( QObject *, const char *, const char *, const QStringList & )
    {
        return new KZstdFilter;
    }
};

K_EXPORT_COMPONENT_FACTORY( kzstdfilter, KZstdFilterFactory )

class KZstdFilter::KZstdFilterPrivate
{
public:
    KZstdFilterPrivate() : dctx( 0 ), cctx( 0 ), frameDone( false ), endOfInput( false )
    {
        in.src = 0; in.size = 0; in.pos = 0;
        out.dst = 0; out.size = 0; out.pos = 0;
    }
    void free()
    {
        ZSTD_freeDCtx( dctx );
        ZSTD_freeCCtx( cctx );
        dctx = 0;
        cctx = 0;
    }
    ZSTD_DCtx *dctx;
    ZSTD_CCtx *cctx;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
    bool frameDone;     // a frame ended and no other one has started yet
    bool endOfInput;
};

KZstdFilter::KZstdFilter()
    : m_mode( 0 )
{
    d = new KZstdFilterPrivate;
}

KZstdFilter::~KZstdFilter()
{
    d->free();
    delete d;
}

void KZstdFilter::init( int mode )
{
    d->free();
    d->in.src = 0; d->in.size = 0; d->in.pos = 0;
    d->frameDone = false;
    d->endOfInput = false;
    m_mode = mode;

    if ( mode == IO_ReadOnly )
    {
        d->dctx = ZSTD_createDCtx();
        if ( d->dctx && ZSTD_isError( ZSTD_DCtx_setParameter( d->dctx, ZSTD_d_windowLogMax, s_windowLogMax ) ) )
            d->free();
    }
    else if ( mode == IO_WriteOnly )
    {
        d->cctx = ZSTD_createCCtx();
        if ( d->cctx && ( ZSTD_isError( ZSTD_CCtx_setParameter( d->cctx, ZSTD_c_compressionLevel, s_level ) ) ||
                          ZSTD_isError( ZSTD_CCtx_setParameter( d->cctx, ZSTD_c_checksumFlag, 1 ) ) ) )
            d->free();
    }
    else
    {
        kdWarning(7005) << "Unsupported mode " << mode << ". Only IO_ReadOnly and IO_WriteOnly supported" << endl;
        return;
    }

    if ( !d->dctx && !d->cctx )
        kdWarning(7005) << "KZstdFilter: could not initialize libzstd" << endl;
}

void KZstdFilter::terminate()
{
    d->free();
}

void KZstdFilter::reset()
{
    init( m_mode );
}

void KZstdFilter::setOutBuffer( char * data, uint maxlen )
{
    d->out.dst = data;
    d->out.size = maxlen;
    d->out.pos = 0;
}

void KZstdFilter::setInBuffer( const char * data, uint size )
{
    d->in.src = data;
    d->in.size = size;
    d->in.pos = 0;
}

int KZstdFilter::inBufferAvailable() const
{
    return d->in.size - d->in.pos;
}

int KZstdFilter::outBufferAvailable() const
{
    return d->out.size - d->out.pos;
}

KZstdFilter::Result KZstdFilter::uncompress()
{
    if ( !d->dctx )
        return ERROR;

    // Between two frames (or after the last one)
    if ( d->frameDone && d->in.pos == d->in.size )
        return d->endOfInput ? END : OK;

    size_t ret = ZSTD_decompressStream( d->dctx, &d->out, &d->in );
    if ( ZSTD_isError( ret ) )
    {
        if ( ZSTD_getErrorCode( ret ) == ZSTD_error_frameParameter_windowTooLarge )
            kdWarning(7005) << "KZstdFilter: the data needs more than 128 MiB of memory to decompress" << endl;
        else
            kdWarning(7005) << "KZstdFilter: " << ZSTD_getErrorName( ret ) << endl;
        return ERROR;
    }

    if ( ret == 0 )
    {
        // A frame is complete and all of it given out. Another one may
        // follow (zstd reads concatenated frames as one stream).
        d->frameDone = true;
        return ( d->endOfInput && d->in.pos == d->in.size ) ? END : OK;
    }

    d->frameDone = false;
    // With all of the input given and room left for the output, the
    // frame can only be incomplete: the data was cut off.
    if ( d->endOfInput && d->out.pos < d->out.size )
    {
        kdWarning(7005) << "KZstdFilter: unexpected end of the data" << endl;
        return ERROR;
    }
    return OK;
}

KZstdFilter::Result KZstdFilter::compress( bool finish )
{
    if ( !d->cctx )
        return ERROR;

    size_t ret = ZSTD_compressStream2( d->cctx, &d->out, &d->in,
                                       finish ? ZSTD_e_end : ZSTD_e_continue );
    if ( ZSTD_isError( ret ) )
    {
        kdWarning(7005) << "KZstdFilter: " << ZSTD_getErrorName( ret ) << endl;
        return ERROR;
    }
    return ( finish && ret == 0 ) ? END : OK;
}

void KZstdFilter::virtual_hook( int id, void* data )
{
    if ( id == VirtualHookEndOfInput )
    {
        d->endOfInput = true;
        *static_cast<bool *>( data ) = true;
    }
    else
        KFilterBase::virtual_hook( id, data );
}
