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

#include "wallpapersources.h"
#include "wallpapersources.moc"
#include "json.h"

#include <string.h>
#include <sys/stat.h>

#include <qbuffer.h>
#include <qdom.h>
#include <qfile.h>
#include <qimage.h>
#include <qregexp.h>
#include <qtimer.h>

#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kio/job.h>
#include <klocale.h>
#include <kstandarddirs.h>

using namespace KNS;

// Response size limits
static const uint maxListResponse = 2 * 1024 * 1024;   // the APOD feed is ~400 KB
static const uint maxResolveResponse = 256 * 1024;
static const uint maxPreviewResponse = 4 * 1024 * 1024;
static const int maxPreviewDimension = 4096;
static const int maxPreviewJobs = 3;
// Items taken from one response
static const int maxItems = 100;
// Wikimedia: the size of the picture installed
static const int wikimediaWidth = 3840;
// Days of a page of the pictures of the day
static const int daysPerPage = 20;

const Q_ULLONG WallpaperSource::maxDownloadSize = Q_ULLONG( 200 ) * 1024 * 1024;

// The user agent of all requests: generic, nothing about the user or
// the system.
static const char userAgent[] = "KDE3-KNewStuff/1.0 (wallpaper downloader; +https://github.com/kde3-revival)";

static const char keysFile[] = "knewstuffwallpapersrc";

// ------------------------------------------------------------------
// The sources
// ------------------------------------------------------------------

namespace {
struct SourceInfo
{
  const char *id;
  const char *name;            // I18N_NOOP
  const char *icon;
  const char *domains;         // the source's own domains, comma separated
  int spacing;                 // msecs between API requests
};
}

static const SourceInfo sourceInfos[] = {
  { "wallhaven", I18N_NOOP( "Wallhaven" ), "background", "wallhaven.cc", 1500 },
  { "wikimedia", I18N_NOOP( "Wikimedia Commons" ), "folder_image", "wikimedia.org", 0 },
  { "bing", I18N_NOOP( "Bing Image of the Day" ), "today", "bing.com", 0 },
  { "apod", I18N_NOOP( "NASA Astronomy Picture of the Day" ), "date", "nasa.gov", 0 },
  { "unsplash", I18N_NOOP( "Unsplash" ), "camera_unmount", "unsplash.com", 0 },
  { 0, 0, 0, 0, 0 }
};

static const SourceInfo *sourceInfo( const QString &id )
{
  for ( int i = 0; sourceInfos[ i ].id; ++i )
    if ( id == sourceInfos[ i ].id )
      return &sourceInfos[ i ];
  return 0;
}

QStringList WallpaperSource::ids()
{
  QStringList list;
  for ( int i = 0; sourceInfos[ i ].id; ++i )
    list.append( QString::fromLatin1( sourceInfos[ i ].id ) );
  return list;
}

bool WallpaperSource::isKnown( const QString &id )
{
  return sourceInfo( id ) != 0;
}

QString WallpaperSource::displayName( const QString &id )
{
  const SourceInfo *info = sourceInfo( id );
  return info ? i18n( info->name ) : QString::null;
}

QString WallpaperSource::iconName( const QString &id )
{
  const SourceInfo *info = sourceInfo( id );
  return info ? QString::fromLatin1( info->icon ) : QString::fromLatin1( "knewstuff" );
}

QString WallpaperSource::note( const QString &id )
{
  if ( id == "wallhaven" )
    return i18n( "Only pictures marked safe for work are listed. Wallhaven does not "
                 "record licenses: see the picture's page before you use it elsewhere." );
  if ( id == "wikimedia" )
    return i18n( "Free pictures, most under Creative Commons licenses that ask for "
                 "attribution: the author and the license are kept with the picture." );
  if ( id == "bing" )
    return i18n( "The pictures are copyrighted by their authors and offered by "
                 "Microsoft for use as wallpaper." );
  if ( id == "apod" ) {
    if ( apiKey( id ).isEmpty() )
      return i18n( "The last 30 days come from the APOD feed. Older pictures come from "
                   "NASA's API with the shared DEMO_KEY, which allows only a few "
                   "requests per hour: you can enter your own free key from api.nasa.gov." );
    return i18n( "The last 30 days come from the APOD feed, older pictures from "
                 "NASA's API with your key." );
  }
  if ( id == "unsplash" ) {
    if ( apiKey( id ).isEmpty() )
      return i18n( "Unsplash needs an access key: register an application at "
                 "unsplash.com/developers and enter its access key with \"API Key...\". "
                 "Nothing is requested from Unsplash without a key." );
    return i18n( "Photos under the Unsplash license." );
  }
  return QString::null;
}

bool WallpaperSource::usesApiKey( const QString &id )
{
  return id == "apod" || id == "unsplash";
}

static QRegExp keyPattern()
{
  return QRegExp( "^[A-Za-z0-9_\\-]{1,128}$" );
}

QString WallpaperSource::apiKey( const QString &id )
{
  if ( !usesApiKey( id ) )
    return QString::null;
  KConfig cfg( keysFile, true, false );
  cfg.setGroup( "Keys" );
  QString key = cfg.readEntry( id ).stripWhiteSpace();
  if ( !keyPattern().exactMatch( key ) )
    return QString::null;
  return key;
}

bool WallpaperSource::setApiKey( const QString &id, const QString &key )
{
  QString k = key.stripWhiteSpace();
  if ( !usesApiKey( id ) || ( !k.isEmpty() && !keyPattern().exactMatch( k ) ) )
    return false;

  // The keys are private to the user: the file is written with mode 0600
  // (KConfig keeps the mode of an existing file, so it is set again).
  QString path = locateLocal( "config", keysFile );
  if ( QFile::exists( path ) )
    ::chmod( QFile::encodeName( path ), 0600 );
  {
    KConfig cfg( keysFile, false, false );
    cfg.setFileWriteMode( 0600 );
    cfg.setGroup( "Keys" );
    if ( k.isEmpty() )
      cfg.deleteEntry( id );
    else
      cfg.writeEntry( id, k );
    cfg.sync();
  }
  ::chmod( QFile::encodeName( path ), 0600 );
  return true;
}

// ------------------------------------------------------------------

WallpaperSource::WallpaperSource( const QString &id, QObject *parent )
  : QObject( parent ), mId( id ), mPreviewsRunning( 0 ), mLastRequest( -1000000 )
{
  const SourceInfo *info = sourceInfo( id );
  if ( info )
    mDomains = QStringList::split( ',', QString::fromLatin1( info->domains ) );
  mQueueTimer = new QTimer( this );
  connect( mQueueTimer, SIGNAL( timeout() ), SLOT( slotStartQueued() ) );
  mClock.start();
}

WallpaperSource::~WallpaperSource()
{
  QMap<KIO::Job *, Request>::Iterator it;
  for ( it = mRequests.begin(); it != mRequests.end(); ++it )
    it.key()->kill( true );
}

QValueList<WallpaperSource::Tab> WallpaperSource::tabs() const
{
  QValueList<Tab> list;
  Tab t;
  t.search = false;
  if ( mId == "wallhaven" ) {
    t.name = i18n( "Toplist" ); list.append( t );
    t.name = i18n( "Latest" ); list.append( t );
    t.name = i18n( "Search" ); t.search = true; list.append( t );
  } else if ( mId == "wikimedia" ) {
    t.name = i18n( "Picture of the Day" ); list.append( t );
    t.name = i18n( "Featured Pictures" ); list.append( t );
  } else if ( mId == "bing" ) {
    t.name = i18n( "Image of the Day" ); list.append( t );
  } else if ( mId == "apod" ) {
    t.name = i18n( "Picture of the Day" ); list.append( t );
  } else if ( mId == "unsplash" ) {
    t.name = i18n( "Latest" ); list.append( t );
    t.name = i18n( "Search" ); t.search = true; list.append( t );
  }
  return list;
}

void WallpaperSource::setSearchText( int tab, const QString &text )
{
  mSearch[ tab ] = text.simplifyWhiteSpace().left( 100 );
}

QString WallpaperSource::searchText( int tab ) const
{
  return mSearch.contains( tab ) ? mSearch[ tab ] : QString::null;
}

bool WallpaperSource::isAllowedUrl( const KURL &url ) const
{
  if ( !url.isValid() || url.protocol().lower() != "https" || url.host().isEmpty() ||
       url.hasUser() || url.hasPass() )
    return false;
  if ( url.port() != 0 && url.port() != 443 )
    return false;
  QString host = url.host().lower();
  for ( QStringList::ConstIterator it = mDomains.begin(); it != mDomains.end(); ++it )
    if ( host == *it || host.endsWith( "." + *it ) )
      return true;
  return false;
}

void WallpaperSource::setupJob( KIO::TransferJob *job )
{
  // anonymous: no cookies, a generic user agent and no language header; a
  // certificate that does not validate fails the job instead of asking
  job->addMetaData( "cookies", "none" );
  job->addMetaData( "UserAgent", userAgent );
  job->addMetaData( "SendLanguageSettings", "false" );
  job->addMetaData( "SendReferrer", "false" );
  job->addMetaData( "ssl_militant", "TRUE" );
  job->addMetaData( "no-auth", "true" );
}

static QDate utcToday()
{
  return QDateTime::currentDateTime( Qt::UTC ).date();
}

KURL WallpaperSource::apiUrl( int tab, int page ) const
{
  QString q;
  if ( mId == "wallhaven" ) {
    // purity 100: safe for work only, whatever the account would allow
    q = QString( "https://wallhaven.cc/api/v1/search?purity=100&categories=111&page=%1" ).arg( page + 1 );
    if ( tab == 0 )
      q += "&sorting=toplist&topRange=1M";
    else if ( tab == 1 )
      q += "&sorting=date_added&order=desc";
    else
      q += "&sorting=relevance&q=" + KURL::encode_string( searchText( tab ) );
    return KURL( q );
  }

  if ( mId == "wikimedia" ) {
    q = "https://commons.wikimedia.org/w/api.php?action=query&format=json&formatversion=2";
    if ( tab == 0 ) {
      // the templates of the pictures of the day of the page's dates
      QStringList titles;
      QDate today = utcToday();
      for ( int i = 0; i < daysPerPage; ++i )
        titles.append( "Template:Potd/" + today.addDays( -( page * daysPerPage + i ) ).toString( Qt::ISODate ) );
      q += "&prop=images&imlimit=500&titles=" + KURL::encode_string( titles.join( "|" ) );
    } else {
      q += "&generator=categorymembers&gcmtitle=Category:Featured_pictures_on_Wikimedia_Commons"
           "&gcmtype=file&gcmsort=timestamp&gcmdir=desc&gcmlimit=20"
           "&prop=imageinfo&iiprop=url%7Csize%7Cmime%7Cextmetadata%7Ctimestamp&iiurlwidth=330"
           "&iiextmetadatafilter=Artist%7CLicenseShortName%7CLicenseUrl%7CObjectName%7CImageDescription%7CCredit";
      if ( page > 0 ) {
        QString cont = mContinue.contains( page ) ? mContinue[ page ] : QString::null;
        q += "&continue=gcmcontinue%7C%7C&gcmcontinue=" + KURL::encode_string( cont );
      }
    }
    return KURL( q );
  }

  if ( mId == "bing" ) {
    KConfigGroup cfg( KGlobal::config(), "KNewStuff" );
    QString market = cfg.readEntry( "BingMarket", "en-US" );
    if ( !QRegExp( "^[a-z]{2}-[A-Z]{2}$" ).exactMatch( market ) )
      market = "en-US";
    // the archive holds the last 15 days: 8 from today, 8 from 7 days ago
    return KURL( QString( "https://www.bing.com/HPImageArchive.aspx?format=js&idx=%1&n=8&mkt=%2" )
                 .arg( page == 0 ? 0 : 7 ).arg( market ) );
  }

  if ( mId == "apod" ) {
    if ( page == 0 )
      return KURL( "https://science.nasa.gov/feed/apod-basic/" );
    QString key = apiKey( mId );
    if ( key.isEmpty() )
      key = "DEMO_KEY";
    QDate oldest = mApodOldest.isValid() ? mApodOldest : utcToday().addDays( -30 );
    QDate end = oldest.addDays( -1 - ( page - 1 ) * daysPerPage );
    QDate start = end.addDays( -( daysPerPage - 1 ) );
    return KURL( QString( "https://api.nasa.gov/planetary/apod?api_key=%1&start_date=%2&end_date=%3&thumbs=false" )
                 .arg( key ).arg( start.toString( Qt::ISODate ) ).arg( end.toString( Qt::ISODate ) ) );
  }

  if ( mId == "unsplash" ) {
    if ( tab == 0 )
      return KURL( QString( "https://api.unsplash.com/photos?order_by=latest&per_page=20&page=%1" ).arg( page + 1 ) );
    return KURL( QString( "https://api.unsplash.com/search/photos?per_page=20&page=%1&query=%2" )
                 .arg( page + 1 ).arg( KURL::encode_string( searchText( tab ) ) ) );
  }

  return KURL();
}

void WallpaperSource::listContents( int tab, int page )
{
  // one listing per tab at a time
  QMap<KIO::Job *, Request>::Iterator it;
  for ( it = mRequests.begin(); it != mRequests.end(); ++it ) {
    if ( ( it.data().kind == List || it.data().kind == PotdFiles ) && it.data().tab == tab ) {
      it.key()->kill( true );
      mRequests.remove( it );
      break;
    }
  }
  QValueList<Request>::Iterator qit = mQueue.begin();
  while ( qit != mQueue.end() ) {
    if ( ( ( *qit ).kind == List || ( *qit ).kind == PotdFiles ) && ( *qit ).tab == tab )
      qit = mQueue.remove( qit );
    else
      ++qit;
  }

  Request r;
  r.kind = List;
  r.tab = tab;
  r.page = page;
  r.key = 0;
  r.apodFeed = ( mId == "apod" && page == 0 );

  if ( mId == "unsplash" && apiKey( mId ).isEmpty() ) {
    failed( r, i18n( "Unsplash needs an access key: click \"API Key...\" to enter one." ) );
    return;
  }
  if ( tab >= int( tabs().count() ) ) {
    failed( r, i18n( "Invalid item." ) );
    return;
  }
  if ( tabs()[ tab ].search && searchText( tab ).isEmpty() ) {
    failed( r, i18n( "Enter the words to search for." ) );
    return;
  }
  if ( mId == "wikimedia" && tab == 1 && page > 0 && !mContinue.contains( page ) ) {
    emit contentsListed( tab, page, OcsContentList(), 0, false );
    return;
  }

  r.url = apiUrl( tab, page );
  queue( r );
}

void WallpaperSource::queue( const Request &r )
{
  mQueue.append( r );
  slotStartQueued();
}

void WallpaperSource::slotStartQueued()
{
  const SourceInfo *info = sourceInfo( mId );
  int spacing = info ? info->spacing : 0;
  while ( !mQueue.isEmpty() ) {
    long now = mClock.elapsed();
    if ( now < mLastRequest )             // the clock was set back
      mLastRequest = now - spacing;
    if ( spacing > 0 && now - mLastRequest < spacing ) {
      // the source limits the requests per minute
      if ( !mQueueTimer->isActive() )
        mQueueTimer->start( spacing - ( now - mLastRequest ), true );
      return;
    }
    Request r = mQueue.first();
    mQueue.remove( mQueue.begin() );
    mLastRequest = now;
    start( r );
  }
}

void WallpaperSource::start( const Request &r )
{
  if ( !isAllowedUrl( r.url ) ) {
    failed( r, i18n( "The server sent an invalid response." ) );
    return;
  }

  kdDebug() << "wallpaper source " << mId << ": " << ( mId == "apod" ? r.url.host() + r.url.path() : r.url.url() ) << endl;

  bool preview = r.kind == Preview;
  KIO::TransferJob *job = KIO::get( r.url, !preview, false );
  setupJob( job );
  if ( preview ) {
    job->addMetaData( "errorPage", "false" );
  } else {
    // the status of an error is told from the response header; nothing
    // that carries a key is kept in the cache
    job->addMetaData( "errorPage", "true" );
    job->addMetaData( "PropagateHttpHeader", "true" );
    job->addMetaData( "UseCache", "false" );
    if ( mId == "unsplash" ) {
      job->addMetaData( "customHTTPHeader", "Authorization: Client-ID " + apiKey( mId ) +
                                            "\r\nAccept-Version: v1" );
    }
  }
  connect( job, SIGNAL( redirection( KIO::Job *, const KURL & ) ),
           SLOT( slotRedirection( KIO::Job *, const KURL & ) ) );
  connect( job, SIGNAL( data( KIO::Job *, const QByteArray & ) ),
           SLOT( slotData( KIO::Job *, const QByteArray & ) ) );
  connect( job, SIGNAL( result( KIO::Job * ) ),
           SLOT( slotResult( KIO::Job * ) ) );
  mRequests.insert( job, r );
}

void WallpaperSource::resolveDownload( const QString &id, int number )
{
  Request r;
  r.kind = Resolve;
  r.tab = 0;
  r.page = 0;
  r.id = id;
  r.key = number;
  r.apodFeed = false;

  if ( !mItems.contains( id ) ) {
    failed( r, i18n( "Invalid item." ) );
    return;
  }
  const OcsContent &c = mItems[ id ];

  if ( mId == "wikimedia" ) {
    // the picture scaled to at most 3840 pixels wide by Wikimedia: the
    // originals can be huge
    r.url = KURL( QString( "https://commons.wikimedia.org/w/api.php?action=query&format=json&formatversion=2"
                           "&prop=imageinfo&iiprop=url%7Csize%7Cmime&iiurlwidth=%1&pageids=%2" )
                  .arg( wikimediaWidth ).arg( id ) );
    queue( r );
    return;
  }
  if ( mId == "unsplash" ) {
    // the download is announced to Unsplash, as its API guidelines ask;
    // it answers with the link of the photo
    r.url = c.download;
    queue( r );
    return;
  }

  if ( !isAllowedUrl( c.download ) ) {
    failed( r, i18n( "The server did not send a usable download link." ) );
    return;
  }
  mResolved.append( qMakePair( id, c.download ) );
  QTimer::singleShot( 0, this, SLOT( slotResolved() ) );
}

void WallpaperSource::slotResolved()
{
  while ( !mResolved.isEmpty() ) {
    QPair<QString, KURL> p = mResolved.first();
    mResolved.remove( mResolved.begin() );
    emit downloadResolved( p.first, 1, p.second );
  }
}

void WallpaperSource::fetchPreview( const KURL &url, int key )
{
  if ( !isAllowedUrl( url ) )
    return;
  mPreviewQueue.append( qMakePair( url, key ) );
  startPreviews();
}

void WallpaperSource::startPreviews()
{
  while ( mPreviewsRunning < maxPreviewJobs && !mPreviewQueue.isEmpty() ) {
    QPair<KURL, int> next = mPreviewQueue.first();
    mPreviewQueue.remove( mPreviewQueue.begin() );
    Request r;
    r.kind = Preview;
    r.tab = 0;
    r.page = 0;
    r.url = next.first;
    r.key = next.second;
    r.apodFeed = false;
    ++mPreviewsRunning;
    start( r );
  }
}

void WallpaperSource::slotData( KIO::Job *job, const QByteArray &data )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() || data.isEmpty() )
    return;
  Request &r = it.data();
  uint limit = ( r.kind == List || r.kind == PotdFiles ) ? maxListResponse :
               r.kind == Resolve ? maxResolveResponse : maxPreviewResponse;
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

void WallpaperSource::slotRedirection( KIO::Job *job, const KURL &url )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() || isAllowedUrl( url ) )
    return;
  Request r = it.data();
  mRequests.remove( it );
  job->kill( true );
  failed( r, i18n( "The server redirected to a location that is not allowed." ) );
}

// The message for an HTTP error of an API request, from the status line.
QString WallpaperSource::httpError( KIO::Job *job ) const
{
  QString headers = job->queryMetaData( "HTTP-Headers" );
  QString status = headers.section( '\n', 0, 0 );
  int code = status.section( ' ', 1, 1 ).toInt();
  if ( code == 429 || ( code == 403 && mId == "wallhaven" ) ) {
    if ( mId == "apod" && apiKey( mId ).isEmpty() )
      return i18n( "NASA limits the requests made with the shared DEMO_KEY. Try again "
                   "later, or enter your own free API key from api.nasa.gov." );
    return i18n( "%1 received too many requests. Please wait a minute and try again." )
           .arg( displayName( mId ) );
  }
  if ( code == 401 || code == 403 ) {
    if ( usesApiKey( mId ) )
      return i18n( "%1 did not accept the API key." ).arg( displayName( mId ) );
    return i18n( "%1 refused the request." ).arg( displayName( mId ) );
  }
  if ( code >= 500 )
    return i18n( "%1 is not available at the moment (error %2)." ).arg( displayName( mId ) ).arg( code );
  if ( code > 0 )
    return i18n( "%1 sent the error %2." ).arg( displayName( mId ) ).arg( code );
  return i18n( "The server sent an invalid response." );
}

void WallpaperSource::slotResult( KIO::Job *job )
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
  if ( r.kind != Preview && static_cast<KIO::TransferJob *>( job )->isErrorPage() ) {
    failed( r, httpError( job ) );
    return;
  }
  finished( r );
}

void WallpaperSource::failed( const Request &r, const QString &message )
{
  kdDebug() << "wallpaper source " << mId << ": request failed: " << message << endl;
  switch ( r.kind ) {
    case List:
    case PotdFiles:
      emit listingFailed( r.tab, message );
      break;
    case Resolve:
      emit downloadFailed( r.id, r.key, message );
      break;
    case Preview:
      --mPreviewsRunning;
      startPreviews();
      break;
  }
}

// A picture link given by a source: https in the source's domains, and
// a picture by its name (APOD lists videos and pages too).
static bool isPictureName( const KURL &url )
{
  return QRegExp( "\\.(jpe?g|png|webp)$", false ).search( url.path() ) >= 0;
}

bool WallpaperSource::addItem( OcsContent &c, OcsContentList &items )
{
  // the ids name the previews and the install status: kept simple
  if ( !QRegExp( "^[A-Za-z0-9_\\-]{1,64}$" ).exactMatch( c.id ) || c.name.isEmpty() )
    return false;
  if ( !isAllowedUrl( c.download ) && !( mId == "wikimedia" ) )
    return false;
  if ( !isAllowedUrl( c.preview ) )
    c.preview = KURL();
  if ( !isAllowedUrl( c.detailPage ) )
    c.detailPage = KURL();
  if ( c.width < 0 || c.width > 1000000 ) c.width = 0;
  if ( c.height < 0 || c.height > 1000000 ) c.height = 0;
  c.source = displayName( mId );
  mItems[ c.id ] = c;
  items.append( c );
  return true;
}

// One line of untrusted text.
static QString line( const QString &s, uint max )
{
  return htmlToPlainText( s, max );
}

// The format of a preview, among those of the installer (no autodetection
// among all kimgio formats), or 0
static const char *previewFormat( const QByteArray &data )
{
  QBuffer buffer( data );
  buffer.open( IO_ReadOnly );
  const char *format = QImageIO::imageFormat( &buffer );
  static const char * const allowed[] = { "JPEG", "PNG", "GIF", "BMP", "WEBP", "AVIF", "JXL", 0 };
  for ( int i = 0; format && allowed[ i ]; ++i )
    if ( qstrcmp( format, allowed[ i ] ) == 0 )
      return format;
  return "none";
}

void WallpaperSource::finished( Request &r )
{
  if ( r.kind == Preview ) {
    --mPreviewsRunning;
    // the size is read from the header: only small pictures are decoded
    QImage image;
    int width = 0, height = 0;
    if ( imageHeaderSize( r.data, &width, &height ) &&
         width > 0 && height > 0 &&
         width <= maxPreviewDimension && height <= maxPreviewDimension &&
         image.loadFromData( r.data, previewFormat( r.data ) ) )
      emit previewLoaded( r.key, image );
    startPreviews();
    return;
  }

  QVariant json;
  QString error;
  if ( !r.apodFeed && !parseJson( r.data, &json, &error ) ) {
    kdDebug() << "wallpaper source " << mId << ": " << error << endl;
    failed( r, i18n( "The server sent an invalid response." ) );
    return;
  }

  if ( r.kind == Resolve ) {
    KURL link;
    if ( mId == "wikimedia" ) {
      QValueList<QVariant> pages = jsonList( jsonValue( json, "query" ), "pages" );
      if ( !pages.isEmpty() ) {
        QValueList<QVariant> info = jsonList( pages.first(), "imageinfo" );
        if ( !info.isEmpty() ) {
          const QVariant &ii = info.first();
          QString mime = jsonString( ii, "mime" );
          bool plain = mime == "image/jpeg" || mime == "image/png" || mime == "image/webp";
          if ( !plain || jsonNumber( ii, "width" ) > wikimediaWidth )
            link = KURL( jsonString( ii, "thumburl" ) );
          else
            link = KURL( jsonString( ii, "url" ) );
        }
      }
    } else if ( mId == "unsplash" ) {
      link = KURL( jsonString( json, "url" ) );
    }
    if ( !isAllowedUrl( link ) ) {
      failed( r, i18n( "The server did not send a usable download link." ) );
      return;
    }
    emit downloadResolved( r.id, r.key, link );
    return;
  }

  if ( mId == "wikimedia" && r.tab == 0 && r.kind == List ) {
    potdTemplates( r, json );
    return;
  }

  OcsContentList items;
  int total = 0;
  bool more = false;
  bool ok = false;
  if ( mId == "wallhaven" )
    ok = parseWallhaven( r, json, items, &total, &more, &error );
  else if ( mId == "wikimedia" )
    ok = parseWikimediaFiles( r, json, items, &more, &error );
  else if ( mId == "bing" ) {
    ok = parseBing( json, items, &error );
    more = ( r.page == 0 );
  } else if ( mId == "apod" ) {
    if ( r.apodFeed )
      ok = parseApodFeed( r.data, items, &error );
    else {
      ok = parseApodApi( json, items, &error );
      if ( ok && items.isEmpty() ) {
        failed( r, i18n( "NASA's APOD API sent no usable pictures for these dates." ) );
        return;
      }
    }
    // older days come from the API, back to the first APOD
    more = ok && ( r.apodFeed || mApodOldest.addDays( -r.page * daysPerPage ) > QDate( 1995, 6, 20 ) );
  } else if ( mId == "unsplash" ) {
    ok = parseUnsplash( json, items, &error );
    more = int( items.count() ) >= 20 || ( r.tab == 1 && jsonNumber( json, "total_pages" ) > r.page + 1 );
    total = int( jsonNumber( json, "total" ) );
  }

  if ( !ok ) {
    failed( r, error.isEmpty() ? i18n( "The server sent an invalid response." ) : error );
    return;
  }
  emit contentsListed( r.tab, r.page, items, total, more );
}

// ------------------------------------------------------------------
// Wallhaven
// ------------------------------------------------------------------

bool WallpaperSource::parseWallhaven( const Request &, const QVariant &json, OcsContentList &items,
                                      int *total, bool *more, QString *error )
{
  if ( json.type() != QVariant::Map || jsonValue( json, "data" ).type() != QVariant::List ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  QValueList<QVariant> data = jsonList( json, "data" );
  int count = 0;
  for ( QValueList<QVariant>::ConstIterator it = data.begin(); it != data.end() && count < maxItems; ++it, ++count ) {
    const QVariant &w = *it;
    // safe for work only, whatever the server sends
    if ( jsonString( w, "purity" ) != "sfw" )
      continue;
    OcsContent c;
    c.id = jsonString( w, "id" );
    c.width = int( jsonNumber( w, "dimension_x" ) );
    c.height = int( jsonNumber( w, "dimension_y" ) );
    QString category = line( jsonString( w, "category" ), 30 );
    c.name = i18n( "Wallhaven %1" ).arg( c.id );
    if ( !category.isEmpty() )
      c.name += " (" + category + ")";
    c.summary = line( jsonString( w, "source" ), 300 );
    if ( !c.summary.isEmpty() )
      c.summary = i18n( "Source: %1" ).arg( c.summary );
    c.license = i18n( "Unknown (see the web page)" );
    c.changed = QDateTime::fromString( jsonString( w, "created_at" ).left( 19 ).replace( ' ', 'T' ), Qt::ISODate );
    c.downloads = QMAX( int( jsonNumber( w, "favorites" ) ), 0 );
    c.detailPage = KURL( jsonString( w, "url" ) );
    c.preview = KURL( jsonString( jsonValue( w, "thumbs" ), "small" ) );
    c.download = KURL( jsonString( w, "path" ) );
    OcsDownload d;
    d.number = 1;
    d.name = "wallhaven-" + c.id;
    d.mimeType = line( jsonString( w, "file_type" ), 50 );
    d.sizeKiB = QMAX( int( jsonNumber( w, "file_size" ) / 1024 ), 0 );
    c.files.append( d );
    addItem( c, items );
  }
  QVariant meta = jsonValue( json, "meta" );
  *total = QMAX( int( jsonNumber( meta, "total" ) ), 0 );
  *more = jsonNumber( meta, "current_page" ) < jsonNumber( meta, "last_page" );
  return true;
}

// ------------------------------------------------------------------
// Wikimedia Commons
// ------------------------------------------------------------------

// The files of the templates of the pictures of the day: then their data.
void WallpaperSource::potdTemplates( const Request &r, const QVariant &json )
{
  QMap<QString, QString> files;      // file title -> date
  QValueList<QVariant> pages = jsonList( jsonValue( json, "query" ), "pages" );
  QRegExp potd( "^Template:Potd/([0-9]{4}-[0-9]{2}-[0-9]{2})$" );
  for ( QValueList<QVariant>::ConstIterator it = pages.begin(); it != pages.end(); ++it ) {
    if ( potd.search( jsonString( *it, "title" ) ) != 0 )
      continue;
    QValueList<QVariant> images = jsonList( *it, "images" );
    if ( images.isEmpty() )
      continue;
    QString file = jsonString( images.first(), "title" );
    if ( file.startsWith( "File:" ) && file.length() < 300 && !files.contains( file ) )
      files.insert( file, potd.cap( 1 ) );
  }

  if ( files.isEmpty() ) {
    emit contentsListed( r.tab, r.page, OcsContentList(), 0, false );
    return;
  }

  Request next;
  next.kind = PotdFiles;
  next.tab = r.tab;
  next.page = r.page;
  next.key = 0;
  next.apodFeed = false;
  next.potd = files;
  next.url = KURL( "https://commons.wikimedia.org/w/api.php?action=query&format=json&formatversion=2"
                   "&prop=imageinfo&iiprop=url%7Csize%7Cmime%7Cextmetadata&iiurlwidth=330"
                   "&iiextmetadatafilter=Artist%7CLicenseShortName%7CLicenseUrl%7CObjectName%7CImageDescription%7CCredit"
                   "&titles=" + KURL::encode_string( QStringList( files.keys() ).join( "|" ) ) );
  queue( next );
}

bool WallpaperSource::parseWikimediaFiles( const Request &r, const QVariant &json, OcsContentList &items,
                                           bool *more, QString *error )
{
  QVariant query = jsonValue( json, "query" );
  if ( json.type() != QVariant::Map ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  QValueList<QVariant> pages = jsonList( query, "pages" );

  // the pictures of the day newest first
  QMap<QString, QVariant> byDate;
  int count = 0;
  for ( QValueList<QVariant>::ConstIterator it = pages.begin(); it != pages.end() && count < maxItems; ++it, ++count ) {
    if ( r.kind == PotdFiles ) {
      QString title = jsonString( *it, "title" );
      if ( r.potd.contains( title ) )
        byDate.insert( r.potd[ title ] + "/" + title, *it );
    } else {
      byDate.insert( QString().sprintf( "%04d", count ), *it );
    }
  }

  QValueList<QVariant> ordered = byDate.values();
  if ( r.kind == PotdFiles ) {
    QValueList<QVariant> reversed;
    for ( QValueList<QVariant>::ConstIterator it = ordered.begin(); it != ordered.end(); ++it )
      reversed.prepend( *it );
    ordered = reversed;
  }

  for ( QValueList<QVariant>::ConstIterator it = ordered.begin(); it != ordered.end(); ++it ) {
    const QVariant &p = *it;
    QValueList<QVariant> info = jsonList( p, "imageinfo" );
    if ( info.isEmpty() )
      continue;
    const QVariant &ii = info.first();
    QString mime = jsonString( ii, "mime" );
    if ( mime != "image/jpeg" && mime != "image/png" && mime != "image/tiff" && mime != "image/webp" )
      continue;
    QVariant meta = jsonValue( ii, "extmetadata" );

    OcsContent c;
    c.id = jsonString( p, "pageid" );
    QString title = jsonString( p, "title" );
    if ( title.startsWith( "File:" ) )
      title = title.mid( 5 );
    QString fileName = title;
    c.name = line( jsonString( jsonValue( meta, "ObjectName" ), "value" ), 200 );
    if ( c.name.isEmpty() ) {
      c.name = title;
      int dot = c.name.findRev( '.' );
      if ( dot > 0 )
        c.name.truncate( dot );
      c.name = line( c.name, 200 );
    }
    c.author = line( jsonString( jsonValue( meta, "Artist" ), "value" ), 200 );
    if ( c.author.isEmpty() )
      c.author = line( jsonString( jsonValue( meta, "Credit" ), "value" ), 200 );
    c.license = line( jsonString( jsonValue( meta, "LicenseShortName" ), "value" ), 100 );
    KURL licenseUrl( line( jsonString( jsonValue( meta, "LicenseUrl" ), "value" ), 300 ) );
    if ( licenseUrl.isValid() && ( licenseUrl.protocol() == "https" || licenseUrl.protocol() == "http" ) )
      c.licenseUrl = licenseUrl.url();
    c.summary = line( jsonString( jsonValue( meta, "ImageDescription" ), "value" ), 1000 );
    c.width = int( jsonNumber( ii, "width" ) );
    c.height = int( jsonNumber( ii, "height" ) );
    c.preview = KURL( jsonString( ii, "thumburl" ) );
    c.detailPage = KURL( jsonString( ii, "descriptionurl" ) );
    if ( r.kind == PotdFiles ) {
      QString date = r.potd[ jsonString( p, "title" ) ];
      c.changed = QDateTime( QDate::fromString( date, Qt::ISODate ) );
      c.version = date;
    } else {
      c.changed = QDateTime::fromString( jsonString( ii, "timestamp" ).left( 19 ), Qt::ISODate );
    }
    OcsDownload d;
    d.number = 1;
    d.name = fileName;
    d.mimeType = mime;
    d.sizeKiB = 0;
    c.files.append( d );
    addItem( c, items );
  }

  *more = false;
  if ( r.kind == PotdFiles ) {
    // back to the first picture of the day (November 2004)
    *more = utcToday().addDays( -( r.page + 1 ) * daysPerPage ) > QDate( 2004, 11, 28 );
  } else {
    QString cont = jsonString( jsonValue( json, "continue" ), "gcmcontinue" );
    if ( !cont.isEmpty() && cont.length() < 500 ) {
      mContinue[ r.page + 1 ] = cont;
      *more = true;
    }
  }
  return true;
}

// ------------------------------------------------------------------
// Bing
// ------------------------------------------------------------------

bool WallpaperSource::parseBing( const QVariant &json, OcsContentList &items, QString *error )
{
  if ( jsonValue( json, "images" ).type() != QVariant::List ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  QValueList<QVariant> images = jsonList( json, "images" );
  QRegExp base( "^/th\\?id=OHR\\.([A-Za-z0-9]+)_[A-Za-z0-9_\\-]+$" );
  int count = 0;
  for ( QValueList<QVariant>::ConstIterator it = images.begin(); it != images.end() && count < maxItems; ++it, ++count ) {
    const QVariant &b = *it;
    QString urlbase = jsonString( b, "urlbase" );
    QString date = jsonString( b, "startdate" );
    if ( base.search( urlbase ) != 0 || !QRegExp( "^[0-9]{8}$" ).exactMatch( date ) )
      continue;
    OcsContent c;
    c.id = date;
    QString copyright = line( jsonString( b, "copyright" ), 300 );
    // "Place, Country (© Author/Agency)"
    int open = copyright.findRev( "(" );
    QString place = open > 0 ? copyright.left( open ).stripWhiteSpace() : copyright;
    QString credit = open > 0 ? copyright.mid( open + 1 ) : QString::null;
    if ( credit.endsWith( ")" ) )
      credit.truncate( credit.length() - 1 );
    credit = credit.stripWhiteSpace();
    c.name = line( jsonString( b, "title" ), 200 );
    if ( c.name.isEmpty() || c.name == "Info" )
      c.name = place;
    c.summary = place;
    c.author = credit;
    if ( c.author.startsWith( QString( QChar( 0xa9 ) ) ) )
      c.author = c.author.mid( 1 ).stripWhiteSpace();
    c.license = credit.isEmpty() ? i18n( "Copyrighted, for use as wallpaper" )
                                 : i18n( "%1, for use as wallpaper" ).arg( credit );
    c.changed = QDateTime( QDate( date.left( 4 ).toInt(), date.mid( 4, 2 ).toInt(), date.mid( 6, 2 ).toInt() ) );
    c.version = c.changed.date().toString( Qt::ISODate );
    // Ultra HD (3840x2160)
    c.width = 3840;
    c.height = 2160;
    c.preview = KURL( "https://www.bing.com" + urlbase + "_400x240.jpg" );
    c.download = KURL( "https://www.bing.com" + urlbase + "_UHD.jpg" );
    c.detailPage = KURL( jsonString( b, "copyrightlink" ) );
    OcsDownload d;
    d.number = 1;
    d.name = "bing-" + c.version + "-" + base.cap( 1 );
    d.mimeType = "image/jpeg";
    d.sizeKiB = 0;
    c.files.append( d );
    addItem( c, items );
  }
  return true;
}

// ------------------------------------------------------------------
// NASA APOD
// ------------------------------------------------------------------

// The link of the original picture of an APOD feed item; the pictures of
// science.nasa.gov are also served scaled ("dynamicimage").
static KURL apodBase( const KURL &hd )
{
  // some links name a small rendition of the picture:
  // ".../picture.jpg/jcr:content/renditions/cq5dam.thumbnail.48.48.png"
  KURL u( hd );
  QString path = u.path();
  int rendition = path.find( "/jcr:content/" );
  if ( rendition > 0 ) {
    u.setPath( path.left( rendition ) );
    u.setQuery( QString::null );
  }
  return u;
}

static KURL apodOriginal( const KURL &hd )
{
  KURL u( apodBase( hd ) );
  QString path = u.path();
  if ( path.startsWith( "/dynamicimage/assets/" ) )
    path = "/content/dam/" + path.mid( 21 );
  u.setPath( path );
  u.setQuery( QString::null );
  return u;
}

static KURL apodPreview( const KURL &hd )
{
  KURL u( apodBase( hd ) );
  QString path = u.path();
  if ( u.host() == "assets.science.nasa.gov" ) {
    if ( path.startsWith( "/content/dam/" ) )
      path = "/dynamicimage/assets/" + path.mid( 13 );
    u.setPath( path );
    u.setQuery( "w=320&h=320&fit=clip" );
  }
  return u;
}

static QDate rfc822Date( const QString &s )
{
  // "Sat, 21 Nov 2026 05:05:00 +0000"
  static const char * const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  QStringList parts = QStringList::split( ' ', s.simplifyWhiteSpace() );
  if ( parts.count() < 4 )
    return QDate();
  int month = 0;
  for ( int i = 0; i < 12; ++i )
    if ( parts[ 2 ] == months[ i ] )
      month = i + 1;
  if ( !month )
    return QDate();
  return QDate( parts[ 3 ].toInt(), month, parts[ 1 ].toInt() );
}

static QString apodLicense( const QString &copyright )
{
  if ( copyright.isEmpty() )
    return i18n( "Public domain (NASA)" );
  return i18n( "Copyright: %1" ).arg( copyright );
}

bool WallpaperSource::parseApodFeed( const QByteArray &data, OcsContentList &items, QString *error )
{
  QDomDocument doc;
  if ( !parseUntrustedXml( data, &doc ) || doc.documentElement().tagName() != "rss" ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  QDomElement channel = doc.documentElement().namedItem( "channel" ).toElement();
  int count = 0;
  for ( QDomNode n = channel.firstChild(); !n.isNull() && count < maxItems; n = n.nextSibling() ) {
    QDomElement e = n.toElement();
    if ( e.tagName() != "item" )
      continue;
    ++count;
    QDate date = rfc822Date( e.namedItem( "pubDate" ).toElement().text() );
    KURL hd( apodBase( KURL( e.namedItem( "apod:hdurl" ).toElement().text().stripWhiteSpace() ) ) );
    if ( !date.isValid() )
      continue;
    if ( !mApodOldest.isValid() || date < mApodOldest )
      mApodOldest = date;
    // videos and other media have no picture link
    if ( !isAllowedUrl( hd ) || !isPictureName( hd ) )
      continue;

    OcsContent c;
    c.id = date.toString( "yyyyMMdd" );
    c.name = line( e.namedItem( "title" ).toElement().text(), 200 );
    QString copyright = line( e.namedItem( "apod:copyright" ).toElement().text(), 300 );
    QString credit = line( e.namedItem( "apod:credit" ).toElement().text(), 300 );
    c.author = !copyright.isEmpty() ? copyright : credit;
    c.license = apodLicense( copyright );
    QString explanation = line( e.namedItem( "apod:explanation" ).toElement().text(), 1000 );
    if ( explanation.startsWith( "Explanation:" ) )
      explanation = explanation.mid( 12 ).stripWhiteSpace();
    c.summary = explanation;
    c.changed = QDateTime( date );
    c.version = date.toString( Qt::ISODate );
    c.detailPage = KURL( e.namedItem( "link" ).toElement().text().stripWhiteSpace() );
    c.download = apodOriginal( hd );
    c.preview = apodPreview( hd );
    QRegExp size( "_([0-9]{2,5})x([0-9]{2,5})\\.[A-Za-z]+$" );
    if ( size.search( c.download.path() ) >= 0 ) {
      c.width = size.cap( 1 ).toInt();
      c.height = size.cap( 2 ).toInt();
    }
    OcsDownload d;
    d.number = 1;
    d.name = "apod-" + c.version + "-" + c.download.fileName();
    d.sizeKiB = 0;
    c.files.append( d );
    addItem( c, items );
  }
  return true;
}

bool WallpaperSource::parseApodApi( const QVariant &json, OcsContentList &items, QString *error )
{
  if ( json.type() != QVariant::List ) {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  QValueList<QVariant> list = json.toList();
  QValueList<QVariant> reversed;      // newest first
  int count = 0;
  for ( QValueList<QVariant>::ConstIterator it = list.begin(); it != list.end() && count < maxItems; ++it, ++count )
    reversed.prepend( *it );

  for ( QValueList<QVariant>::ConstIterator it = reversed.begin(); it != reversed.end(); ++it ) {
    const QVariant &a = *it;
    if ( jsonString( a, "media_type" ) != "image" )
      continue;
    QDate date = QDate::fromString( jsonString( a, "date" ).left( 10 ), Qt::ISODate );
    KURL hd( jsonString( a, "hdurl" ) );
    KURL url( jsonString( a, "url" ) );
    if ( !hd.isValid() )
      hd = url;
    // the API also answers with the NASA logo for days it could not read
    if ( !date.isValid() || !isAllowedUrl( hd ) || !isPictureName( hd ) ||
         hd.path().find( "nasa-logo" ) >= 0 || url.path().find( "nasa-logo" ) >= 0 )
      continue;
    OcsContent c;
    c.id = date.toString( "yyyyMMdd" );
    c.name = line( jsonString( a, "title" ), 200 );
    QString copyright = line( jsonString( a, "copyright" ), 300 );
    c.author = copyright;
    c.license = apodLicense( copyright );
    c.summary = line( jsonString( a, "explanation" ), 1000 );
    c.changed = QDateTime( date );
    c.version = date.toString( Qt::ISODate );
    c.detailPage = KURL( "https://apod.nasa.gov/apod/ap" + date.toString( "yyMMdd" ) + ".html" );
    c.download = hd.host() == "assets.science.nasa.gov" ? apodOriginal( hd ) : hd;
    c.preview = url.host() == "assets.science.nasa.gov" ? apodPreview( url ) : url;
    OcsDownload d;
    d.number = 1;
    d.name = "apod-" + c.version + "-" + c.download.fileName();
    d.sizeKiB = 0;
    c.files.append( d );
    addItem( c, items );
  }
  return true;
}

// ------------------------------------------------------------------
// Unsplash
// ------------------------------------------------------------------

bool WallpaperSource::parseUnsplash( const QVariant &json, OcsContentList &items, QString *error )
{
  QValueList<QVariant> photos;
  if ( json.type() == QVariant::List )
    photos = json.toList();
  else if ( jsonValue( json, "results" ).type() == QVariant::List )
    photos = jsonList( json, "results" );
  else {
    *error = i18n( "The server sent an invalid response." );
    return false;
  }
  int count = 0;
  for ( QValueList<QVariant>::ConstIterator it = photos.begin(); it != photos.end() && count < maxItems; ++it, ++count ) {
    const QVariant &u = *it;
    OcsContent c;
    c.id = jsonString( u, "id" );
    QVariant user = jsonValue( u, "user" );
    c.author = line( jsonString( user, "name" ), 200 );
    // Unsplash asks for "Photo by <name> on Unsplash" with links to the
    // photographer and to Unsplash, marked as referrals
    static const char utm[] = "utm_source=kde3_wallpapers&utm_medium=referral";
    KURL profile( jsonString( jsonValue( user, "links" ), "html" ) );
    if ( isAllowedUrl( profile ) ) {
      profile.setQuery( utm );
      c.authorUrl = profile.url();
    }
    c.summary = line( jsonString( u, "description" ), 1000 );
    QString alt = line( jsonString( u, "alt_description" ), 200 );
    c.name = alt.isEmpty() ? i18n( "Photo by %1" ).arg( c.author ) : alt;
    if ( c.name.length() > 1 )
      c.name[ 0 ] = c.name[ 0 ].upper();
    c.license = c.author.isEmpty() ? i18n( "Unsplash License" )
                                   : i18n( "Photo by %1 on Unsplash (Unsplash License)" ).arg( c.author );
    c.licenseUrl = "https://unsplash.com/license";
    c.width = int( jsonNumber( u, "width" ) );
    c.height = int( jsonNumber( u, "height" ) );
    c.changed = QDateTime::fromString( jsonString( u, "created_at" ).left( 19 ), Qt::ISODate );
    c.preview = KURL( jsonString( jsonValue( u, "urls" ), "small" ) );
    c.detailPage = KURL( jsonString( jsonValue( u, "links" ), "html" ) );
    if ( c.detailPage.isValid() )
      c.detailPage.setQuery( utm );
    c.download = KURL( jsonString( jsonValue( u, "links" ), "download_location" ) );
    if ( c.download.host() != "api.unsplash.com" )
      continue;
    OcsDownload d;
    d.number = 1;
    d.name = "unsplash-" + c.id;
    d.sizeKiB = 0;
    c.files.append( d );
    addItem( c, items );
  }
  return true;
}
