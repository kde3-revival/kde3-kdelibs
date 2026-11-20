/*
   Copyright (c) 1997 Christian Esken (esken@kde.org)
                 2000 Charles Samuels (charles@kde.org)
                 2000 Stefan Schimanski (1Stein@gmx.de)
                 2000 Matthias Ettrich (ettrich@kde.org)
                 2000 Waldo Bastian <bastian@kde.org>
                 2000-2003 Carsten Pfeiffer <pfeiffer@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

// C headers
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <config.h>

// QT headers
#include <qfile.h>
#include <qfileinfo.h>
#include <qstringlist.h>
#include <qtextstream.h>

// KDE headers
#include <dcopclient.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpassivepopup.h>
#include <kiconloader.h>
#include <kmacroexpander.h>
#include <kprocess.h>
#include <kstandarddirs.h>
#include <kuniqueapplication.h>
#include <kwin.h>

#ifdef HAVE_PULSEAUDIO
#include "kpulseplayer.h"
#endif

#include "knotify.h"
#include "knotify.moc"

class KNotifyPrivate
{
public:
    KConfig* globalEvents;
    KConfig* globalConfig;
    QMap<QString, KConfig*> events;
    QMap<QString, KConfig*> configs;
    QString externalPlayer;
    bool externalPlayerConfigured; // "External player" set, not auto-detected
    KProcess *externalPlayerProc;
#ifdef HAVE_PULSEAUDIO
    KPulsePlayer *pulsePlayer;     // created on the first sound
#endif

    int externalPlayerEventId;

    bool useExternal;
    int volume;
    bool inStartup;
    QString startupEvents;
};

extern "C"{

KDE_EXPORT int kdemain(int argc, char **argv)
{
    KAboutData aboutdata("knotify", I18N_NOOP("KNotify"),
                         "3.0", I18N_NOOP("KDE Notification Server"),
                         KAboutData::License_GPL, "(C) 1997-2003, KDE Developers");
    aboutdata.addAuthor("Carsten Pfeiffer",I18N_NOOP("Current Maintainer"),"pfeiffer@kde.org");
    aboutdata.addAuthor("Christian Esken",0,"esken@kde.org");
    aboutdata.addAuthor("Stefan Westerfeld",I18N_NOOP("Sound support"),"stefan@space.twc.de");
    aboutdata.addAuthor("Charles Samuels",I18N_NOOP("Previous Maintainer"),"charles@kde.org");

    KCmdLineArgs::init( argc, argv, &aboutdata );
    KUniqueApplication::addCmdLineOptions();


    // initialize application
    if ( !KUniqueApplication::start() ) {
        kdDebug() << "Running knotify found" << endl;
        return 0;
    }

    KUniqueApplication app;
    app.disableSessionManagement();

    // start notify service
    KNotify *notify = new KNotify;

    app.dcopClient()->setDefaultObject( "Notify" );
    app.dcopClient()->setDaemonMode( true );
    // kdDebug() << "knotify starting" << endl;

    int ret = app.exec();
    delete notify;
    return ret;
}
}// end extern "C"

KNotify::KNotify()
    : QObject(), DCOPObject("Notify")
{
    d = new KNotifyPrivate;
    d->globalEvents = new KConfig("knotify/eventsrc", true, false, "data");
    d->globalConfig = new KConfig("knotify.eventsrc", true, false);
    d->externalPlayerProc = 0;
#ifdef HAVE_PULSEAUDIO
    d->pulsePlayer = 0;
#endif
    d->inStartup = true;

    d->volume = 100;

    loadConfig();
}

KNotify::~KNotify()
{
    reconfigure();

#ifdef HAVE_PULSEAUDIO
    delete d->pulsePlayer;
#endif
    delete d;
}


void KNotify::loadConfig() {
    // load external player settings
    KConfig *kc = KGlobal::config();
    kc->setGroup("Misc");
    // Sounds are played unless the user explicitly switched them off
    // ("No audio output" in the kcm). See notifyBySound() for which
    // player is used.
    d->useExternal = kc->readBoolEntry( "Use external player", true );
    d->externalPlayer = kc->readPathEntry("External player");
    d->externalPlayerConfigured = !d->externalPlayer.isEmpty();

    // try to locate a suitable player if none is configured
    if ( d->externalPlayer.isEmpty() ) {
        QStringList players;
        players << "pw-play" << "paplay" << "aplay" << "wavplay" << "auplay";
        QStringList::Iterator it = players.begin();
        while ( d->externalPlayer.isEmpty() && it != players.end() ) {
            d->externalPlayer = KStandardDirs::findExe( *it );
            ++it;
        }
    }

    // load default volume
    d->volume = kc->readNumEntry( "Volume", 100 );
}


void KNotify::reconfigure()
{
    kapp->config()->reparseConfiguration();
    loadConfig();

    // clear loaded config files
    d->globalConfig->reparseConfiguration();
    for ( QMapIterator<QString,KConfig*> it = d->configs.begin(); it != d->configs.end(); ++it )
        delete it.data();
    d->configs.clear();
}


void KNotify::notify(const QString &event, const QString &fromApp,
                     const QString &text, QString sound, QString file,
                     int present, int level)
{
    notify( event, fromApp, text, sound, file, present, level, 0, 1 );
}

void KNotify::notify(const QString &event, const QString &fromApp,
                     const QString &text, QString sound, QString file,
                     int present, int level, int winId)
{
    notify( event, fromApp, text, sound, file, present, level, winId, 1 );
}

void KNotify::notify(const QString &event, const QString &fromApp,
                     const QString &text, QString sound, QString file,
                     int present, int level, int winId, int eventId )
{
    // kdDebug() << "event=" << event << " fromApp=" << fromApp << " text=" << text << " sound=" << sound <<
    //    " file=" << file << " present=" << present << " level=" << level <<  " winId=" << winId << " eventId=" << eventId << endl;
    if( d->inStartup ) {
        d->startupEvents += "(" + event + ":" + fromApp + ")";
    }

    QString commandline;
    KConfig *eventsFile = NULL;
    KConfig *configFile = NULL;

    // check for valid events
    if ( !event.isEmpty() ) {

        // get config file
        if ( d->events.contains( fromApp ) ) {
            eventsFile = d->events[fromApp];
        } else {
            eventsFile=new KConfig(locate("data", fromApp+"/eventsrc"),true,false);
            d->events.insert( fromApp, eventsFile );
        }
        if ( d->configs.contains( fromApp) ) {
            configFile = d->configs[fromApp];
        } else {
            configFile=new KConfig(fromApp+".eventsrc",true,false);
            d->configs.insert( fromApp, configFile );
        }

        if ( !eventsFile->hasGroup( event ) && isGlobal(event) )
        {
            eventsFile = d->globalEvents;
            configFile = d->globalConfig;
        }

        eventsFile->setGroup( event );
        configFile->setGroup( event );

        // get event presentation
        if ( present==-1 )
            present = configFile->readNumEntry( "presentation", -1 );
        if ( present==-1 )
            present = eventsFile->readNumEntry( "default_presentation", 0 );

        // get sound file name
        if( present & KNotifyClient::Sound ) {
            QString theSound = configFile->readPathEntry( "soundfile" );
            if ( theSound.isEmpty() )
                theSound = eventsFile->readPathEntry( "default_sound" );
            if ( !theSound.isEmpty() )
                sound = theSound;
        }

        // get log file name
        if( present & KNotifyClient::Logfile ) {
            QString theFile = configFile->readPathEntry( "logfile" );
            if ( theFile.isEmpty() )
                theFile = eventsFile->readPathEntry( "default_logfile" );
            if ( !theFile.isEmpty() )
                file = theFile;
        }

        // get default event level
        if( present & KNotifyClient::Messagebox )
            level = eventsFile->readNumEntry( "level", 0 );

        // get command line
        if (present & KNotifyClient::Execute ) {
            commandline = configFile->readPathEntry( "commandline" );
            if ( commandline.isEmpty() )
                commandline = eventsFile->readPathEntry( "default_commandline" );
        }
    }

    // emit event
    if ( present & KNotifyClient::Sound ) // && QFile(sound).isReadable()
        notifyBySound( sound, fromApp, eventId, event );

    if ( present & KNotifyClient::Execute )
        notifyByExecute( commandline, event, fromApp, text, winId, eventId );

    if ( present & KNotifyClient::Logfile ) // && QFile(file).isWritable()
        notifyByLogfile( text, file );

    if ( present & KNotifyClient::Stderr )
        notifyByStderr( text );

    if ( present & KNotifyClient::Taskbar )
        notifyByTaskbar( checkWinId( fromApp, winId ));

    if ( present & KNotifyClient::PassivePopup )
        notifyByPassivePopup( text, fromApp, eventsFile, checkWinId( fromApp, winId ));
    else if ( present & KNotifyClient::Messagebox )
        notifyByMessagebox( text, level, checkWinId( fromApp, winId ));

    QByteArray qbd;
    QDataStream ds(qbd, IO_WriteOnly);
    ds << event << fromApp << text << sound << file << present << level
        << winId << eventId;
    emitDCOPSignal("notifySignal(QString,QString,QString,QString,QString,int,int,int,int)", qbd);

}


// Which player plays a sound (knotifyrc [Misc]):
// - "Use external player" false ("No audio output"): none, the event is
//   finished with NoSoundSupport;
// - otherwise, with "External player" set: that player, always;
// - otherwise the sound server through KPulsePlayer, when built with it;
//   when it cannot decode the file, or no server can be reached, the
//   first player found in $PATH (see loadConfig()); with none found the
//   event is finished with NoSoundSupport.
bool KNotify::notifyBySound( const QString &sound, const QString &appname,
                             int eventId, const QString &event )
{
    if (sound.isEmpty()) {
        soundFinished( eventId, NoSoundFile );
        return false;
    }

    // get file name
    QString soundFile(sound);
    if ( QFileInfo(sound).isRelative() )
    {
        QString search = QString("%1/sounds/%2").arg(appname).arg(sound);
        soundFile = KGlobal::instance()->dirs()->findResource("data", search);
        if ( soundFile.isEmpty() )
            soundFile = locate( "sound", sound );
    }
    if ( soundFile.isEmpty() )
    {
        soundFinished( eventId, NoSoundFile );
        return false;
    }


    // kdDebug() << "KNotify::notifyBySound - trying to play file " << soundFile << endl;

    if ( !d->useExternal ) {
        soundFinished( eventId, NoSoundSupport );
        return false;
    }

#ifdef HAVE_PULSEAUDIO
    if ( !d->externalPlayerConfigured ) {
        if ( !d->pulsePlayer ) {
            const KAboutData *about = KGlobal::instance()->aboutData();
            d->pulsePlayer = new KPulsePlayer( about ? about->programName()
                                                     : QString::fromLatin1( "KNotify" ) );
            connect( d->pulsePlayer, SIGNAL( finished( int, bool ) ),
                     SLOT( slotPulseFinished( int, bool ) ) );
            connect( d->pulsePlayer, SIGNAL( unavailable( int, const QString & ) ),
                     SLOT( slotPulseUnavailable( int, const QString & ) ) );
        }
        d->pulsePlayer->setVolume( d->volume );
        if ( d->pulsePlayer->play( soundFile, eventId, event ) )
            return true;
        // libsndfile cannot decode it, maybe the external player can
    }
#else
    Q_UNUSED( event );
#endif

    return playExternal( soundFile, eventId );
}

bool KNotify::playExternal( const QString &soundFile, int eventId )
{
    if ( d->externalPlayer.isEmpty() ) {
        // no player configured or found
        soundFinished( eventId, NoSoundSupport );
        return false;

    } else {
        // use an external player to play the sound
        KProcess *proc = d->externalPlayerProc;
        if (!proc)
        {
           proc = d->externalPlayerProc = new KProcess;
           connect( proc, SIGNAL( processExited( KProcess * )),
                    SLOT( slotPlayerProcessExited( KProcess * )));
        }
        if (proc->isRunning())
        {
           soundFinished( eventId, PlayerBusy );
           return false; // Skip
        }
        proc->clearArguments();
        (*proc) << d->externalPlayer;
        // pass the volume to the players known to take it
        const int volume = QMIN( QMAX( d->volume, 0 ), 100 );
        const QString player = QFileInfo( d->externalPlayer ).fileName();
        if ( player == "pw-play" ) {      // linear, 0.0 .. 1.0
            // built by hand: the decimal point must not depend on the locale
            QString frac = QString::number( volume % 100 );
            if ( frac.length() < 2 )
                frac.prepend( '0' );
            (*proc) << QString::fromLatin1( "--volume=%1.%2" ).arg( volume / 100 ).arg( frac );
        } else if ( player == "paplay" ) {  // pa_volume_t, 0 .. 65536 (100%)
            // a PulseAudio volume is cubic: give the value for the same linear
            // amplitude as above, i.e. pa_sw_volume_from_linear( volume / 100.0 )
            int paVolume = (int)( 65536 * cbrt( volume / 100.0 ) + 0.5 );
            paVolume = QMIN( QMAX( paVolume, 0 ), 65536 );
            (*proc) << QString::fromLatin1( "--volume=%1" ).arg( paVolume );
        }
        (*proc) << QFile::encodeName( soundFile );
        if ( !proc->start( KProcess::NotifyOnExit ) ) {
            // it cannot be forked or executed (a configured player that does
            // not exist, for instance), so no processExited() will report the
            // event: report it here, as for a player that fails
            kdDebug() << "KNotify: cannot start the player " << d->externalPlayer << endl;
            soundFinished( eventId, Unknown );
            return false;
        }
        d->externalPlayerEventId = eventId;
        return true;
    }
}

bool KNotify::notifyByMessagebox(const QString &text, int level, WId winId)
{
    // ignore empty messages
    if ( text.isEmpty() )
        return false;

    // display message box for specified event level
    switch( level ) {
    default:
    case KNotifyClient::Notification:
        KMessageBox::informationWId( winId, text, i18n("Notification"), 0, false );
        break;
    case KNotifyClient::Warning:
        KMessageBox::sorryWId( winId, text, i18n("Warning"), false );
        break;
    case KNotifyClient::Error:
        KMessageBox::errorWId( winId, text, i18n("Error"), false );
        break;
    case KNotifyClient::Catastrophe:
        KMessageBox::errorWId( winId, text, i18n("Catastrophe!"), false );
        break;
    }

    return true;
}

bool KNotify::notifyByPassivePopup( const QString &text,
                                    const QString &appName,
                                    KConfig* eventsFile,
                                    WId senderWinId )
{
    KIconLoader iconLoader( appName );
    if ( eventsFile != NULL ) {
        KConfigGroup config( eventsFile, "!Global!" );
        QString iconName = config.readEntry( "IconName", appName );
        QPixmap icon = iconLoader.loadIcon( iconName, KIcon::Small );
        QString title = config.readEntry( "Comment", appName );
        KPassivePopup::message(title, text, icon, senderWinId);
    } else
        kdError() << "No events for app " << appName << "defined!" <<endl;

    return true;
}

bool KNotify::notifyByExecute(const QString &command, const QString& event,
                              const QString& fromApp, const QString& text,
                              int winId, int eventId) {
    if (!command.isEmpty()) {
	// kdDebug() << "executing command '" << command << "'" << endl;
        QMap<QChar,QString> subst;
        subst.insert( 'e', event );
        subst.insert( 'a', fromApp );
        subst.insert( 's', text );
        subst.insert( 'w', QString::number( winId ));
        subst.insert( 'i', QString::number( eventId ));
        QString execLine = KMacroExpander::expandMacrosShellQuote( command, subst );
        if ( execLine.isEmpty() )
            execLine = command; // fallback

	KProcess p;
	p.setUseShell(true);
	p << execLine;
	p.start(KProcess::DontCare);
	return true;
    }
    return false;
}


bool KNotify::notifyByLogfile(const QString &text, const QString &file)
{
    // ignore empty messages
    if ( text.isEmpty() )
        return true;

    // open file in append mode
    QFile logFile(file);
    if ( !logFile.open(IO_WriteOnly | IO_Append) )
        return false;

    // append msg
    QTextStream strm( &logFile );
    strm << "- KNotify " << QDateTime::currentDateTime().toString() << ": ";
    strm << text << endl;

    // close file
    logFile.close();
    return true;
}

bool KNotify::notifyByStderr(const QString &text)
{
    // ignore empty messages
    if ( text.isEmpty() )
        return true;

    // open stderr for output
    QTextStream strm( stderr, IO_WriteOnly );

    // output msg
    strm << "KNotify " << QDateTime::currentDateTime().toString() << ": ";
    strm << text << endl;

    return true;
}

bool KNotify::notifyByTaskbar( WId win )
{
    if( win == 0 )
        return false;
    KWin::demandAttention( win );
    return true;
}

bool KNotify::isGlobal(const QString &eventname)
{
    return d->globalEvents->hasGroup( eventname );
}

void KNotify::setVolume( int volume )
{
    if ( volume<0 ) volume=0;
    if ( volume>=100 ) volume=100;
    d->volume = volume;
#ifdef HAVE_PULSEAUDIO
    // also for the sounds playing now
    if ( d->pulsePlayer )
        d->pulsePlayer->setVolume( volume );
#endif
}

void KNotify::slotPlayerProcessExited( KProcess *proc )
{
    soundFinished( d->externalPlayerEventId,
                   (proc->normalExit() && proc->exitStatus() == 0) ? PlayedOK : Unknown );
}

void KNotify::slotPulseFinished( int eventId, bool ok )
{
    soundFinished( eventId, ok ? PlayedOK : Unknown );
}

void KNotify::slotPulseUnavailable( int eventId, const QString &soundFile )
{
    // no sound server: fall back to the auto-detected player
    playExternal( soundFile, eventId );
}

void KNotify::soundFinished( int eventId, PlayingFinishedStatus reason )
{
    QByteArray data;
    QDataStream stream( data, IO_WriteOnly );
    stream << eventId << (int) reason;

    DCOPClient::mainClient()->emitDCOPSignal( "KNotify", "playingFinished(int,int)", data );
}

WId KNotify::checkWinId( const QString &appName, WId senderWinId )
{
    if ( senderWinId == 0 )
    {
        QCString senderId = kapp->dcopClient()->senderId();
        QCString compare = (appName + "-mainwindow").latin1();
        int len = compare.length();
        // kdDebug() << "notifyByPassivePopup: appName=" << appName << " sender=" << senderId << endl;

        QCStringList objs = kapp->dcopClient()->remoteObjects( senderId );
        for (QCStringList::ConstIterator it = objs.begin(); it != objs.end(); ++it ) {
            QCString obj( *it );
            if ( obj.left(len) == compare) {
                // kdDebug( ) << "found " << obj << endl;
                QCString replyType;
                QByteArray data, replyData;

                if ( kapp->dcopClient()->call(senderId, obj, "getWinID()", data, replyType, replyData) ) {
                    QDataStream answer(replyData, IO_ReadOnly);
                    if (replyType == "int") {
                        answer >> senderWinId;
                        // kdDebug() << "SUCCESS, found getWinID(): type='" << QString(replyType)
                        //      << "' senderWinId=" << senderWinId << endl;
                    }
		}
            }
        }
    }
    return senderWinId;
}

void KNotify::sessionReady()
{
    if( d->inStartup && !d->startupEvents.isEmpty())
        kdDebug() << "There were knotify events while startup:" << d->startupEvents << endl;
    d->inStartup = false;
}

// vim: sw=4 sts=4 ts=8 et
