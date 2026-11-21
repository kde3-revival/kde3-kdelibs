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
#ifndef KNEWSTUFF_WALLPAPERSOURCES_H
#define KNEWSTUFF_WALLPAPERSOURCES_H

// Private to libknewstuff, not installed.
//
// Wallpaper sources besides the KDE Store, for the "Get New Wallpapers"
// dialog of the background module and for the picture of the day of the
// desktop. An application lists them in the [KNewStuff] group of its
// configuration, after the OCS provider (see ocs.h):
//
//   WallpaperSources=wallhaven,wikimedia,bing,apod,unsplash
//
//   wallhaven  Wallhaven (https://wallhaven.cc/help/api): toplist, latest,
//              search; safe for work only (purity 100), 45 requests per
//              minute: the requests are spaced
//   wikimedia  Wikimedia Commons: Picture of the Day, Featured pictures;
//              author and license from the file's metadata
//   bing       Bing's image of the day (HPImageArchive.aspx), in UHD
//   apod       NASA's Astronomy Picture of the Day: the APOD feed of
//              science.nasa.gov for the last 30 days, older ones from the
//              APOD API of api.nasa.gov (DEMO_KEY or the user's key);
//              videos are left out
//   unsplash   Unsplash, only with the user's access key
//
// The data of the sources is untrusted: the responses are size limited, the
// JSON reader bounded, HTML fields reduced to plain text, and only https
// links into the source's own domains are followed. The requests are
// anonymous: no cookies, a generic user agent, no language header.

#include <qmap.h>
#include <qobject.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qvariant.h>

#include <kurl.h>

#include "ocs.h"

class QTimer;

namespace KIO { class Job; class TransferJob; }

namespace KNS {

class Provider;

// Provider side table (provider.cpp)
bool sourceIsProvider( const Provider *p );
QString sourceId( const Provider *p );
void sourceSetProvider( Provider *p, const QString &id );

class WallpaperSource : public QObject
{
    Q_OBJECT
  public:
    /** All known sources, in the order of the dialog. */
    static QStringList ids();
    static bool isKnown( const QString &id );
    static QString displayName( const QString &id );
    static QString iconName( const QString &id );
    /** Text about the source shown under its list, may be empty. */
    static QString note( const QString &id );

    /** Whether the source uses an API key the user can set. */
    static bool usesApiKey( const QString &id );
    /** The user's key, or QString::null. */
    static QString apiKey( const QString &id );
    /**
     * Stores the user's key (an empty one removes it) in the file
     * knewstuffwallpapersrc, readable by the user only. Returns false if
     * the key has characters no key has.
     */
    static bool setApiKey( const QString &id, const QString &key );

    /** The largest download accepted. */
    static const Q_ULLONG maxDownloadSize;

    WallpaperSource( const QString &id, QObject *parent );
    ~WallpaperSource();

    QString id() const { return mId; }

    struct Tab
    {
      QString name;
      bool search;      // the list shows the results of a search
    };
    QValueList<Tab> tabs() const;

    /** Words to search for in the search tab @p tab. */
    void setSearchText( int tab, const QString &text );
    QString searchText( int tab ) const;

    /** Lists one page (0-based) of tab @p tab; a listing of the same tab
        in progress is cancelled. Newest first for the pictures of the day. */
    void listContents( int tab, int page );
    /** Finds the link of the picture to install of item @p id. */
    void resolveDownload( const QString &id, int number );
    /** Loads a preview picture; @p key is returned with the image. The
        same picture asked for several times is loaded once. */
    void fetchPreview( const KURL &url, int key );
    /** Cancels the resolution of download links in progress: no signal
        is emitted for them. */
    void cancelDownload();

    /** Whether @p url may be fetched: https in the source's domains. */
    bool isAllowedUrl( const KURL &url ) const;

    /** Sets up a job like the source's own (anonymous, no cookies...). */
    static void setupJob( KIO::TransferJob *job );

  signals:
    // as those of OcsClient, so that the download dialog takes both
    void contentsListed( int sortMode, int page, const QValueList<KNS::OcsContent> &items, int total, bool more );
    void listingFailed( int sortMode, const QString &message );
    void downloadResolved( const QString &id, int number, const KURL &link );
    void downloadFailed( const QString &id, int number, const QString &message );
    void previewLoaded( int key, const QImage &image );

  private slots:
    void slotData( KIO::Job *job, const QByteArray &data );
    void slotResult( KIO::Job *job );
    void slotRedirection( KIO::Job *job, const KURL &url );
    void slotStartQueued();
    void slotResolved();
    void slotStartPreviews();
    void slotWatchdog();

  private:
    enum Kind { List, PotdFiles, Resolve, Preview };
    struct Request
    {
      Kind kind;
      int tab;
      int page;
      QString id;
      KURL url;
      QStringList dates;      // PotdFiles: the dates of the files asked for
      QMap<QString, QString> potd;  // PotdFiles: file title -> date
      QByteArray data;
      int key;
      bool apodFeed;
      int active;             // when data last came (mClock)
    };

    void queue( const Request &r );
    void start( const Request &r );
    void failed( const Request &r, const QString &message );
    void previewDone( const Request &r, const QImage &image );
    QString httpError( KIO::Job *job ) const;
    void finished( Request &r );

    bool parseWallhaven( const Request &r, const QVariant &json, OcsContentList &items, int *total, bool *more, QString *error );
    bool parseWikimediaFiles( const Request &r, const QVariant &json, OcsContentList &items, bool *more, QString *error );
    bool parseBing( const QVariant &json, OcsContentList &items, QString *error );
    bool parseApodApi( const QVariant &json, OcsContentList &items, QString *error );
    bool parseApodFeed( const QByteArray &data, OcsContentList &items, QString *error );
    bool parseUnsplash( const QVariant &json, OcsContentList &items, QString *error );
    void potdTemplates( const Request &r, const QVariant &json );

    KURL apiUrl( int tab, int page ) const;
    bool addItem( OcsContent &c, OcsContentList &items );

    QString mId;
    QStringList mDomains;           // the source's own domains
    QMap<KIO::Job *, Request> mRequests;
    QValueList<Request> mQueue;     // waiting for the request spacing
    QTimer *mQueueTimer;
    QValueList<KURL> mPreviewQueue;
    QMap<QString, QValueList<int> > mPreviewKeys;  // queued or loading
    int mPreviewsRunning;
    QTimer *mWatchdog;
    QMap<int, QString> mSearch;     // tab -> words
    QMap<QString, OcsContent> mItems;  // by id, for the download
    QMap<int, QString> mContinue;   // Wikimedia featured: page -> continuation
    QDate mApodOldest;              // oldest date the APOD feed listed
    QValueList<QPair<QString, KURL> > mResolved;
    long mLastRequest;              // msecs since mClock started
    QTime mClock;
};

}

#endif
