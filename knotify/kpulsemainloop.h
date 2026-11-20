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
#ifndef _KPULSEMAINLOOP_H_
#define _KPULSEMAINLOOP_H_

#include <qobject.h>
#include <qmap.h>
#include <qvaluelist.h>

#include <pulse/mainloop-api.h>

struct KPulseIoEvent;
struct KPulseTimeEvent;
struct KPulseDeferEvent;

/**
 * A pa_mainloop_api implementation driven by the Qt event loop, so that
 * the asynchronous libpulse API can be used from the GUI thread without
 * a pa_threaded_mainloop and without any locking.
 *
 * - io events are QSocketNotifiers (one per direction: Read for
 *   PA_IO_EVENT_INPUT, Write for PA_IO_EVENT_OUTPUT, Exception for
 *   PA_IO_EVENT_ERROR), created when first enabled;
 * - time events are Qt timers; the absolute times libpulse passes are
 *   wall clock (gettimeofday) times, or CLOCK_MONOTONIC times when libpulse
 *   marks them as such, exactly as pa_mainloop interprets them;
 * - defer events are run by a zero timer, i.e. once per event loop pass
 *   for as long as at least one of them is enabled.
 *
 * Freeing an event (from anywhere, including from its own callback) only
 * marks it dead: it gets no more callbacks, and it is deleted, and its
 * destroy callback called, from a zero timer once control is back in the
 * event loop. The destructor frees every event still alive, calling the
 * destroy callbacks, like pa_mainloop_free() does. So it must be deleted
 * after every libpulse object using it (pa_context_unref() first), and
 * never from inside one of its own callbacks.
 *
 * quit() only records the value and emits quitRequested(); the Qt event
 * loop is not touched.
 */
class KPulseMainloop : public QObject
{
    Q_OBJECT

public:
    KPulseMainloop( QObject *parent = 0, const char *name = 0 );
    ~KPulseMainloop();

    /** The vtable to pass to pa_context_new() and friends. */
    pa_mainloop_api *api() { return &m_api; }

    /** The value of the last quit() call through the api, 0 if none. */
    int quitValue() const { return m_quitValue; }

signals:
    void quitRequested( int retval );

protected:
    void timerEvent( QTimerEvent *e );

private:
    // pa_mainloop_api entry points
    static pa_io_event *ioNew( pa_mainloop_api *a, int fd, pa_io_event_flags_t events,
                               pa_io_event_cb_t cb, void *userdata );
    static void ioEnable( pa_io_event *e, pa_io_event_flags_t events );
    static void ioFree( pa_io_event *e );
    static void ioSetDestroy( pa_io_event *e, pa_io_event_destroy_cb_t cb );

    static pa_time_event *timeNew( pa_mainloop_api *a, const struct timeval *tv,
                                   pa_time_event_cb_t cb, void *userdata );
    static void timeRestart( pa_time_event *e, const struct timeval *tv );
    static void timeFree( pa_time_event *e );
    static void timeSetDestroy( pa_time_event *e, pa_time_event_destroy_cb_t cb );

    static pa_defer_event *deferNew( pa_mainloop_api *a, pa_defer_event_cb_t cb, void *userdata );
    static void deferEnable( pa_defer_event *e, int b );
    static void deferFree( pa_defer_event *e );
    static void deferSetDestroy( pa_defer_event *e, pa_defer_event_destroy_cb_t cb );

    static void quit( pa_mainloop_api *a, int retval );

    friend class KPulseIoNotifier;
    void dispatchIo( KPulseIoEvent *e, pa_io_event_flags_t flag );
    void armTimer( KPulseTimeEvent *e, const struct timeval *tv );
    void disarmTimer( KPulseTimeEvent *e );
    void dispatchTimer( KPulseTimeEvent *e );
    void dispatchDefers();
    void updateDeferTimer();
    void scheduleCleanup();
    void cleanup( bool all );

    pa_mainloop_api m_api;
    int m_quitValue;

    QValueList<KPulseIoEvent *> m_ioEvents;
    QValueList<KPulseTimeEvent *> m_timeEvents;
    QValueList<KPulseDeferEvent *> m_deferEvents;
    QMap<int, KPulseTimeEvent *> m_timers;    // Qt timer id -> time event

    int m_enabledDefers;    // enabled, alive defer events
    int m_deferTimer;       // Qt timer id running the defer events, or 0
    int m_cleanupTimer;     // Qt timer id of the pending cleanup, or 0
    int m_dead;             // events freed but not deleted yet

    KPulseMainloop( const KPulseMainloop & );
    KPulseMainloop &operator=( const KPulseMainloop & );
};

#endif // _KPULSEMAINLOOP_H_
