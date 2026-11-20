/************************************************************************\
 *
 *  (c) 2016 by Serghei Amelian <serghei.amelian@gmail.com>
 *  (c) 2026 rewrite: asynchronous libpulse + libsndfile, no threads
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
#ifndef _KPULSEPLAYER_H_
#define _KPULSEPLAYER_H_

#include <qobject.h>
#include <qstring.h>


class KPulsePlayerPrivate;


/**
 * Plays sound files on a PulseAudio (or pipewire-pulse) server.
 *
 * Files are decoded with libsndfile (WAV, FLAC, Ogg Vorbis/Opus, ...,
 * detected by content) and streamed with the asynchronous libpulse API
 * from the Qt event loop (see KPulseMainloop), without threads.
 *
 * One server connection is opened on the first play() and kept; no server
 * is autospawned. When it fails, is not ready within 3 seconds, or the
 * server goes away, the sounds still waiting for it are reported with
 * unavailable(), the playing ones with finished(id, false), and the next
 * play() connects again.
 *
 * Every signal is emitted from the event loop, never from inside play()
 * or setVolume(), so slots may call play() again.
 */
class KPulsePlayer : public QObject
{
    Q_OBJECT
    friend class KPulsePlayerPrivate;

public:
    /**
     * @param appName the application.name the server shows for the
     * sounds (in mixers, for instance)
     */
    KPulsePlayer( const QString &appName = QString::fromLatin1( "KNotify" ),
                  QObject *parent = 0, const char *name = 0 );
    ~KPulsePlayer();

    /**
     * Plays @p file. @p id is passed back by finished() or unavailable();
     * ids need not be unique. @p eventName is shown by the server as the
     * stream name (media.name, event.id); the file name when empty.
     *
     * When @p file is already playing it is restarted from the beginning
     * instead and, when it ends, finished() is emitted for every id that
     * (re)started it.
     *
     * Returns false, and emits nothing for @p id, when @p file cannot be
     * decoded; another player may still be able to play it.
     */
    bool play( const QString &file, int id, const QString &eventName = QString::null );

    /**
     * The volume, 0..100 (clamped), of new sounds and of the playing ones.
     * It is a linear amplitude factor (100 leaves the volume alone).
     */
    void setVolume( int volume );
    int volume() const;

    /** The number of sounds playing or waiting for the server. */
    uint activeSounds() const;

signals:
    /** The sound started for @p id ended; @p ok is false on any error. */
    void finished( int id, bool ok );

    /**
     * No server could be reached before the sound for @p id was started;
     * @p file has not been played at all.
     */
    void unavailable( int id, const QString &file );

private slots:
    void reap();
    void connectTimeout();

private:
    KPulsePlayerPrivate *d;

    KPulsePlayer( const KPulsePlayer & );
    KPulsePlayer &operator=( const KPulsePlayer & );
};


#endif // _KPULSEPLAYER_H_
