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

#include "kxzfilter.h"

#include <string.h>
#include <lzma.h>

#include <kdebug.h>
#include <klibloader.h>

// The archives come from anywhere (web sites, the KDE Store, mail): refuse
// a stream that needs more memory to decompress.  xz -9 needs 65 MiB.
static const uint64_t s_decoderMemLimit = 128 << 20;
// What the xz tool uses by default
static const uint32_t s_preset = 6;

class KXzFilterFactory : public KLibFactory
{
public:
    KXzFilterFactory() : KLibFactory() {}
    virtual ~KXzFilterFactory() {}
    QObject *createObject( QObject *, const char *name, const char *, const QStringList & )
    {
        // name: the service (desktop file) the filter is made for
        return new KXzFilter( name && strcmp( name, "klzmafilter" ) == 0 );
    }
};

K_EXPORT_COMPONENT_FACTORY( kxzfilter, KXzFilterFactory )

class KXzFilter::KXzFilterPrivate
{
public:
    KXzFilterPrivate( bool lzma ) : lzma( lzma ), initialized( false ),
                                    initError( false ), endOfInput( false )
    {
        lzma_stream init = LZMA_STREAM_INIT;
        stream = init;
    }
    lzma_stream stream;
    bool lzma;          // write the legacy lzma format
    bool initialized;   // stream needs lzma_end()
    bool initError;
    bool endOfInput;
};

KXzFilter::KXzFilter( bool lzma )
    : m_mode( 0 )
{
    d = new KXzFilterPrivate( lzma );
}

KXzFilter::~KXzFilter()
{
    if ( d->initialized )
        lzma_end( &d->stream );
    delete d;
}

void KXzFilter::init( int mode )
{
    if ( d->initialized )
        lzma_end( &d->stream );
    // KFilterDev sets the output buffer before calling init()
    uint8_t *nextOut = d->stream.next_out;
    size_t availOut = d->stream.avail_out;
    lzma_stream init = LZMA_STREAM_INIT;
    d->stream = init;
    d->stream.next_out = nextOut;
    d->stream.avail_out = availOut;
    d->initialized = false;
    d->initError = false;
    d->endOfInput = false;
    m_mode = mode;

    lzma_ret ret;
    if ( mode == IO_ReadOnly )
    {
        // xz or lzma, several xz streams one after the other as the xz tool
        // reads them (LZMA_FINISH tells the end, see setEndOfInput())
        ret = lzma_auto_decoder( &d->stream, s_decoderMemLimit, LZMA_CONCATENATED );
    }
    else if ( mode == IO_WriteOnly )
    {
        if ( d->lzma )
        {
            lzma_options_lzma options;
            if ( lzma_lzma_preset( &options, s_preset ) )
                ret = LZMA_OPTIONS_ERROR;
            else
                ret = lzma_alone_encoder( &d->stream, &options );
        }
        else
            ret = lzma_easy_encoder( &d->stream, s_preset, LZMA_CHECK_CRC64 );
    }
    else
    {
        kdWarning(7005) << "Unsupported mode " << mode << ". Only IO_ReadOnly and IO_WriteOnly supported" << endl;
        d->initError = true;
        return;
    }

    if ( ret != LZMA_OK )
    {
        kdWarning(7005) << "KXzFilter: could not initialize liblzma (" << int(ret) << ")" << endl;
        d->initError = true;
        return;
    }
    d->initialized = true;
}

void KXzFilter::terminate()
{
    if ( d->initialized )
        lzma_end( &d->stream );
    d->initialized = false;
}

void KXzFilter::reset()
{
    init( m_mode );
}

void KXzFilter::setOutBuffer( char * data, uint maxlen )
{
    d->stream.next_out = reinterpret_cast<uint8_t *>( data );
    d->stream.avail_out = maxlen;
}

void KXzFilter::setInBuffer( const char * data, uint size )
{
    d->stream.next_in = reinterpret_cast<const uint8_t *>( data );
    d->stream.avail_in = size;
}

int KXzFilter::inBufferAvailable() const
{
    return d->stream.avail_in;
}

int KXzFilter::outBufferAvailable() const
{
    return d->stream.avail_out;
}

static const char *errorText( lzma_ret ret )
{
    switch ( ret ) {
    case LZMA_MEM_ERROR:         return "out of memory";
    case LZMA_MEMLIMIT_ERROR:    return "the data needs more than 128 MiB of memory to decompress";
    case LZMA_FORMAT_ERROR:      return "not xz or lzma data";
    case LZMA_OPTIONS_ERROR:     return "unsupported options";
    case LZMA_DATA_ERROR:        return "corrupt data";
    case LZMA_BUF_ERROR:         return "unexpected end of the data";
    case LZMA_UNSUPPORTED_CHECK: return "unsupported integrity check";
    default:                     return "error";
    }
}

KXzFilter::Result KXzFilter::uncompress()
{
    if ( !d->initialized || m_mode != IO_ReadOnly )
        return ERROR;

    lzma_ret ret = lzma_code( &d->stream, d->endOfInput ? LZMA_FINISH : LZMA_RUN );
    switch ( ret ) {
    case LZMA_OK:
        // With all of the input given, liblzma stops early only when the
        // output buffer is full; anything else means the data was cut off
        // (it would return LZMA_BUF_ERROR on the next call).
        if ( d->endOfInput && d->stream.avail_out > 0 )
        {
            kdWarning(7005) << "KXzFilter: " << errorText( LZMA_BUF_ERROR ) << endl;
            return ERROR;
        }
        return OK;
    case LZMA_STREAM_END:
        return END;
    case LZMA_BUF_ERROR:
        // No progress possible: harmless while there is input to come
        if ( !d->endOfInput )
            return OK;
        kdWarning(7005) << "KXzFilter: " << errorText( ret ) << endl;
        return ERROR;
    default:
        kdWarning(7005) << "KXzFilter: " << errorText( ret ) << endl;
        return ERROR;
    }
}

KXzFilter::Result KXzFilter::compress( bool finish )
{
    if ( !d->initialized || m_mode != IO_WriteOnly )
        return ERROR;

    lzma_ret ret = lzma_code( &d->stream, finish ? LZMA_FINISH : LZMA_RUN );
    switch ( ret ) {
    case LZMA_OK:
        return OK;
    case LZMA_STREAM_END:
        return END;
    default:
        kdWarning(7005) << "KXzFilter: " << errorText( ret ) << endl;
        return ERROR;
    }
}

void KXzFilter::virtual_hook( int id, void* data )
{
    if ( id == VirtualHookEndOfInput )
    {
        d->endOfInput = true;
        *static_cast<bool *>( data ) = true;
    }
    else
        KFilterBase::virtual_hook( id, data );
}
