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

#include "dailypicture.h"
#include "dailypicture.moc"

#include "ocs.h"
#include "wallpapersources.h"

#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>

#include <kdebug.h>
#include <kio/job.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <ktempfile.h>

using namespace KNS;

class DailyPicture::Private
{
  public:
    Private() : source( 0 ), job( 0 ), temp( 0 ), written( 0 ), tooLarge( false ),
                badRedirect( false ), keep( 7 ) {}
    WallpaperSource *source;
    KIO::TransferJob *job;
    KTempFile *temp;
    Q_ULLONG written;
    bool tooLarge;
    bool badRedirect;
    QString sourceId;
    QString dir;
    int keep;
    OcsContent content;
    QString date;
};

// "2026-11-21.jpg": a picture of the day in its folder
static QRegExp pictureName()
{
  return QRegExp( "^[0-9]{4}-[0-9]{2}-[0-9]{2}\\.(jpg|jpeg|jpe|png|gif|bmp|webp|avif|jxl)$" );
}

static QStringList pictures( const QString &directory )
{
  QDir dir( directory );
  QStringList files = dir.entryList( QDir::Files, QDir::Name );
  QStringList result;
  QRegExp re = pictureName();
  for ( QStringList::ConstIterator it = files.begin(); it != files.end(); ++it )
    if ( re.exactMatch( *it ) )
      result.append( *it );
  result.sort();
  return result;
}

DailyPicture::DailyPicture( QObject *parent, const char *name )
  : QObject( parent, name ), d( new Private )
{
}

DailyPicture::~DailyPicture()
{
  cancel();
  delete d;
}

QStringList DailyPicture::sources()
{
  QStringList list;
  list << "bing" << "wikimedia" << "apod";
  return list;
}

QString DailyPicture::sourceName( const QString &source )
{
  if ( source == "wikimedia" )
    return i18n( "Wikimedia Commons Picture of the Day" );
  return WallpaperSource::displayName( source );
}

QString DailyPicture::latestPicture( const QString &directory )
{
  QStringList list = pictures( directory );
  if ( list.isEmpty() )
    return QString::null;
  QString dir = directory;
  if ( !dir.endsWith( "/" ) )
    dir += '/';
  return dir + list.last();
}

bool DailyPicture::isRunning() const
{
  return d->source != 0;
}

void DailyPicture::cancel()
{
  if ( d->job )
    d->job->kill( true );
  d->job = 0;
  delete d->temp;
  d->temp = 0;
  if ( d->source )
    d->source->deleteLater();
  d->source = 0;
}

bool DailyPicture::fetch( const QString &source, const QString &directory, int keep )
{
  cancel();
  if ( !sources().contains( source ) )
    return false;

  d->sourceId = source;
  d->dir = directory;
  if ( !d->dir.endsWith( "/" ) )
    d->dir += '/';
  d->keep = QMAX( keep, 1 );
  d->tooLarge = false;
  d->badRedirect = false;
  d->written = 0;
  KStandardDirs::makeDir( d->dir, 0700 );

  d->source = new WallpaperSource( source, this );
  connect( d->source, SIGNAL( contentsListed( int, int, const QValueList<KNS::OcsContent> &, int, bool ) ),
           SLOT( slotListed( int, int, const QValueList<KNS::OcsContent> &, int, bool ) ) );
  connect( d->source, SIGNAL( listingFailed( int, const QString & ) ),
           SLOT( slotListingFailed( int, const QString & ) ) );
  connect( d->source, SIGNAL( downloadResolved( const QString &, int, const KURL & ) ),
           SLOT( slotResolved( const QString &, int, const KURL & ) ) );
  connect( d->source, SIGNAL( downloadFailed( const QString &, int, const QString & ) ),
           SLOT( slotResolveFailed( const QString &, int, const QString & ) ) );
  // the pictures of the day are the first tab of each source
  d->source->listContents( 0, 0 );
  return true;
}

void DailyPicture::slotListed( int, int, const QValueList<KNS::OcsContent> &items, int, bool )
{
  if ( sender() != d->source )
    return;

  // the newest picture
  const OcsContent *newest = 0;
  for ( QValueList<OcsContent>::ConstIterator it = items.begin(); it != items.end(); ++it )
    if ( ( *it ).changed.isValid() && ( !newest || ( *it ).changed > newest->changed ) )
      newest = &*it;
  if ( !newest ) {
    done( QString::null, false, i18n( "The source has no picture of the day." ) );
    return;
  }
  d->content = *newest;
  d->date = newest->changed.date().toString( Qt::ISODate );

  // already here
  QStringList list = pictures( d->dir );
  for ( QStringList::ConstIterator it = list.begin(); it != list.end(); ++it ) {
    if ( ( *it ).startsWith( d->date + "." ) ) {
      done( d->dir + *it, false, QString::null );
      return;
    }
  }

  d->source->resolveDownload( d->content.id, 1 );
}

void DailyPicture::slotListingFailed( int, const QString &message )
{
  if ( sender() != d->source )
    return;
  done( QString::null, false, message );
}

void DailyPicture::slotResolveFailed( const QString &, int, const QString &message )
{
  if ( sender() != d->source )
    return;
  done( QString::null, false, message );
}

void DailyPicture::slotResolved( const QString &id, int, const KURL &link )
{
  if ( sender() != d->source || id != d->content.id || d->job )
    return;

  delete d->temp;
  d->temp = new KTempFile( locateLocal( "tmp", "knewstuff-daily-" ), ".download" );
  d->temp->setAutoDelete( true );
  if ( d->temp->status() != 0 || !d->temp->file() ) {
    done( QString::null, false, i18n( "Could not create a temporary file." ) );
    return;
  }

  KIO::TransferJob *job = KIO::get( link, true, false );
  WallpaperSource::setupJob( job );
  job->addMetaData( "errorPage", "false" );
  d->job = job;
  connect( job, SIGNAL( redirection( KIO::Job *, const KURL & ) ),
           SLOT( slotRedirection( KIO::Job *, const KURL & ) ) );
  connect( job, SIGNAL( data( KIO::Job *, const QByteArray & ) ),
           SLOT( slotData( KIO::Job *, const QByteArray & ) ) );
  connect( job, SIGNAL( result( KIO::Job * ) ), SLOT( slotResult( KIO::Job * ) ) );
}

void DailyPicture::slotRedirection( KIO::Job *job, const KURL &url )
{
  if ( job != d->job || !d->source || d->source->isAllowedUrl( url ) )
    return;
  d->badRedirect = true;
  job->kill( false );
}

void DailyPicture::slotData( KIO::Job *job, const QByteArray &data )
{
  if ( job != d->job || !d->temp || d->tooLarge || data.isEmpty() )
    return;
  d->written += data.size();
  QFile *f = d->temp->file();
  if ( d->written > WallpaperSource::maxDownloadSize ||
       !f || f->writeBlock( data.data(), data.size() ) != (Q_LONG)data.size() ) {
    d->tooLarge = true;
    job->kill( false );
  }
}

void DailyPicture::slotResult( KIO::Job *job )
{
  if ( job != d->job )
    return;
  d->job = 0;

  if ( d->badRedirect ) {
    done( QString::null, false, i18n( "The server redirected the download to a location that is not allowed." ) );
    return;
  }
  if ( d->tooLarge ) {
    done( QString::null, false, i18n( "The download is too large, or it could not be saved." ) );
    return;
  }
  if ( job->error() ) {
    done( QString::null, false, job->errorString() );
    return;
  }
  if ( !d->temp->close() ) {
    done( QString::null, false, i18n( "The download could not be saved." ) );
    return;
  }

  OcsDownload download;
  download.number = 1;
  download.name = d->date;
  download.sizeKiB = 0;
  QString installed;
  QString error = OcsInstaller::installPicture( d->temp->name(), d->dir, d->content, download, &installed );
  if ( !error.isEmpty() ) {
    done( QString::null, false, error );
    return;
  }
  prune();
  done( installed, true, QString::null );
}

void DailyPicture::prune()
{
  QStringList list = pictures( d->dir );
  int extra = int( list.count() ) - d->keep;
  for ( QStringList::ConstIterator it = list.begin(); it != list.end() && extra > 0; ++it, --extra ) {
    QFile::remove( d->dir + *it );
    QFile::remove( d->dir + *it + ".desktop" );
  }
}

void DailyPicture::done( const QString &picture, bool isNew, const QString &error )
{
  QString result = picture;
  if ( result.isEmpty() )
    result = latestPicture( d->dir );
  QString date = d->date;
  if ( !error.isEmpty() )
    kdDebug() << "picture of the day (" << d->sourceId << "): " << error << endl;
  cancel();
  emit finished( result, isNew, date, error );
}
