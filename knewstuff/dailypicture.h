/*
    This file is part of KNewStuff.
    Copyright (c) 2026 The KDE 3 revival authors

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef KNEWSTUFF_DAILYPICTURE_H
#define KNEWSTUFF_DAILYPICTURE_H

#include <qobject.h>
#include <qstringlist.h>

#include <kdelibs_export.h>

class KURL;

namespace KIO { class Job; }

namespace KNS {

struct OcsContent;

/**
 * Fetches the picture of the day of a wallpaper source (Bing, Wikimedia
 * Commons, NASA APOD) into a folder, in the background with KIO; used by
 * the desktop for its "picture of the day" wallpaper.
 *
 * The pictures are named after their day, "YYYY-MM-DD.<ext>", so that the
 * newest one sorts last, each with a .desktop file that holds its title,
 * author and license (the [Wallpaper] group of the background module).
 * Only the source's own servers are contacted, anonymously; the downloads
 * are checked like those of the "Get New Wallpapers" dialog.
 *
 * @since 3.5.11
 */
class KDE_EXPORT DailyPicture : public QObject
{
    Q_OBJECT
  public:
    DailyPicture( QObject *parent = 0, const char *name = 0 );
    ~DailyPicture();

    /** The sources that have a picture of the day: "bing", "wikimedia",
        "apod". */
    static QStringList sources();
    /** The translated name of @p source. */
    static QString sourceName( const QString &source );
    /**
     * The newest picture of the day in @p directory (by name), or
     * QString::null if there is none.
     */
    static QString latestPicture( const QString &directory );

    /**
     * Fetches the newest picture of @p source into @p directory (created
     * if needed), unless it is already there, and keeps the @p keep newest
     * pictures of the folder. Emits finished() when done; a fetch in
     * progress is cancelled. Returns false if @p source is unknown.
     */
    bool fetch( const QString &source, const QString &directory, int keep = 7 );

    /** Cancels the fetch in progress (finished() is not emitted). */
    void cancel();
    bool isRunning() const;

  signals:
    /**
     * The fetch is over. @p picture is the newest picture of the source in
     * the folder (QString::null on failure), @p isNew whether it was
     * downloaded now, @p date its day (yyyy-MM-dd), @p error a message if
     * the fetch failed (for example when offline).
     */
    void finished( const QString &picture, bool isNew, const QString &date, const QString &error );

  private slots:
    void slotListed( int tab, int page, const QValueList<KNS::OcsContent> &items, int total, bool more );
    void slotListingFailed( int tab, const QString &message );
    void slotResolved( const QString &id, int number, const KURL &link );
    void slotResolveFailed( const QString &id, int number, const QString &message );
    void slotData( KIO::Job *job, const QByteArray &data );
    void slotRedirection( KIO::Job *job, const KURL &url );
    void slotResult( KIO::Job *job );

  private:
    void done( const QString &picture, bool isNew, const QString &error );
    void prune();

    class Private;
    Private *d;
};

}

#endif
