/*  This file is part of the KDE libraries
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

/*
 Balloons to look at: kballoontest --scenario <name> [--timeout <ms>]
 [--quit-after <ms>] [--report-after <ms>] [--report-every <ms>].  Every balloon is reported on stdout with its
 number, geometry and the point its tail points at; its signals too.

 monitors  four balloons per monitor, anchored to points near its
           corners: with a picture and actions, a picture only, actions
           only, text only
 stack     14 balloons at one anchor: stacked, those that don't fit queued
 widget    balloons at small windows standing for tray icons: in a
           horizontal panel at the bottom, in a vertical one at the left
 text      long text (wrapped), markup in plain text (shown as it is),
           rich text, no title
 corner    a balloon without an anchor (the corner of the work area)
 big       a balloon larger than the monitor (a long text, many actions):
           made to fit
 queue     ten balloons without timeout at one anchor on the second
           monitor, then a step every 1.5 s: close() the first one waiting
           (it never comes), make the first one taller (the last ones shown
           wait again), hide() it (those waiting come), setTimeout( 800 ) on
           balloon 2 (the pointer is over it: it stays)
*/

#include "kballoontest.h"

#include <stdio.h>

#include <qapplication.h>
#include <qbitmap.h>
#include <qdesktopwidget.h>
#include <qlayout.h>
#include <qmap.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qptrlist.h>
#include <qtimer.h>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kballoon.h>
#include <kcmdlineargs.h>
#include <kguiitem.h>

static QMap< const QObject*, int > numbers;

Reporter::Reporter( QObject* parent )
    : QObject( parent )
{
}

void Reporter::watch( KBalloon* balloon, int n )
{
    numbers[ balloon ] = n;
    connect( balloon, SIGNAL( clicked() ), SLOT( clicked() ) );
    connect( balloon, SIGNAL( actionActivated( int ) ), SLOT( actionActivated( int ) ) );
    connect( balloon, SIGNAL( timedOut() ), SLOT( timedOut() ) );
    connect( balloon, SIGNAL( destroyed( QObject* ) ), SLOT( gone( QObject* ) ) );
}

int Reporter::number( const QObject* balloon ) const
{
    QMap< const QObject*, int >::ConstIterator it = numbers.find( balloon );
    return it != numbers.end() ? *it : -1;
}

void Reporter::print( const QObject* balloon, const char* what, int id ) const
{
    if ( id >= 0 )
        printf( "balloon %d: %s %d\n", number( balloon ), what, id );
    else
        printf( "balloon %d: %s\n", number( balloon ), what );
    fflush( stdout );
}

void Reporter::clicked()
{
    print( sender(), "clicked" );
}

void Reporter::actionActivated( int id )
{
    print( sender(), "action", id );
}

void Reporter::timedOut()
{
    print( sender(), "timed out" );
}

void Reporter::gone( QObject* balloon )
{
    print( balloon, "deleted" );
    numbers.remove( balloon );
}

// where each balloon is
void Reporter::report()
{
    for ( QMap< const QObject*, int >::ConstIterator it = numbers.begin(); it != numbers.end(); ++it ) {
        const KBalloon* b = static_cast< const KBalloon* >( it.key() );
        const QRect g = b->geometry();
        const QPoint tip = b->anchorPoint();
        // (the contents fit: their widget is as large as its layout wants)
        const QWidget* content = static_cast< const QWidget* >( const_cast< KBalloon* >( b )->child( "content", "QWidget", false ) );
        const QRect c = content != 0 ? content->geometry() : QRect();
        const QSize hint = content != 0 && content->layout() != 0 ? content->layout()->totalSizeHint() : QSize();
        printf( "balloon %d: \"%s\" geometry %d,%d %dx%d anchor %d,%d visible %d queued %d translucent %d"
                " contents %d,%d %dx%d needs %dx%d\n", *it, b->title().latin1(),
                g.x(), g.y(), g.width(), g.height(), tip.x(), tip.y(), b->isVisible() ? 1 : 0,
                b->isQueued() ? 1 : 0, b->isTranslucent() ? 1 : 0, c.x(), c.y(), c.width(), c.height(),
                hint.width(), hint.height() );
    }
    fflush( stdout );
}

static Reporter* reporter = 0;
static int counter = 0;
static int timeoutMsec = 0;
static QPtrList< KBalloon > sequence; // the balloons of the scenario "queue"
static int stepNumber = 0;

// an avatar: a coloured disc with initials
void Reporter::step()
{
    ++stepNumber;
    report();
    KBalloon* first = sequence.first();
    switch ( stepNumber ) {
    case 1:
        for ( QPtrListIterator< KBalloon > it( sequence ); it.current() != 0; ++it )
            if ( it.current()->isQueued() ) {
                printf( "step 1: close() \"%s\" (queued)\n", it.current()->title().latin1() );
                it.current()->close();
                break;
            }
        break;
    case 2:
        printf( "step 2: \"%s\" taller\n", first->title().latin1() );
        first->setText( "Taller now.\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10" );
        break;
    case 3:
        printf( "step 3: hide() \"%s\"\n", first->title().latin1() );
        first->hide();
        break;
    case 4:
        printf( "step 4: setTimeout( 800 ) on \"%s\"\n", sequence.at( 1 )->title().latin1() );
        sequence.at( 1 )->setTimeout( 800 );
        break;
    default:
        return;
    }
    fflush( stdout );
    QTimer::singleShot( 1500, this, SLOT( step() ) );
}

static QPixmap avatar( const QColor& color, const QString& initials, int size = 40 )
{
    QPixmap pm( size, size );
    pm.fill( Qt::white );
    QPainter p( &pm );
    p.setPen( Qt::NoPen );
    p.setBrush( color );
    p.drawEllipse( 0, 0, size, size );
    p.setPen( Qt::white );
    QFont f = p.font();
    f.setBold( true );
    f.setPixelSize( size * 2 / 5 );
    p.setFont( f );
    p.drawText( QRect( 0, 0, size, size ), Qt::AlignCenter, initials );
    p.end();
    QBitmap mask( size, size, true );
    QPainter m( &mask );
    m.setPen( Qt::NoPen );
    m.setBrush( Qt::color1 );
    m.drawEllipse( 0, 0, size, size );
    m.end();
    pm.setMask( mask );
    return pm;
}

static KBalloon* balloon( const QString& title, const QString& text, const QPixmap& picture, int actions )
{
    KBalloon* b = new KBalloon;
    b->setTitle( title );
    b->setText( text );
    if ( !picture.isNull() )
        b->setPixmap( picture );
    if ( actions >= 1 )
        b->insertAction( "&View", 1 );
    if ( actions >= 2 )
        b->insertAction( KGuiItem( "&Ignore", "cancel" ), 2 );
    b->setTimeout( timeoutMsec );
    reporter->watch( b, ++counter );
    return b;
}

static void monitors()
{
    QDesktopWidget* desktop = QApplication::desktop();
    for ( int i = 0; i < desktop->numScreens(); ++i ) {
        const QRect g = desktop->screenGeometry( i );
        const QString m = QString( "Monitor %1" ).arg( i + 1 );
        KBalloon* b = balloon( m + ": picture, actions", "Anchored near the bottom right corner.\nTwo actions.",
                               avatar( QColor( 40, 120, 200 ), "BR" ), 2 );
        b->setAnchor( QPoint( g.right() - 40, g.bottom() - 12 ) );
        b->show();
        b = balloon( m + ": text only", "Anchored near the top left corner.", QPixmap(), 0 );
        b->setAnchor( QPoint( g.left() + 40, g.top() + 12 ) );
        b->show();
        b = balloon( m + ": picture", "Anchored near the top right corner.", avatar( QColor( 200, 80, 40 ), "TR" ),
                     0 );
        b->setAnchor( QPoint( g.right() - 12, g.top() + 40 ) );
        b->show();
        b = balloon( m + ": actions", "Anchored near the bottom left corner.", QPixmap(), 1 );
        b->setAnchor( QPoint( g.left() + 12, g.bottom() - 40 ) );
        b->show();
    }
}

static void stack()
{
    const QRect g = QApplication::desktop()->screenGeometry( 0 );
    const QPoint anchor( g.right() - 60, g.bottom() - 8 );
    for ( int i = 0; i < 14; ++i ) {
        KBalloon* b = balloon( QString( "Message %1" ).arg( i + 1 ), "Balloons at one anchor stack upwards.",
                               i % 2 == 0 ? avatar( QColor( 60, 160, 90 ), QString::number( i + 1 ) ) : QPixmap(),
                               i % 3 == 0 ? 2 : 0 );
        b->setAnchor( anchor );
        b->show();
    }
    KBalloon* b = balloon( "Elsewhere", "Another anchor, not stacked.", QPixmap(), 0 );
    b->setAnchor( QPoint( g.left() + 100, g.top() + 100 ) );
    b->show();
}

static QWidget* trayIcon( const QRect& r, const QColor& color )
{
    QWidget* w = new QWidget( 0, "trayicon", Qt::WStyle_Customize | Qt::WStyle_NoBorder | Qt::WX11BypassWM );
    w->setGeometry( r );
    w->setPaletteBackgroundColor( color );
    w->show();
    return w;
}

static void widgets()
{
    const QRect g = QApplication::desktop()->screenGeometry( 0 );
    // a horizontal panel at the bottom, its tray icon
    QWidget* panel = trayIcon( QRect( g.left(), g.bottom() - 31, g.width(), 32 ), QColor( 90, 90, 100 ) );
    QWidget* icon = trayIcon( QRect( g.right() - 80, g.bottom() - 27, 24, 24 ), QColor( 230, 160, 30 ) );
    Q_UNUSED( panel );
    KBalloon* b = balloon( "Tray icon (bottom panel)", "The tail points at the top edge of the icon.",
                           avatar( QColor( 150, 60, 160 ), "KM" ), 2 );
    b->setAnchor( icon );
    b->show();
    // a vertical panel at the left
    QWidget* vpanel = trayIcon( QRect( g.left(), g.top(), 32, g.height() - 40 ), QColor( 90, 100, 90 ) );
    QWidget* vicon = trayIcon( QRect( g.left() + 4, g.top() + g.height() / 3, 24, 24 ), QColor( 30, 160, 230 ) );
    Q_UNUSED( vpanel );
    b = balloon( "Tray icon (left panel)", "The tail points at the right edge of the icon.", QPixmap(), 1 );
    b->setAnchor( vicon );
    b->show();
}

static void text()
{
    const QRect g = QApplication::desktop()->screenGeometry( 0 );
    KBalloon* b = balloon( "A <b>title</b> & markup",
                           "Plain text with markup shown as it is: <b>not bold</b> <img src=\"/etc/passwd\"> "
                           "&amp; an ampersand. And a long line that has to be wrapped because it is much longer "
                           "than fifty characters, the widest a balloon gets.\nA second line:\ta tab, three   spaces.",
                           avatar( QColor( 200, 40, 90 ), "TX" ), 2 );
    b->setAnchor( QPoint( g.right() - 50, g.bottom() - 10 ) );
    b->show();
    b = new KBalloon;
    b->setRichText( "<b>Rich text</b> from the application: <i>italic</i>, "
                    "<font color=\"#a00000\">colour</font>." );
    b->setTimeout( timeoutMsec );
    reporter->watch( b, ++counter );
    b->setAnchor( QPoint( g.left() + 50, g.top() + 10 ) );
    b->show();
}

static void big()
{
    QString text;
    for ( int i = 0; i < 80; ++i )
        text += QString( "Line %1 of a text far too long for any monitor.\n" ).arg( i + 1 );
    KBalloon* b = balloon( "Too large", text, avatar( QColor( 90, 90, 160 ), "XL" ), 0 );
    for ( int i = 0; i < 12; ++i )
        b->insertAction( QString( "Action number %1" ).arg( i + 1 ) );
    const QRect g = QApplication::desktop()->screenGeometry( 1 );
    b->setAnchor( QPoint( g.right() - 30, g.bottom() - 10 ) );
    b->show();
}

static void queue()
{
    const QRect g = QApplication::desktop()->screenGeometry( QApplication::desktop()->numScreens() - 1 );
    for ( int i = 0; i < 10; ++i ) {
        KBalloon* b = balloon( QString( "Queued %1" ).arg( i + 1 ), "No timeout.", QPixmap(), i == 0 ? 1 : 0 );
        b->setAnchor( QPoint( g.right() - 30, g.bottom() - 10 ) );
        b->show();
        sequence.append( b );
    }
    QTimer::singleShot( 1500, reporter, SLOT( step() ) );
}

static void corner()
{
    KBalloon* b = balloon( "No anchor", "In the corner of the work area, without a tail.",
                           avatar( QColor( 100, 100, 100 ), "NA" ), 0 );
    b->show();
}

static KCmdLineOptions options[] =
{
    { "scenario <name>", "monitors, stack, widget, text, corner, big or queue", "monitors" },
    { "timeout <ms>", "Timeout of the balloons (0: none)", "0" },
    { "quit-after <ms>", "Quit after this long (0: never)", "0" },
    { "report-after <ms>", "Print where the balloons are after this long", "1000" },
    { "report-every <ms>", "Print where the balloons are this often (0: once)", "0" },
    KCmdLineLastOption
};

int main( int argc, char** argv )
{
    KAboutData about( "kballoontest", "KBalloon test", "1.0" );
    KCmdLineArgs::init( argc, argv, &about );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    timeoutMsec = args->getOption( "timeout" ).toInt();
    const QCString scenario = args->getOption( "scenario" );
    const int quitAfter = args->getOption( "quit-after" ).toInt();
    const int reportAfter = args->getOption( "report-after" ).toInt();
    const int reportEvery = args->getOption( "report-every" ).toInt();
    args->clear();
    reporter = new Reporter( &app );
    if ( scenario == "stack" )
        stack();
    else if ( scenario == "widget" )
        widgets();
    else if ( scenario == "text" )
        text();
    else if ( scenario == "corner" )
        corner();
    else if ( scenario == "big" )
        big();
    else if ( scenario == "queue" )
        queue();
    else
        monitors();
    if ( reportAfter > 0 )
        QTimer::singleShot( reportAfter, reporter, SLOT( report() ) );
    QTimer every;
    if ( reportEvery > 0 ) {
        QObject::connect( &every, SIGNAL( timeout() ), reporter, SLOT( report() ) );
        every.start( reportEvery );
    }
    if ( quitAfter > 0 )
        QTimer::singleShot( quitAfter, &app, SLOT( quit() ) );
    return app.exec();
}

#include "kballoontest.moc"
