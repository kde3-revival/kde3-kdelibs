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


#ifndef __kzstdfilter__h
#define __kzstdfilter__h

#include "kfilterbase.h"

/**
 * Compression filter for Zstandard (application/zstd), with libzstd.
 */
class KZstdFilter : public KFilterBase
{
public:
    KZstdFilter();
    virtual ~KZstdFilter();

    virtual void init( int mode );
    virtual int mode() const { return m_mode; }
    virtual void terminate();
    virtual void reset();
    virtual bool readHeader() { return true; } // libzstd reads the headers
    virtual bool writeHeader( const QCString & ) { return true; }
    virtual void setOutBuffer( char * data, uint maxlen );
    virtual void setInBuffer( const char * data, uint size );
    virtual int  inBufferAvailable() const;
    virtual int  outBufferAvailable() const;
    virtual Result uncompress();
    virtual Result compress( bool finish );

protected:
    virtual void virtual_hook( int id, void* data );

private:
    class KZstdFilterPrivate;
    KZstdFilterPrivate *d;
    int m_mode;
};

#endif
