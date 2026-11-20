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
#include <stdio.h>

#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <pulse/volume.h>
#include <sndfile.h>

#include <qfile.h>
#include <qfileinfo.h>
#include <qguardedptr.h>
#include <qtimer.h>
#include <qvaluelist.h>
#include <kdebug.h>

#include "kpulsemainloop.h"
#include "kpulseplayer.h"


struct KPulseSound
{
    enum State {
        Waiting,    // for the server connection
        Playing,    // a stream exists (connecting, playing or draining)
        Done        // to be reported and deleted by reap()
    };
    enum Result { Played, Failed, Unavailable };

    KPulsePlayerPrivate *d;
    QString file;
    QString eventName;
    QValueList<int> ids;        // every play() request this sound answers

    SNDFILE *sndfile;
    SF_INFO info;

    pa_stream *stream;
    pa_operation *drainOp;
    pa_operation *flushOp;      // of the last rewind
    bool eof;                   // the decoder hit the end, draining

    State state;
    Result result;
};


class KPulsePlayerPrivate
{
public:
    KPulsePlayerPrivate( KPulsePlayer *q, const QString &appName );
    ~KPulsePlayerPrivate();

    bool play( const QString &file, int id, const QString &eventName );
    void setVolume( int volume );
    void reap();

    KPulsePlayer *q;
    QString appName;
    int volume;

    KPulseMainloop *mainloop;
    pa_context *context;
    bool contextReady;
    bool contextFailed;         // tear the context down in reap()

    QValueList<KPulseSound *> sounds;
    QTimer *reapTimer;
    QTimer *connectTimer;       // gives up on a connection not ready in time

    void connectTimeout();

private:
    void connectContext();
    void startStream( KPulseSound *s );
    void feed( KPulseSound *s, size_t nbytes );
    void rewind( KPulseSound *s );
    void finish( KPulseSound *s, KPulseSound::Result result );
    void releaseStream( KPulseSound *s );
    static void cancel( pa_operation **op );
    void schedule();
    pa_volume_t paVolume() const;

    static void contextStateCallback( pa_context *c, void *userdata );
    static void streamStateCallback( pa_stream *p, void *userdata );
    static void streamWriteCallback( pa_stream *p, size_t nbytes, void *userdata );
    static void streamDrainCallback( pa_stream *p, int success, void *userdata );
    static void streamFlushCallback( pa_stream *p, int success, void *userdata );
};


// The channel map of the file when libsndfile knows it, else the default
// for that many channels.
static void channelMap( SNDFILE *sndfile, int channels, pa_channel_map *map )
{
    static const struct { int sf; pa_channel_position_t pa; } positions[] = {
        { SF_CHANNEL_MAP_MONO, PA_CHANNEL_POSITION_MONO },
        { SF_CHANNEL_MAP_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT },
        { SF_CHANNEL_MAP_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT },
        { SF_CHANNEL_MAP_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER },
        { SF_CHANNEL_MAP_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT },
        { SF_CHANNEL_MAP_FRONT_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT },
        { SF_CHANNEL_MAP_FRONT_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER },
        { SF_CHANNEL_MAP_REAR_CENTER, PA_CHANNEL_POSITION_REAR_CENTER },
        { SF_CHANNEL_MAP_REAR_LEFT, PA_CHANNEL_POSITION_REAR_LEFT },
        { SF_CHANNEL_MAP_REAR_RIGHT, PA_CHANNEL_POSITION_REAR_RIGHT },
        { SF_CHANNEL_MAP_LFE, PA_CHANNEL_POSITION_LFE },
        { SF_CHANNEL_MAP_FRONT_LEFT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER },
        { SF_CHANNEL_MAP_FRONT_RIGHT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER },
        { SF_CHANNEL_MAP_SIDE_LEFT, PA_CHANNEL_POSITION_SIDE_LEFT },
        { SF_CHANNEL_MAP_SIDE_RIGHT, PA_CHANNEL_POSITION_SIDE_RIGHT },
        { SF_CHANNEL_MAP_TOP_CENTER, PA_CHANNEL_POSITION_TOP_CENTER },
        { SF_CHANNEL_MAP_TOP_FRONT_LEFT, PA_CHANNEL_POSITION_TOP_FRONT_LEFT },
        { SF_CHANNEL_MAP_TOP_FRONT_RIGHT, PA_CHANNEL_POSITION_TOP_FRONT_RIGHT },
        { SF_CHANNEL_MAP_TOP_FRONT_CENTER, PA_CHANNEL_POSITION_TOP_FRONT_CENTER },
        { SF_CHANNEL_MAP_TOP_REAR_LEFT, PA_CHANNEL_POSITION_TOP_REAR_LEFT },
        { SF_CHANNEL_MAP_TOP_REAR_RIGHT, PA_CHANNEL_POSITION_TOP_REAR_RIGHT },
        { SF_CHANNEL_MAP_TOP_REAR_CENTER, PA_CHANNEL_POSITION_TOP_REAR_CENTER }
    };

    int sfMap[PA_CHANNELS_MAX];
    if ( sf_command( sndfile, SFC_GET_CHANNEL_MAP_INFO, sfMap, int( channels * sizeof( int ) ) ) == SF_TRUE ) {
        pa_channel_map_init( map );
        map->channels = channels;
        int i;
        for ( i = 0; i < channels; ++i ) {
            uint j;
            for ( j = 0; j < sizeof( positions ) / sizeof( positions[0] ); ++j )
                if ( positions[j].sf == sfMap[i] )
                    break;
            if ( j == sizeof( positions ) / sizeof( positions[0] ) )
                break;
            map->map[i] = positions[j].pa;
        }
        if ( i == channels && pa_channel_map_valid( map ) )
            return;
    }
    pa_channel_map_init_extend( map, channels, PA_CHANNEL_MAP_DEFAULT );
}


KPulsePlayerPrivate::KPulsePlayerPrivate( KPulsePlayer *q, const QString &appName )
    : q( q ), appName( appName ), volume( 100 ), context( 0 ),
      contextReady( false ), contextFailed( false )
{
    mainloop = new KPulseMainloop;
    reapTimer = new QTimer( q );
    QObject::connect( reapTimer, SIGNAL( timeout() ), q, SLOT( reap() ) );
    connectTimer = new QTimer( q );
    QObject::connect( connectTimer, SIGNAL( timeout() ), q, SLOT( connectTimeout() ) );
}

KPulsePlayerPrivate::~KPulsePlayerPrivate()
{
    // no more signals: just let go of everything
    for ( QValueList<KPulseSound *>::Iterator it = sounds.begin(); it != sounds.end(); ++it ) {
        releaseStream( *it );
        sf_close( ( *it )->sndfile );
        delete *it;
    }
    sounds.clear();
    if ( context ) {
        pa_context_set_state_callback( context, 0, 0 );
        pa_context_disconnect( context );
        pa_context_unref( context );
    }
    // after the context: it frees its events through the main loop api
    delete mainloop;
}

void KPulsePlayerPrivate::schedule()
{
    if ( !reapTimer->isActive() )
        reapTimer->start( 0, true );
}

// The "Volume" setting is a linear amplitude factor, as it was for aRts
// (StereoVolumeControl::scaleFactor) and as pw-play --volume takes it.
pa_volume_t KPulsePlayerPrivate::paVolume() const
{
    return pa_sw_volume_from_linear( volume / 100.0 );
}

bool KPulsePlayerPrivate::play( const QString &file, int id, const QString &eventName )
{
    // the same file still playing: restart it, like the old player did
    for ( QValueList<KPulseSound *>::Iterator it = sounds.begin(); it != sounds.end(); ++it ) {
        KPulseSound *s = *it;
        if ( s->state != KPulseSound::Done && s->file == file ) {
            s->ids.append( id );
            rewind( s );
            return true;
        }
    }

    SF_INFO info;
    info.format = 0;
    SNDFILE *sndfile = sf_open( QFile::encodeName( file ), SFM_READ, &info );
    if ( !sndfile ) {
        kdDebug() << "KPulsePlayer: cannot decode " << file << ": " << sf_strerror( 0 ) << endl;
        return false;
    }
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_FLOAT32NE;
    ss.rate = info.samplerate > 0 ? uint32_t( info.samplerate ) : 0;
    ss.channels = info.channels > 0 && info.channels <= int( PA_CHANNELS_MAX ) ? uint8_t( info.channels ) : 0;
    if ( !pa_sample_spec_valid( &ss ) ) {
        kdDebug() << "KPulsePlayer: unsupported " << info.channels << " channels, "
                  << info.samplerate << " Hz in " << file << endl;
        sf_close( sndfile );
        return false;
    }

    KPulseSound *s = new KPulseSound;
    s->d = this;
    s->file = file;
    s->eventName = eventName;
    s->ids.append( id );
    s->sndfile = sndfile;
    s->info = info;
    s->stream = 0;
    s->drainOp = 0;
    s->flushOp = 0;
    s->eof = false;
    s->state = KPulseSound::Waiting;
    s->result = KPulseSound::Failed;
    sounds.append( s );

    if ( contextReady )
        startStream( s );
    else if ( !context )
        connectContext();
    // else connecting: contextStateCallback() starts it
    return true;
}

void KPulsePlayerPrivate::connectContext()
{
    pa_proplist *props = pa_proplist_new();
    pa_proplist_sets( props, PA_PROP_APPLICATION_NAME, appName.utf8() );
    pa_proplist_sets( props, PA_PROP_APPLICATION_ID, "org.kde.knotify" );
    pa_proplist_sets( props, PA_PROP_APPLICATION_ICON_NAME, "knotify" );
    context = pa_context_new_with_proplist( mainloop->api(), appName.utf8(), props );
    pa_proplist_free( props );
    contextReady = false;
    contextFailed = false;
    if ( !context ) {
        contextFailed = true;
        schedule();
        return;
    }
    pa_context_set_state_callback( context, contextStateCallback, this );
    // no PA_CONTEXT_NOFAIL: an unreachable server must fail, so that the
    // caller can use another player; and no autospawn: knotify must not
    // start a (PulseAudio) daemon of its own, the external players are
    // the fallback when there is no server
    if ( pa_context_connect( context, 0, PA_CONTEXT_NOAUTOSPAWN, 0 ) < 0 ) {
        if ( !contextFailed )    // else contextStateCallback() said it
            kdDebug() << "KPulsePlayer: cannot connect: " << pa_strerror( pa_context_errno( context ) ) << endl;
        contextFailed = true;
        schedule();
        return;
    }
    // libpulse gives up on a server that accepts the connection but does
    // not answer only after its own timeout (30 s); notification sounds
    // are useless that late, and the external player fallback would be
    // just as late. A local server is ready within milliseconds, so 3 s
    // is a generous limit even on a loaded system.
    connectTimer->start( 3000, true );
}

void KPulsePlayerPrivate::connectTimeout()
{
    if ( !context || contextReady || contextFailed )
        return;
    kdDebug() << "KPulsePlayer: the server did not answer within 3 s" << endl;
    // reap() reports the waiting sounds as unavailable and drops the
    // context; the next play() connects again
    contextFailed = true;
    schedule();
}

void KPulsePlayerPrivate::contextStateCallback( pa_context *c, void *userdata )
{
    KPulsePlayerPrivate *d = static_cast<KPulsePlayerPrivate *>( userdata );
    switch ( pa_context_get_state( c ) ) {
    case PA_CONTEXT_READY: {
        d->connectTimer->stop();
        d->contextReady = true;
        const QValueList<KPulseSound *> sounds = d->sounds;
        for ( QValueList<KPulseSound *>::ConstIterator it = sounds.begin(); it != sounds.end(); ++it )
            if ( ( *it )->state == KPulseSound::Waiting )
                d->startStream( *it );
        break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        kdDebug() << "KPulsePlayer: no server connection: " << pa_strerror( pa_context_errno( c ) ) << endl;
        // libpulse fails the streams itself; the context is dropped in reap()
        d->contextReady = false;
        d->contextFailed = true;
        d->schedule();
        break;
    default:
        break;
    }
}

void KPulsePlayerPrivate::startStream( KPulseSound *s )
{
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_FLOAT32NE;
    ss.rate = s->info.samplerate;
    ss.channels = s->info.channels;
    pa_channel_map map;
    channelMap( s->sndfile, ss.channels, &map );

    const QString name = s->eventName.isEmpty() ? QFileInfo( s->file ).fileName() : s->eventName;
    pa_proplist *props = pa_proplist_new();
    pa_proplist_sets( props, PA_PROP_MEDIA_ROLE, "event" );
    pa_proplist_sets( props, PA_PROP_MEDIA_NAME, name.utf8() );
    pa_proplist_sets( props, PA_PROP_MEDIA_FILENAME, s->file.utf8() );
    if ( !s->eventName.isEmpty() )
        pa_proplist_sets( props, PA_PROP_EVENT_ID, s->eventName.utf8() );
    s->stream = pa_stream_new_with_proplist( context, name.utf8(), &ss, &map, props );
    pa_proplist_free( props );
    s->state = KPulseSound::Playing;
    if ( !s->stream ) {
        kdDebug() << "KPulsePlayer: cannot create a stream: " << pa_strerror( pa_context_errno( context ) ) << endl;
        finish( s, KPulseSound::Failed );
        return;
    }
    pa_stream_set_state_callback( s->stream, streamStateCallback, s );
    pa_stream_set_write_callback( s->stream, streamWriteCallback, s );

    // full volume: no volume at all, so that the server's own (restored
    // event role) volume applies, just like the external players get it
    pa_cvolume cv;
    pa_cvolume_set( &cv, ss.channels, paVolume() );
    if ( pa_stream_connect_playback( s->stream, 0, 0, PA_STREAM_NOFLAGS,
                                     volume < 100 ? &cv : 0, 0 ) < 0 ) {
        kdDebug() << "KPulsePlayer: cannot connect a stream: " << pa_strerror( pa_context_errno( context ) ) << endl;
        finish( s, KPulseSound::Failed );
    }
}

void KPulsePlayerPrivate::streamStateCallback( pa_stream *p, void *userdata )
{
    KPulseSound *s = static_cast<KPulseSound *>( userdata );
    switch ( pa_stream_get_state( p ) ) {
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
        kdDebug() << "KPulsePlayer: stream for " << s->file << " failed: "
                  << pa_strerror( pa_context_errno( pa_stream_get_context( p ) ) ) << endl;
        s->d->finish( s, KPulseSound::Failed );
        break;
    default:
        break;
    }
}

void KPulsePlayerPrivate::streamWriteCallback( pa_stream *, size_t nbytes, void *userdata )
{
    KPulseSound *s = static_cast<KPulseSound *>( userdata );
    s->d->feed( s, nbytes );
}

// Decode up to nbytes straight into the stream's buffers; at the end of
// the file, drain the stream.
void KPulsePlayerPrivate::feed( KPulseSound *s, size_t nbytes )
{
    if ( s->state != KPulseSound::Playing || s->eof )
        return;
    const size_t frameSize = s->info.channels * sizeof( float );
    while ( nbytes >= frameSize ) {
        void *data;
        size_t size = nbytes;
        if ( pa_stream_begin_write( s->stream, &data, &size ) < 0 || !data ) {
            kdDebug() << "KPulsePlayer: write: " << pa_strerror( pa_context_errno( context ) ) << endl;
            finish( s, KPulseSound::Failed );
            return;
        }
        if ( size > nbytes )
            size = nbytes;
        const sf_count_t frames = size / frameSize;
        if ( frames == 0 ) {
            pa_stream_cancel_write( s->stream );
            break;
        }
        const sf_count_t got = sf_readf_float( s->sndfile, static_cast<float *>( data ), frames );
        if ( got > 0 ) {
            if ( pa_stream_write( s->stream, data, got * frameSize, 0, 0, PA_SEEK_RELATIVE ) < 0 ) {
                kdDebug() << "KPulsePlayer: write: " << pa_strerror( pa_context_errno( context ) ) << endl;
                finish( s, KPulseSound::Failed );
                return;
            }
            nbytes -= got * frameSize;
        } else
            pa_stream_cancel_write( s->stream );
        if ( got < frames ) {
            if ( sf_error( s->sndfile ) != SF_ERR_NO_ERROR ) {
                kdDebug() << "KPulsePlayer: decoding " << s->file << ": " << sf_strerror( s->sndfile ) << endl;
                finish( s, KPulseSound::Failed );
                return;
            }
            s->eof = true;
            s->drainOp = pa_stream_drain( s->stream, streamDrainCallback, s );
            if ( !s->drainOp )
                finish( s, KPulseSound::Failed );
            return;
        }
    }
}

void KPulsePlayerPrivate::streamDrainCallback( pa_stream *, int success, void *userdata )
{
    KPulseSound *s = static_cast<KPulseSound *>( userdata );
    s->d->finish( s, success ? KPulseSound::Played : KPulseSound::Failed );
}

void KPulsePlayerPrivate::rewind( KPulseSound *s )
{
    if ( sf_seek( s->sndfile, 0, SEEK_SET ) < 0 ) {
        kdDebug() << "KPulsePlayer: cannot rewind " << s->file << endl;
        return;    // let it end, all its ids are reported then
    }
    // a cancelled operation's callback is not called
    cancel( &s->drainOp );
    s->eof = false;
    // a stream not ready yet has not asked for data
    if ( s->stream && pa_stream_get_state( s->stream ) == PA_STREAM_READY ) {
        // drop what the server still has buffered; it then asks for new
        // data, and in case that request was already in flight the flush
        // callback feeds what is writable
        cancel( &s->flushOp );
        s->flushOp = pa_stream_flush( s->stream, streamFlushCallback, s );
    }
}

void KPulsePlayerPrivate::streamFlushCallback( pa_stream *p, int, void *userdata )
{
    // releaseStream() cancels this operation before the sound goes away
    KPulseSound *s = static_cast<KPulseSound *>( userdata );
    const size_t writable = pa_stream_writable_size( p );
    if ( writable != size_t( -1 ) && writable > 0 )
        s->d->feed( s, writable );
}

// Called from libpulse callbacks: only mark the sound, reap() does the rest.
void KPulsePlayerPrivate::finish( KPulseSound *s, KPulseSound::Result result )
{
    if ( s->state == KPulseSound::Done )
        return;
    s->state = KPulseSound::Done;
    s->result = result;
    schedule();
}

// Drop an operation, and with it its callback (and userdata).
void KPulsePlayerPrivate::cancel( pa_operation **op )
{
    if ( !*op )
        return;
    if ( pa_operation_get_state( *op ) == PA_OPERATION_RUNNING )
        pa_operation_cancel( *op );
    pa_operation_unref( *op );
    *op = 0;
}

// Stop and drop the stream: after this libpulse holds no pointer to s.
void KPulsePlayerPrivate::releaseStream( KPulseSound *s )
{
    cancel( &s->drainOp );
    cancel( &s->flushOp );
    if ( s->stream ) {
        pa_stream_set_state_callback( s->stream, 0, 0 );
        pa_stream_set_write_callback( s->stream, 0, 0 );
        if ( pa_stream_get_state( s->stream ) == PA_STREAM_READY
             || pa_stream_get_state( s->stream ) == PA_STREAM_CREATING )
            pa_stream_disconnect( s->stream );
        pa_stream_unref( s->stream );
        s->stream = 0;
    }
}

void KPulsePlayerPrivate::reap()
{
    if ( contextFailed ) {
        // everything still going on with this context is over
        for ( QValueList<KPulseSound *>::Iterator it = sounds.begin(); it != sounds.end(); ++it ) {
            KPulseSound *s = *it;
            if ( s->state == KPulseSound::Waiting ) {
                s->state = KPulseSound::Done;
                s->result = KPulseSound::Unavailable;
            } else if ( s->state == KPulseSound::Playing ) {
                s->state = KPulseSound::Done;
                s->result = KPulseSound::Failed;
            }
            releaseStream( s );
        }
        if ( context ) {
            pa_context_set_state_callback( context, 0, 0 );
            pa_context_disconnect( context );
            pa_context_unref( context );
            context = 0;
        }
        contextReady = false;
        contextFailed = false;
        connectTimer->stop();
    }

    // unlink before emitting: the slots may call play()
    QValueList<KPulseSound *> done;
    for ( QValueList<KPulseSound *>::Iterator it = sounds.begin(); it != sounds.end(); ) {
        if ( ( *it )->state == KPulseSound::Done ) {
            done.append( *it );
            it = sounds.remove( it );
        } else
            ++it;
    }
    for ( QValueList<KPulseSound *>::Iterator it = done.begin(); it != done.end(); ++it ) {
        releaseStream( *it );
        sf_close( ( *it )->sndfile );
    }

    // the player may be deleted by a slot
    QGuardedPtr<KPulsePlayer> guard( q );
    for ( QValueList<KPulseSound *>::Iterator it = done.begin(); it != done.end(); ++it ) {
        KPulseSound *s = *it;
        for ( QValueList<int>::ConstIterator id = s->ids.begin(); id != s->ids.end() && guard; ++id ) {
            if ( s->result == KPulseSound::Unavailable )
                emit q->unavailable( *id, s->file );
            else
                emit q->finished( *id, s->result == KPulseSound::Played );
        }
        delete s;
    }
}

void KPulsePlayerPrivate::setVolume( int v )
{
    v = QMIN( QMAX( v, 0 ), 100 );
    if ( v == volume )
        return;
    volume = v;
    if ( !contextReady )
        return;
    for ( QValueList<KPulseSound *>::Iterator it = sounds.begin(); it != sounds.end(); ++it ) {
        KPulseSound *s = *it;
        if ( !s->stream || pa_stream_get_state( s->stream ) != PA_STREAM_READY )
            continue;    // a stream still connecting keeps the volume it was created with
        pa_cvolume cv;
        pa_cvolume_set( &cv, s->info.channels, paVolume() );
        pa_operation *op = pa_context_set_sink_input_volume( context, pa_stream_get_index( s->stream ),
                                                             &cv, 0, 0 );
        if ( op )
            pa_operation_unref( op );
    }
}


KPulsePlayer::KPulsePlayer( const QString &appName, QObject *parent, const char *name )
    : QObject( parent, name )
{
    d = new KPulsePlayerPrivate( this, appName );
}

KPulsePlayer::~KPulsePlayer()
{
    delete d;
}

bool KPulsePlayer::play( const QString &file, int id, const QString &eventName )
{
    return d->play( file, id, eventName );
}

void KPulsePlayer::setVolume( int volume )
{
    d->setVolume( volume );
}

int KPulsePlayer::volume() const
{
    return d->volume;
}

uint KPulsePlayer::activeSounds() const
{
    uint n = 0;
    for ( QValueList<KPulseSound *>::ConstIterator it = d->sounds.begin(); it != d->sounds.end(); ++it )
        if ( ( *it )->state != KPulseSound::Done )
            ++n;
    return n;
}

void KPulsePlayer::reap()
{
    d->reap();
}

void KPulsePlayer::connectTimeout()
{
    d->connectTimeout();
}

#include "kpulseplayer.moc"
