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
 The look follows Kopete's notification balloon (a tooltip-coloured
 rounded window with a tail at the corner next to the anchor); the code
 is new.

 The outline is one polygon: the body (a rectangle with rounded corners),
 at the tail's corner the tail instead of the rounded corner.  The tail
 occupies TailSize pixels on its two sides of the window, the window's
 corner is its tip.

 ARGB: the window is created with a 32-bit visual and adopted by the
 widget (QWidget::create(), as QGLWidget does with its visuals).  Qt 3
 only paints in the default visual: nothing of Qt paints into this window
 (no background, see createWindow()); the outline is drawn with XRender
 (anti-aliased polygons) into an ARGB pixmap copied into the window.  The
 contents (labels, buttons) are in a child window with the default visual
 inside the outline; the X server composites a child with another depth
 into its parent (Composite's implicit redirection), opaque.  The input
 shape is the outline (no clicks on the transparent corners).
*/

#include "kballoon.h"

#include <config.h>

#include <qapplication.h>
#include <qcursor.h>
#include <qdatetime.h>
#include <qdesktopwidget.h>
#include <qguardedptr.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmap.h>
#include <qpainter.h>
#include <qpointarray.h>
#include <qptrlist.h>
#include <qsimplerichtext.h>
#include <qstylesheet.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qvaluelist.h>
#include <qvaluevector.h>

#include <kdialog.h>
#include <kguiitem.h>
#include <kpushbutton.h>
#include <kwin.h>

#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>
#endif
#include <fixx11h.h>

// the geometry of the outline (pixels)
static const int TailSize = 16; // the tip is this far from the body, across and down
static const int TailBase = 10; // the tail meets the edges this far beyond a corner's arc
static const int Radius = 8; // of the corners
static const int ContentInset = 4; // the contents stay inside the corners' arcs
static const int Spacing = 6; // between stacked balloons
static const int CornerMargin = 8; // without an anchor: from the work area's corner
static const int MaxPicture = 64; // larger pictures are scaled down
static const int ScaledPicture = 48;

namespace
{

enum Tail { NoTail, TailTopLeft, TailTopRight, TailBottomRight, TailBottomLeft };

struct Point
{
    double x, y;
};

struct Action
{
    int id;
    KGuiItem item;
};

// the balloons shown, in the order they were shown, and those waiting
QPtrList< KBalloon >& shownBalloons()
{
    static QPtrList< KBalloon > list;
    return list;
}

QPtrList< KBalloon >& queuedBalloons()
{
    static QPtrList< KBalloon > list;
    return list;
}

// the monitor of p, or the nearest one
int screenOf( const QPoint& p )
{
    QDesktopWidget* desktop = QApplication::desktop();
    int screen = desktop->screenNumber( p );
    if ( screen >= 0 )
        return screen;
    int best = -1;
    long bestDistance = 0;
    for ( int i = 0; i < desktop->numScreens(); ++i ) {
        QRect r = desktop->screenGeometry( i );
        long dx = p.x() < r.left() ? r.left() - p.x() : p.x() > r.right() ? p.x() - r.right() : 0;
        long dy = p.y() < r.top() ? r.top() - p.y() : p.y() > r.bottom() ? p.y() - r.bottom() : 0;
        long distance = dx * dx + dy * dy;
        if ( best < 0 || distance < bestDistance ) {
            best = i;
            bestDistance = distance;
        }
    }
    return best >= 0 ? best : desktop->primaryScreen();
}

// the outline of a balloon of w x h with the tail at the corner tail, in
// pixel edge coordinates, clockwise on the screen
QValueVector< Point > outline( int w, int h, int tail )
{
    const double left = ( tail == TailTopLeft || tail == TailBottomLeft ) ? TailSize : 0;
    const double top = ( tail == TailTopLeft || tail == TailTopRight ) ? TailSize : 0;
    const double right = ( tail == TailTopRight || tail == TailBottomRight ) ? w - TailSize : w;
    const double bottom = ( tail == TailBottomLeft || tail == TailBottomRight ) ? h - TailSize : h;
    const double r = Radius;
    const double base = Radius + TailBase;
    const int steps = 8; // per quarter of a circle
    QValueVector< Point > points;
    // the corners clockwise from the top left one: the arc's centre and
    // its first angle, or the tail
    const double cx[ 4 ] = { left + r, right - r, right - r, left + r };
    const double cy[ 4 ] = { top + r, top + r, bottom - r, bottom - r };
    const int corner[ 4 ] = { TailTopLeft, TailTopRight, TailBottomRight, TailBottomLeft };
    for ( int c = 0; c < 4; ++c ) {
        if ( corner[ c ] == tail ) {
            Point p1, tip, p2;
            switch ( tail ) {
            case TailTopLeft:
                p1.x = left; p1.y = top + base; tip.x = 0; tip.y = 0; p2.x = left + base; p2.y = top;
                break;
            case TailTopRight:
                p1.x = right - base; p1.y = top; tip.x = w; tip.y = 0; p2.x = right; p2.y = top + base;
                break;
            case TailBottomRight:
                p1.x = right; p1.y = bottom - base; tip.x = w; tip.y = h; p2.x = right - base; p2.y = bottom;
                break;
            default: // TailBottomLeft
                p1.x = left + base; p1.y = bottom; tip.x = 0; tip.y = h; p2.x = left; p2.y = bottom - base;
                break;
            }
            points.append( p1 );
            points.append( tip );
            points.append( p2 );
            continue;
        }
        // (screen coordinates, y down: from 180 degrees clockwise)
        const double start = M_PI + c * M_PI / 2;
        for ( int i = 0; i <= steps; ++i ) {
            const double a = start + i * ( M_PI / 2 ) / steps;
            Point p;
            p.x = cx[ c ] + r * cos( a );
            p.y = cy[ c ] + r * sin( a );
            points.append( p );
        }
    }
    return points;
}

// the polygon moved inwards by d (each edge parallel to its original)
QValueVector< Point > inset( const QValueVector< Point >& p, double d )
{
    const int n = p.size();
    double area = 0;
    for ( int i = 0; i < n; ++i ) {
        const Point& a = p[ i ];
        const Point& b = p[ ( i + 1 ) % n ];
        area += a.x * b.y - b.x * a.y;
    }
    const double side = area >= 0 ? 1 : -1;
    QValueVector< Point > normal( n ); // of the edge from i to i + 1, inwards
    for ( int i = 0; i < n; ++i ) {
        const Point& a = p[ i ];
        const Point& b = p[ ( i + 1 ) % n ];
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double length = sqrt( dx * dx + dy * dy );
        if ( length <= 0 )
            length = 1;
        normal[ i ].x = -dy / length * side;
        normal[ i ].y = dx / length * side;
    }
    QValueVector< Point > result( n );
    for ( int i = 0; i < n; ++i ) {
        const Point& n1 = normal[ ( i + n - 1 ) % n ];
        const Point& n2 = normal[ i ];
        double k = 1 + n1.x * n2.x + n1.y * n2.y;
        if ( k < 0.1 )
            k = 0.1;
        result[ i ].x = p[ i ].x + d * ( n1.x + n2.x ) / k;
        result[ i ].y = p[ i ].y + d * ( n1.y + n2.y ) / k;
    }
    return result;
}

QRegion outlineRegion( const QValueVector< Point >& p )
{
    QPointArray a( p.size() );
    for ( unsigned int i = 0; i < p.size(); ++i )
        a.setPoint( i, (int) floor( p[ i ].x + 0.5 ), (int) floor( p[ i ].y + 0.5 ) );
    return QRegion( a, true );
}

// the colour of the outline: the text colour, softened
QColor borderColor( const QColorGroup& cg )
{
    const QColor fg = cg.foreground();
    const QColor bg = cg.background();
    return QColor( ( fg.red() * 3 + bg.red() * 2 ) / 5, ( fg.green() * 3 + bg.green() * 2 ) / 5,
                   ( fg.blue() * 3 + bg.blue() * 2 ) / 5 );
}

Atom atom( Display* dpy, const char* name )
{
    return XInternAtom( dpy, name, False );
}

#ifdef HAVE_XRENDER
// a compositing manager for the screen
bool compositing( Display* dpy, int screen )
{
    int event, error;
    if ( !XRenderQueryExtension( dpy, &event, &error ) )
        return false;
    QCString name;
    name.sprintf( "_NET_WM_CM_S%d", screen );
    return XGetSelectionOwner( dpy, atom( dpy, name.data() ) ) != None;
}

// a 32-bit TrueColor visual with alpha
Visual* argbVisual( Display* dpy, int screen )
{
    XVisualInfo templ;
    templ.screen = screen;
    templ.depth = 32;
    templ.c_class = TrueColor;
    int count = 0;
    XVisualInfo* infos = XGetVisualInfo( dpy, VisualScreenMask | VisualDepthMask | VisualClassMask, &templ,
                                         &count );
    Visual* visual = 0;
    for ( int i = 0; i < count && visual == 0; ++i ) {
        XRenderPictFormat* format = XRenderFindVisualFormat( dpy, infos[ i ].visual );
        if ( format != 0 && format->type == PictTypeDirect && format->direct.alphaMask != 0 )
            visual = infos[ i ].visual;
    }
    if ( infos != 0 )
        XFree( infos );
    return visual;
}

// one colormap for the ARGB windows (per process, kept)
Colormap argbColormap( Display* dpy, int screen, Visual* visual )
{
    static Colormap colormap = None;
    static int colormapScreen = -1;
    if ( colormap == None || colormapScreen != screen ) {
        colormap = XCreateColormap( dpy, RootWindow( dpy, screen ), visual, AllocNone );
        colormapScreen = screen;
    }
    return colormap;
}

XRenderColor renderColor( const QColor& c )
{
    XRenderColor r;
    r.red = c.red() * 257;
    r.green = c.green() * 257;
    r.blue = c.blue() * 257;
    r.alpha = 0xffff;
    return r;
}

void fillPolygon( Display* dpy, Picture dst, const QColor& color, const QValueVector< Point >& p )
{
    XRenderColor c = renderColor( color );
    Picture src = XRenderCreateSolidFill( dpy, &c );
    QMemArray< XPointDouble > points( p.size() );
    for ( unsigned int i = 0; i < p.size(); ++i ) {
        points[ i ].x = p[ i ].x;
        points[ i ].y = p[ i ].y;
    }
    XRenderCompositeDoublePoly( dpy, PictOpOver, src, dst, XRenderFindStandardFormat( dpy, PictStandardA8 ),
                                0, 0, 0, 0, points.data(), points.size(), 1 );
    XRenderFreePicture( dpy, src );
}
#endif

// the contents' widget, in a window of its own in the ARGB case
class Content : public QWidget
{
public:
    Content( QWidget* parent )
        : QWidget( parent, "content" )
    {
    }

    void adopt( WId window )
    {
        create( window, true, true );
    }
};

} // namespace

class KBalloon::Private
{
public:
    Private()
        : rich( false ), nextId( 1 ), timeoutMsec( 6000 ), autoDelete( true ), hasPoint( false ),
          created( false ), argb( false ), queued( false ), tail( NoTail ), content( 0 ), picture( 0 ),
          label( 0 ), buttonRow( 0 ), buttonsDirty( true ), remaining( 0 ), paused( false ),
          cache( None ), cacheTail( -1 ), cacheFill( 0 ), cacheBorder( 0 )
    {
    }

    QString title;
    QString text;
    bool rich;
    QPixmap pixmap;
    QValueList< Action > actions;
    int nextId;
    int timeoutMsec;
    bool autoDelete;

    bool hasPoint;
    QPoint point;
    QGuardedPtr< QWidget > widget;

    bool created; // the window (and its contents)
    bool argb;
    bool queued;
    int tail; // where it is now
    QRect body; // window relative
    QSize bodySize; // what the contents need

    QWidget* content;
    QLabel* picture;
    QLabel* label;
    QHBoxLayout* buttonRow;
    QPtrList< QWidget > buttons;
    QMap< const QObject*, int > buttonIds;
    bool buttonsDirty;

    QTimer timer;
    QTime started;
    int remaining;
    bool paused;

    // the ARGB outline drawn (for its size, tail and colours)
    Pixmap cache;
    QSize cacheSize;
    int cacheTail;
    QRgb cacheFill, cacheBorder;

    // where the tail of a balloon on the side above/left (or not) of the
    // anchor would point; false without an anchor
    bool anchorFor( bool above, bool left, QPoint* tip, int* screen ) const;
    // the monitor it goes onto
    int screen() const;
};

int KBalloon::Private::screen() const
{
    QPoint tip;
    int s;
    if ( !anchorFor( true, true, &tip, &s ) )
        s = screenOf( QCursor::pos() );
    return s;
}

bool KBalloon::Private::anchorFor( bool above, bool left, QPoint* tip, int* screen ) const
{
    if ( hasPoint ) {
        *tip = point;
        *screen = screenOf( point );
        return true;
    }
    QWidget* w = widget;
    if ( w == 0 || !w->isVisible() )
        return false;
    const QRect r( w->mapToGlobal( QPoint( 0, 0 ) ), w->size() );
    *screen = screenOf( r.center() );
    const QRect s = QApplication::desktop()->screenGeometry( *screen );
    // a horizontal panel (nearer to the top or bottom of the monitor):
    // the tail at the edge above or below; a vertical one: at its side
    const int vertical = QMIN( QABS( r.top() - s.top() ), QABS( s.bottom() - r.bottom() ) );
    const int horizontal = QMIN( QABS( r.left() - s.left() ), QABS( s.right() - r.right() ) );
    if ( vertical <= horizontal )
        *tip = QPoint( r.center().x(), above ? r.top() : r.bottom() );
    else
        *tip = QPoint( left ? r.left() : r.right(), r.center().y() );
    return true;
}

KBalloon::KBalloon( QWidget* parent, const char* name )
    : QWidget( parent, name != 0 ? name : "kballoon", WType_TopLevel | WStyle_Customize | WStyle_NoBorder )
{
    d = new Private;
    setPalette( QToolTip::palette() );
    connect( &d->timer, SIGNAL( timeout() ), SLOT( slotTimeout() ) );
}

KBalloon::~KBalloon()
{
    queuedBalloons().removeRef( this );
    const bool shown = shownBalloons().removeRef( this );
    if ( d->cache != None )
        XFreePixmap( x11Display(), d->cache );
    delete d;
    d = 0;
    if ( shown )
        relayoutAll();
}

KBalloon* KBalloon::message( const QString& title, const QString& text, const QPixmap& pixmap,
                             QWidget* anchor, int timeout )
{
    KBalloon* balloon = new KBalloon;
    balloon->setTitle( title );
    balloon->setText( text );
    balloon->setPixmap( pixmap );
    if ( anchor != 0 )
        balloon->setAnchor( anchor );
    balloon->setTimeout( timeout );
    balloon->show();
    return balloon;
}

void KBalloon::setTitle( const QString& title )
{
    d->title = title;
    setCaption( title );
    updateContents();
}

QString KBalloon::title() const
{
    return d->title;
}

void KBalloon::setText( const QString& text )
{
    d->text = text;
    d->rich = false;
    updateContents();
}

void KBalloon::setRichText( const QString& text )
{
    d->text = text;
    d->rich = true;
    updateContents();
}

QString KBalloon::text() const
{
    return d->text;
}

bool KBalloon::isRichText() const
{
    return d->rich;
}

void KBalloon::setPixmap( const QPixmap& pixmap )
{
    d->pixmap = pixmap;
    updateContents();
}

QPixmap KBalloon::pixmap() const
{
    return d->pixmap;
}

int KBalloon::insertAction( const QString& text, int id )
{
    return insertAction( KGuiItem( text ), id );
}

int KBalloon::insertAction( const KGuiItem& item, int id )
{
    if ( id < 0 ) {
        id = d->nextId;
        for ( QValueList< Action >::ConstIterator it = d->actions.begin(); it != d->actions.end(); ++it )
            if ( (*it).id >= id )
                id = (*it).id + 1;
    }
    Action action;
    action.id = id;
    action.item = item;
    d->actions.append( action );
    d->nextId = id + 1;
    d->buttonsDirty = true;
    updateContents();
    return id;
}

void KBalloon::clearActions()
{
    d->actions.clear();
    d->buttonsDirty = true;
    updateContents();
}

void KBalloon::setTimeout( int msec )
{
    d->timeoutMsec = QMAX( 0, msec );
    if ( isVisible() ) {
        d->timer.stop();
        d->paused = false;
        d->remaining = d->timeoutMsec;
        if ( d->remaining > 0 ) {
            d->started.start();
            d->timer.start( d->remaining, true );
        }
    }
}

int KBalloon::timeout() const
{
    return d->timeoutMsec;
}

void KBalloon::setAutoDelete( bool autoDelete )
{
    d->autoDelete = autoDelete;
}

bool KBalloon::autoDelete() const
{
    return d->autoDelete;
}

void KBalloon::setAnchor( const QPoint& point )
{
    d->hasPoint = true;
    d->point = point;
    d->widget = 0;
    if ( isVisible() )
        relayoutAll();
}

void KBalloon::setAnchor( QWidget* widget )
{
    d->hasPoint = false;
    d->widget = widget;
    if ( isVisible() )
        relayoutAll();
}

void KBalloon::clearAnchor()
{
    d->hasPoint = false;
    d->widget = 0;
    if ( isVisible() )
        relayoutAll();
}

QPoint KBalloon::anchorPoint() const
{
    QPoint tip;
    int screen;
    const bool above = d->tail == TailBottomLeft || d->tail == TailBottomRight;
    const bool left = d->tail == TailTopRight || d->tail == TailBottomRight;
    if ( d->tail == NoTail ) { // (where it would open)
        QPoint p;
        if ( !d->anchorFor( true, true, &p, &screen ) )
            return QPoint();
        const QRect s = QApplication::desktop()->screenGeometry( screen );
        if ( !d->anchorFor( p.y() > s.center().y(), p.x() > s.center().x(), &tip, &screen ) )
            return QPoint();
        return tip;
    }
    if ( !d->anchorFor( above, left, &tip, &screen ) )
        return QPoint();
    return tip;
}

bool KBalloon::isQueued() const
{
    return d->queued;
}

bool KBalloon::isTranslucent() const
{
    return d->argb;
}

// The window: ARGB with a compositing manager, else this one; and the
// window manager hints (see the class description).
void KBalloon::createWindow()
{
    d->created = true;
    Display* dpy = x11Display();
    const int screen = x11Screen();
#ifdef HAVE_XRENDER
    Visual* visual = compositing( dpy, screen ) ? argbVisual( dpy, screen ) : 0;
    if ( visual != 0 ) {
        XSetWindowAttributes a;
        a.background_pixel = 0; // transparent
        a.border_pixel = 0;
        a.colormap = argbColormap( dpy, screen, visual );
        Window w = XCreateWindow( dpy, RootWindow( dpy, screen ), x(), y(), QMAX( 1, width() ),
                                  QMAX( 1, height() ), 0, 32, InputOutput, visual,
                                  CWBackPixel | CWBorderPixel | CWColormap, &a );
        create( w, true, true );
        // nothing of Qt's paints this window (its pixels would be
        // transparent): no background, transparent until painted
        setBackgroundMode( NoBackground );
        XSetWindowBackground( dpy, w, 0 );
        d->argb = true;
        const QString title = caption(); // (the new window has the application's name)
        setCaption( QString::null );
        setCaption( title );
    }
#endif
    const Window w = winId();
    KWin::setType( w, NET::Notification );
    KWin::setState( w, NET::SkipTaskbar | NET::SkipPager | NET::KeepAbove );
    KWin::setOnAllDesktops( w, true );
    // no focus: the input hint off, no WM_TAKE_FOCUS
    XWMHints* hints = XGetWMHints( dpy, w );
    XWMHints h;
    if ( hints != 0 ) {
        h = *hints;
        XFree( hints );
    } else
        h.flags = 0;
    h.flags |= InputHint;
    h.input = False;
    XSetWMHints( dpy, w, &h );
    Atom protocols[ 2 ] = { atom( dpy, "WM_DELETE_WINDOW" ), atom( dpy, "_NET_WM_PING" ) };
    XSetWMProtocols( dpy, w, protocols, 2 );
    if ( d->argb ) {
        long one = 1;
        XChangeProperty( dpy, w, atom( dpy, "_KDE_NET_WM_SHADOW_FROM_ALPHA" ), XA_CARDINAL, 32,
                         PropModeReplace, (unsigned char*) &one, 1 );
    }
    buildContents();
}

void KBalloon::buildContents()
{
    Content* content = new Content( this );
    d->content = content;
    if ( d->argb ) {
        // its own window with the default visual (see the top)
        Display* dpy = x11Display();
        const int screen = x11Screen();
        XSetWindowAttributes a;
        a.background_pixel = colorGroup().background().pixel( screen );
        a.border_pixel = BlackPixel( dpy, screen );
        a.colormap = QPaintDevice::x11AppColormap( screen );
        Window w = XCreateWindow( dpy, winId(), 0, 0, 1, 1, 0, QPaintDevice::x11AppDepth( screen ),
                                  InputOutput, (Visual*) QPaintDevice::x11AppVisual( screen ),
                                  CWBackPixel | CWBorderPixel | CWColormap, &a );
        content->adopt( w );
    }
    QVBoxLayout* top = new QVBoxLayout( d->content, QMAX( 0, KDialog::marginHint() - ContentInset ),
                                        KDialog::spacingHint() );
    QHBoxLayout* row = new QHBoxLayout( top, KDialog::spacingHint() );
    d->picture = new QLabel( d->content, "picture" );
    d->picture->setAlignment( AlignTop | AlignHCenter );
    row->addWidget( d->picture );
    d->label = new QLabel( d->content, "text" );
    d->label->setTextFormat( RichText );
    d->label->setAlignment( AlignTop | AlignLeft | WordBreak );
    row->addWidget( d->label, 1 );
    d->buttonRow = new QHBoxLayout( top, KDialog::spacingHint() );
    d->buttonRow->addStretch( 1 );
    d->buttonsDirty = true;
}

// the contents' widgets from the settings, the size they need
void KBalloon::updateContents()
{
    if ( !d->created )
        return;
    // the picture
    if ( d->pixmap.isNull() ) {
        d->picture->clear();
        d->picture->hide();
    } else {
        QPixmap pm = d->pixmap;
        if ( pm.width() > MaxPicture || pm.height() > MaxPicture )
            pm.convertFromImage( pm.convertToImage().smoothScale( ScaledPicture, ScaledPicture,
                                                                  QImage::ScaleMin ) );
        d->picture->setPixmap( pm );
        d->picture->show();
    }
    // the text: the title escaped, plain text escaped
    QString html;
    if ( !d->title.isEmpty() )
        html = "<b>" + QStyleSheet::escape( d->title ) + "</b>";
    if ( !d->text.isEmpty() ) {
        if ( !html.isEmpty() )
            html += "<br>";
        if ( d->rich )
            html += d->text;
        else {
            QString plain = QStyleSheet::escape( d->text );
            plain.replace( "\r\n", "<br>" );
            plain.replace( '\n', "<br>" );
            html += plain;
        }
    }
    d->label->setText( html );
    // as wide as the text needs, at most about 50 characters (a third of
    // the monitor): wrapped beyond
    QPoint tip;
    int screen;
    if ( !d->anchorFor( false, false, &tip, &screen ) )
        screen = screenOf( QCursor::pos() );
    const QRect s = QApplication::desktop()->screenGeometry( screen );
    const int widest = QMAX( 120, QMIN( d->label->fontMetrics().width( 'x' ) * 50, s.width() / 3 ) );
    QSimpleRichText measure( html, d->label->font() );
    measure.setWidth( 32767 );
    const int natural = measure.widthUsed() + 2;
    d->label->setFixedWidth( QMIN( natural, widest ) );
    d->label->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum, true ) );
    // the buttons
    if ( d->buttonsDirty ) {
        d->buttonsDirty = false;
        d->buttons.setAutoDelete( true );
        d->buttons.clear();
        d->buttonIds.clear();
        for ( QValueList< Action >::ConstIterator it = d->actions.begin(); it != d->actions.end(); ++it ) {
            KPushButton* button = new KPushButton( (*it).item, d->content );
            button->setFocusPolicy( NoFocus );
            d->buttonRow->addWidget( button );
            d->buttons.append( button );
            d->buttonIds[ button ] = (*it).id;
            connect( button, SIGNAL( clicked() ), SLOT( slotAction() ) );
            button->show();
        }
    }
    QLayout* layout = d->content->layout();
    layout->invalidate();
    layout->activate();
    QSize size = layout->totalSizeHint();
    if ( layout->hasHeightForWidth() )
        size.setHeight( layout->totalHeightForWidth( size.width() ) );
    size = size.expandedTo( layout->totalMinimumSize() );
    d->bodySize = QSize( size.width() + 2 * ContentInset, size.height() + 2 * ContentInset )
                  .expandedTo( QSize( 2 * ( Radius + TailBase ) + 8, 2 * ( Radius + TailBase ) + 8 ) );
    if ( isVisible() || d->queued )
        relayoutAll();
}

/*
 The geometry: the side of the anchor towards the middle of its monitor,
 or another one where the balloon fits on the monitor, or clamped to it
 (no tail then); moved off the balloons shown before it (no tail then
 either).  False when it would leave the monitor because of them (it
 waits).
*/
bool KBalloon::place()
{
    QDesktopWidget* desktop = QApplication::desktop();
    const QSize body = d->bodySize;
    int screen = 0;
    QPoint tip;
    QRect geom;
    QRect area;
    int tail = NoTail;
    bool up = true;
    if ( d->anchorFor( true, true, &tip, &screen ) ) {
        area = desktop->screenGeometry( screen );
        // the preferred side from the middle of the anchor (a widget's)
        QPoint middle = tip;
        if ( !d->hasPoint && (QWidget*) d->widget != 0 )
            middle = QRect( d->widget->mapToGlobal( QPoint( 0, 0 ) ), d->widget->size() ).center();
        const bool aboveFirst = middle.y() > area.center().y();
        const bool leftFirst = middle.x() > area.center().x();
        const QSize size( body.width() + TailSize, body.height() + TailSize );
        bool found = false;
        for ( int i = 0; i < 4 && !found; ++i ) {
            const bool above = i < 2 ? aboveFirst : !aboveFirst;
            const bool left = i % 2 == 0 ? leftFirst : !leftFirst;
            QPoint p;
            int s;
            d->anchorFor( above, left, &p, &s );
            QRect g( left ? p.x() - size.width() + 1 : p.x(), above ? p.y() - size.height() + 1 : p.y(),
                     size.width(), size.height() );
            if ( i == 0 || area.contains( g ) ) {
                geom = g;
                up = above;
                tail = above ? ( left ? TailBottomRight : TailBottomLeft ) : ( left ? TailTopRight : TailTopLeft );
                found = area.contains( g );
            }
        }
        if ( !found ) { // the body alone, clamped
            geom = QRect( geom.x() + ( tail == TailTopLeft || tail == TailBottomLeft ? TailSize : 0 ),
                          geom.y() + ( tail == TailTopLeft || tail == TailTopRight ? TailSize : 0 ),
                          body.width(), body.height() );
            geom.moveTopLeft( QPoint( QMAX( area.left(), QMIN( geom.x(), area.right() - geom.width() + 1 ) ),
                                      QMAX( area.top(), QMIN( geom.y(), area.bottom() - geom.height() + 1 ) ) ) );
            tail = NoTail;
        }
    } else {
        screen = screenOf( QCursor::pos() );
        area = desktop->availableGeometry( screen ) & desktop->screenGeometry( screen );
        geom = QRect( area.right() - CornerMargin - body.width() + 1, area.bottom() - CornerMargin - body.height() + 1,
                      body.width(), body.height() );
    }
    // off the balloons shown before this one
    bool moved = false;
    for ( int round = 0; round < 64; ++round ) {
        bool hit = false;
        for ( QPtrListIterator< KBalloon > it( shownBalloons() ); it.current() != 0 && it.current() != this; ++it ) {
            const QRect other = it.current()->geometry();
            if ( !other.intersects( geom ) )
                continue;
            if ( !moved && tail != NoTail ) { // the body alone
                geom = QRect( geom.x() + ( tail == TailTopLeft || tail == TailBottomLeft ? TailSize : 0 ),
                              geom.y() + ( tail == TailTopLeft || tail == TailTopRight ? TailSize : 0 ),
                              body.width(), body.height() );
                tail = NoTail;
                moved = true;
                if ( !other.intersects( geom ) )
                    continue;
            }
            moved = true;
            hit = true;
            if ( up )
                geom.moveBottom( other.top() - Spacing );
            else
                geom.moveTop( other.bottom() + Spacing );
        }
        if ( !hit )
            break;
    }
    if ( moved && !area.contains( geom ) )
        return false;
    d->tail = tail;
    const int left = ( tail == TailTopLeft || tail == TailBottomLeft ) ? TailSize : 0;
    const int top = ( tail == TailTopLeft || tail == TailTopRight ) ? TailSize : 0;
    d->body = QRect( left, top, body.width(), body.height() );
    d->content->setGeometry( d->body.x() + ContentInset, d->body.y() + ContentInset,
                             body.width() - 2 * ContentInset, body.height() - 2 * ContentInset );
    if ( geom != geometry() )
        setGeometry( geom );
    updateShape();
    update();
    return true;
}

// all balloons placed again (one went, one changed), then those waiting
// shown where there is room now
void KBalloon::relayoutAll()
{
    for ( QPtrListIterator< KBalloon > it( shownBalloons() ); it.current() != 0; ++it )
        it.current()->place();
    // (in order on each monitor: none before one that still waits there)
    QPtrList< KBalloon > queue = queuedBalloons();
    QValueList< int > waiting;
    for ( QPtrListIterator< KBalloon > it( queue ); it.current() != 0; ++it ) {
        KBalloon* b = it.current();
        const int screen = b->d->screen();
        if ( waiting.contains( screen ) )
            continue;
        if ( !b->place() ) {
            waiting.append( screen );
            continue;
        }
        queuedBalloons().removeRef( b );
        b->d->queued = false;
        b->showNow();
    }
}

void KBalloon::show()
{
    if ( !d->created ) {
        createWindow();
        updateContents();
    }
    if ( isVisible() ) {
        relayoutAll();
        return;
    }
    if ( d->queued )
        return;
    // (after those waiting on its monitor: in order)
    bool after = false;
    const int screen = d->screen();
    for ( QPtrListIterator< KBalloon > it( queuedBalloons() ); it.current() != 0 && !after; ++it )
        after = it.current()->d->screen() == screen;
    if ( after || !place() ) {
        d->queued = true;
        queuedBalloons().append( this );
        return;
    }
    showNow();
}

// placed: shown, the timeout started
void KBalloon::showNow()
{
    shownBalloons().append( this );
    QWidget::show();
    raise();
    d->paused = false;
    d->remaining = d->timeoutMsec;
    if ( d->remaining > 0 ) {
        d->started.start();
        d->timer.start( d->remaining, true );
    }
}

void KBalloon::hide()
{
    if ( d->queued ) {
        queuedBalloons().removeRef( this );
        d->queued = false;
        if ( d->autoDelete )
            deleteLater();
        return;
    }
    QWidget::hide();
}

void KBalloon::hideEvent( QHideEvent* e )
{
    QWidget::hideEvent( e );
    d->timer.stop();
    d->paused = false;
    if ( shownBalloons().removeRef( this ) )
        relayoutAll();
    if ( d->autoDelete )
        deleteLater();
}

// the mask (XShape) or the input shape (ARGB), from the outline
void KBalloon::updateShape()
{
    const QRegion region = outlineRegion( outline( width(), height(), d->tail ) );
    if ( !d->argb ) {
        setMask( region );
        return;
    }
#ifdef ShapeInput
    int event, error;
    if ( !XShapeQueryExtension( x11Display(), &event, &error ) )
        return;
    QMemArray< QRect > rects = region.rects();
    QMemArray< XRectangle > xrects( rects.size() );
    for ( unsigned int i = 0; i < rects.size(); ++i ) {
        xrects[ i ].x = rects[ i ].x();
        xrects[ i ].y = rects[ i ].y();
        xrects[ i ].width = rects[ i ].width();
        xrects[ i ].height = rects[ i ].height();
    }
    XShapeCombineRectangles( x11Display(), winId(), ShapeInput, 0, 0, xrects.data(), xrects.size(), ShapeSet,
                             Unsorted );
#endif
}

void KBalloon::resizeEvent( QResizeEvent* e )
{
    QWidget::resizeEvent( e );
    if ( d->created )
        updateShape();
}

void KBalloon::paintEvent( QPaintEvent* )
{
    const QColor fill = colorGroup().background();
    const QColor border = borderColor( colorGroup() );
    const QValueVector< Point > shape = outline( width(), height(), d->tail );
#ifdef HAVE_XRENDER
    if ( d->argb ) {
        // the outline drawn once per size and tail, then copied
        Display* dpy = x11Display();
        XRenderPictFormat* argb = XRenderFindStandardFormat( dpy, PictStandardARGB32 );
        if ( d->cache == None || d->cacheSize != size() || d->cacheTail != d->tail || d->cacheFill != fill.rgb()
             || d->cacheBorder != border.rgb() ) {
            if ( d->cache != None )
                XFreePixmap( dpy, d->cache );
            d->cache = XCreatePixmap( dpy, winId(), width(), height(), 32 );
            d->cacheSize = size();
            d->cacheTail = d->tail;
            d->cacheFill = fill.rgb();
            d->cacheBorder = border.rgb();
            Picture pic = XRenderCreatePicture( dpy, d->cache, argb, 0, 0 );
            XRenderColor transparent = { 0, 0, 0, 0 };
            XRenderFillRectangle( dpy, PictOpSrc, pic, &transparent, 0, 0, width(), height() );
            fillPolygon( dpy, pic, border, shape );
            fillPolygon( dpy, pic, fill, inset( shape, 1.0 ) );
            XRenderFreePicture( dpy, pic );
        }
        Picture src = XRenderCreatePicture( dpy, d->cache, argb, 0, 0 );
        Picture dst = XRenderCreatePicture( dpy, winId(), XRenderFindVisualFormat( dpy, (Visual*) x11Visual() ),
                                            0, 0 );
        XRenderComposite( dpy, PictOpSrc, src, None, dst, 0, 0, 0, 0, 0, 0, width(), height() );
        XRenderFreePicture( dpy, src );
        XRenderFreePicture( dpy, dst );
        return;
    }
#endif
    // XShape: the mask is the outline, its edge pixels the border
    const QRegion region = outlineRegion( shape );
    QRegion inner = region;
    const int dx[ 4 ] = { 1, -1, 0, 0 };
    const int dy[ 4 ] = { 0, 0, 1, -1 };
    for ( int i = 0; i < 4; ++i ) {
        QRegion shifted = region;
        shifted.translate( dx[ i ], dy[ i ] );
        inner &= shifted;
    }
    QPainter p( this );
    p.fillRect( rect(), fill );
    p.setClipRegion( region - inner );
    p.fillRect( rect(), border );
}

void KBalloon::mouseReleaseEvent( QMouseEvent* e )
{
    if ( !rect().contains( e->pos() ) )
        return;
    if ( e->button() == LeftButton ) {
        QGuardedPtr< KBalloon > self( this );
        emit clicked();
        if ( !self.isNull() )
            hide();
    } else if ( e->button() == RightButton || e->button() == MidButton )
        hide();
}

/*
 The timeout pauses while the mouse pointer is over the balloon.  Whether
 it still is is looked at every HoverPoll ms while paused: a balloon that
 moves (one before it went) leaves the pointer without a leave event.
*/
static const int HoverPoll = 250;

void KBalloon::enterEvent( QEvent* )
{
    if ( d->timer.isActive() && !d->paused ) {
        d->remaining = QMAX( 0, d->remaining - d->started.elapsed() );
        d->paused = true;
        d->timer.start( HoverPoll, true );
    }
}

void KBalloon::leaveEvent( QEvent* )
{
    checkHover();
}

void KBalloon::checkHover()
{
    if ( !d->paused )
        return;
    if ( isVisible() && geometry().contains( QCursor::pos() ) ) {
        d->timer.start( HoverPoll, true );
        return;
    }
    // (at least a second to read on)
    d->paused = false;
    d->remaining = QMAX( d->remaining, 1000 );
    d->started.start();
    d->timer.start( d->remaining, true );
}

void KBalloon::slotAction()
{
    QMap< const QObject*, int >::ConstIterator it = d->buttonIds.find( sender() );
    if ( it == d->buttonIds.end() )
        return;
    const int id = *it;
    QGuardedPtr< KBalloon > self( this );
    emit actionActivated( id );
    if ( !self.isNull() )
        hide();
}

void KBalloon::slotTimeout()
{
    if ( d->paused ) {
        checkHover();
        return;
    }
    QGuardedPtr< KBalloon > self( this );
    emit timedOut();
    if ( !self.isNull() )
        hide();
}

#include "kballoon.moc"
