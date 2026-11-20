/************************************************************************\
 *
 *  (c) 2016 by Serghei Amelian <serghei.amelian@gmail.com>
 *  (c) 2026 rewrite: PulseAudio main loop on top of the Qt event loop
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
\************************************************************************/
#include <sys/time.h>
#include <limits.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>

#include <qevent.h>
#include <qsocketnotifier.h>

#include "kpulsemainloop.h"

// libpulse marks a struct timeval holding a CLOCK_MONOTONIC time (as
// returned by pa_rtclock_now()) rather than a wall clock time by setting
// this bit in tv_usec. It is PA_TIMEVAL_RTCLOCK from the private header
// pulsecore/core-rtclock.h; pa_mainloop and the glib main loop adapter
// both honour it. A valid tv_usec is below 1000000 (< 2^20), so the bit
// is unambiguous.
static const long KPULSE_TIMEVAL_RTCLOCK = 1L << 30;

enum { NotifierRead = 0, NotifierWrite, NotifierError, NotifierCount };

static const pa_io_event_flags_t notifierFlag[NotifierCount] = {
    PA_IO_EVENT_INPUT, PA_IO_EVENT_OUTPUT, PA_IO_EVENT_ERROR
};

static const QSocketNotifier::Type notifierType[NotifierCount] = {
    QSocketNotifier::Read, QSocketNotifier::Write, QSocketNotifier::Exception
};

class KPulseIoNotifier;

struct KPulseIoEvent
{
    KPulseMainloop *loop;
    int fd;
    pa_io_event_flags_t events;
    pa_io_event_cb_t cb;
    pa_io_event_destroy_cb_t destroy;
    void *userdata;
    bool dead;
    KPulseIoNotifier *notifier[NotifierCount];
};

struct KPulseTimeEvent
{
    KPulseMainloop *loop;
    struct timeval tv;      // as given by libpulse, passed back to cb
    pa_usec_t due;          // tv converted to the monotonic clock
    int timerId;            // Qt timer, 0 when disabled
    pa_time_event_cb_t cb;
    pa_time_event_destroy_cb_t destroy;
    void *userdata;
    bool dead;
};

struct KPulseDeferEvent
{
    KPulseMainloop *loop;
    bool enabled;
    pa_defer_event_cb_t cb;
    pa_defer_event_destroy_cb_t destroy;
    void *userdata;
    bool dead;
};

// The opaque libpulse handles are our own structs.
static inline KPulseIoEvent *ioEvent( pa_io_event *e )
{ return reinterpret_cast<KPulseIoEvent *>( e ); }
static inline pa_io_event *paEvent( KPulseIoEvent *e )
{ return reinterpret_cast<pa_io_event *>( e ); }
static inline KPulseTimeEvent *timeEvent( pa_time_event *e )
{ return reinterpret_cast<KPulseTimeEvent *>( e ); }
static inline pa_time_event *paEvent( KPulseTimeEvent *e )
{ return reinterpret_cast<pa_time_event *>( e ); }
static inline KPulseDeferEvent *deferEvent( pa_defer_event *e )
{ return reinterpret_cast<KPulseDeferEvent *>( e ); }
static inline pa_defer_event *paEvent( KPulseDeferEvent *e )
{ return reinterpret_cast<pa_defer_event *>( e ); }

// A socket notifier that hands its activation straight to the main loop
// (QSocketNotifier delivers it to itself as a QEvent::SockAct).
class KPulseIoNotifier : public QSocketNotifier
{
public:
    KPulseIoNotifier( KPulseIoEvent *e, int which )
        : QSocketNotifier( e->fd, notifierType[which] ), m_event( e ), m_flag( notifierFlag[which] )
    {}

protected:
    bool event( QEvent *e )
    {
        if ( e->type() == QEvent::SockAct ) {
            m_event->loop->dispatchIo( m_event, m_flag );
            return true;
        }
        return QSocketNotifier::event( e );
    }

private:
    KPulseIoEvent *m_event;
    pa_io_event_flags_t m_flag;
};


KPulseMainloop::KPulseMainloop( QObject *parent, const char *name )
    : QObject( parent, name ), m_quitValue( 0 ), m_enabledDefers( 0 ),
      m_deferTimer( 0 ), m_cleanupTimer( 0 ), m_dead( 0 )
{
    m_api.userdata = this;
    m_api.io_new = ioNew;
    m_api.io_enable = ioEnable;
    m_api.io_free = ioFree;
    m_api.io_set_destroy = ioSetDestroy;
    m_api.time_new = timeNew;
    m_api.time_restart = timeRestart;
    m_api.time_free = timeFree;
    m_api.time_set_destroy = timeSetDestroy;
    m_api.defer_new = deferNew;
    m_api.defer_enable = deferEnable;
    m_api.defer_free = deferFree;
    m_api.defer_set_destroy = deferSetDestroy;
    m_api.quit = quit;
}

KPulseMainloop::~KPulseMainloop()
{
    cleanup( true );
}

static inline KPulseMainloop *loopOf( pa_mainloop_api *a )
{
    return static_cast<KPulseMainloop *>( a->userdata );
}


// ---- io events -------------------------------------------------------

pa_io_event *KPulseMainloop::ioNew( pa_mainloop_api *a, int fd, pa_io_event_flags_t events,
                                    pa_io_event_cb_t cb, void *userdata )
{
    KPulseMainloop *self = loopOf( a );
    KPulseIoEvent *e = new KPulseIoEvent;
    e->loop = self;
    e->fd = fd;
    e->events = PA_IO_EVENT_NULL;
    e->cb = cb;
    e->destroy = 0;
    e->userdata = userdata;
    e->dead = false;
    for ( int i = 0; i < NotifierCount; ++i )
        e->notifier[i] = 0;
    self->m_ioEvents.append( e );
    ioEnable( paEvent( e ), events );
    return paEvent( e );
}

void KPulseMainloop::ioEnable( pa_io_event *pe, pa_io_event_flags_t events )
{
    KPulseIoEvent *e = ioEvent( pe );
    if ( e->dead )
        return;
    e->events = events;
    // PA_IO_EVENT_HANGUP has no notifier of its own: like poll(), select()
    // reports a hangup as readability.
    for ( int i = 0; i < NotifierCount; ++i ) {
        const bool want = ( events & notifierFlag[i] ) != 0;
        if ( e->notifier[i] )
            e->notifier[i]->setEnabled( want );
        else if ( want )
            e->notifier[i] = new KPulseIoNotifier( e, i );
    }
}

void KPulseMainloop::ioFree( pa_io_event *pe )
{
    KPulseIoEvent *e = ioEvent( pe );
    if ( e->dead )
        return;
    e->dead = true;
    // unregister right away: libpulse may close the fd and a new io event
    // may get the same fd before the cleanup runs
    for ( int i = 0; i < NotifierCount; ++i )
        if ( e->notifier[i] )
            e->notifier[i]->setEnabled( false );
    e->loop->m_dead++;
    e->loop->scheduleCleanup();
}

void KPulseMainloop::ioSetDestroy( pa_io_event *pe, pa_io_event_destroy_cb_t cb )
{
    ioEvent( pe )->destroy = cb;
}

void KPulseMainloop::dispatchIo( KPulseIoEvent *e, pa_io_event_flags_t flag )
{
    if ( e->dead || !( e->events & flag ) )
        return;
    e->cb( &m_api, paEvent( e ), e->fd, flag, e->userdata );
}


// ---- time events -----------------------------------------------------

// Convert an absolute libpulse time to the monotonic clock of
// pa_rtclock_now(), so that wall clock changes do not move the deadline
// once it is armed.
static pa_usec_t monotonicDue( const struct timeval *tv )
{
    struct timeval t = *tv;
    if ( t.tv_usec & KPULSE_TIMEVAL_RTCLOCK ) {
        t.tv_usec &= ~KPULSE_TIMEVAL_RTCLOCK;
        return pa_timeval_load( &t );
    }
    struct timeval wallNow;
    gettimeofday( &wallNow, 0 );
    const pa_usec_t now = pa_rtclock_now();
    const pa_usec_t due = pa_timeval_load( &t );
    const pa_usec_t wall = pa_timeval_load( &wallNow );
    return due > wall ? now + ( due - wall ) : now;
}

pa_time_event *KPulseMainloop::timeNew( pa_mainloop_api *a, const struct timeval *tv,
                                        pa_time_event_cb_t cb, void *userdata )
{
    KPulseMainloop *self = loopOf( a );
    KPulseTimeEvent *e = new KPulseTimeEvent;
    e->loop = self;
    e->tv.tv_sec = 0;
    e->tv.tv_usec = 0;
    e->due = 0;
    e->timerId = 0;
    e->cb = cb;
    e->destroy = 0;
    e->userdata = userdata;
    e->dead = false;
    self->m_timeEvents.append( e );
    self->armTimer( e, tv );
    return paEvent( e );
}

void KPulseMainloop::timeRestart( pa_time_event *pe, const struct timeval *tv )
{
    KPulseTimeEvent *e = timeEvent( pe );
    if ( e->dead )
        return;
    e->loop->armTimer( e, tv );
}

void KPulseMainloop::timeFree( pa_time_event *pe )
{
    KPulseTimeEvent *e = timeEvent( pe );
    if ( e->dead )
        return;
    e->dead = true;
    e->loop->disarmTimer( e );
    e->loop->m_dead++;
    e->loop->scheduleCleanup();
}

void KPulseMainloop::timeSetDestroy( pa_time_event *pe, pa_time_event_destroy_cb_t cb )
{
    timeEvent( pe )->destroy = cb;
}

// (Re)arm e for tv, or disable it when tv is 0.
void KPulseMainloop::armTimer( KPulseTimeEvent *e, const struct timeval *tv )
{
    disarmTimer( e );
    if ( !tv )
        return;
    e->tv = *tv;
    e->due = monotonicDue( tv );
    startDueTimer( e );
}

// Start the Qt timer of e for its monotonic deadline e->due.
void KPulseMainloop::startDueTimer( KPulseTimeEvent *e )
{
    const pa_usec_t now = pa_rtclock_now();
    // round up: Qt timers have millisecond resolution and the event must
    // never be dispatched before it is due
    pa_usec_t ms = e->due > now ? ( e->due - now + PA_USEC_PER_MSEC - 1 ) / PA_USEC_PER_MSEC : 0;
    if ( ms > INT_MAX )
        ms = INT_MAX;
    e->timerId = startTimer( int( ms ) );
    if ( e->timerId )
        m_timers.insert( e->timerId, e );
    else
        qWarning( "KPulseMainloop: could not start a timer" );
}

void KPulseMainloop::disarmTimer( KPulseTimeEvent *e )
{
    if ( !e->timerId )
        return;
    killTimer( e->timerId );
    m_timers.remove( e->timerId );
    e->timerId = 0;
}

void KPulseMainloop::dispatchTimer( KPulseTimeEvent *e )
{
    // Qt timers repeat; this one is single shot
    disarmTimer( e );
    if ( e->dead )
        return;
    if ( pa_rtclock_now() < e->due ) {
        // early (coarse Qt timer, or the wall clock that Qt3 timers run on
        // stepped forward, e.g. on resume from suspend): wait for the rest
        // of the monotonic deadline. Not via armTimer( e->tv ): that
        // converts from the wall clock again, which after a forward step
        // gives a deadline in the past, the timer fires at once and this
        // would spin until e->due.
        startDueTimer( e );
        return;
    }
    // like pa_mainloop: the event is disabled before the callback, which
    // may restart or free it
    struct timeval tv = e->tv;
    e->cb( &m_api, paEvent( e ), &tv, e->userdata );
}


// ---- defer events ----------------------------------------------------

pa_defer_event *KPulseMainloop::deferNew( pa_mainloop_api *a, pa_defer_event_cb_t cb, void *userdata )
{
    KPulseMainloop *self = loopOf( a );
    KPulseDeferEvent *e = new KPulseDeferEvent;
    e->loop = self;
    e->enabled = false;
    e->cb = cb;
    e->destroy = 0;
    e->userdata = userdata;
    e->dead = false;
    self->m_deferEvents.append( e );
    deferEnable( paEvent( e ), 1 );    // new defer events start enabled
    return paEvent( e );
}

void KPulseMainloop::deferEnable( pa_defer_event *pe, int b )
{
    KPulseDeferEvent *e = deferEvent( pe );
    if ( e->dead || e->enabled == ( b != 0 ) )
        return;
    e->enabled = ( b != 0 );
    e->loop->m_enabledDefers += e->enabled ? 1 : -1;
    e->loop->updateDeferTimer();
}

void KPulseMainloop::deferFree( pa_defer_event *pe )
{
    KPulseDeferEvent *e = deferEvent( pe );
    if ( e->dead )
        return;
    deferEnable( pe, 0 );
    e->dead = true;
    e->loop->m_dead++;
    e->loop->scheduleCleanup();
}

void KPulseMainloop::deferSetDestroy( pa_defer_event *pe, pa_defer_event_destroy_cb_t cb )
{
    deferEvent( pe )->destroy = cb;
}

void KPulseMainloop::updateDeferTimer()
{
    if ( m_enabledDefers > 0 && !m_deferTimer ) {
        m_deferTimer = startTimer( 0 );
        if ( !m_deferTimer )
            qWarning( "KPulseMainloop: could not start a timer" );
    } else if ( m_enabledDefers == 0 && m_deferTimer ) {
        killTimer( m_deferTimer );
        m_deferTimer = 0;
    }
}

void KPulseMainloop::dispatchDefers()
{
    // a snapshot: callbacks may add events; freed ones stay allocated
    // (only marked dead) until the cleanup
    const QValueList<KPulseDeferEvent *> events = m_deferEvents;
    for ( QValueList<KPulseDeferEvent *>::ConstIterator it = events.begin(); it != events.end(); ++it ) {
        KPulseDeferEvent *e = *it;
        if ( !e->dead && e->enabled )
            e->cb( &m_api, paEvent( e ), e->userdata );
    }
}


// ---- quit ------------------------------------------------------------

void KPulseMainloop::quit( pa_mainloop_api *a, int retval )
{
    KPulseMainloop *self = loopOf( a );
    self->m_quitValue = retval;
    emit self->quitRequested( retval );
}


// ---- dispatching and deferred deletion -------------------------------

void KPulseMainloop::timerEvent( QTimerEvent *te )
{
    const int id = te->timerId();
    if ( id == m_deferTimer ) {
        dispatchDefers();
    } else if ( id == m_cleanupTimer ) {
        killTimer( m_cleanupTimer );
        m_cleanupTimer = 0;
        cleanup( false );
    } else {
        QMap<int, KPulseTimeEvent *>::Iterator it = m_timers.find( id );
        if ( it != m_timers.end() )
            dispatchTimer( it.data() );
        else
            killTimer( id );    // stale
    }
}

void KPulseMainloop::scheduleCleanup()
{
    if ( !m_cleanupTimer )
        m_cleanupTimer = startTimer( 0 );
}

// Delete the dead events (all events when all is true), calling their
// destroy callbacks. The callbacks may free further events, or (unless
// all is true) create new ones.
void KPulseMainloop::cleanup( bool all )
{
    while ( all ? !( m_ioEvents.isEmpty() && m_timeEvents.isEmpty() && m_deferEvents.isEmpty() )
                : m_dead > 0 ) {
        // unlink first, so that callbacks never see a half deleted event
        QValueList<KPulseIoEvent *> io;
        QValueList<KPulseTimeEvent *> time;
        QValueList<KPulseDeferEvent *> defer;
        for ( QValueList<KPulseIoEvent *>::Iterator it = m_ioEvents.begin(); it != m_ioEvents.end(); ) {
            if ( all || ( *it )->dead ) {
                io.append( *it );
                it = m_ioEvents.remove( it );
            } else
                ++it;
        }
        for ( QValueList<KPulseTimeEvent *>::Iterator it = m_timeEvents.begin(); it != m_timeEvents.end(); ) {
            if ( all || ( *it )->dead ) {
                time.append( *it );
                it = m_timeEvents.remove( it );
            } else
                ++it;
        }
        for ( QValueList<KPulseDeferEvent *>::Iterator it = m_deferEvents.begin(); it != m_deferEvents.end(); ) {
            if ( all || ( *it )->dead ) {
                defer.append( *it );
                it = m_deferEvents.remove( it );
            } else
                ++it;
        }

        // mark everything unlinked dead before the first destroy callback,
        // so that freeing one of them from a callback is a no-op
        for ( QValueList<KPulseIoEvent *>::Iterator it = io.begin(); it != io.end(); ++it ) {
            KPulseIoEvent *e = *it;
            if ( e->dead )
                m_dead--;
            e->dead = true;
            for ( int i = 0; i < NotifierCount; ++i ) {
                delete e->notifier[i];
                e->notifier[i] = 0;
            }
        }
        for ( QValueList<KPulseTimeEvent *>::Iterator it = time.begin(); it != time.end(); ++it ) {
            KPulseTimeEvent *e = *it;
            if ( e->dead )
                m_dead--;
            e->dead = true;
            disarmTimer( e );
        }
        for ( QValueList<KPulseDeferEvent *>::Iterator it = defer.begin(); it != defer.end(); ++it ) {
            KPulseDeferEvent *e = *it;
            if ( e->dead )
                m_dead--;
            else if ( e->enabled ) {
                e->enabled = false;
                m_enabledDefers--;
            }
            e->dead = true;
        }
        updateDeferTimer();

        for ( QValueList<KPulseIoEvent *>::Iterator it = io.begin(); it != io.end(); ++it ) {
            KPulseIoEvent *e = *it;
            if ( e->destroy )
                e->destroy( &m_api, paEvent( e ), e->userdata );
            delete e;
        }
        for ( QValueList<KPulseTimeEvent *>::Iterator it = time.begin(); it != time.end(); ++it ) {
            KPulseTimeEvent *e = *it;
            if ( e->destroy )
                e->destroy( &m_api, paEvent( e ), e->userdata );
            delete e;
        }
        for ( QValueList<KPulseDeferEvent *>::Iterator it = defer.begin(); it != defer.end(); ++it ) {
            KPulseDeferEvent *e = *it;
            if ( e->destroy )
                e->destroy( &m_api, paEvent( e ), e->userdata );
            delete e;
        }
    }
}

#include "kpulsemainloop.moc"
