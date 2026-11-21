/*
    This file is part of KNewStuff.
    Copyright (c) 2003 Josef Spillner <spillner@kde.org>

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
#ifndef KNEWSTUFF_DOWNLOADDIALOG_H
#define KNEWSTUFF_DOWNLOADDIALOG_H

#include <qvaluelist.h>

#include <kdialogbase.h>
#include <kio/global.h>
#include <knewstuff/provider.h>

namespace KIO
{
  class Job;
}

class KListView;
class QTextBrowser;
class QFrame;
class QVBoxLayout;
class QImage;
class KNewStuffGeneric;

namespace KNS
{

class ProviderLoader;
class Entry;
struct OcsContent;
class Provider;
class Engine;

/**
 * @short Common download dialog for data browsing and installation.
 *
 * It provides an easy-to-use convenience method named open() which does all
 * the work, unless a more complex operation is needed.
 * \code
 * KNewStuff::DownloadDialog::open("kdesktop/wallpapers");
 * \endcode
 *
 * @author Josef Spillner (spillner@kde.org)
 * \par Maintainer:
 * Josef Spillner (spillner@kde.org)
 */
class KDE_EXPORT DownloadDialog : public KDialogBase
{
    Q_OBJECT
    struct Private;
  public:
    /**
      Constructor.

      @param engine a pre-built engine object, or NULL if the download
                    dialog should create an engine on its own
      @param parent the parent window
      @param caption the dialog caption
    */
    DownloadDialog(Engine *engine, QWidget *parent, const QString& caption);

    /**
      Alternative constructor.
      Always uses an internal engine.

      @param parent the parent window
      @param caption the dialog caption
    */
    DownloadDialog(QWidget *parent, const QString& caption);

    /**
      Destructor.
    */
    ~DownloadDialog();

    /**
      Restricts the display of available data to a certain data type.

      @param type a Hotstuff data type such as "korganizer/calendar"
    */
    void setType(QString type);
	// ### KDE 4.0: use const QString&

    /**
      Fetches descriptions of all available data, optionally considering
      a previously set type.
    */
    void load();

    /**
      Explicitly uses this provider list instead of the one read from
      the application configuration.

      @param providerList the URL of the provider list

      @since 3.4
    */
    void setProviderList(const QString& providerList);

    /**
      Fetches descriptions of all available data, optionally considering
      a previously set type.

      @param providerList the URl to the list of providers; if empty
             we first try the ProvidersUrl from KGlobal::config, then we
             fall back to a hardcoded value.
    */
    void load(QString providerList); // KDE4: merge with load() above

    /**
      Adds another provider to the download dialog.
      This is normally done internally.

      @param p the Hotstuff provider to be added
    */
    void addProvider(Provider *p);

    /**
      Adds an additional entry to the current provider.
      This is normally done internally.

      @param entry a Hotstuff data entry to be added
    */
    void addEntry(Entry *entry);

    /**
      Adds an additional entry to the current provider.
      This is normally done internal.
      This version takes into accounts the download variant.

      @param entry a Hotstuff data entry to be added
      @param variants all variants this entry is intended for
    */
    void addEntry(Entry *entry, const QStringList& variants);

    /**
      Clears the entry list of the current provider.
      This is normally done internally.
    */
    void clear();

    /**
      Constructor.

      @param engine a pre-built engine object, or NULL if the download
                    dialog should create an engine on its own
      @param parent the parent window
    */
    DownloadDialog(Engine *engine, QWidget *parent = 0);
    // ### KDE 4.0: remove and make caption/parent argument optional

    /**
      Alternative constructor.
      Always uses an internal engine.

      @param parent the parent window
    */
    DownloadDialog(QWidget *parent = 0);
    // ### KDE 4.0: remove and make caption/parent argument optional

    /**
      Opens the download dialog.
      This is a convenience method which automatically sets up the dialog.
      @see setType()
      @see load()

      @param type a data type such as "korganizer/calendar"
      @param caption the dialog caption
    */
    static void open(const QString& type, const QString& caption);

    /**
      Opens the download dialog.
      This is a convenience method which automatically sets up the dialog.
      @see setType()
      @see load()

      @param type a data type such as "korganizer/calendar"
      @deprecated use open( const QString& type, const QString& caption );
    */
    static void open(QString type) KDE_DEPRECATED; // ### KDE 4.0: remove and make caption/parent argument optional

    /**
      Returns the list of installed data entries.

      @return list of data entries which have been installed
    */
    QPtrList<Entry> installedEntries();
    // ### KDE 4.0: the open() method should return this

    /**
      Shows an "Install and Set" button next to "Install" on the pages of
      the KDE Store and of the other wallpaper sources: it installs the
      picture (or takes it, when it is installed already) and closes the
      dialog; installedAndSetFile() then names the picture, which the
      application uses at once (the background module as the wallpaper).
      Not offered with an engine.

      @param enable whether the button is shown

      @since 3.5.11
    */
    void setInstallAndSetEnabled(bool enable);

    /**
      The picture chosen with "Install and Set", or QString::null.

      @since 3.5.11
    */
    QString installedAndSetFile() const;

  public slots:
    /**
      Availability of the provider list.

      @param list list of Hotstuff providers
    */
    void slotProviders(Provider::List *list);

  protected slots:
    void slotApply();
    void slotOk();

  private slots:
    void slotResult(KIO::Job *job);
    void slotData(KIO::Job *job, const QByteArray &a);
    void slotInstall();
    void slotDetails();
    void slotInstalled(KIO::Job *job);
    void slotTab();
    void slotSelected();
    void slotPage(QWidget *w);
    void slotFinish();
    void slotEmail(const QString& link);
    // Open Collaboration Services providers
    void slotOcsListed(int sortMode, int page, const QValueList<KNS::OcsContent> &items, int total, bool more);
    void slotOcsListingFailed(int sortMode, const QString &message);
    void slotOcsMore();
    void slotOcsPreview(int key, const QImage &image);
    void slotOcsResolved(const QString &id, int number, const KURL &link);
    void slotOcsResolveFailed(const QString &id, int number, const QString &message);
    void slotOcsDownloaded(KIO::Job *job);
    void slotOcsDownloadData(KIO::Job *job, const QByteArray &data);
    void slotOcsRedirection(KIO::Job *job, const KURL &url);
    // other wallpaper sources, "Install and Set"
    void slotOcsInstallAndSet();
    void slotOcsSearch();
    void slotOcsApiKey();
    void slotOcsCancel();
    void slotOcsWatchdog();

  private:
    void init(Engine *e);
    Entry *getEntry();
    void loadProvider(Provider *p);
    void install(Entry *e);
    int installStatus(Entry *e);
    QListViewItem *currentEntryItem();
    void ocsAddPage(QWidget *frame, Provider *p, QTextBrowser *rt,
                    QWidget **tabs, QVBoxLayout **layouts, KListView **lists);
    void ocsFetch(QWidget *frame, int tab);
    void ocsShowDetails(Entry *e);
    void ocsInstall(Entry *e);
    void ocsFinish();
    void ocsUpdateItems(Entry *e);
    QPixmap ocsItemPixmap(Entry *e, const QColor &background);
    QString ocsStatusKey(Entry *e);
    void ocsSetPicture(const QStringList &files);
    QWidget *activeFrame();
    void ocsSyncPage();
    void ocsUpdateButtons();
    void ocsProgress(const QString &text);

    ProviderLoader *m_loader;
    QString m_entryname;
    KListView *lv_r, *lv_d, *lv_l;
    QTextBrowser *m_rt;
    QFrame *m_frame;
    QListViewItem *m_entryitem;
    QPtrList<Entry> m_entries;
    Entry *m_entry;
    KNewStuffGeneric *m_s;
    int m_curtab;
    QMap<QWidget*, QValueList<KListView*>* > m_map;
    QMap<QWidget*, Provider*> m_providers;
    QMap<QWidget*, QTextBrowser*> m_rts;
    QMap<QWidget*, QValueList<QPushButton*>* > m_buttons;
    QMap<KIO::Job*, Provider*> m_jobs;
    QMap<KIO::Job*, QString> m_data;
    QString m_filter;
    Engine *m_engine;
    Private *d;
};

}

#endif

