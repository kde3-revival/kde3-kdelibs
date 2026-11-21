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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <qbuffer.h>
#include <qtimer.h>
#include <qxml.h>
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
#include <kstaticdeleter.h>
#include <ktar.h>
#include <ktempfile.h>
#include <kzip.h>

using namespace KNS;

// Response size limits
static const uint maxListResponse = 2 * 1024 * 1024;   // a page is ~100 KB
static const uint maxDownloadResponse = 256 * 1024;
static const uint maxPreviewResponse = 4 * 1024 * 1024;
// Largest accepted preview picture
static const int maxPreviewDimension = 4096;
// Concurrent preview downloads, of all clients and sources together
static const int maxPreviewJobs = 4;

const Q_ULLONG OcsInstaller::maxDownloadSize = Q_ULLONG( 512 ) * 1024 * 1024;
static const Q_ULLONG maxExtractedFile = Q_ULLONG( 128 ) * 1024 * 1024;
// everything decompressed from one download, pictures or not
static const Q_ULLONG maxExtractedTotal = Q_ULLONG( 512 ) * 1024 * 1024;
// largest picture installed (width or height)
static const int maxPictureDimension = 16384;
// deepest element nesting accepted in a response (Qt's reader recurses)
static const int maxXmlDepth = 32;
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

// Checks the decoded document before it is parsed: it is refused if it has
// a DTD (internal entities could expand without bounds) or any other
// markup declaration, or if its elements nest deeper than maxXmlDepth
// (Qt's reader and QDom recurse per level).
static bool isAcceptableXml( const QString &text )
{
  const uint n = text.length();
  int depth = 0;
  uint i = 0;
  while ( i < n ) {
    if ( text[ i ] != '<' ) {
      ++i;
      continue;
    }
    if ( text.mid( i, 9 ) == "<![CDATA[" ) {
      int end = text.find( "]]>", i + 9 );
      if ( end < 0 ) return false;
      i = end + 3;
    } else if ( text.mid( i, 4 ) == "<!--" ) {
      int end = text.find( "-->", i + 4 );
      if ( end < 0 ) return false;
      i = end + 3;
    } else if ( i + 1 < n && text[ i + 1 ] == '!' ) {
      return false;                       // <!DOCTYPE, <!ENTITY, ...
    } else if ( i + 1 < n && text[ i + 1 ] == '?' ) {
      int end = text.find( "?>", i + 2 );
      if ( end < 0 ) return false;
      i = end + 2;
    } else if ( i + 1 < n && text[ i + 1 ] == '/' ) {
      --depth;
      int end = text.find( '>', i + 2 );
      if ( end < 0 ) return false;
      i = end + 1;
    } else {
      // a start tag; quoted attribute values may contain '>'
      uint j = i + 1;
      QChar quote;
      while ( j < n ) {
        QChar c = text[ j ];
        if ( !quote.isNull() ) {
          if ( c == quote ) quote = QChar();
        } else if ( c == '"' || c == '\'' ) {
          quote = c;
        } else if ( c == '>' ) {
          break;
        }
        ++j;
      }
      if ( j >= n ) return false;
      if ( text[ j - 1 ] != '/' && ++depth > maxXmlDepth )
        return false;
      i = j + 1;
    }
  }
  return true;
}

bool KNS::parseUntrustedXml( const QByteArray &data, QDomDocument *doc )
{
  // Decode the bytes the way the XML reader does (UTF-8, or UTF-16 with a
  // byte order mark, or the declared encoding), so that the checks see
  // what the parser will see.
  QByteArray copy( data );
  copy.detach();
  QBuffer buffer( copy );
  buffer.open( IO_ReadOnly );
  QXmlInputSource source( &buffer );
  QString text = source.data();
  if ( text.find( QChar( 0 ) ) >= 0 || !isAcceptableXml( text ) )
    return false;

  QString message;
  int line, column;
  if ( !doc->setContent( text, false, &message, &line, &column ) ) {
    kdDebug() << "OCS: parse error " << message << " at " << line << ":" << column << endl;
    return false;
  }
  return true;
}

// Parses an OCS response; returns the <data> element.
static bool parseOcsDocument( const QByteArray &data, QDomElement *dataElement,
                              int *total, QString *error )
{
  QDomDocument doc;
  if ( !parseUntrustedXml( data, &doc ) ) {
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

// The bit counts of the U32 of a JPEG XL SizeHeader:
// U32( 1 + u(9), 1 + u(13), 1 + u(18), 1 + u(30) )
static const int jxlSizeBits[ 4 ] = { 9, 13, 18, 30 };

// Reads the bits of a JPEG XL codestream, least significant bit first.
namespace {
struct JxlBits
{
  JxlBits( const uchar *data, uint size ) : p( data ), n( size ), pos( 0 ), ok( true ) {}
  uint read( int count )
  {
    uint v = 0;
    for ( int i = 0; i < count; ++i, ++pos ) {
      if ( ( pos >> 3 ) >= n ) {
        ok = false;
        return 0;
      }
      v |= uint( ( p[ pos >> 3 ] >> ( pos & 7 ) ) & 1 ) << i;
    }
    return v;
  }
  // the U32 of a size in the SizeHeader
  Q_ULLONG size()
  {
    return Q_ULLONG( read( jxlSizeBits[ read( 2 ) ] ) ) + 1;
  }
  const uchar *p;
  uint n;
  uint pos;
  bool ok;
};
}

// The size of a JPEG XL picture, from the SizeHeader of its codestream
// (ISO/IEC 18181-1), bare or in the ISO BMFF container.
static bool jxlSize( const uchar *p, uint n, int *width, int *height )
{
  if ( n >= 12 && memcmp( p, "\0\0\0\x0cJXL \r\n\x87\n", 12 ) == 0 ) {
    // the container: find the codestream box (jxlc, or the first jxlp)
    uint i = 0;
    const uchar *cs = 0;
    uint csSize = 0;
    while ( i + 8 <= n ) {
      Q_ULLONG boxSize = ( uint( p[ i ] ) << 24 ) | ( p[ i + 1 ] << 16 ) | ( p[ i + 2 ] << 8 ) | p[ i + 3 ];
      uint header = 8;
      if ( boxSize == 1 ) {               // 64-bit size
        if ( i + 16 > n ) return false;
        boxSize = 0;
        for ( int k = 0; k < 8; ++k )
          boxSize = ( boxSize << 8 ) | p[ i + 8 + k ];
        header = 16;
      } else if ( boxSize == 0 ) {        // up to the end of the file
        boxSize = n - i;
      }
      if ( boxSize < header ) return false;
      // the codestream: up to the end of the box (or of what was read)
      const uint payload = uint( QMIN( boxSize, Q_ULLONG( n - i ) ) ) - header;
      if ( memcmp( p + i + 4, "jxlc", 4 ) == 0 ) {
        cs = p + i + header;
        csSize = payload;
        break;
      }
      if ( memcmp( p + i + 4, "jxlp", 4 ) == 0 ) {
        if ( payload < 4 ) return false;
        cs = p + i + header + 4;          // after the index of the part
        csSize = payload - 4;
        break;
      }
      if ( boxSize > n - i ) return false;
      i += boxSize;
    }
    if ( !cs ) return false;
    p = cs;
    n = csSize;
  }
  if ( n < 2 || p[ 0 ] != 0xff || p[ 1 ] != 0x0a )
    return false;

  JxlBits bits( p + 2, n - 2 );
  const bool small = bits.read( 1 );
  Q_ULLONG h = small ? Q_ULLONG( bits.read( 5 ) + 1 ) * 8 : bits.size();
  const uint ratio = bits.read( 3 );
  Q_ULLONG w;
  if ( ratio == 0 ) {
    w = small ? Q_ULLONG( bits.read( 5 ) + 1 ) * 8 : bits.size();
  } else {
    static const uint ratios[ 8 ][ 2 ] = { { 1, 1 }, { 1, 1 }, { 12, 10 }, { 4, 3 },
                                           { 3, 2 }, { 16, 9 }, { 5, 4 }, { 2, 1 } };
    w = h * ratios[ ratio ][ 0 ] / ratios[ ratio ][ 1 ];
  }
  if ( !bits.ok || w > 0x7fffffff || h > 0x7fffffff )
    return false;
  *width = int( w );
  *height = int( h );
  return true;
}

// The size of an AVIF picture: the largest of the image spatial extents
// ("ispe" properties) in its header, which covers a grid of tiles too.
static bool avifSize( const uchar *p, uint n, int *width, int *height )
{
  uint w = 0, h = 0;
  bool found = false;
  // box: size (4), "ispe", version and flags (4), width (4), height (4)
  for ( uint i = 4; i + 16 <= n; ++i ) {
    if ( p[ i ] != 'i' || memcmp( p + i, "ispe", 4 ) != 0 )
      continue;
    uint boxSize = ( uint( p[ i - 4 ] ) << 24 ) | ( p[ i - 3 ] << 16 ) | ( p[ i - 2 ] << 8 ) | p[ i - 1 ];
    if ( boxSize != 20 )
      continue;
    uint bw = ( uint( p[ i + 8 ] ) << 24 ) | ( p[ i + 9 ] << 16 ) | ( p[ i + 10 ] << 8 ) | p[ i + 11 ];
    uint bh = ( uint( p[ i + 12 ] ) << 24 ) | ( p[ i + 13 ] << 16 ) | ( p[ i + 14 ] << 8 ) | p[ i + 15 ];
    w = QMAX( w, bw );
    h = QMAX( h, bh );
    found = true;
  }
  if ( !found || w > 0x7fffffff || h > 0x7fffffff )
    return false;
  *width = int( w );
  *height = int( h );
  return true;
}

// The width and height of a picture, read from its header, so that a
// picture is never decoded before its size is known.
static bool pictureSize( const QByteArray &data, int *width, int *height )
{
  const uchar *p = reinterpret_cast<const uchar *>( data.data() );
  uint n = data.size();

  if ( n >= 30 && memcmp( p, "RIFF", 4 ) == 0 && memcmp( p + 8, "WEBP", 4 ) == 0 ) {
    if ( memcmp( p + 12, "VP8X", 4 ) == 0 ) {   // extended: canvas size - 1
      *width = ( p[ 24 ] | ( p[ 25 ] << 8 ) | ( p[ 26 ] << 16 ) ) + 1;
      *height = ( p[ 27 ] | ( p[ 28 ] << 8 ) | ( p[ 29 ] << 16 ) ) + 1;
      return true;
    }
    if ( memcmp( p + 12, "VP8 ", 4 ) == 0 ) {   // lossy: after the frame tag and start code
      if ( p[ 23 ] != 0x9d || p[ 24 ] != 0x01 || p[ 25 ] != 0x2a ) return false;
      *width = ( p[ 26 ] | ( p[ 27 ] << 8 ) ) & 0x3fff;
      *height = ( p[ 28 ] | ( p[ 29 ] << 8 ) ) & 0x3fff;
      return true;
    }
    if ( memcmp( p + 12, "VP8L", 4 ) == 0 ) {   // lossless: 14 bits each, size - 1
      if ( p[ 20 ] != 0x2f ) return false;
      uint bits = p[ 21 ] | ( p[ 22 ] << 8 ) | ( p[ 23 ] << 16 ) | ( uint( p[ 24 ] ) << 24 );
      *width = ( bits & 0x3fff ) + 1;
      *height = ( ( bits >> 14 ) & 0x3fff ) + 1;
      return true;
    }
    return false;
  }

  if ( n >= 12 && memcmp( p + 4, "ftypavi", 7 ) == 0 )
    return avifSize( p, n, width, height );

  if ( ( n >= 2 && p[ 0 ] == 0xff && p[ 1 ] == 0x0a ) ||
       ( n >= 12 && memcmp( p, "\0\0\0\x0cJXL ", 8 ) == 0 ) )
    return jxlSize( p, n, width, height );

  if ( n >= 24 && p[ 0 ] == 0x89 && p[ 1 ] == 'P' && p[ 2 ] == 'N' && p[ 3 ] == 'G' ) {
    uint w = ( uint( p[ 16 ] ) << 24 ) | ( p[ 17 ] << 16 ) | ( p[ 18 ] << 8 ) | p[ 19 ];
    uint h = ( uint( p[ 20 ] ) << 24 ) | ( p[ 21 ] << 16 ) | ( p[ 22 ] << 8 ) | p[ 23 ];
    if ( w > 0x7fffffff || h > 0x7fffffff ) return false;
    *width = w;
    *height = h;
    return true;
  }

  if ( n >= 26 && p[ 0 ] == 'B' && p[ 1 ] == 'M' ) {
    uint headerSize = p[ 14 ] | ( p[ 15 ] << 8 ) | ( p[ 16 ] << 16 ) | ( uint( p[ 17 ] ) << 24 );
    if ( headerSize == 12 ) {             // OS/2 BITMAPCOREHEADER
      *width = p[ 18 ] | ( p[ 19 ] << 8 );
      *height = p[ 20 ] | ( p[ 21 ] << 8 );
      return true;
    }
    int w = int( p[ 18 ] | ( p[ 19 ] << 8 ) | ( p[ 20 ] << 16 ) | ( uint( p[ 21 ] ) << 24 ) );
    int h = int( p[ 22 ] | ( p[ 23 ] << 8 ) | ( p[ 24 ] << 16 ) | ( uint( p[ 25 ] ) << 24 ) );
    if ( h < 0 && h != INT_MIN ) h = -h;  // top-down bitmap
    *width = w;
    *height = h;
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

bool KNS::imageHeaderSize( const QByteArray &data, int *width, int *height )
{
  return pictureSize( data, width, height );
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
// PreviewSlots
// ------------------------------------------------------------------

static PreviewSlots *s_previewSlots = 0;
static KStaticDeleter<PreviewSlots> s_previewSlotsDeleter;

PreviewSlots *PreviewSlots::self()
{
  if ( !s_previewSlots )
    s_previewSlotsDeleter.setObject( s_previewSlots, new PreviewSlots );
  return s_previewSlots;
}

PreviewSlots::PreviewSlots()
  : QObject( 0, "KNS::PreviewSlots" ), mRunning( 0 ), mPending( false )
{
}

bool PreviewSlots::take()
{
  if ( mRunning >= maxPreviewJobs )
    return false;
  ++mRunning;
  return true;
}

void PreviewSlots::release()
{
  if ( mRunning > 0 )
    --mRunning;
  if ( !mPending ) {
    mPending = true;
    QTimer::singleShot( 0, this, SLOT( slotReleased() ) );
  }
}

void PreviewSlots::slotReleased()
{
  mPending = false;
  emit released();
}

// ------------------------------------------------------------------
// OcsClient
// ------------------------------------------------------------------

OcsClient::OcsClient( const KURL &base, const QString &categories, QObject *parent )
  : QObject( parent ), mBase( base ), mPreviewsRunning( 0 )
{
  mBase.adjustPath( +1 );
  mWatchdog = new QTimer( this );
  connect( mWatchdog, SIGNAL( timeout() ), SLOT( slotWatchdog() ) );
  mClock.start();
  connect( PreviewSlots::self(), SIGNAL( released() ), SLOT( slotStartPreviews() ) );

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
  // nothing more is started for this client; its preview slots are given
  // back to the others
  disconnect( PreviewSlots::self(), 0, this, 0 );
  mPreviewQueue.clear();
  QMap<KIO::Job *, Request> requests = mRequests;
  mRequests.clear();
  QMap<KIO::Job *, Request>::Iterator it;
  for ( it = requests.begin(); it != requests.end(); ++it )
    it.key()->kill( true );
  for ( ; mPreviewsRunning > 0; --mPreviewsRunning )
    PreviewSlots::self()->release();
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
  // HTTP errors are errors, not data; anonymous: no cookies; a certificate
  // that does not validate fails the job instead of asking the user
  job->addMetaData( "errorPage", "false" );
  job->addMetaData( "cookies", "none" );
  job->addMetaData( "ssl_militant", "TRUE" );
  connect( job, SIGNAL( redirection( KIO::Job *, const KURL & ) ),
           SLOT( slotRedirection( KIO::Job *, const KURL & ) ) );
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
  addRequest( startJob( url, true ), r );
}

void OcsClient::addRequest( KIO::TransferJob *job, Request &r )
{
  r.active = mClock.elapsed();
  mRequests.insert( job, r );
  if ( !mWatchdog->isActive() )
    mWatchdog->start( 5000 );
}

// Cancels the requests that got no data for requestTimeout seconds.
void OcsClient::slotWatchdog()
{
  int now = mClock.elapsed();
  QMap<KIO::Job *, Request>::Iterator it = mRequests.begin();
  while ( it != mRequests.end() ) {
    if ( now - it.data().active < requestTimeout * 1000 && now >= it.data().active ) {
      ++it;
      continue;
    }
    KIO::Job *job = it.key();
    Request r = it.data();
    mRequests.remove( it );
    job->kill( true );
    failed( r, i18n( "The server did not answer in time." ) );
    it = mRequests.begin();         // failed() may have started others
  }
  if ( mRequests.isEmpty() )
    mWatchdog->stop();
}

void OcsClient::cancelDownload()
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.begin();
  while ( it != mRequests.end() ) {
    if ( it.data().kind == Download ) {
      it.key()->kill( true );
      mRequests.remove( it );
      it = mRequests.begin();
    } else
      ++it;
  }
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
  addRequest( startJob( url, true ), r );
}

void OcsClient::fetchPreview( const KURL &url, int key )
{
  if ( !isAllowedUrl( url ) )
    return;
  QMap<QString, QValueList<int> >::Iterator it = mPreviewKeys.find( url.url() );
  if ( it != mPreviewKeys.end() ) {
    // queued or loading already
    it.data().append( key );
    return;
  }
  mPreviewKeys[ url.url() ].append( key );
  mPreviewQueue.append( url );
  slotStartPreviews();
}

void OcsClient::slotStartPreviews()
{
  while ( !mPreviewQueue.isEmpty() && PreviewSlots::self()->take() ) {
    KURL url = mPreviewQueue.first();
    mPreviewQueue.remove( mPreviewQueue.begin() );

    Request r;
    r.kind = Preview;
    r.sortMode = 0;
    r.page = 0;
    r.number = 0;
    r.key = 0;
    r.tooLarge = false;
    r.url = url;
    ++mPreviewsRunning;
    addRequest( startJob( url, false ), r );
  }
}

// A preview ended, loaded or not: its slot is given back.
void OcsClient::previewDone( const Request &r, const QImage &image )
{
  QValueList<int> keys = mPreviewKeys[ r.url.url() ];
  mPreviewKeys.remove( r.url.url() );
  if ( !image.isNull() )
    for ( QValueList<int>::ConstIterator it = keys.begin(); it != keys.end(); ++it )
      emit previewLoaded( *it, image );
  --mPreviewsRunning;
  PreviewSlots::self()->release();
}

void OcsClient::slotData( KIO::Job *job, const QByteArray &data )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() || data.isEmpty() )
    return;

  Request &r = it.data();
  r.active = mClock.elapsed();
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

void OcsClient::slotRedirection( KIO::Job *job, const KURL &url )
{
  QMap<KIO::Job *, Request>::Iterator it = mRequests.find( job );
  if ( it == mRequests.end() || isAllowedUrl( url ) )
    return;
  Request r = it.data();
  mRequests.remove( it );
  job->kill( true );
  failed( r, i18n( "The server redirected to a location that is not allowed." ) );
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
      int width = 0, height = 0;
      QImage image;
      if ( !( pictureSize( r.data, &width, &height ) &&
              width > 0 && height > 0 &&
              width <= maxPreviewDimension && height <= maxPreviewDimension &&
              image.loadFromData( r.data ) ) )
        image = QImage();
      previewDone( r, image );
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
      previewDone( r, QImage() );
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

  // a full page (counting the items left out) means there may be more
  emit contentsListed( r.sortMode, r.page, list, total < 0 ? 0 : total,
                       count >= pageSize() );
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
// leading dots (hidden files, "." and ".."), at most 200 bytes encoded
// (file systems allow 255, and KSaveFile adds a suffix while writing).
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
  if ( QFile::encodeName( s ).length() > 200 ) {
    int dot = s.findRev( '.' );
    QString ext = ( dot > 0 && s.length() - dot <= 10 ) ? s.mid( dot ) : QString::null;
    QString base = s.left( s.length() - ext.length() );
    while ( !base.isEmpty() && QFile::encodeName( base + ext ).length() > 200 )
      base.truncate( base.length() - 1 );
    s = base + ext;
  }
  if ( s.isEmpty() )
    s = fallback;
  return s;
}

// The picture formats that are installed: the common, well-kept decoders
// only (kimgio also reads EPS through Ghostscript and many old formats).
// The desktop (kdesktop's background renderer) reads them all through
// QImage and kimgio.
static const char * const installableFormats[] = { "JPEG", "PNG", "GIF", "BMP",
                                                   "WEBP", "AVIF", "JXL", 0 };

// The format of a picture that may be installed, from its first bytes, or
// 0. Its size must be known from its header and be sane.
static const char *installableFormat( const QByteArray &head )
{
  QBuffer buffer( head );
  buffer.open( IO_ReadOnly );
  const char *format = QImageIO::imageFormat( &buffer );
  if ( !format )
    return 0;
  bool known = false;
  for ( int i = 0; installableFormats[ i ]; ++i )
    if ( qstrcmp( format, installableFormats[ i ] ) == 0 )
      known = true;
  if ( !known )
    return 0;
  int width = 0, height = 0;
  if ( !pictureSize( head, &width, &height ) ||
       width <= 0 || height <= 0 ||
       width > maxPictureDimension || height > maxPictureDimension )
    return 0;
  return format;
}

// The first bytes of a file: enough for any picture header (a JPEG's size
// may follow its EXIF and colour profile segments).
static const uint headSize = 512 * 1024;

static QByteArray readHead( QIODevice *dev, uint size )
{
  QByteArray head( size );
  uint got = 0;
  while ( got < size ) {
    Q_LONG n = dev->readBlock( head.data() + got, size - got );
    if ( n <= 0 )
      break;
    got += n;
  }
  head.resize( got );
  return head;
}

static const char *installableFormat( const QString &file )
{
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return 0;
  return installableFormat( readHead( &f, headSize ) );
}

// The file name with an extension that matches the picture format.
static QString imageFileName( const QString &name, const char *format )
{
  QString fmt = QString::fromLatin1( format ).lower();
  QStringList exts;
  if ( fmt == "jpeg" )
    exts << "jpg" << "jpeg" << "jpe";
  else
    exts << fmt;

  int dot = name.findRev( '.' );
  QString ext = dot > 0 ? name.mid( dot + 1 ).lower() : QString::null;
  if ( exts.contains( ext ) )
    return name;
  return name + "." + exts.first();
}

// Copies @p head, then the rest of @p source, into @p path. KSaveFile
// writes a temporary file next to it and renames it over @p path.
static bool saveDevice( const QByteArray &head, QIODevice *source, const QString &path, Q_ULLONG limit )
{
  KSaveFile out( path, 0644 );
  if ( out.status() != 0 || !out.file() )
    return false;

  Q_ULLONG total = head.size();
  if ( total > limit ||
       ( head.size() && out.file()->writeBlock( head.data(), head.size() ) != (Q_LONG)head.size() ) ) {
    out.abort();
    return false;
  }

  char buffer[ 65536 ];
  for ( ;; ) {
    Q_LONG n = source ? source->readBlock( buffer, sizeof( buffer ) ) : 0;
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
  if ( !content.licenseUrl.isEmpty() )
    cfg.writeEntry( "LicenseUrl", content.licenseUrl );
  if ( !content.authorUrl.isEmpty() )
    cfg.writeEntry( "AuthorUrl", content.authorUrl );
  if ( content.detailPage.isValid() )
    cfg.writeEntry( "Url", content.detailPage.url() );
  if ( !content.source.isEmpty() )
    cfg.writeEntry( "Source", content.source );
  // the size of the picture installed (a source may send a smaller one
  // than the original it lists)
  {
    QFile f( imagePath );
    int width = 0, height = 0;
    if ( f.open( IO_ReadOnly ) && pictureSize( readHead( &f, headSize ), &width, &height ) &&
         width > 0 && height > 0 )
      cfg.writeEntry( "Resolution", QString( "%1x%2" ).arg( width ).arg( height ) );
  }
  if ( !content.source.isEmpty() && !content.summary.isEmpty() )
    cfg.writeEntry( "Comment", content.summary );
  cfg.sync();
}

// Whether a path exists, also as a dangling symbolic link.
static bool pathExists( const QString &path )
{
  struct stat st;
  return lstat( QFile::encodeName( path ), &st ) == 0;
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
  QString dir;          // where the pictures go
  QString prefix;       // prepended to their names
  bool zip;
  QStringList *installed;
  QStringList created;  // files that did not exist before
  QStringList usedNames;
  Q_ULLONG total;       // bytes decompressed, pictures or not
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

    // Only the header is read to tell a picture: the rest of other files
    // is never decompressed.
    QByteArray head = readHead( dev, headSize );
    x.total += head.size();
    const char *format = x.total <= maxExtractedTotal ? installableFormat( head ) : 0;
    if ( !format ) {
      delete dev;
      if ( x.total > maxExtractedTotal )
        x.error = i18n( "The archive is too large." );
      continue;
    }

    if ( x.installed->count() >= uint( maxArchiveImages ) ) {
      delete dev;
      x.error = i18n( "The archive has too many pictures." );
      return;
    }

    QString name = imageFileName( safeFileName( x.prefix + *it, "image" ), format );
    if ( x.usedNames.contains( name.lower() ) ) {
      int dot = name.findRev( '.' );
      QString base = name.left( dot ), ext = name.mid( dot );
      for ( int i = 2; x.usedNames.contains( name.lower() ); ++i )
        name = QString( "%1-%2%3" ).arg( base ).arg( i ).arg( ext );
    }
    x.usedNames.append( name.lower() );

    QString path = x.dir + name;
    bool existed = pathExists( path );
    Q_ULLONG limit = QMIN( maxExtractedFile, maxExtractedTotal - ( x.total - head.size() ) );
    bool ok = saveDevice( head, dev, path, limit );
    delete dev;
    if ( !ok ) {
      x.error = i18n( "Could not write the file '%1', or the archive is too large." ).arg( path );
      return;
    }
    x.total += QFileInfo( path ).size() - head.size();
    if ( !existed )
      x.created.append( path );
    x.installed->append( path );
  }
}

// The mime type of the compression of a file, from its first 6 bytes, or
// QString::null.
static QString compressionMimeType( const uchar *magic )
{
  if ( magic[ 0 ] == 0x1f && magic[ 1 ] == 0x8b )
    return "application/x-gzip";
  if ( magic[ 0 ] == 'B' && magic[ 1 ] == 'Z' && magic[ 2 ] == 'h' )
    return "application/x-bzip2";
  if ( memcmp( magic, "\3757zXZ\0", 6 ) == 0 )
    return "application/x-xz";
  if ( memcmp( magic, "\050\265\057\375", 4 ) == 0 )
    return "application/zstd";
  if ( memcmp( magic, "\004\042\115\030", 4 ) == 0 )
    return "application/x-lz4";
  return QString::null;
}

// Decompresses a gzip, bzip2, xz, zstd or lz4 file into @p out.
static bool decompress( const QString &file, const QString &mimeType, KTempFile &out, Q_ULLONG limit )
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
    if ( total > limit || f->writeBlock( chunk, n ) != n )
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

// An upper bound of the entries KTar or KZip would create for @p file,
// counted before the archive is opened (opening builds every entry):
// tar headers have a valid checksum, zip entries start with a local
// header signature. Stops counting past @p limit.
static int countArchiveEntries( const QString &file, bool zip, int limit )
{
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return limit + 1;

  int count = 0;
  char block[ 512 ];
  if ( zip ) {
    // a signature may straddle two reads: keep the last 3 bytes
    char buffer[ 65536 + 3 ];
    uint kept = 0;
    for ( ;; ) {
      Q_LONG n = f.readBlock( buffer + kept, 65536 );
      if ( n <= 0 )
        break;
      uint size = kept + n;
      for ( uint i = 0; i + 3 < size; ++i )
        if ( buffer[ i ] == 'P' && buffer[ i + 1 ] == 'K' && buffer[ i + 2 ] == 3 && buffer[ i + 3 ] == 4 )
          if ( ++count > limit )
            return count;
      kept = QMIN( size, 3u );
      memmove( buffer, buffer + size - kept, kept );
    }
    return count;
  }

  while ( f.readBlock( block, sizeof( block ) ) == 512 ) {
    // the checksum as KTar computes it: the field itself counts as spaces
    int check = 0;
    for ( int i = 0; i < 512; ++i )
      check += ( i >= 148 && i < 156 ) ? ' ' : uchar( block[ i ] );
    char field[ 9 ];
    memcpy( field, block + 148, 8 );
    field[ 8 ] = 0;
    char *end;
    long stored = strtol( field, &end, 8 );
    if ( end != field && stored == check && ++count > limit )
      return count;
  }
  return count;
}

static QString installImage( const QString &file, const char *format, const QString &dir,
                             bool wallpaper, const OcsContent &content,
                             const OcsDownload &download, QWidget *parent,
                             QStringList *installed )
{
  QString name = imageFileName( safeFileName( download.name, "ocs-" + content.id ), format );
  QString path = dir + name;

  if ( pathExists( path ) ) {
    int result = KMessageBox::warningContinueCancel( parent,
        "<qt>" + i18n( "The file '%1' already exists. Do you want to overwrite it?" ).arg( QStyleSheet::escape( path ) ) + "</qt>",
        QString::null, i18n( "Overwrite" ) );
    if ( result == KMessageBox::Cancel )
      return QString::null;
  }

  QFile source( file );
  if ( !source.open( IO_ReadOnly ) ||
       !saveDevice( QByteArray(), &source, path, OcsInstaller::maxDownloadSize ) )
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

  // The type is told from the content, not from the name. Archives are
  // checked first: the SVG reader claims any gzip file.
  QFile f( file );
  if ( !f.open( IO_ReadOnly ) )
    return i18n( "Could not read the downloaded file." );
  uchar magic[ 6 ];
  Q_LONG n = f.readBlock( reinterpret_cast<char *>( magic ), sizeof( magic ) );
  f.close();
  if ( n < 6 )
    return i18n( "The downloaded file is neither a picture nor a supported archive." );

  KTempFile plain( locateLocal( "tmp", "knewstuff-ocs-" ), ".tar" );
  plain.setAutoDelete( true );

  QString archiveFile;
  bool zip = false;
  if ( magic[ 0 ] == 'P' && magic[ 1 ] == 'K' && magic[ 2 ] == 3 && magic[ 3 ] == 4 ) {
    archiveFile = file;
    zip = true;
  } else if ( !compressionMimeType( magic ).isNull() ) {
    if ( !decompress( file, compressionMimeType( magic ), plain, maxExtractedTotal ) )
      return i18n( "The downloaded archive is damaged or too large." );
    if ( isTar( plain.name() ) ) {
      archiveFile = plain.name();
    } else {
      // a single compressed picture
      const char *format = installableFormat( plain.name() );
      if ( !format )
        return i18n( "The downloaded file is neither a picture nor a supported archive." );
      OcsDownload d = download;
      d.name.replace( QRegExp( "\\.(gz|bz2|xz|zst|lz4)$", false ), QString::null );
      return installImage( plain.name(), format, target, wallpaper, content, d, parent, installed );
    }
  } else if ( isTar( file ) ) {
    archiveFile = file;
  } else {
    const char *format = installableFormat( file );
    if ( format )
      return installImage( file, format, target, wallpaper, content, download, parent, installed );
    return i18n( "The downloaded file is neither a picture in a supported format "
                 "(JPEG, PNG, GIF, BMP, WebP, AVIF, JPEG XL) nor a supported archive." );
  }

  if ( countArchiveEntries( archiveFile, zip, maxArchiveEntries ) > maxArchiveEntries )
    return i18n( "The archive has too many entries." );

  KArchive *archive = zip ? static_cast<KArchive *>( new KZip( archiveFile ) )
                          : static_cast<KArchive *>( new KTar( archiveFile, "application/x-tar" ) );
  if ( !archive->open( IO_ReadOnly ) || !archive->directory() ) {
    delete archive;
    return i18n( "The downloaded archive is damaged." );
  }

  // Wallpapers go next to the others, named after the item (the
  // background module does not look into folders); other content goes
  // into a folder named after the item.
  QString itemName = safeFileName( content.name, "ocs-" + content.id );
  QString folder = target;
  QString prefix;
  bool created = false;
  if ( wallpaper ) {
    prefix = itemName + " - ";
  } else {
    folder = target + itemName + "/";
    if ( pathExists( folder ) ) {
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
  }

  Extraction x;
  x.dir = folder;
  x.prefix = prefix;
  x.zip = zip;
  x.installed = installed;
  x.total = 0;
  x.entries = 0;
  extractImages( archive->directory(), 0, x );
  archive->close();
  delete archive;

  if ( x.error.isEmpty() && installed->isEmpty() )
    x.error = i18n( "The archive does not contain any pictures in a supported format "
                    "(JPEG, PNG, GIF, BMP, WebP, AVIF, JPEG XL)." );

  if ( !x.error.isEmpty() ) {
    // only what this installation created; overwritten files stay
    for ( QStringList::ConstIterator it = x.created.begin(); it != x.created.end(); ++it )
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
        QString base = QFileInfo( *it ).baseName( true ).mid( prefix.length() );
        base.replace( '_', ' ' );
        caption += " - " + base;
      }
      writeWallpaperDesktop( *it, caption, content );
    }
  }

  return QString::null;
}

QString OcsInstaller::installPicture( const QString &file, const QString &dir,
                                      const OcsContent &content, const OcsDownload &download,
                                      QString *installed )
{
  KImageIO::registerFormats();

  QString target = dir;
  if ( !target.endsWith( "/" ) )
    target += '/';

  const char *format = installableFormat( file );
  // kdesktop decodes it in its own process: at most 8192 pixels a side
  if ( format ) {
    QFile f( file );
    int width = 0, height = 0;
    if ( !f.open( IO_ReadOnly ) || !pictureSize( readHead( &f, headSize ), &width, &height ) ||
         width > 8192 || height > 8192 )
      format = 0;
  }
  if ( !format )
    return i18n( "The downloaded file is not a picture in a supported format "
                 "(JPEG, PNG, GIF, BMP, WebP, AVIF, JPEG XL), or it is too large." );

  QString path = target + imageFileName( safeFileName( download.name, "picture" ), format );
  QFile source( file );
  if ( !source.open( IO_ReadOnly ) ||
       !saveDevice( QByteArray(), &source, path, maxDownloadSize ) )
    return i18n( "Could not write the file '%1'." ).arg( path );

  writeWallpaperDesktop( path, content.name, content );
  if ( installed )
    *installed = path;
  return QString::null;
}
