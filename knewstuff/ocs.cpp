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

#include "ocs.h"
#include "ocs.moc"

#include <string.h>
#include <sys/stat.h>

#include <qbuffer.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qstylesheet.h>

#include <kconfig.h>
#include <kdebug.h>
#include <kfilterdev.h>
#include <kglobal.h>
#include <kimageio.h>
#include <kio/job.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <ksavefile.h>
#include <ksimpleconfig.h>
#include <kstandarddirs.h>
#include <ktar.h>
#include <ktempfile.h>
#include <kzip.h>

using namespace KNS;

// Response size limits
static const uint maxListResponse = 8 * 1024 * 1024;
static const uint maxDownloadResponse = 256 * 1024;
static const uint maxPreviewResponse = 4 * 1024 * 1024;
// Largest accepted preview picture
static const int maxPreviewDimension = 4096;
// Concurrent preview downloads
static const int maxPreviewJobs = 3;

const Q_ULLONG OcsInstaller::maxDownloadSize = Q_ULLONG( 512 ) * 1024 * 1024;
static const Q_ULLONG maxExtractedFile = Q_ULLONG( 128 ) * 1024 * 1024;
static const Q_ULLONG maxExtractedTotal = Q_ULLONG( 512 ) * 1024 * 1024;
static const int maxArchiveEntries = 5000;
static const int maxArchiveImages = 500;
static const int maxArchiveDepth = 16;

// ------------------------------------------------------------------
// Helpers for untrusted text
// ------------------------------------------------------------------

// One line of text, without control characters, at most @p max characters.
static QString cleanLine( const QString &s, uint max )
{
  QString t = s;
  for ( uint i = 0; i < t.length(); ++i ) {
    ushort u = t[ i ].unicode();
    if ( u < 0x20 || u == 0x7f )
      t[ i ] = ' ';
  }
  t = t.simplifyWhiteSpace();
  if ( t.length() > max )
    t = t.left( max );
  return t;
}

// The document is refused if it has a DTD (internal entities could expand
// without bounds) or any other markup declaration.
static bool hasMarkupDeclaration( const QByteArray &data )
{
  const char *p = data.data();
  uint n = data.size();
  for ( uint i = 0; i + 1 < n; ++i ) {
    if ( p[ i ] != '<' || p[ i + 1 ] != '!' )
      continue;
    if ( i + 9 <= n && qstrncmp( p + i, "<![CDATA[", 9 ) == 0 )
      continue;
    if ( i + 4 <= n && qstrncmp( p + i, "<!--", 4 ) == 0 )
      continue;
    return true;
  }
  return false;
}

// Parses an OCS response; returns the <data> element.
static bool parseOcsDocument( const QByteArray &data, QDomElement *dataElement,
                              int *total, QString *error )
{
  if ( hasMarkupDeclaration( data ) ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }

  QDomDocument doc;
  QString message;
  int line, column;
  if ( !doc.setContent( data, false, &message, &line, &column ) ) {
    kdDebug() << "OCS: parse error " << message << " at " << line << ":" << column << endl;
    *error = i18n( "The server sent an invalid response." );
    return false;
  }

  QDomElement root = doc.documentElement();
  if ( root.tagName() != "ocs" ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }

  QDomElement meta = root.namedItem( "meta" ).toElement();
  int status = meta.namedItem( "statuscode" ).toElement().text().toInt();
  if ( status != 100 && status != 200 ) {
    QString text = cleanLine( meta.namedItem( "message" ).toElement().text(), 200 );
    if ( text.isEmpty() )
      text = QString::number( status );
    *error = i18n( "The server reported an error: %1" ).arg( text );
    return false;
  }

  if ( total )
    *total = meta.namedItem( "totalitems" ).toElement().text().toInt();
  *dataElement = root.namedItem( "data" ).toElement();
  return true;
}

// The width and height of a PNG, JPEG or GIF picture, read from its
// header, so that a picture is never decoded before its size is known.
static bool pictureSize( const QByteArray &data, int *width, int *height )
{
  const uchar *p = reinterpret_cast<const uchar *>( data.data() );
  uint n = data.size();

  if ( n >= 24 && p[ 0 ] == 0x89 && p[ 1 ] == 'P' && p[ 2 ] == 'N' && p[ 3 ] == 'G' ) {
    *width = ( p[ 16 ] << 24 ) | ( p[ 17 ] << 16 ) | ( p[ 18 ] << 8 ) | p[ 19 ];
    *height = ( p[ 20 ] << 24 ) | ( p[ 21 ] << 16 ) | ( p[ 22 ] << 8 ) | p[ 23 ];
    return true;
  }

  if ( n >= 10 && p[ 0 ] == 'G' && p[ 1 ] == 'I' && p[ 2 ] == 'F' ) {
    *width = p[ 6 ] | ( p[ 7 ] << 8 );
    *height = p[ 8 ] | ( p[ 9 ] << 8 );
    return true;
  }

  if ( n >= 4 && p[ 0 ] == 0xff && p[ 1 ] == 0xd8 ) {
    uint i = 2;
    while ( i + 9 < n ) {
      if ( p[ i ] != 0xff ) return false;
      uchar marker = p[ i + 1 ];
      if ( marker == 0xff ) { ++i; continue; }
      uint length = ( p[ i + 2 ] << 8 ) | p[ i + 3 ];
      // SOF0..SOF15 except DHT (c4), JPG (c8) and DAC (cc)
      if ( marker >= 0xc0 && marker <= 0xcf &&
           marker != 0xc4 && marker != 0xc8 && marker != 0xcc ) {
        *height = ( p[ i + 5 ] << 8 ) | p[ i + 6 ];
        *width = ( p[ i + 7 ] << 8 ) | p[ i + 8 ];
        return true;
      }
      if ( length < 2 ) return false;
      i += 2 + length;
    }
  }

  return false;
}

// "cc-by-sa" -> "CC-BY-SA" etc.; the OCS servers carry the license as a tag.
static QString licenseFromTags( const QString &tags )
{
  static const char * const known[][2] = {
    { "cc0", "CC0" },
    { "cc-by", "CC-BY" },
    { "cc-by-sa", "CC-BY-SA" },
    { "cc-by-nd", "CC-BY-ND" },
    { "cc-by-nc", "CC-BY-NC" },
    { "cc-by-nc-sa", "CC-BY-NC-SA" },
    { "cc-by-nc-nd", "CC-BY-NC-ND" },
    { "gplv2", "GPLv2" },
    { "gplv2-later", "GPLv2+" },
    { "gplv3", "GPLv3" },
    { "lgplv2", "LGPLv2" },
    { "lgplv3", "LGPLv3" },
    { "agplv3", "AGPLv3" },
    { "mit-license", "MIT" },
    { "bsd", "BSD" },
    { "apache", "Apache" },
    { "artistic", "Artistic" },
    { "public-domain", "Public Domain" },
    { 0, 0 }
  };

  QStringList list = QStringList::split( ',', tags );
  for ( QStringList::ConstIterator it = list.begin(); it != list.end(); ++it ) {
    QString tag = ( *it ).stripWhiteSpace().lower();
    for ( int i = 0; known[ i ][ 0 ]; ++i )
      if ( tag == known[ i ][ 0 ] )
        return QString::fromLatin1( known[ i ][ 1 ] );
  }
  return QString::null;
}

// ------------------------------------------------------------------
// OcsClient
// ------------------------------------------------------------------

OcsClient::OcsClient( const KURL &base, const QString &categories, QObject *parent )
  : QObject( parent ), mBase( base ), mPreviewsRunning( 0 )
{
  mBase.adjustPath( +1 );

  // Category ids, separated by commas, spaces or 'x' (the OCS separator)
  QStringList ids;
  QStringList tokens = QStringList::split( QRegExp( "[,;x\\s]+" ), categories );
  QRegExp number( "^[0-9]{1,9}$" );
  for ( QStringList::ConstIterator it = tokens.begin(); it != tokens.end(); ++it )
    if ( number.exactMatch( *it ) )
      ids.append( *it );
  mCategories = ids.join( "x" );
}

OcsClient::~OcsClient()
{
  QMap<KIO::Job *, Request>::Iterator it;
  for ( it = mRequests.begin(); it != mRequests.end(); ++it )
    it.key()->kill( true );
}

int OcsClient::pageSize()
{
  return 20;
}

bool OcsClient::isAllowedUrl( const KURL &url ) const
{
  if ( !url.isValid() || url.host().isEmpty() )
    return false;
  QString protocol = url.protocol().lower();
  if ( protocol == "https" )
    return true;
  // plain http only if the provider is accessed with plain http
  return protocol == "http" && mBase.protocol().lower() == "http";
}

KIO::TransferJob *OcsClient::startJob( const KURL &url, bool reload )
{
  KIO::TransferJob *job = KIO::get( url, reload, false );
  // HTTP errors are errors, not data; anonymous: no cookies
  job->addMetaData( "errorPage", "false" );
  job->addMetaData( "cookies", "none" );
  connect( job, SIGNAL( data( KIO::Job *, const QByteArray & ) ),
           SLOT( slotData( KIO::Job *, const QByteArray & ) ) );
  connect( job, SIGNAL( result( KIO::Job * ) ),
           SLOT( slotResult( KIO::Job * ) ) );
  return job;
}

void OcsClient::listContents( int sortMode, int page )
{
  // one listing per sort mode at a time
  QMap<KIO::Job *, Request>::Iterator it;
  for ( it = mRequests.begin(); it != mRequests.end(); ++it ) {
    if ( it.data().kind == List && it.data().sortMode == sortMode ) {
      it.key()->kill( true );
      mRequests.remove( it );
      break;
    }
  }

  if ( mCategories.isEmpty() ) {
    emit listingFailed( sortMode, i18n( "No content categories are configured." ) );
    return;
  }

  static const char * const modes[] = { "high", "down", "new" };
  if ( sortMode < 0 || sortMode > 2 )
    sortMode = SortRating;

  KURL url( mBase );
  url.addPath( "content/data" );
  url.setQuery( QString( "categories=%1&sortmode=%2&page=%3&pagesize=%4" )
                .arg( mCategories ).arg( modes[ sortMode ] )
                .arg( page ).arg( pageSize() ) );

  kdDebug() << "OCS: list " << url.url() << endl;

  Request r;
  r.kind = List;
  r.sortMode = sortMode;
  r.page = page;
  r.number = 0;
  r.key = 0;
  r.tooLarge = false;
  mRequests.insert( startJob( url, true ), r );
}

void OcsClient::resolveDownload( const QString &id, int number )
{
  Request r;
  r.kind = Download;
  r.sortMode = 0;
  r.page = 0;
  r.id = id;
  r.number = number;
  r.key = 0;
  r.tooLarge = false;

  if ( !QRegExp( "^[0-9]{1,12}$" ).exactMatch( id ) || number < 1 ) {
    failed( r, i18n( "Invalid item." ) );
    return;
  }

  KURL url( mBase );
  url.addPath( QString( "content/download/%1/%2" ).arg( id ).arg( number ) );
  mRequests.insert( startJob( url, true ), r );
}

void OcsClient::fetchPreview( const KURL &url, int key )
{
  if ( !isAllowedUrl( url ) )
    return;
  mPreviewQueue.append( qMakePair( url, key ) );
  startPreviews();
}

void OcsClient::startPreviews()
{
  while ( mPreviewsRunning < maxPreviewJobs && !mPreviewQueue.isEmpty() ) {
    QPair<KURL, int> next = mPreviewQueue.first();
    mPreviewQueue.remove( mPreviewQueue.begin() );

    Request r;
    r.kind = Preview;
    r.sortMode = 0;
    r.page = 0;
    r.number = 0;
    r.key = next.second;
    r.tooLarge = false;
    mRequests.insert( startJob( next.first, false ), r );
    ++mPreviewsRunning;
  }
}

void OcsClient::slotData( KIO::Job *job, const QByteArray &data )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() || data.isEmpty() )
    return;

  Request &r = it.data();
  uint limit = r.kind == List ? maxListResponse :
               r.kind == Download ? maxDownloadResponse : maxPreviewResponse;
  uint size = r.data.size();
  if ( size + data.size() > limit ) {
    Request copy = r;
    mRequests.remove( it );
    job->kill( true );
    failed( copy, i18n( "The server sent too much data." ) );
    return;
  }
  r.data.resize( size + data.size() );
  memcpy( r.data.data() + size, data.data(), data.size() );
}

void OcsClient::slotResult( KIO::Job *job )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() )
    return;
  Request r = it.data();
  mRequests.remove( it );

  if ( job->error() ) {
    failed( r, job->errorString() );
    return;
  }

  switch ( r.kind ) {
    case List:
      finishList( r );
      break;
    case Download:
      finishDownload( r );
      break;
    case Preview: {
      --mPreviewsRunning;
      int width = 0, height = 0;
      QImage image;
      if ( pictureSize( r.data, &width, &height ) &&
           width > 0 && height > 0 &&
           width <= maxPreviewDimension && height <= maxPreviewDimension &&
           image.loadFromData( r.data ) )
        emit previewLoaded( r.key, image );
      startPreviews();
      break;
    }
  }
}

void OcsClient::failed( const Request &r, const QString &message )
{
  kdDebug() << "OCS: request failed: " << message << endl;
  switch ( r.kind ) {
    case List:
      emit listingFailed( r.sortMode, message );
      break;
    case Download:
      emit downloadFailed( r.id, r.number, message );
      break;
    case Preview:
      --mPreviewsRunning;
      startPreviews();
      break;
  }
}

void OcsClient::finishList( const Request &r )
{
  QDomElement data;
  QString error;
  int total = 0;
  if ( !parseOcsDocument( r.data, &data, &total, &error ) ) {
    failed( r, error );
    return;
  }

  OcsContentList list;
  int count = 0;
  for ( QDomNode n = data.firstChild(); !n.isNull(); n = n.nextSibling() ) {
    QDomElement e = n.toElement();
    if ( e.tagName() != "content" )
      continue;
    if ( ++count > 4 * pageSize() )
      break;
    OcsContent c;
    if ( parseContent( e, c ) )
      list.append( c );
  }

  emit contentsListed( r.sortMode, r.page, list, total < 0 ? 0 : total );
}

void OcsClient::finishDownload( const Request &r )
{
  QDomElement data;
  QString error;
  if ( !parseOcsDocument( r.data, &data, 0, &error ) ) {
    failed( r, error );
    return;
  }

  QDomElement content = data.namedItem( "content" ).toElement();
  KURL link( content.namedItem( "downloadlink" ).toElement().text().stripWhiteSpace() );
  if ( !isAllowedUrl( link ) ) {
    failed( r, i18n( "The server did not send a usable download link." ) );
    return;
  }

  emit downloadResolved( r.id, r.number, link );
}

bool OcsClient::parseContent( const QDomElement &element, OcsContent &c ) const
{
  QMap<QString, QString> fields;
  int count = 0;
  for ( QDomNode n = element.firstChild(); !n.isNull(); n = n.nextSibling() ) {
    QDomElement e = n.toElement();
    if ( e.isNull() )
      continue;
    if ( ++count > 1000 )
      break;
    if ( !fields.contains( e.tagName() ) )
      fields.insert( e.tagName(), e.text() );
  }

  c.id = fields[ "id" ].stripWhiteSpace();
  if ( !QRegExp( "^[0-9]{1,12}$" ).exactMatch( c.id ) )
    return false;
  c.name = cleanLine( fields[ "name" ], 200 );
  if ( c.name.isEmpty() )
    return false;
  // the publisher asked not to be shown in desktop clients
  if ( fields[ "ghns_excluded" ].stripWhiteSpace() == "1" )
    return false;

  c.version = cleanLine( fields[ "version" ], 50 );
  c.author = cleanLine( fields[ "personid" ], 100 );
  c.summary = cleanLine( fields[ "summary" ], 1000 );
  c.license = cleanLine( fields[ "license" ], 100 );
  if ( c.license.isEmpty() )
    c.license = licenseFromTags( fields[ "tags" ] );
  c.changed = QDateTime::fromString( fields[ "changed" ].stripWhiteSpace().left( 19 ), Qt::ISODate );
  if ( !c.changed.isValid() )
    c.changed = QDateTime::fromString( fields[ "created" ].stripWhiteSpace().left( 19 ), Qt::ISODate );
  c.score = QMIN( QMAX( fields[ "score" ].toInt(), 0 ), 100 );
  c.downloads = QMAX( fields[ "downloads" ].toInt(), 0 );

  KURL preview( fields[ "smallpreviewpic1" ].stripWhiteSpace() );
  if ( !isAllowedUrl( preview ) )
    preview = KURL( fields[ "previewpic1" ].stripWhiteSpace() );
  if ( isAllowedUrl( preview ) )
    c.preview = preview;

  KURL page( fields[ "detailpage" ].stripWhiteSpace() );
  if ( page.isValid() && ( page.protocol() == "https" || page.protocol() == "http" ) )
    c.detailPage = page;

  for ( int i = 1; i <= 100; ++i ) {
    QString n = QString::number( i );
    if ( !fields.contains( "downloadlink" + n ) )
      break;
    if ( fields[ "downloadlink" + n ].stripWhiteSpace().isEmpty() )
      continue;
    // paid downloads need an account
    if ( fields[ "downloadprice" + n ].toDouble() > 0 )
      continue;

    OcsDownload d;
    d.number = i;
    d.name = cleanLine( fields[ "downloadname" + n ], 200 );
    d.sizeKiB = QMAX( fields[ "downloadsize" + n ].toInt(), 0 );
    QStringList tags = QStringList::split( "##", fields[ "downloadtags" + n ] );
    for ( QStringList::ConstIterator it = tags.begin(); it != tags.end(); ++it ) {
      if ( ( *it ).startsWith( "mimetype=" ) )
        d.mimeType = cleanLine( ( *it ).mid( 9 ), 100 );
    }
    if ( d.name.isEmpty() )
      d.name = i18n( "File %1" ).arg( i );
    c.files.append( d );
  }

  return true;
}

// ------------------------------------------------------------------
// OcsInstaller
// ------------------------------------------------------------------

// A plain file name: no directory separators, no control characters, no
// leading dots (hidden files, "." and ".."), not too long.
static QString safeFileName( const QString &name, const QString &fallback )
{
  QString s = name;
  for ( uint i = 0; i < s.length(); ++i ) {
    ushort u = s[ i ].unicode();
    if ( u < 0x20 || u == 0x7f || u == ':' || u == '/' || u == '\\' )
      s[ i ] = '_';
  }
  s = s.stripWhiteSpace();
  while ( s.startsWith( "." ) )
    s.remove( 0, 1 );
  if ( s.length() > 120 ) {
    int dot = s.findRev( '.' );
    QString ext = ( dot > 0 && s.length() - dot <= 10 ) ? s.mid( dot ) : QString::null;
    s = s.left( 120 - ext.length() ) + ext;
  }
  if ( s.isEmpty() )
    s = fallback;
  return s;
}

// The picture format of a device, or 0. The SVG reader claims anything
// gzip compressed; a compressed file is never taken for a picture here.
static const char *pictureFormat( QIODevice *dev )
{
  char magic[ 2 ];
  if ( dev->readBlock( magic, 2 ) != 2 || !dev->at( 0 ) )
    return 0;
  if ( uchar( magic[ 0 ] ) == 0x1f && uchar( magic[ 1 ] ) == 0x8b )
    return 0;
  return QImageIO::imageFormat( dev );
}

static const char *pictureFormat( const QString &file )
{
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return 0;
  return pictureFormat( &f );
}

// The file name with an extension that matches the picture format.
static QString imageFileName( const QString &name, const char *format )
{
  QString fmt = QString::fromLatin1( format ).lower();
  QStringList exts;
  if ( fmt == "jpeg" )
    exts << "jpg" << "jpeg" << "jpe";
  else if ( fmt == "tiff" )
    exts << "tif" << "tiff";
  else if ( fmt == "svg" )
    exts << "svg" << "svgz";
  else
    exts << fmt;

  int dot = name.findRev( '.' );
  QString ext = dot > 0 ? name.mid( dot + 1 ).lower() : QString::null;
  if ( exts.contains( ext ) )
    return name;
  return name + "." + exts.first();
}

// Copies a device into @p path (written to a temporary file next to it,
// then renamed, so a link at the destination is replaced, not followed).
static bool saveDevice( QIODevice *source, const QString &path, Q_ULLONG limit )
{
  KSaveFile out( path, 0644 );
  if ( out.status() != 0 || !out.file() )
    return false;

  char buffer[ 65536 ];
  Q_ULLONG total = 0;
  for ( ;; ) {
    Q_LONG n = source->readBlock( buffer, sizeof( buffer ) );
    if ( n < 0 ) {
      out.abort();
      return false;
    }
    if ( n == 0 )
      break;
    total += n;
    if ( total > limit || out.file()->writeBlock( buffer, n ) != n ) {
      out.abort();
      return false;
    }
  }
  return out.close();
}

static void writeWallpaperDesktop( const QString &imagePath, const QString &caption,
                                   const OcsContent &content )
{
  QString desktopPath = imagePath + ".desktop";
  QFile::remove( desktopPath );
  KSimpleConfig cfg( desktopPath );
  cfg.setGroup( "Wallpaper" );
  cfg.writeEntry( "Name", caption );
  cfg.writeEntry( "File", QFileInfo( imagePath ).fileName() );
  cfg.writeEntry( "ImageType", "pixmap" );
  if ( !content.author.isEmpty() )
    cfg.writeEntry( "Author", content.author );
  if ( !content.license.isEmpty() )
    cfg.writeEntry( "License", content.license );
  if ( content.detailPage.isValid() )
    cfg.writeEntry( "Url", content.detailPage.url() );
  cfg.sync();
}

QString OcsInstaller::targetDirectory( bool *wallpaper )
{
  *wallpaper = false;

  KConfig *cfg = KGlobal::config();
  KConfigGroupSaver saver( cfg, "KNewStuff" );

  QString res = cfg->readEntry( "StandardResource" );
  if ( !res.isEmpty() ) {
    *wallpaper = ( res == "wallpaper" );
    return KGlobal::dirs()->saveLocation( res.latin1() );
  }

  QString target = cfg->readEntry( "TargetDir" );
  if ( !target.isEmpty() )
    return KGlobal::dirs()->saveLocation( "data", target + "/" );

  QString path = cfg->readEntry( "InstallPath" );
  if ( !path.isEmpty() ) {
    QString dir = QDir::home().path() + "/" + path + "/";
    if ( !KStandardDirs::makeDir( dir ) && !QFileInfo( dir ).isDir() )
      return QString::null;
    return dir;
  }

  return QString::null;
}

namespace {

struct Extraction
{
  QString dir;
  bool zip;
  QStringList *installed;
  QStringList usedNames;
  Q_ULLONG total;
  int entries;
  QString error;
};

}

static void extractImages( const KArchiveDirectory *dir, int depth, Extraction &x )
{
  if ( depth > maxArchiveDepth )
    return;

  QStringList names = dir->entries();
  names.sort();
  for ( QStringList::ConstIterator it = names.begin(); it != names.end(); ++it ) {
    if ( !x.error.isEmpty() )
      return;
    if ( ++x.entries > maxArchiveEntries ) {
      x.error = i18n( "The archive has too many entries." );
      return;
    }
    if ( *it == "." || *it == ".." || ( *it ).find( '/' ) >= 0 )
      continue;

    const KArchiveEntry *e = dir->entry( *it );
    if ( !e || !e->symlink().isEmpty() )
      continue;

    mode_t mode = e->permissions();
    if ( e->isDirectory() ) {
      extractImages( static_cast<const KArchiveDirectory *>( e ), depth + 1, x );
      continue;
    }
    // regular files only (tar entries may carry no type bits)
    if ( ( mode & S_IFMT ) != 0 && !S_ISREG( mode ) )
      continue;

    const KArchiveFile *f = static_cast<const KArchiveFile *>( e );
    if ( f->size() <= 0 || Q_ULLONG( f->size() ) > maxExtractedFile )
      continue;

    // KArchiveFile::device() is not virtual
    QIODevice *dev = x.zip ? static_cast<const KZipFileEntry *>( f )->device() : f->device();
    if ( !dev )
      continue;

    QByteArray data;
    bool ok = true;
    char chunk[ 65536 ];
    for ( ;; ) {
      Q_LONG n = dev->readBlock( chunk, sizeof( chunk ) );
      if ( n < 0 ) { ok = false; break; }
      if ( n == 0 ) break;
      uint size = data.size();
      if ( Q_ULLONG( size ) + n > maxExtractedFile ) { ok = false; break; }
      data.resize( size + n );
      memcpy( data.data() + size, chunk, n );
    }
    delete dev;
    if ( !ok )
      continue;

    QBuffer buffer( data );
    buffer.open( IO_ReadOnly );
    const char *format = pictureFormat( &buffer );
    if ( !format )
      continue;

    if ( x.total + data.size() > maxExtractedTotal ) {
      x.error = i18n( "The archive is too large." );
      return;
    }
    if ( x.installed->count() >= uint( maxArchiveImages ) ) {
      x.error = i18n( "The archive has too many images." );
      return;
    }

    QString name = imageFileName( safeFileName( *it, "image" ), format );
    if ( x.usedNames.contains( name.lower() ) ) {
      int dot = name.findRev( '.' );
      QString base = name.left( dot ), ext = name.mid( dot );
      for ( int i = 2; x.usedNames.contains( name.lower() ); ++i )
        name = QString( "%1-%2%3" ).arg( base ).arg( i ).arg( ext );
    }
    x.usedNames.append( name.lower() );

    QString path = x.dir + name;
    buffer.at( 0 );
    if ( !saveDevice( &buffer, path, maxExtractedFile ) ) {
      x.error = i18n( "Could not write the file '%1'." ).arg( path );
      return;
    }
    x.total += data.size();
    x.installed->append( path );
  }
}

// Decompresses a gzip or bzip2 file into @p out.
static bool decompress( const QString &file, const QString &mimeType, KTempFile &out )
{
  QIODevice *dev = KFilterDev::deviceForFile( file, mimeType, true );
  if ( !dev )
    return false;
  if ( !dev->open( IO_ReadOnly ) ) {
    delete dev;
    return false;
  }
  QFile *f = out.file();
  bool ok = f != 0;
  char chunk[ 65536 ];
  Q_ULLONG total = 0;
  while ( ok ) {
    Q_LONG n = dev->readBlock( chunk, sizeof( chunk ) );
    if ( n < 0 ) { ok = false; break; }
    if ( n == 0 ) break;
    total += n;
    if ( total > maxExtractedTotal || f->writeBlock( chunk, n ) != n )
      ok = false;
  }
  delete dev;
  return out.close() && ok;
}

static bool isTar( const QString &file )
{
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return false;
  char header[ 512 ];
  if ( f.readBlock( header, sizeof( header ) ) != 512 )
    return false;
  return qstrncmp( header + 257, "ustar", 5 ) == 0;
}

static QString installImage( const QString &file, const char *format, const QString &dir,
                             bool wallpaper, const OcsContent &content,
                             const OcsDownload &download, QWidget *parent,
                             QStringList *installed )
{
  QString name = imageFileName( safeFileName( download.name, "ocs-" + content.id ), format );
  QString path = dir + name;

  if ( QFileInfo( path ).exists() ) {
    int result = KMessageBox::warningContinueCancel( parent,
        "<qt>" + i18n( "The file '%1' already exists. Do you want to overwrite it?" ).arg( QStyleSheet::escape( path ) ) + "</qt>",
        QString::null, i18n( "Overwrite" ) );
    if ( result == KMessageBox::Cancel )
      return QString::null;
  }

  QFile source( file );
  if ( !source.open( IO_ReadOnly ) || !saveDevice( &source, path, OcsInstaller::maxDownloadSize ) )
    return i18n( "Could not write the file '%1'." ).arg( path );

  installed->append( path );
  if ( wallpaper )
    writeWallpaperDesktop( path, content.name, content );
  return QString::null;
}

QString OcsInstaller::install( const QString &file, const QString &dir, bool wallpaper,
                               const OcsContent &content, const OcsDownload &download,
                               QWidget *parent, QStringList *installed )
{
  KImageIO::registerFormats();

  QString target = dir;
  if ( !target.endsWith( "/" ) )
    target += '/';

  // An archive (checked first: the SVG reader claims any gzip file), or else
  // a picture
  const char *format = 0;
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return i18n( "Could not read the downloaded file." );
  uchar magic[ 6 ];
  Q_LONG n = f.readBlock( reinterpret_cast<char *>( magic ), sizeof( magic ) );
  f.close();
  if ( n < 6 )
    return i18n( "The downloaded file is neither a picture nor a supported archive." );
  // the name of the downloaded file is not trusted for the type

  KTempFile plain( locateLocal( "tmp", "knewstuff-ocs-" ), ".tar" );
  plain.setAutoDelete( true );

  KArchive *archive = 0;
  bool zip = false;
  if ( magic[ 0 ] == 'P' && magic[ 1 ] == 'K' && magic[ 2 ] == 3 && magic[ 3 ] == 4 ) {
    archive = new KZip( file );
    zip = true;
  } else if ( ( magic[ 0 ] == 0x1f && magic[ 1 ] == 0x8b ) ||
              ( magic[ 0 ] == 'B' && magic[ 1 ] == 'Z' && magic[ 2 ] == 'h' ) ) {
    QString mime = magic[ 0 ] == 0x1f ? "application/x-gzip" : "application/x-bzip2";
    if ( !decompress( file, mime, plain ) )
      return i18n( "The downloaded archive is damaged or too large." );
    if ( isTar( plain.name() ) ) {
      archive = new KTar( plain.name(), "application/x-tar" );
    } else {
      // a single compressed picture (or an SVGZ)
      format = pictureFormat( plain.name() );
      if ( !format )
        return i18n( "The downloaded file is neither a picture nor a supported archive." );
      OcsDownload d = download;
      d.name.replace( QRegExp( "\\.(gz|bz2)$", false ), QString::null );
      return installImage( plain.name(), format, target, wallpaper, content, d, parent, installed );
    }
  } else if ( isTar( file ) ) {
    archive = new KTar( file, "application/x-tar" );
  } else if ( magic[ 0 ] == 0xfd && magic[ 1 ] == '7' && magic[ 2 ] == 'z' &&
              magic[ 3 ] == 'X' && magic[ 4 ] == 'Z' && magic[ 5 ] == 0 ) {
    return i18n( "Archives compressed with XZ are not supported. "
                 "If the item offers other files, please choose one of those." );
  } else {
    format = pictureFormat( file );
    if ( format )
      return installImage( file, format, target, wallpaper, content, download, parent, installed );
    return i18n( "The downloaded file is neither a picture nor a supported archive." );
  }

  if ( !archive->open( IO_ReadOnly ) || !archive->directory() ) {
    delete archive;
    return i18n( "The downloaded archive is damaged." );
  }

  QString folder = target + safeFileName( content.name, "ocs-" + content.id ) + "/";
  bool created = false;
  if ( QFileInfo( folder ).exists() ) {
    int result = KMessageBox::warningContinueCancel( parent,
        "<qt>" + i18n( "The folder '%1' already exists. Do you want to overwrite the files in it?" ).arg( QStyleSheet::escape( folder ) ) + "</qt>",
        QString::null, i18n( "Overwrite" ) );
    if ( result == KMessageBox::Cancel ) {
      delete archive;
      return QString::null;
    }
    if ( !QFileInfo( folder ).isDir() ) {
      delete archive;
      return i18n( "Could not write the file '%1'." ).arg( folder );
    }
  } else {
    if ( !QDir().mkdir( folder ) ) {
      delete archive;
      return i18n( "Could not create the folder '%1'." ).arg( folder );
    }
    created = true;
  }

  Extraction x;
  x.dir = folder;
  x.zip = zip;
  x.installed = installed;
  x.total = 0;
  x.entries = 0;
  extractImages( archive->directory(), 0, x );
  archive->close();
  delete archive;

  if ( x.error.isEmpty() && installed->isEmpty() )
    x.error = i18n( "The archive does not contain any pictures." );

  if ( !x.error.isEmpty() ) {
    for ( QStringList::ConstIterator it = installed->begin(); it != installed->end(); ++it )
      QFile::remove( *it );
    installed->clear();
    if ( created )
      QDir().rmdir( folder );
    return x.error;
  }

  if ( wallpaper ) {
    for ( QStringList::ConstIterator it = installed->begin(); it != installed->end(); ++it ) {
      QString caption = content.name;
      if ( installed->count() > 1 ) {
        QString base = QFileInfo( *it ).baseName( true );
        base.replace( '_', ' ' );
        caption += " - " + base;
      }
      writeWallpaperDesktop( *it, caption, content );
    }
  }

  return QString::null;
}
