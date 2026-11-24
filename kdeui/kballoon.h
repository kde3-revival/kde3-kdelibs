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

#ifndef KBALLOON_H
#define KBALLOON_H

#include <qwidget.h>

#include <kdelibs_export.h>

class KGuiItem;

/**
 * @short A notification balloon pointing at a place on the screen.
 *
 * A KBalloon is a small top-level window in the colors of the tooltips,
 * with rounded corners and a tail that points at an anchor: a point on
 * the screen or a widget, typically an application's icon in the system
 * tray.  It shows a title, a text, optionally a picture (the sender's
 * avatar, an icon) and buttons for actions; it goes away by itself after
 * a timeout, when it is clicked, or when an action is chosen.
 *
 * \code
 *    KBalloon* balloon = new KBalloon;
 *    balloon->setTitle( from );               // plain text, never markup
 *    balloon->setText( subject );             // plain text, never markup
 *    balloon->setPixmap( avatar );
 *    balloon->insertAction( i18n( "Reply" ), 1 );
 *    balloon->setAnchor( trayIcon );
 *    connect( balloon, SIGNAL( clicked() ), this, SLOT( showMessage() ) );
 *    connect( balloon, SIGNAL( actionActivated( int ) ), this, SLOT( reply( int ) ) );
 *    balloon->show();                         // deletes itself when it goes
 * \endcode
 *
 * Placement: the balloon goes onto the monitor of its anchor (Xinerama,
 * RandR) and opens towards that monitor's middle, the tail at the
 * corner next to the anchor; it flips above or below and left or right
 * of the anchor to stay on the monitor.  Without an anchor it goes into
 * the bottom right corner of the work area of the monitor with the mouse
 * pointer, without a tail.
 *
 * Stacking: balloons shown at the same time don't overlap.  A balloon
 * that would cover one shown before it is moved further away from its
 * anchor (and loses its tail); one for which there is no room left on
 * the monitor waits (isQueued()) and is shown when another one goes.
 * The balloons that stay move back when one goes.
 *
 * Timeout: counted while the balloon is shown, paused while the mouse
 * pointer is over it.
 *
 * Window: a window of the type _NET_WM_WINDOW_TYPE_NOTIFICATION
 * (NET::Notification), which does not take the focus (its input hint is
 * off), is not in the taskbar or the pager and is on all desktops.  When
 * a compositing manager runs (an owner of the selection _NET_WM_CM_S<n>)
 * when the balloon is first shown, the window has a 32-bit ARGB visual:
 * the edges are anti-aliased (drawn with XRender) and the window asks the
 * compositing manager for a shadow made from its alpha channel
 * (_KDE_NET_WM_SHADOW_FROM_ALPHA).  Otherwise the window is shaped
 * (XShape) to the balloon.  The visual is chosen once: a balloon keeps
 * it when the compositing manager starts or stops while it exists.
 *
 * The contents are child widgets as usual (the palette is the tooltips'
 * one); in the ARGB case they are in a child window with the default
 * visual inside the balloon's outline.  Don't call winId() before the
 * balloon is first shown: the window is replaced then.
 *
 * @since 3.5.11
 */
class KDEUI_EXPORT KBalloon : public QWidget
{
    Q_OBJECT

public:
    /**
     * Creates a balloon.  It is a top-level window whatever @p parent is;
     * a @p parent deletes it with itself.
     */
    KBalloon( QWidget* parent = 0, const char* name = 0 );

    /**
     * Destroys the balloon.  The balloons shown after it move into its
     * place.
     */
    virtual ~KBalloon();

    /**
     * Sets the title, shown in bold above the text.  It is plain text:
     * markup in it is shown as it is, never interpreted.
     */
    void setTitle( const QString& title );

    /**
     * The title.
     */
    QString title() const;

    /**
     * Sets the text from plain text: everything in it is shown as it is
     * (no markup is interpreted, it may come from anywhere: a message's
     * subject, a chat message); line breaks are kept and long lines are
     * wrapped.
     * @see setRichText()
     */
    void setText( const QString& text );

    /**
     * Sets the text from rich text (Qt's subset of HTML, see QStyleSheet),
     * for text that the application made itself.  Never pass text from
     * elsewhere (a message, a file, the network) without escaping it with
     * QStyleSheet::escape(): markup in it would be interpreted, images
     * would be loaded.
     * @see setText()
     */
    void setRichText( const QString& text );

    /**
     * The text as it was set (plain or rich).
     */
    QString text() const;

    /**
     * Whether the text was set with setRichText().
     */
    bool isRichText() const;

    /**
     * Sets the picture shown to the left of the text (a sender's avatar,
     * an application's icon).  Pictures larger than 64 x 64 pixels are
     * scaled down to 48 x 48 pixels (keeping the aspect ratio).  A null
     * pixmap removes it.
     */
    void setPixmap( const QPixmap& pixmap );

    /**
     * The picture as it was set.
     */
    QPixmap pixmap() const;

    /**
     * Adds a button for an action below the text: choosing it emits
     * actionActivated( @p id ) and closes the balloon.
     * @param text the button's text (with an accelerator, as for QPushButton)
     * @param id the action's id; a negative one: a new id is chosen
     * @return the action's id
     */
    int insertAction( const QString& text, int id = -1 );

    /**
     * Adds a button for an action, with the text, icon and tooltip of
     * @p item.
     * @see insertAction( const QString&, int )
     */
    int insertAction( const KGuiItem& item, int id = -1 );

    /**
     * Removes all the action buttons.
     */
    void clearActions();

    /**
     * Sets after how many milliseconds the shown balloon goes away by
     * itself (timedOut()).  0: never.  The default is 6 seconds.
     */
    void setTimeout( int msec );

    /**
     * The timeout in milliseconds.
     */
    int timeout() const;

    /**
     * Whether the balloon deletes itself when it is hidden (when it goes
     * away by itself, or with hide() or close()).  The default is true.
     */
    void setAutoDelete( bool autoDelete );

    /**
     * Whether the balloon deletes itself when it is hidden.
     */
    bool autoDelete() const;

    /**
     * Anchors the balloon at @p point (global coordinates): the tip of
     * its tail is there.
     */
    void setAnchor( const QPoint& point );

    /**
     * Anchors the balloon at @p widget (an icon in the system tray, a
     * button): the tail points at the middle of the widget's edge that
     * faces the balloon.  Where the widget is is read whenever the
     * balloon is placed.  When the widget is deleted or hidden, the
     * balloon has no anchor.
     */
    void setAnchor( QWidget* widget );

    /**
     * Removes the anchor: the balloon goes into the corner of the screen.
     */
    void clearAnchor();

    /**
     * Where the tail points when the balloon is placed now (global
     * coordinates); a null point without an anchor.
     */
    QPoint anchorPoint() const;

    /**
     * Whether show() was called but the balloon waits for room on its
     * monitor (see the class description).
     */
    bool isQueued() const;

    /**
     * Whether the balloon is drawn anti-aliased in a 32-bit ARGB window
     * (a compositing manager ran when it was first shown).
     */
    bool isTranslucent() const;

    /**
     * Shows a balloon with @p title, @p text (plain text, see setText())
     * and @p pixmap at @p anchor (0: in the corner of the screen).  It
     * deletes itself when it goes.
     */
    static KBalloon* message( const QString& title, const QString& text, const QPixmap& pixmap,
                              QWidget* anchor, int timeout = 6000 );

public slots:
    /**
     * Places and shows the balloon (or queues it when there is no room on
     * its monitor), and starts the timeout.  Called again while it is
     * shown, it is placed again (the anchor may have moved).
     */
    virtual void show();

    /**
     * Hides the balloon (and deletes it when autoDelete() is set); a
     * queued one leaves the queue.  No signal is emitted.
     */
    virtual void hide();

signals:
    /**
     * The balloon (not one of its buttons) was clicked with the left
     * mouse button.  It closes after this signal; other buttons close it
     * without a signal.
     */
    void clicked();

    /**
     * The action @p id was chosen.  The balloon closes after this signal.
     */
    void actionActivated( int id );

    /**
     * The timeout went by.  The balloon closes after this signal.
     */
    void timedOut();

protected:
    virtual void paintEvent( QPaintEvent* e );
    virtual void resizeEvent( QResizeEvent* e );
    virtual void mouseReleaseEvent( QMouseEvent* e );
    virtual void enterEvent( QEvent* e );
    virtual void leaveEvent( QEvent* e );
    virtual void hideEvent( QHideEvent* e );
    virtual void closeEvent( QCloseEvent* e );

private slots:
    void slotAction();
    void slotTimeout();
    void slotMenuAction( int id );
    void slotRelayout();

private:
    void createWindow();
    void buildContents();
    void updateContents();
    void fitContents();
    void updateShape();
    bool place();
    void showNow();
    void checkHover();
    void cancel();
    static void relayoutAll();
    static void relayoutPass();

    class Private;
    Private* d;
};

#endif
