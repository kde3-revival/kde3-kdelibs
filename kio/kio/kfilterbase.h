/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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

#ifndef __kfilterbase__h
#define __kfilterbase__h

#include <qobject.h>
#include <qstring.h>

#include <kdelibs_export.h>

#ifdef Q_WS_WIN
#undef ERROR //avoid conflicts
#endif

class QIODevice;

/**
 * This is the base class for compression filters
 * such as gzip, bzip2, xz, zstd and lz4. It's pretty much internal.
 * Don't use directly, use KFilterDev instead.
 */
class KIO_EXPORT KFilterBase : public QObject // needs to inherit QObject for KLibFactory::create
{
    Q_OBJECT
public:
    KFilterBase();
    virtual ~KFilterBase();

    /**
     * Sets the device on which the filter will work
     * @param dev the device on which the filter will work
     * @param autodelete if true, @p dev is deleted when the filter is deleted
     */
    void setDevice( QIODevice * dev, bool autodelete = false );
    // Note that this isn't in the constructor, because of KLibFactory::create,
    // but it should be called before using the filterbase !

    /**
     * Returns the device on which the filter will work.
     * @returns the device on which the filter will work
     */
    QIODevice * device() { return m_dev; }
    /** \internal */
    virtual void init( int mode ) = 0;
    /** \internal */
    virtual int mode() const = 0;
    /** \internal */
    virtual void terminate() {}
    /** \internal */
    virtual void reset() {}
    /** \internal */
    virtual bool readHeader() = 0;
    /** \internal */
    virtual bool writeHeader( const QCString & filename ) = 0;
    /** \internal */
    virtual void setOutBuffer( char * data, uint maxlen ) = 0;
    /** \internal */
    virtual void setInBuffer( const char * data, uint size ) = 0;
    /** \internal */
    virtual bool inBufferEmpty() const { return inBufferAvailable() == 0; }
    /** \internal */
    virtual int  inBufferAvailable() const = 0;
    /** \internal */
    virtual bool outBufferFull() const { return outBufferAvailable() == 0; }
    /** \internal */
    virtual int  outBufferAvailable() const = 0;

    /** \internal */
    enum Result { OK, END, ERROR };
    /** \internal */
    virtual Result uncompress() = 0;
    /** \internal */
    virtual Result compress( bool finish ) = 0;

    /**
     * \internal
     * Tells the filter that the underlying device has no more data after
     * what was given with setInBuffer(). A filter that supports this then
     * finishes the stream: uncompress() returns END once the compressed
     * data was complete and all of it was given out, OK as long as there
     * is uncompressed data left that did not fit into the output buffer,
     * and ERROR when the compressed data was cut off. reset() and init()
     * forget the end of the input.
     *
     * @return true if the filter supports this (xz, lzma, zstd and lz4),
     * false if it does not (gzip and bzip2: they find the end of their
     * data themselves and do not tell when it is missing)
     */
    bool setEndOfInput();

    /**
     * Call this to create the appropriate filter for the file
     * named @p fileName.
     * @param fileName the name of the file to filter
     * @return the filter for the @p fileName, or 0 if not found
     */
    static KFilterBase * findFilterByFileName( const QString & fileName );

    /**
     * Call this to create the appropriate filter for the mimetype
     * @p mimeType. For instance application/x-gzip.
     * @param mimeType the mime type of the file to filter
     * @return the filter for the @p mimeType, or 0 if not found
     */
    static KFilterBase * findFilterByMimeType( const QString & mimeType );

protected:
    QIODevice * m_dev;
    bool m_bAutoDel;
protected:
    /**
     * \internal
     * The id of virtual_hook() that setEndOfInput() calls; @p data is a
     * bool * that a filter supporting the end of input sets to true.
     */
    enum { VirtualHookEndOfInput = 0x4b464531 };
    virtual void virtual_hook( int id, void* data );
private:
    class KFilterBasePrivate;
};

#endif
