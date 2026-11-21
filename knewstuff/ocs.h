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
#ifndef KNEWSTUFF_OCS_H
#define KNEWSTUFF_OCS_H

// Private to libknewstuff, not installed.
//
// Access to an Open Collaboration Services (OCS) content provider such as
// the KDE Store (https://api.kde-look.org/ocs/v1/), as used by KNewStuff 3.
// An application selects it in the [KNewStuff] group of its configuration:
//
//   OcsUrl=https://api.kde-look.org/ocs/v1/
//   OcsCategories=296,297,298      (category ids, see content/categories)
//   OcsName=KDE Store              (optional, name of the provider page)
//   OcsWebsite=https://store.kde.org/  (optional, shown for uploads)
//
// The content is listed anonymously; uploading needs an account on the
// store and is not supported.

#include <qcstring.h>
#include <qdatetime.h>
#include <qdom.h>
#include <qimage.h>
#include <qmap.h>
#include <qobject.h>
#include <qpair.h>
#include <qstring.h>
#include <qvaluelist.h>

#include <kurl.h>

class QWidget;

namespace KIO { class Job; class TransferJob; }

namespace KNS {

class Provider;

/** One downloadable file of an OCS content item. */
struct OcsDownload
{
  int number;          // index of the file in the item (downloadlinkN)
  QString name;        // file name proposed by the server
  QString mimeType;    // from the download tags, may be empty
  int sizeKiB;         // size announced by the server, 0 if unknown
};

/** One OCS content item, or a picture of a wallpaper source. */
struct OcsContent
{
  OcsContent() : score( 0 ), downloads( 0 ), width( 0 ), height( 0 ) {}
  QString id;          // digits only (OCS), letters, digits, '-' and '_' (sources)
  QString name;
  QString version;
  QString author;
  QString summary;
  QString license;
  QDateTime changed;
  int score;           // 0..100
  int downloads;
  KURL preview;        // small preview picture
  KURL detailPage;
  QValueList<OcsDownload> files;
  // wallpaper sources (wallpapersources.h)
  QString licenseUrl;
  QString source;      // the name of the source, for the attribution
  int width;           // of the picture, 0 if unknown
  int height;
  KURL download;       // the picture, or where to ask for it
};

typedef QValueList<OcsContent> OcsContentList;

// Provider side table (provider.cpp)
bool ocsIsProvider( const Provider *p );
KURL ocsBaseUrl( const Provider *p );
QString ocsCategories( const Provider *p );
void ocsSetProvider( Provider *p, const KURL &base, const QString &categories );

/**
 * Parses an untrusted XML document: refused if it has a DTD or other
 * markup declarations, or elements nested deeper than 32 levels.
 */
bool parseUntrustedXml( const QByteArray &data, QDomDocument *doc );

/**
 * The size of a picture (JPEG, PNG, GIF, BMP, WebP, AVIF, JPEG XL) read
 * from its header, without decoding it.
 */
bool imageHeaderSize( const QByteArray &data, int *width, int *height );

/**
 * Asynchronous, anonymous OCS client: content listing, resolution of the
 * (short-lived) download links and preview pictures.
 * The server data is untrusted: the responses are size limited, documents
 * with a DTD are refused, and only http(s) links are accepted (only https
 * when the provider itself is accessed with https).
 */
class OcsClient : public QObject
{
    Q_OBJECT
  public:
    enum SortMode { SortRating = 0, SortDownloads = 1, SortNewest = 2 };

    OcsClient( const KURL &base, const QString &categories, QObject *parent );
    ~OcsClient();

    static int pageSize();

    /** Lists one page (0-based); a listing of the same sort mode in
        progress is cancelled. */
    void listContents( int sortMode, int page );
    /** Asks the server for a fresh link to file @p number of item @p id. */
    void resolveDownload( const QString &id, int number );
    /** Loads a preview picture; @p key is returned with the image. */
    void fetchPreview( const KURL &url, int key );

    /** Whether @p url may be fetched for this provider. */
    bool isAllowedUrl( const KURL &url ) const;

  signals:
    // spelled out, as in the slots of DownloadDialog, for the signature match
    void contentsListed( int sortMode, int page, const QValueList<KNS::OcsContent> &items, int total, bool more );
    void listingFailed( int sortMode, const QString &message );
    void downloadResolved( const QString &id, int number, const KURL &link );
    void downloadFailed( const QString &id, int number, const QString &message );
    void previewLoaded( int key, const QImage &image );

  private slots:
    void slotData( KIO::Job *job, const QByteArray &data );
    void slotResult( KIO::Job *job );
    void slotRedirection( KIO::Job *job, const KURL &url );

  private:
    enum Kind { List, Download, Preview };
    struct Request
    {
      Kind kind;
      int sortMode;
      int page;
      QString id;
      int number;
      int key;
      QByteArray data;
      bool tooLarge;
    };

    KIO::TransferJob *startJob( const KURL &url, bool reload );
    void startPreviews();
    void failed( const Request &r, const QString &message );
    void finishList( const Request &r );
    void finishDownload( const Request &r );
    bool parseContent( const QDomElement &e, OcsContent &c ) const;

    KURL mBase;
    QString mCategories;
    QMap<KIO::Job *, Request> mRequests;
    QValueList<QPair<KURL, int> > mPreviewQueue;
    int mPreviewsRunning;
};

/**
 * Installs a downloaded OCS file.
 */
class OcsInstaller
{
  public:
    /**
     * The install location configured in the [KNewStuff] group of the
     * application's configuration (StandardResource, TargetDir or
     * InstallPath, as for KNewStuffGeneric); created if needed.
     * Returns QString::null if none is configured.
     * @p wallpaper is set when the location is the wallpaper resource.
     */
    static QString targetDirectory( bool *wallpaper );

    /**
     * Installs @p file into @p dir: an image is copied, the images of a
     * tar (plain, gzip, bzip2) or zip archive are extracted into a folder
     * named after the item. Nothing but images is ever written, and every
     * name is reduced to a plain file name (no paths, links or devices).
     * For wallpapers a .desktop file with the item's name is written next
     * to each image.
     *
     * @return QString::null on success, otherwise an error message
     */
    static QString install( const QString &file, const QString &dir, bool wallpaper,
                            const OcsContent &content, const OcsDownload &download,
                            QWidget *parent, QStringList *installed );

    /**
     * Installs the single picture @p file into @p dir without asking
     * anything (an existing file of the same name is replaced): for the
     * pictures of the day of the desktop. The name is @p download's name
     * with the extension of the picture's format; a .desktop file with
     * the name, author and license is written next to it.
     *
     * @return QString::null on success, otherwise an error message
     */
    static QString installPicture( const QString &file, const QString &dir,
                                   const OcsContent &content, const OcsDownload &download,
                                   QString *installed );

    /** Limits for downloads and extraction. */
    static const Q_ULLONG maxDownloadSize;
};

}

#endif
