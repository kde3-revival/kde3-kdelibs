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
#include <qfontmetrics.h>
#include <qguardedptr.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmap.h>
#include <qpainter.h>
#include <qpointarray.h>
#include <qpopupmenu.h>
#include <qptrlist.h>
#include <qsimplerichtext.h>
#include <qstylesheet.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qvaluelist.h>
#include <qvaluevector.h>

#include <kdialog.h>
#include <kguiitem.h>
#include <klocale.h>
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
// while the pointer is over the balloon, whether it still is is looked at
// this often (ms; see enterEvent())
static const int HoverPoll = 250;

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

// the work area of the current desktop (_NET_WORKAREA) on the monitor
// screen (Qt 3's availableGeometry() is the monitor's whole area with
// several monitors)
QRect workArea( int screen )
{
    const QRect monitor = QApplication::desktop()->screenGeometry( screen );
    Display* dpy = qt_xdisplay();
    QRect area = monitor;
    Atom type;
    int format;
    unsigned long n, left;
    unsigned char* data = 0;
    long desktop = 0;
    if ( XGetWindowProperty( dpy, qt_xrootwin(), atom( dpy, "_NET_CURRENT_DESKTOP" ), 0, 1, False, XA_CARDINAL,
                             &type, &format, &n, &left, &data ) == Success && data != 0 && format == 32 && n == 1 )
        desktop = *(long*) data;
    if ( data != 0 )
        XFree( data );
    data = 0;
    if ( desktop >= 0 && desktop < 1024
         && XGetWindowProperty( dpy, qt_xrootwin(), atom( dpy, "_NET_WORKAREA" ), desktop * 4, 4, False,
                                XA_CARDINAL, &type, &format, &n, &left, &data ) == Success
         && data != 0 && format == 32 && n == 4 ) {
        const long* v = (const long*) data;
        const QRect work( (int) v[ 0 ], (int) v[ 1 ], (int) v[ 2 ], (int) v[ 3 ] );
        if ( work.intersects( monitor ) )
            area = work & monitor;
    }
    if ( data != 0 )
        XFree( data );
    return area;
}

// plain text as rich text that shows it as it is: escaped, line breaks
// kept, runs of spaces and tabs kept (still wrapped at single spaces)
QString plainToRich( const QString& text )
{
    const QString escaped = QStyleSheet::escape( text );
    QString result;
    bool space = true; // (at the start of a line: kept)
    for ( unsigned int i = 0; i < escaped.length(); ++i ) {
        const QChar c = escaped[ i ];
        if ( c == '\r' && i + 1 < escaped.length() && escaped[ i + 1 ] == '\n' )
            continue;
        if ( c == '\n' || c == '\r' ) {
            result += "<br>";
            space = true;
        } else if ( c == '\t' ) {
            result += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
            space = true;
        } else if ( c == ' ' ) {
            result += space ? "&nbsp;" : " ";
            space = true;
        } else {
            result += c;
            space = false;
        }
    }
    return result;
}

// Whether a word may be broken before its character @p i: not inside a
// surrogate pair, not before a combining mark or a variation selector,
// not next to a joiner, not before a character beyond the BMP (Qt 3 has
// no properties for those: a mark or selector would look like a letter).
bool canBreakBefore( const QString& word, unsigned int i )
{
    const ushort c = word[ i ].unicode();
    const ushort before = word[ i - 1 ].unicode();
    if ( ( c >= 0xd800 && c <= 0xdfff ) || c == 0x200d || before == 0x200d
         || ( c >= 0xfe00 && c <= 0xfe0f ) )
        return false;
    const QChar::Category category = word[ i ].category();
    return category != QChar::Mark_NonSpacing && category != QChar::Mark_SpacingCombining
           && category != QChar::Mark_Enclosing;
}

// The end of the first line of @p word from @p start that is at most
// @p width wide (in @p metrics) and ends where it may be broken (or at
// the end); without such a place before the edge the first one after it
// (that line is wider).  The widths of longer and longer pieces only:
// about n log n for a line of n characters.
unsigned int lineEnd( const QString& word, unsigned int start, const QFontMetrics& metrics, int width )
{
    const unsigned int rest = word.length() - start;
    // a length that does not fit (or all), doubled from one
    unsigned int fits = 0;
    unsigned int over = 1;
    while ( over < rest && metrics.width( word.mid( start, over ) ) <= width ) {
        fits = over;
        over = QMIN( rest, 2 * over );
    }
    if ( over >= rest && metrics.width( word.mid( start, rest ) ) <= width )
        return word.length();
    // the longest length that fits: between fits and over
    while ( over - fits > 1 ) {
        const unsigned int middle = fits + ( over - fits ) / 2;
        if ( metrics.width( word.mid( start, middle ) ) <= width )
            fits = middle;
        else
            over = middle;
    }
    unsigned int end = start + fits;
    while ( end > start && !canBreakBefore( word, end ) )
        --end;
    if ( end > start )
        return end;
    for ( end = start + QMAX( 1u, fits ); end < word.length() && !canBreakBefore( word, end ); ++end )
        ;
    return end;
}

// Where rich text may wrap between words: white space, but the spaces
// that do not break (no-break, figure, narrow no-break space).
bool isWordSeparator( const QChar& c )
{
    const ushort u = c.unicode();
    return c.isSpace() && u != 0xa0 && u != 0x2007 && u != 0x202f;
}

// @p text with each word wider than @p width (in @p metrics) broken into
// lines no wider (between characters, @p lineBreak between): rich text
// wraps only between words, a longer one would be cut off at the edge.
// With @p escape the text is escaped for rich text.
QString breakLongWords( const QString& text, const QFontMetrics& metrics, int width,
                        const QString& lineBreak, bool escape )
{
    QString result;
    unsigned int i = 0;
    while ( i < text.length() ) {
        unsigned int end = i;
        while ( end < text.length() && !isWordSeparator( text[ end ] ) )
            ++end;
        const QString word = text.mid( i, end - i );
        if ( metrics.width( word ) > width ) {
            for ( unsigned int start = 0; start < word.length(); ) {
                const unsigned int stop = lineEnd( word, start, metrics, width );
                const QString line = word.mid( start, stop - start );
                if ( start > 0 )
                    result += lineBreak;
                result += escape ? QStyleSheet::escape( line ) : line;
                start = stop;
            }
        } else
            result += escape ? QStyleSheet::escape( word ) : word;
        for ( ; end < text.length() && isWordSeparator( text[ end ] ); ++end )
            result += text[ end ];
        i = end;
    }
    return result;
}

// The rule between the paragraphs of setTextLines(): a thin line in the
// colour of the text faded into the background, a little room above and
// below (Qt's <hr> without its sunken shade, see QTextHorizontalLine).
QString ruleHtml( const QColorGroup& cg )
{
    const QColor bg = cg.background();
    const QColor fg = cg.foreground();
    const QColor line( ( bg.red() * 3 + fg.red() ) / 4, ( bg.green() * 3 + fg.green() ) / 4,
                       ( bg.blue() * 3 + fg.blue() ) / 4 );
    return "<hr noshade color=\"" + line.name() + "\">";
}

// the title (bold) and the text of a balloon in rich text, the words
// wider than @p width broken (but those of rich text); with @p lines
// (setTextLines()) those paragraphs, @p rule between them
QString balloonHtml( const QString& title, const QString& text, bool rich, const QFont& font, int width,
                     const QStringList* lines = 0, const QString& rule = QString::null )
{
    if ( lines ) {
        QString html = balloonHtml( title, QString::null, false, font, width );
        for ( QStringList::ConstIterator it = lines->begin(); it != lines->end(); ++it ) {
            if ( it != lines->begin() )
                html += rule;
            else if ( !html.isEmpty() )
                html += "<br>";
            html += plainToRich( breakLongWords( *it, QFontMetrics( font ), width, "\n", false ) );
        }
        return html;
    }
    QString html;
    if ( !title.isEmpty() ) {
        QFont bold( font );
        bold.setBold( true );
        html = "<b>" + breakLongWords( title, QFontMetrics( bold ), width, "<br>", true ) + "</b>";
    }
    if ( !text.isEmpty() ) {
        if ( !html.isEmpty() )
            html += "<br>";
        if ( rich )
            html += text;
        else
            html += plainToRich( breakLongWords( text, QFontMetrics( font ), width, "\n", false ) );
    }
    return html;
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
// the colormaps for the ARGB windows (one per display, screen and visual,
// kept for the process)
struct ArgbColormap
{
    Display* dpy;
    int screen;
    Visual* visual;
    Colormap colormap;
};

Colormap argbColormap( Display* dpy, int screen, Visual* visual )
{
    static QValueList< ArgbColormap > colormaps;
    for ( QValueList< ArgbColormap >::ConstIterator it = colormaps.begin(); it != colormaps.end(); ++it )
        if ( (*it).dpy == dpy && (*it).screen == screen && (*it).visual == visual )
            return (*it).colormap;
    ArgbColormap c;
    c.dpy = dpy;
    c.screen = screen;
    c.visual = visual;
    c.colormap = XCreateColormap( dpy, RootWindow( dpy, screen ), visual, AllocNone );
    colormaps.append( c );
    return c.colormap;
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

// the size a layout wants (its height for its width)
QSize contentsSize( QLayout* layout )
{
    layout->invalidate();
    layout->activate();
    QSize size = layout->totalSizeHint();
    if ( layout->hasHeightForWidth() )
        size.setHeight( layout->totalHeightForWidth( size.width() ) );
    return size.expandedTo( layout->totalMinimumSize() );
}

} // namespace

class KBalloon::Private
{
public:
    Private()
        : rich( false ), separated( false ), nextId( 1 ), timeoutMsec( 6000 ), autoDelete( true ), hasPoint( false ),
          created( false ), argb( false ), queued( false ), tail( NoTail ), content( 0 ), picture( 0 ),
          label( 0 ), buttonRows( 0 ), more( 0 ), moreMenu( 0 ), menuAction( -1 ), buttonsDirty( true ), remaining( 0 ), paused( false ),
          cache( None ), cacheTail( -1 ), cacheFill( 0 ), cacheBorder( 0 ), requeueing( false )
    {
    }

    QString title;
    QString text;
    bool rich;
    QStringList lines; // setTextLines()
    bool separated;
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
    QVBoxLayout* buttonRows; // rows of buttons, as many as the monitor's width needs
    KPushButton* more; // the actions beyond the rows the monitor's height allows
    QPopupMenu* moreMenu;
    int menuAction; // chosen in the menu, told later (-1: none)
    QPtrList< QLayout > rows;
    QString html; // the label's text
    QRect fitArea; // the area the contents were fitted to (fitContents())
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
    // where it may be on the monitor screen: all of it at an anchor, the
    // work area without one
    QRect area( int screen ) const;
    bool requeueing; // hidden to wait again: not gone
};

QRect KBalloon::Private::area( int screen ) const
{
    if ( hasPoint || ( (QWidget*) widget != 0 && widget->isVisible() ) )
        return QApplication::desktop()->screenGeometry( screen );
    return workArea( screen );
}

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
    const bool queued = queuedBalloons().removeRef( this );
    const bool shown = shownBalloons().removeRef( this );
    if ( d->cache != None )
        XFreePixmap( x11Display(), d->cache );
    delete d;
    d = 0;
    if ( shown || queued )
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
    d->separated = false;
    d->lines.clear();
    updateContents();
}

void KBalloon::setTextLines( const QStringList& lines )
{
    d->lines = lines;
    d->text = lines.join( "\n" );
    d->rich = false;
    d->separated = true;
    updateContents();
}

void KBalloon::setRichText( const QString& text )
{
    d->text = text;
    d->rich = true;
    d->separated = false;
    d->lines.clear();
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
        // (counted anew; paused while the pointer is over it)
        d->timer.stop();
        d->paused = false;
        d->remaining = d->timeoutMsec;
        if ( d->remaining > 0 ) {
            if ( geometry().contains( QCursor::pos() ) ) {
                d->paused = true;
                d->timer.start( HoverPoll, true );
            } else {
                d->started.start();
                d->timer.start( d->remaining, true );
            }
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
    d->picture->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) );
    row->addWidget( d->picture, 0, AlignTop );
    d->label = new QLabel( d->content, "text" );
    d->label->setTextFormat( RichText );
    d->label->setAlignment( AlignTop | AlignLeft | WordBreak );
    row->addWidget( d->label );
    row->addStretch( 1 ); // (room the buttons make)
    d->buttonRows = new QVBoxLayout( top, KDialog::spacingHint() );
    d->more = new KPushButton( i18n( "more actions", "More" ), d->content, "more" );
    d->more->setFocusPolicy( NoFocus );
    d->moreMenu = new QPopupMenu( d->more, "moremenu" );
    d->more->setPopup( d->moreMenu );
    d->more->hide();
    connect( d->moreMenu, SIGNAL( activated( int ) ), SLOT( slotMenuAction( int ) ) );
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
    // the text: the title escaped, plain text escaped (its long words
    // breakable once the width is known, fitContents())
    // (setTextLines(): the rules in the label, measured as line breaks:
    // a rule is as wide as the label, it needs no width)
    const QStringList* lines = d->separated ? &d->lines : 0;
    const QString html = balloonHtml( d->title, d->text, d->rich, d->label->font(), QWIDGETSIZE_MAX,
                                      lines, ruleHtml( d->label->colorGroup() ) );
    d->label->setText( html );
    d->html = balloonHtml( d->title, d->text, d->rich, d->label->font(), QWIDGETSIZE_MAX, lines, "<br>" );
    // the buttons
    if ( d->buttonsDirty ) {
        d->buttonsDirty = false;
        d->buttons.setAutoDelete( true );
        d->buttons.clear();
        d->buttonIds.clear();
        for ( QValueList< Action >::ConstIterator it = d->actions.begin(); it != d->actions.end(); ++it ) {
            KPushButton* button = new KPushButton( (*it).item, d->content );
            button->setFocusPolicy( NoFocus );
            d->buttons.append( button );
            d->buttonIds[ button ] = (*it).id;
            connect( button, SIGNAL( clicked() ), SLOT( slotAction() ) );
            button->show();
        }
    }
    fitContents();
    if ( isVisible() || d->queued )
        relayoutAll();
}

/*
 The contents fitted to the area of the monitor the balloon goes onto
 (again whenever that changes, see place()): the text as wide as it
 needs, at most about 50 characters (a third of the monitor), wrapped
 beyond; the buttons in as many rows as that width needs (one wider than
 the monitor alone: narrower, its text cut); the text shorter (its end
 cut) when it is all higher than the monitor.
*/
void KBalloon::fitContents()
{
    const QRect s = d->area( d->screen() );
    d->fitArea = s;
    const QSize most( QMAX( 40, s.width() - TailSize - 2 * ContentInset ),
                      QMAX( 40, s.height() - TailSize - 2 * ContentInset ) );
    const int margin = QMAX( 0, KDialog::marginHint() - ContentInset );
    const int spacing = KDialog::spacingHint();
    const int widest = QMAX( 120, QMIN( d->label->fontMetrics().width( 'x' ) * 50, s.width() / 3 ) );
    QSimpleRichText measure( d->html, d->label->font() );
    measure.setWidth( 32767 );
    const int natural = measure.widthUsed() + 2;
    const int labelWidth = QMAX( 20, QMIN( QMIN( natural, widest ),
                                           most.width() - 2 * margin
                                           - ( d->picture->isVisible() ? d->picture->sizeHint().width() + spacing
                                                                       : 0 ) ) );
    d->label->setFixedWidth( labelWidth );
    // a word wider than that broken where it reaches the edge
    const QString html = balloonHtml( d->title, d->text, d->rich, d->label->font(),
                                      labelWidth - 2 * d->label->frameWidth(), // (with the margin)
                                      d->separated ? &d->lines : 0, ruleHtml( d->label->colorGroup() ) );
    if ( html != d->label->text() )
        d->label->setText( html );
    d->label->setMinimumHeight( 0 );
    d->label->setMaximumHeight( QWIDGETSIZE_MAX );
    d->label->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum, true ) );
    // the rows of buttons (each right aligned)
    for ( QPtrListIterator< QLayout > it( d->rows ); it.current() != 0; ++it )
        d->buttonRows->removeItem( it.current() ); // (its buttons are added again)
    d->rows.setAutoDelete( true );
    d->rows.clear();
    // as many rows as the height leaves beside some text (and the
    // picture): the actions beyond in the menu of a button "More"
    const int room = most.width() - 2 * margin;
    const int moreWidth = QMIN( room, d->more->sizeHint().expandedTo( d->more->minimumSizeHint() ).width() );
    int rowHeight = d->more->sizeHint().height();
    for ( QPtrListIterator< QWidget > it( d->buttons ); it.current() != 0; ++it )
        rowHeight = QMAX( rowHeight, it.current()->sizeHint().height() );
    // (up to three lines of the text kept)
    const int lines = d->label->fontMetrics().lineSpacing();
    const int top = QMAX( QMAX( lines, QMIN( 3 * lines, d->label->heightForWidth( d->label->width() ) ) ),
                          d->picture->isVisible() ? d->picture->sizeHint().height() : 0 );
    const int maxRows = QMAX( 1, ( most.height() - 2 * margin - top ) / ( rowHeight + spacing ) );
    QValueList< int > widths;
    int rowsNeeded = 0;
    {
        int used = 0;
        for ( QPtrListIterator< QWidget > it( d->buttons ); it.current() != 0; ++it ) {
            QWidget* button = it.current();
            button->setMaximumWidth( QWIDGETSIZE_MAX );
            const int w = QMIN( room, button->sizeHint().expandedTo( button->minimumSizeHint() ).width() );
            if ( w == room )
                button->setMaximumWidth( room );
            widths.append( w );
            if ( rowsNeeded == 0 || used + spacing + w > room ) {
                ++rowsNeeded;
                used = w;
            } else
                used += spacing + w;
        }
    }
    const bool overflow = rowsNeeded > maxRows;
    d->moreMenu->clear();
    QHBoxLayout* row = 0;
    int rowCount = 0;
    int used = 0;
    bool full = false; // (the rest in the menu)
    QValueList< int >::ConstIterator width = widths.begin();
    QValueList< Action >::ConstIterator action = d->actions.begin();
    for ( QPtrListIterator< QWidget > it( d->buttons ); it.current() != 0; ++it, ++width, ++action ) {
        QWidget* button = it.current();
        const int w = *width;
        if ( !full && ( row == 0 || used + spacing + w > room ) ) {
            if ( rowCount == maxRows )
                full = true;
            else {
                row = new QHBoxLayout( d->buttonRows, spacing );
                row->addStretch( 1 );
                d->rows.append( row );
                ++rowCount;
                used = 0;
            }
        }
        // (in the last row, room is kept for "More")
        if ( !full && overflow && rowCount == maxRows
             && ( used > 0 ? used + spacing : 0 ) + w + spacing + moreWidth > room )
            full = true;
        if ( full ) {
            button->hide();
            d->moreMenu->insertItem( (*action).item.text(), (*action).id );
            continue;
        }
        used += ( used > 0 ? spacing : 0 ) + w;
        row->addWidget( button );
        button->show();
    }
    if ( d->moreMenu->count() > 0 ) {
        if ( row == 0 ) {
            row = new QHBoxLayout( d->buttonRows, spacing );
            row->addStretch( 1 );
            d->rows.append( row );
        }
        row->addWidget( d->more );
        d->more->show();
    } else
        d->more->hide();
    QSize size = contentsSize( d->content->layout() );
    if ( size.height() > most.height() ) {
        const int text = d->label->heightForWidth( d->label->width() );
        d->label->setFixedHeight( QMAX( d->label->fontMetrics().lineSpacing(),
                                        text - ( size.height() - most.height() ) ) );
        size = contentsSize( d->content->layout() );
    }
    size = size.boundedTo( most );
    d->bodySize = QSize( size.width() + 2 * ContentInset, size.height() + 2 * ContentInset )
                  .expandedTo( QSize( 2 * ( Radius + TailBase ) + 8, 2 * ( Radius + TailBase ) + 8 ) );
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
    if ( d->area( d->screen() ) != d->fitArea )
        fitContents(); // (another monitor, another work area)
    QDesktopWidget* desktop = QApplication::desktop();
    QSize body = d->bodySize;
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
        area = workArea( screen );
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
    if ( !area.contains( geom ) ) { // (larger than a tiny monitor: as much as there is)
        geom &= area;
        if ( tail != NoTail && geom.size() != body + QSize( TailSize, TailSize ) )
            tail = NoTail;
        body = geom.size() - ( tail != NoTail ? QSize( TailSize, TailSize ) : QSize( 0, 0 ) );
    }
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
/*
 Showing a balloon delivers events, whose handlers may show, hide or
 delete balloons (which relayout again): a relayout asked for during one
 is done after it, and each balloon is looked at only while it still
 exists and is still where it was (shown, queued).
*/
static int relayoutDepth = 0;
static bool relayoutAgain = false;

void KBalloon::relayoutAll()
{
    if ( relayoutDepth > 0 ) {
        relayoutAgain = true;
        return;
    }
    ++relayoutDepth;
    for ( int round = 0; round < 16; ++round ) {
        relayoutAgain = false;
        relayoutPass();
        if ( !relayoutAgain )
            break;
    }
    --relayoutDepth;
    if ( relayoutAgain ) { // (still asked for: later, from the event loop)
        relayoutAgain = false;
        KBalloon* any = !shownBalloons().isEmpty() ? shownBalloons().first()
                        : !queuedBalloons().isEmpty() ? queuedBalloons().first() : 0;
        if ( any != 0 ) // (none left: nothing to place; one deleted relayouts itself)
            QTimer::singleShot( 0, any, SLOT( slotRelayout() ) );
    }
}

void KBalloon::relayoutPass()
{
    // (one that no longer fits, the ones before it larger or its anchor
    // moved: waits again, first on its monitor)
    QValueList< QGuardedPtr< KBalloon > > shown;
    for ( QPtrListIterator< KBalloon > it( shownBalloons() ); it.current() != 0; ++it )
        shown.append( it.current() );
    // (queued before it is hidden: its events may delete or cancel it,
    // which takes it out of the queue)
    QValueList< QGuardedPtr< KBalloon > > requeued;
    for ( QValueList< QGuardedPtr< KBalloon > >::ConstIterator it = shown.begin(); it != shown.end(); ++it ) {
        KBalloon* b = *it;
        if ( b == 0 || shownBalloons().findRef( b ) < 0 || b->place() )
            continue;
        shownBalloons().removeRef( b );
        b->d->timer.stop();
        b->d->paused = false;
        b->d->queued = true;
        // (right after those queued again before it that are still there:
        // their events may have taken some out)
        int at = 0;
        for ( QValueList< QGuardedPtr< KBalloon > >::ConstIterator r = requeued.begin(); r != requeued.end(); ++r )
            if ( !( *r ).isNull() )
                at = QMAX( at, queuedBalloons().findRef( *r ) + 1 );
        queuedBalloons().insert( at, b );
        requeued.append( b );
        QGuardedPtr< KBalloon > guard( b );
        b->d->requeueing = true;
        b->QWidget::hide();
        if ( !guard.isNull() )
            guard->d->requeueing = false;
    }
    // (in order on each monitor: none before one that still waits there)
    QValueList< QGuardedPtr< KBalloon > > queue;
    for ( QPtrListIterator< KBalloon > it( queuedBalloons() ); it.current() != 0; ++it )
        queue.append( it.current() );
    QValueList< int > waiting;
    for ( QValueList< QGuardedPtr< KBalloon > >::ConstIterator it = queue.begin(); it != queue.end(); ++it ) {
        KBalloon* b = *it;
        if ( b == 0 || queuedBalloons().findRef( b ) < 0 || !b->d->queued )
            continue; // (gone, or left the queue meanwhile)
        const int screen = b->d->screen();
        if ( waiting.contains( screen ) )
            continue;
        if ( !b->place() ) {
            waiting.append( screen );
            continue;
        }
        queuedBalloons().removeRef( b );
        b->d->queued = false;
        if ( shownBalloons().findRef( b ) < 0 )
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
        cancel();
        return;
    }
    QWidget::hide();
}

// a queued one leaves the queue (the next ones may fit now)
void KBalloon::cancel()
{
    queuedBalloons().removeRef( this );
    d->queued = false;
    relayoutAll();
    if ( d->autoDelete )
        deleteLater();
}

void KBalloon::closeEvent( QCloseEvent* e )
{
    // (QWidget::close() hides only shown widgets: a queued one is not)
    if ( d->queued )
        cancel();
    e->accept();
}

void KBalloon::hideEvent( QHideEvent* e )
{
    QWidget::hideEvent( e );
    if ( d->requeueing )
        return;
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

// (the menu is still busy with its signal: the receiver may delete the
// balloon with it, so the action is told from the event loop)
void KBalloon::slotMenuAction( int id )
{
    d->menuAction = id;
    QTimer::singleShot( 0, this, SLOT( slotMenuActionLater() ) );
}

void KBalloon::slotMenuActionLater()
{
    if ( d->menuAction < 0 )
        return;
    const int id = d->menuAction;
    d->menuAction = -1;
    QGuardedPtr< KBalloon > self( this );
    emit actionActivated( id );
    if ( !self.isNull() )
        hide();
}

void KBalloon::slotRelayout()
{
    relayoutAll();
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
