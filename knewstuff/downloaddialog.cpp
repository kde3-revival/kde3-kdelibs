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

#include "downloaddialog.h"
#include "downloaddialog.moc"

#include <klocale.h>
#include <klistview.h>
#include <kdebug.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kurl.h>
#include <kconfig.h>
#include <kapplication.h>
#include <kiconloader.h>

#include <kinputdialog.h>
#include <kstandarddirs.h>
#include <ktempfile.h>

#include <knewstuff/entry.h>
#include <knewstuff/knewstuffgeneric.h>
#include <knewstuff/engine.h>

#include "ocs.h"

#include <qlayout.h>
#include <qpushbutton.h>
#include <qdom.h>
#include <qlabel.h>
#include <qtextbrowser.h>
#include <qtabwidget.h>
#include <qtimer.h> // hack
#include <qfile.h>
#include <qimage.h>
#include <qmime.h>
#include <qpainter.h>
#include <qregexp.h>
#include <qstylesheet.h>

using namespace KNS;

struct DownloadDialog::Private
{
    QString m_providerlist;
    QWidget *m_page;
    KListView *m_lvtmp_r, *m_lvtmp_d, *m_lvtmp_l;
    QPtrList<Entry> m_installlist;
    QMap<KIO::Job*, Provider*> m_variantjobs;
    QMap<KIO::Job*, QStringList> m_variants;
    QMap<Provider*, Provider*> m_newproviders;

    // Open Collaboration Services provider pages
    struct OcsTab
    {
      KListView *list;
      QLabel *status;
      QPushButton *more;
      int nextPage;
      int loaded;
      bool busy;
      bool started;
    };
    struct OcsPage
    {
      Provider *provider;
      QString host;                       // of the provider, for the install status
      OcsClient *client;
      QTextBrowser *browser;
      QMimeSourceFactory *factory;
      OcsTab tabs[3];
      QMap<QString, Entry*> entries;      // by OCS id
      QMap<Entry*, OcsContent> content;
      QMap<Entry*, QImage> previews;
      QMap<int, Entry*> previewKeys;
    };
    QMap<QWidget*, OcsPage*> m_ocs;
    QMap<QListViewItem*, Entry*> m_itemEntries;
    QMap<Entry*, QWidget*> m_entryPage;
    int m_nextPreviewKey;
    bool m_ocsResized;

    // the OCS installation in progress
    Entry *m_ocsEntry;
    QWidget *m_ocsPage;
    OcsDownload m_ocsFile;
    KTempFile *m_ocsTemp;
    KTempFile *m_ocsEngineTemp;
    KIO::TransferJob *m_ocsJob;           // the download in progress
    Q_ULLONG m_ocsWritten;
    bool m_ocsTooLarge;
    bool m_ocsBadRedirect;

    OcsPage *ocsPageFor(QObject *client)
    {
      for(QMap<QWidget*, OcsPage*>::Iterator it = m_ocs.begin(); it != m_ocs.end(); ++it)
        if(it.data()->client == client) return it.data();
      return 0;
    }
};

// Thumbnails in the OCS lists
static const int ocsThumbWidth = 80;
static const int ocsThumbHeight = 50;

class NumSortListViewItem : public KListViewItem
{
  public:
  NumSortListViewItem( QListView * parent, QString label1, QString label2 = QString::null, QString label3 = QString::null, QString label4 = QString::null, QString label5 = QString::null, QString label6 = QString::null, QString label7 = QString::null, QString label8 = QString::null )  :
  KListViewItem( parent, label1, label2, label3, label4, label5, label6, label7, label8 )
  {
  }

  QString key(int col, bool asc) const {
    if (col == 2)
    {
      QString s;
      s.sprintf("%08d", text(col).toInt());
      return s;
    }
    return KListViewItem::key( col, asc );
  }
};

class DateSortListViewItem : public KListViewItem
{
  public:
  DateSortListViewItem( QListView * parent, QString label1, QString label2 = QString::null, QString label3 = QString::null, QString label4 = QString::null, QString label5 = QString::null, QString label6 = QString::null, QString label7 = QString::null, QString label8 = QString::null )  :
  KListViewItem( parent, label1, label2, label3, label4, label5, label6, label7, label8 )
  {
  }

  QString key(int col, bool asc) const {
    if (col == 2)
    {
      QString s;
      QDate date = KGlobal::locale()->readDate(text(col));
      s.sprintf("%08d", date.year() * 366 + date.dayOfYear());
      return s;
    }
    return KListViewItem::key( col, asc );
  }
};

// BEGIN deprecated for KDE 4
DownloadDialog::DownloadDialog(Engine *engine, QWidget *)
: KDialogBase(KDialogBase::IconList, i18n("Get Hot New Stuff"),
  KDialogBase::Close, KDialogBase::Close)
{
  init(engine);
}

DownloadDialog::DownloadDialog(QWidget *)
: KDialogBase(KDialogBase::IconList, i18n("Get Hot New Stuff"),
  KDialogBase::Close, KDialogBase::Close)
{
  init(0);
}

void DownloadDialog::open(QString type)
{
  DownloadDialog d;
  d.setType(type);
  d.load();
  d.exec();
}
// END deprecated for KDE 4

DownloadDialog::DownloadDialog(Engine *engine, QWidget *, const QString& caption)
: KDialogBase(KDialogBase::IconList, (caption.isNull() ? i18n("Get Hot New Stuff") : caption),
  KDialogBase::Close, KDialogBase::Close)
{
  init(engine);
}

DownloadDialog::DownloadDialog(QWidget *, const QString& caption)
: KDialogBase(KDialogBase::IconList, (caption.isNull() ? i18n("Get Hot New Stuff") : caption),
  KDialogBase::Close, KDialogBase::Close)
{
  init(0);
}

void DownloadDialog::init(Engine *engine)
{
  resize(700, 400);
  d = new Private();
  d->m_nextPreviewKey = 0;
  d->m_ocsResized = false;
  d->m_ocsEntry = 0;
  d->m_ocsPage = 0;
  d->m_ocsTemp = 0;
  d->m_ocsEngineTemp = 0;
  d->m_ocsJob = 0;
  d->m_ocsWritten = 0;
  d->m_ocsTooLarge = false;
  d->m_ocsBadRedirect = false;

  m_engine = engine;
  d->m_page = NULL;

  connect(this, SIGNAL(aboutToShowPage(QWidget*)), SLOT(slotPage(QWidget*)));

  if(!engine)
  {
    m_loader = new ProviderLoader(this);
    connect(m_loader, SIGNAL(providersLoaded(Provider::List*)), SLOT(slotProviders(Provider::List*)));
  }

  m_entries.setAutoDelete(true);
}

DownloadDialog::~DownloadDialog()
{
    for (QMap<QWidget *, QValueList<QPushButton *>* >::const_iterator it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it)
        delete it.data();
    for (QMap<QWidget *, QValueList<KListView *>* >::const_iterator it = m_map.constBegin(); it != m_map.constEnd(); ++it)
        delete it.data();
    for (QMap<QWidget *, Private::OcsPage *>::const_iterator it = d->m_ocs.constBegin(); it != d->m_ocs.constEnd(); ++it)
    {
        it.data()->browser->setMimeSourceFactory(QMimeSourceFactory::defaultFactory());
        delete it.data()->factory;
        delete it.data();
    }
    if (d->m_ocsJob)
        d->m_ocsJob->kill(true);
    delete d->m_ocsTemp;
    delete d->m_ocsEngineTemp;
    delete d;
}

void DownloadDialog::load()
{
  m_loader->load(m_filter, d->m_providerlist);
}

void DownloadDialog::load(QString providerList)
{
  m_loader->load(m_filter, providerList);
}

void DownloadDialog::clear()
{
  // an OCS installation in progress belongs to the pages removed here
  if(d->m_ocsEntry)
  {
    if(d->m_ocsJob) d->m_ocsJob->kill(true);
    d->m_ocsJob = 0;
    delete d->m_ocsTemp;
    d->m_ocsTemp = 0;
    d->m_ocsEntry = 0;
    d->m_ocsPage = 0;
  }

  QMap<QWidget*, QValueList<KListView*>* >::Iterator it;
  QMap<QWidget*, QValueList<KListView*>* >::Iterator end(m_map.end());
  for(it = m_map.begin(); it != end; ++it)
  {
    QValueList<KListView*> *v = it.data();
    kdDebug() << "clear listviews in " << v << endl;
    if(v)
    {
      (*(v->at(0)))->clear();
      (*(v->at(1)))->clear();
      (*(v->at(2)))->clear();

      //delete (*it);
    }

    if(d->m_ocs.contains(it.key()))
    {
      Private::OcsPage *page = d->m_ocs[it.key()];
      page->browser->setMimeSourceFactory(QMimeSourceFactory::defaultFactory());
      delete page->factory;
      delete page;
      d->m_ocs.remove(it.key());
    }

    delete it.key();
  }
  m_map.clear();
  d->m_itemEntries.clear();
  d->m_entryPage.clear();
}

void DownloadDialog::slotProviders(Provider::List *list)
{
  Provider *p;
  /*QFrame *frame;*/

  for(p = list->first(); p; p = list->next())
  {
    kdDebug() << "++ provider ++ " << p->name() << endl;

    if(!m_filter.isEmpty())
      loadProvider(p);
    else
      addProvider(p);
    /*if(p == list->getFirst())
      slotPage(m_frame);*/ // only if !qtbug
  }
}

void DownloadDialog::addProvider(Provider *p)
{
  QFrame *frame;
  QTabWidget *ctl;
  QWidget *w_d, *w_r, *w_l;
  QWidget *w2;
  QTextBrowser *rt;
  QString tmp;
  int ret;
  QPixmap pix;

  if(m_map.count() == 0)
  {
    frame = addPage(i18n("Welcome"), i18n("Welcome"), QPixmap(""));
    delete frame;
  }

  kdDebug() << "addProvider()/begin" << endl;

  ret = true;
  if(p->icon().path().isEmpty()) ret = false;
  else
  {
    if(!p->icon().protocol().isEmpty())
    {
      ret = KIO::NetAccess::download(p->icon(), tmp, this);
      if(ret) pix = QPixmap(tmp);
    }
    else
    {
      pix = KGlobal::iconLoader()->loadIcon(p->icon().path(), KIcon::Panel);
      ret = true;
    }
  }
  if(!ret) pix = KGlobal::iconLoader()->loadIcon("knewstuff", KIcon::Panel);
  frame = addPage(p->name(), p->name(), pix);
  m_frame = frame;

  w2 = new QWidget(frame);
  w_d = new QWidget(frame);
  w_r = new QWidget(frame);
  w_l = new QWidget(frame);

  ctl = new QTabWidget(frame);
  ctl->addTab(w_r, i18n("Highest Rated"));
  ctl->addTab(w_d, i18n("Most Downloads"));
  ctl->addTab(w_l, i18n("Latest"));

  m_curtab = 0;
  connect(ctl, SIGNAL(currentChanged(QWidget *)), SLOT(slotTab()));

  QHBoxLayout *box = new QHBoxLayout(frame);
  box->add(ctl);

  bool ocs = ocsIsProvider(p);

  d->m_lvtmp_r = new KListView(w_r);
  d->m_lvtmp_d = new KListView(w_d);
  d->m_lvtmp_l = new KListView(w_l);

  if(ocs)
  {
    // the server sorts; every list shows the same columns
    KListView *lists[] = { d->m_lvtmp_r, d->m_lvtmp_d, d->m_lvtmp_l };
    for(int i = 0; i < 3; i++)
    {
      KListView *lv = lists[i];
      lv->addColumn(i18n("Name"));
      lv->addColumn(i18n("Author"));
      lv->addColumn(i18n("Rating"));
      lv->addColumn(i18n("Downloads"));
      lv->addColumn(i18n("Updated"));
      lv->setColumnAlignment(2, Qt::AlignRight);
      lv->setColumnAlignment(3, Qt::AlignRight);
      lv->setSorting(-1);
      lv->setAllColumnsShowFocus(true);
      connect(lv, SIGNAL(selectionChanged()), SLOT(slotSelected()));
    }
  }
  else
  {
  d->m_lvtmp_r->addColumn(i18n("Name"));
  d->m_lvtmp_r->addColumn(i18n("Version"));
  d->m_lvtmp_r->addColumn(i18n("Rating"));
  d->m_lvtmp_r->setSorting(2, false);

  d->m_lvtmp_d->addColumn(i18n("Name"));
  d->m_lvtmp_d->addColumn(i18n("Version"));
  d->m_lvtmp_d->addColumn(i18n("Downloads"));
  d->m_lvtmp_d->setSorting(2, false);

  d->m_lvtmp_l->addColumn(i18n("Name"));
  d->m_lvtmp_l->addColumn(i18n("Version"));
  d->m_lvtmp_l->addColumn(i18n("Release Date"));
  d->m_lvtmp_l->setSorting(2, false);
  }

  connect(d->m_lvtmp_r, SIGNAL(clicked(QListViewItem*)), SLOT(slotSelected()));
  connect(d->m_lvtmp_d, SIGNAL(clicked(QListViewItem*)), SLOT(slotSelected()));
  connect(d->m_lvtmp_l, SIGNAL(clicked(QListViewItem*)), SLOT(slotSelected()));

  rt = new QTextBrowser(frame);
  rt->setMinimumWidth(150);
  if(ocs)
  {
    // room for the preview picture, the rest for the list
    rt->setMinimumWidth(300);
    rt->setMaximumWidth(320);
  }

  QPushButton *in = new QPushButton(i18n("Install"), frame);
  QPushButton *de = new QPushButton(i18n("Details"), frame);
  in->setEnabled(false);
  de->setEnabled(false);

  box->addSpacing(spacingHint());
  QVBoxLayout *vbox = new QVBoxLayout(box);
  vbox->add(rt);
  vbox->addSpacing(spacingHint());
  vbox->add(de);
  vbox->add(in);

  connect(rt, SIGNAL(linkClicked(const QString&)), SLOT(slotEmail(const QString&)));

  connect(in, SIGNAL(clicked()), SLOT(slotInstall()));
  connect(de, SIGNAL(clicked()), SLOT(slotDetails()));

  QVBoxLayout *box2 = new QVBoxLayout(w_r);
  box2->add(d->m_lvtmp_r);
  QVBoxLayout *box3 = new QVBoxLayout(w_d);
  box3->add(d->m_lvtmp_d);
  QVBoxLayout *box4 = new QVBoxLayout(w_l);
  box4->add(d->m_lvtmp_l);

  if(ocs)
  {
    QWidget *tabs[] = { w_r, w_d, w_l };
    QVBoxLayout *layouts[] = { box2, box3, box4 };
    KListView *lists[] = { d->m_lvtmp_r, d->m_lvtmp_d, d->m_lvtmp_l };
    ocsAddPage(frame, p, rt, tabs, layouts, lists);
  }

  QValueList<KListView*> *v = new QValueList<KListView*>;
  *v << d->m_lvtmp_r << d->m_lvtmp_d << d->m_lvtmp_l;
  m_map[frame] = v;
  m_rts[frame] = rt;
  QValueList<QPushButton*> *vb = new QValueList<QPushButton*>;
  *vb << in << de;
  m_buttons[frame] = vb;
  m_providers[frame] = p;

  kdDebug() << "addProvider()/end; d->m_lvtmp_r = " << d->m_lvtmp_r << endl;

  if(m_engine) slotPage(frame);

  QTimer::singleShot(100, this, SLOT(slotFinish()));
}

void DownloadDialog::slotResult(KIO::Job *job)
{
  QDomDocument dom;
  QDomElement knewstuff;

  kdDebug() << "got data: " << m_data[job] << endl;

  kapp->config()->setGroup("KNewStuffStatus");

  dom.setContent(m_data[job]);
  knewstuff = dom.documentElement();

  for(QDomNode pn = knewstuff.firstChild(); !pn.isNull(); pn = pn.nextSibling())
  {
    QDomElement stuff = pn.toElement();

    kdDebug() << "element: " << stuff.tagName() << endl;

    if(stuff.tagName() == "stuff")
    {
      Entry *entry = new Entry(stuff);
      kdDebug() << "TYPE::" << entry->type() << " FILTER::" << m_filter << endl;
      if(!entry->type().isEmpty())
      {
        if((!m_filter.isEmpty()) && (entry->type() != m_filter)) continue;
      }

      /*if((!m_filter.isEmpty()) && (m_jobs[job]))
      {
        Provider *p = m_jobs[job];
        if(d->m_newproviders[p])
        {
          addProvider(p);
          slotPage(m_frame);
          d->m_newproviders[p] = 0;
        }
      }*/
      if((!m_filter.isEmpty()) && (d->m_variantjobs[job]))
      {
        Provider *p = d->m_variantjobs[job];
        if(d->m_newproviders[p])
        {
          addProvider(p);
          slotPage(m_frame);
          d->m_newproviders[p] = 0;
        }
      }

      /*if(m_jobs[job]) addEntry(entry);
      else*/
      if(d->m_variantjobs[job]) addEntry(entry, d->m_variants[job]);
    }
  }
}

int DownloadDialog::installStatus(Entry *entry)
{
  QDate date;
  QString datestring;
  int installed;

  QString lang = KGlobal::locale()->language();

  QString key = ocsStatusKey(entry);
  if(key.isEmpty()) key = entry->name(lang);
  KConfigGroupSaver saver(kapp->config(), "KNewStuffStatus");
  datestring = kapp->config()->readEntry(key);
  if(datestring.isNull()) installed = 0;
  else
  {
    date = QDate::fromString(datestring, Qt::ISODate);
    if(!date.isValid()) installed = 0;
    else if(date < entry->releaseDate()) installed = -1;
    else installed = 1;
  }

  return installed;
}

void DownloadDialog::addEntry(Entry *entry, const QStringList& variants)
{
  QPixmap pix;
  int installed;

  installed = installStatus(entry);

  if(installed > 0) pix = KGlobal::iconLoader()->loadIcon("ok", KIcon::Small);
  else if(installed < 0) pix = KGlobal::iconLoader()->loadIcon("history", KIcon::Small);
  else pix = QPixmap();

  QString lang = KGlobal::locale()->language();

  if(variants.contains("score"))
  {
    KListViewItem *tmp_r = new NumSortListViewItem(lv_r,
      entry->name(lang), entry->version(), QString("%1").arg(entry->rating()));
    tmp_r->setPixmap(0, pix);
  }
  if(variants.contains("downloads"))
  {
    KListViewItem *tmp_d = new NumSortListViewItem(lv_d,
      entry->name(lang), entry->version(), QString("%1").arg(entry->downloads()));
    tmp_d->setPixmap(0, pix);
  }
  if(variants.contains("latest"))
  {
    KListViewItem *tmp_l = new DateSortListViewItem(lv_l,
      entry->name(lang), entry->version(), KGlobal::locale()->formatDate(entry->releaseDate()));
    tmp_l->setPixmap(0, pix);
  }

  m_entries.append(entry);

  kdDebug() << "added entry " << entry->name() << " for variants " << variants << endl;
}

void DownloadDialog::addEntry(Entry *entry)
{
  QStringList variants;

  variants << "score";
  variants << "downloads";
  variants << "latest";

  addEntry(entry, variants);

  // not used anymore due to variants (but still used by engine)
  kdDebug() << "added entry " << entry->name() << endl;
}

void DownloadDialog::slotData(KIO::Job *job, const QByteArray &a)
{
  QCString tmp(a, a.size() + 1);
  m_data[job].append(QString::fromUtf8(tmp));
}

void DownloadDialog::slotDetails()
{
  Entry *e = getEntry();
  if(!e) return;

  QString lang = KGlobal::locale()->language();

  QString info = i18n
  (
    "Name: %1\n"
    "Author: %2\n"
    "License: %3\n"
    "Version: %4\n"
    "Release: %5\n"
    "Rating: %6\n"
    "Downloads: %7\n"
    "Release date: %8\n"
    "Summary: %9\n"
    ).arg(e->name(lang)
    ).arg(e->author()
    ).arg(e->license()
    ).arg(e->version()
    ).arg(e->release()
    ).arg(e->rating()
    ).arg(e->downloads()
    ).arg(KGlobal::locale()->formatDate(e->releaseDate())
    ).arg(e->summary(lang)
  );

  if(d->m_entryPage.contains(e))
  {
    Private::OcsPage *page = d->m_ocs[d->m_entryPage[e]];
    info.append(i18n("Web page: %1\n").arg(page->content[e].detailPage.url()));
  }
  else
  info.append(i18n
  (
    "Preview: %1\n"
    "Payload: %2\n"
    ).arg(e->preview().url()
    ).arg(e->payload().url()
  ));

  // the texts come from the server: never let the message box take them
  // for rich text
  info = "<qt>" + QStyleSheet::escape(info).replace("\n", "<br>") + "</qt>";
  KMessageBox::information(this, info, i18n("Details"));
}

QListViewItem *DownloadDialog::currentEntryItem()
{
  if((m_curtab == 0) && (lv_r->selectedItem())) return lv_r->selectedItem();
  if((m_curtab == 1) && (lv_d->selectedItem())) return lv_d->selectedItem();
  if((m_curtab == 2) && (lv_l->selectedItem())) return lv_l->selectedItem();

  return 0;
}

void DownloadDialog::slotInstall()
{
  Entry *e = getEntry();
  if(!e) return;

  if(d->m_entryPage.contains(e))
  {
    ocsInstall(e);
    return;
  }

  d->m_lvtmp_r->setEnabled( false );
  d->m_lvtmp_l->setEnabled( false );
  d->m_lvtmp_d->setEnabled( false );
  m_entryitem = currentEntryItem();
  m_entryname = m_entryitem->text(0);

  kdDebug() << "download entry now" << endl;

  if(m_engine)
  {
    m_engine->download(e);
    install(e);
  }
  else
  {
    m_s = new KNewStuffGeneric(e->type(), this);
    m_entry = e;
    KURL source = e->payload();
    KURL dest = KURL(m_s->downloadDestination(e));

    KIO::FileCopyJob *job = KIO::file_copy(source, dest, -1, true);
    connect(job, SIGNAL(result(KIO::Job*)), SLOT(slotInstalled(KIO::Job*)));
  }
}

void DownloadDialog::install(Entry *e)
{
  QString key = ocsStatusKey(e);
  if(key.isEmpty()) key = m_entryname;
  {
    KConfigGroupSaver saver(kapp->config(), "KNewStuffStatus");
    kapp->config()->writeEntry(key, e->releaseDate().toString(Qt::ISODate));
    kapp->config()->sync();
  }

  QPixmap pix = KGlobal::iconLoader()->loadIcon("ok", KIcon::Small);

  QString lang = KGlobal::locale()->language();
  
  if(d->m_entryPage.contains(e))
  {
    ocsUpdateItems(e);
  }
  else if(m_entryitem)
  {
    m_entryitem->setPixmap(0, pix);

    QListViewItem *item;
    item = lv_r->findItem(e->name(lang), 0);
    if(item) item->setPixmap(0, pix);
    item = lv_d->findItem(e->name(lang), 0);
    if(item) item->setPixmap(0, pix);
    item = lv_l->findItem(e->name(lang), 0);
    if(item) item->setPixmap(0, pix);
  }

  if(currentEntryItem() == m_entryitem)
  {
    QPushButton *in;
    in = *(m_buttons[d->m_page]->at(0));
    if(in) in->setEnabled(false);
  }

  d->m_installlist.append(e);
  d->m_lvtmp_r->setEnabled( true );
  d->m_lvtmp_l->setEnabled( true );
  d->m_lvtmp_d->setEnabled( true );
}

void DownloadDialog::slotInstalled(KIO::Job *job)
{
  bool ret = job && (job->error() == 0);
  if(ret)
  {
    KIO::FileCopyJob *cjob = ::qt_cast<KIO::FileCopyJob*>(job);
    if(cjob)
    {
      ret = m_s->install(cjob->destURL().path());
    }
    else ret = false;
  }

  if(ret)
  {
    install(m_entry);

    KMessageBox::information(this, i18n("Installation successful."), i18n("Installation"));
  }
  else KMessageBox::error(this, i18n("Installation failed."), i18n("Installation"));
  d->m_lvtmp_r->setEnabled( true );
  d->m_lvtmp_l->setEnabled( true );
  d->m_lvtmp_d->setEnabled( true );

  delete m_s;
}

void DownloadDialog::slotTab()
{
  int tab = static_cast<const QTabWidget *>(sender())->currentPageIndex();
  kdDebug() << "switch tab to: " << tab << endl;

  if(d->m_ocs.contains(d->m_page) && tab >= 0 && tab < 3 &&
     !d->m_ocs[d->m_page]->tabs[tab].started)
    ocsFetch(d->m_page, tab);

  Entry *eold = getEntry();
  m_curtab = tab;
  Entry *e = getEntry();

  if(e == eold) return;

  if(e)
  {
    slotSelected();
  }
  else
  {
    QPushButton *de, *in;
    in = *(m_buttons[d->m_page]->at(0));
    de = *(m_buttons[d->m_page]->at(1));

    if(in) in->setEnabled(false);
    if(de) de->setEnabled(false);

    m_rt->clear();
  }
}

void DownloadDialog::slotSelected()
{
  QString tmp;
  bool enabled;
  Entry *e = getEntry();
  QString lang = KGlobal::locale()->language();
  bool ret;

  if(e && d->m_entryPage.contains(e))
  {
    ocsShowDetails(e);
    return;
  }

  if(e)
  {
    QString lang = KGlobal::locale()->language();

    QListViewItem *item;
    if(m_curtab != 0)
    {
      lv_r->clearSelection();
      item = lv_r->findItem(e->name(lang), 0);
      if(item) lv_r->setSelected(item, true);
    }
    if(m_curtab != 1)
    {
      lv_d->clearSelection();
      item = lv_d->findItem(e->name(lang), 0);
      if(item) lv_d->setSelected(item, true);
    }
    if(m_curtab != 2)
    {
      lv_l->clearSelection();
      item = lv_l->findItem(e->name(lang), 0);
      if(item) lv_l->setSelected(item, true);
    }

    if(!e->preview(lang).isValid())
    {
      ret = 0;
    }
    else
    {
      ret = KIO::NetAccess::download(e->preview(lang), tmp, this);
    }

    QString desc = QString("<b>%1</b><br>").arg(e->name(lang));
    if(!e->authorEmail().isNull())
    {
      desc += QString("<a href='mailto:" + e->authorEmail() + "'>" + e->author() + "</a>");
    }
    else
    {
      desc += QString("%1").arg(e->author());
    }
    desc += QString("<br>%1").arg(KGlobal::locale()->formatDate(e->releaseDate()));
    desc += QString("<br><br>");
    if(ret)
    {
      desc += QString("<img src='%1'>").arg(tmp);
    }
    else
    {
      desc += i18n("Preview not available.");
    }
    desc += QString("<br><i>%1</i>").arg(e->summary(lang));
    desc += QString("<br>(%1)").arg(e->license());

    m_rt->clear();
    m_rt->setText(desc);

    if(installStatus(e) == 1) enabled = false;
    else enabled = true;

    QPushButton *de, *in;
    in = *(m_buttons[d->m_page]->at(0));
    de = *(m_buttons[d->m_page]->at(1));
    if(in) in->setEnabled(enabled);
    if(de) de->setEnabled(true);
  }
}

void DownloadDialog::slotEmail(const QString& link)
{
  kdDebug() << "EMAIL: " << link << endl;
  kapp->invokeMailer(link);
  slotSelected(); // QTextBrowser oddity workaround as it cannot handle mailto: URLs
}

Entry *DownloadDialog::getEntry()
{
  QListViewItem *entryItem = currentEntryItem();

  if(!entryItem)
    return 0;

  QMap<QListViewItem*, Entry*>::Iterator it = d->m_itemEntries.find(entryItem);
  if(it != d->m_itemEntries.end())
    return it.data();

  QString entryName = entryItem->text(0);

  QString lang = KGlobal::locale()->language();

  for(Entry *e = m_entries.first(); e; e = m_entries.next())
    if(e->name(lang) == entryName)
      return e;

  return 0;
}

void DownloadDialog::slotPage(QWidget *w)
{
  Provider *p;

  kdDebug() << "changed widget!!!" << endl;

  if(m_map.find(w) == m_map.end()) return;

  d->m_page = w;

  lv_r = *(m_map[w]->at(0));
  lv_d = *(m_map[w]->at(1));
  lv_l = *(m_map[w]->at(2));
  p = m_providers[w];
  m_rt = m_rts[w];

  kdDebug() << "valid change!!!; lv_r = " << lv_r << endl;

  if(d->m_ocs.contains(w)) return;

  if(m_engine) return;

  if(!m_filter.isEmpty()) return;

  lv_r->clear();
  lv_d->clear();
  lv_l->clear();

  kdDebug() << "-- fetch -- " << p->downloadUrl() << endl;

  loadProvider(p);
}

void DownloadDialog::loadProvider(Provider *p)
{
  QMap<KIO::Job*, Provider*>::Iterator it;

  if(ocsIsProvider(p))
  {
    // the page lists its contents itself, page by page
    for(QMap<QWidget*, Provider*>::Iterator pit = m_providers.begin(); pit != m_providers.end(); ++pit)
      if(pit.data() == p && d->m_ocs.contains(pit.key())) return;
    addProvider(p);
    slotPage(m_frame);
    return;
  }

  for(it = d->m_variantjobs.begin(); it != d->m_variantjobs.end(); it++)
  {
    if(it.data() == p)
    {
      kdDebug() << "-- found provider data in cache" << endl;
      slotResult(it.key());
      return;
    }
  }

  QStringList variants;
  variants << "score";
  variants << "downloads";
  variants << "latest";

  // Optimise URLs so each unique URL only gets fetched once

  QMap<QString, QStringList> urls;

  for(QStringList::Iterator it = variants.begin(); it != variants.end(); it++)
  {
    QString url = p->downloadUrlVariant((*it)).url();
    if(!urls.contains(url))
    {
      urls[url] = QStringList();
    }
    urls[url] << (*it);

    it = variants.remove(it);
  }

  // Now fetch the URLs while keeping the variant list for each attached

  for(QMap<QString, QStringList>::Iterator it = urls.begin(); it != urls.end(); it++)
  {
    QString url = it.key();
    QStringList urlvariants = it.data();

    KIO::TransferJob *variantjob = KIO::get(url);
    d->m_newproviders[p] = p;
    d->m_variantjobs[variantjob] = p;
    d->m_variants[variantjob] = urlvariants;
    m_data[variantjob] = "";

    connect(variantjob, SIGNAL(result(KIO::Job*)), SLOT(slotResult(KIO::Job*)));
    connect(variantjob, SIGNAL(data(KIO::Job*, const QByteArray&)),
      SLOT(slotData(KIO::Job*, const QByteArray&)));
  }

  if(variants.count() == 0) return;

  // If not all variants are given, use default URL for those

  kdDebug() << "-- reached old downloadurl section; variants left: " << variants.count() << endl;

  KIO::TransferJob *job = KIO::get(p->downloadUrl());

  d->m_newproviders[p] = p;
  d->m_variantjobs[job] = p;
  d->m_variants[job] = variants;
  //m_jobs[job] = p; // not used anymore due to variants
  m_data[job] = "";

  connect(job, SIGNAL(result(KIO::Job*)), SLOT(slotResult(KIO::Job*)));
  connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
    SLOT(slotData(KIO::Job*, const QByteArray&)));
}

void DownloadDialog::setType(QString type)
{
  m_filter = type;
}

void DownloadDialog::setProviderList(const QString& providerList)
{
  d->m_providerlist = providerList;
}

void DownloadDialog::slotOk()
{
}

void DownloadDialog::slotApply()
{
}

void DownloadDialog::open(const QString& type, const QString& caption)
{
  DownloadDialog d(0, caption);
  d.setType(type);
  d.load();
  d.exec();
}

void DownloadDialog::slotFinish()
{
  showPage(1);
  //updateBackground();
}

QPtrList<Entry> DownloadDialog::installedEntries()
{
  return d->m_installlist;
}

// BEGIN Open Collaboration Services

// The pictures of the KDE Store are served by a cache that scales them:
// ask for a size that suits the list and the details pane.
static KURL ocsPreviewUrl(const KURL &url)
{
  KURL u(url);
  if(u.host() == "images.pling.com")
  {
    QString path = u.path();
    QRegExp size("^/cache/[0-9]+x[0-9]+(-[0-9]+)?/");
    if(size.search(path) == 0)
    {
      path.replace(0, size.matchedLength(), "/cache/280x171-2/");
      u.setPath(path);
    }
  }
  return u;
}

void DownloadDialog::ocsAddPage(QWidget *frame, Provider *p, QTextBrowser *rt,
                                QWidget **tabs, QVBoxLayout **layouts, KListView **lists)
{
  Private::OcsPage *page = new Private::OcsPage;
  page->provider = p;
  page->host = ocsBaseUrl(p).host();
  page->browser = rt;
  page->factory = new QMimeSourceFactory;
  rt->setMimeSourceFactory(page->factory);

  page->client = new OcsClient(ocsBaseUrl(p), ocsCategories(p), frame);
  connect(page->client, SIGNAL(contentsListed(int, int, const QValueList<KNS::OcsContent> &, int, bool)),
          SLOT(slotOcsListed(int, int, const QValueList<KNS::OcsContent> &, int, bool)));
  connect(page->client, SIGNAL(listingFailed(int, const QString &)),
          SLOT(slotOcsListingFailed(int, const QString &)));
  connect(page->client, SIGNAL(previewLoaded(int, const QImage &)),
          SLOT(slotOcsPreview(int, const QImage &)));
  connect(page->client, SIGNAL(downloadResolved(const QString &, int, const KURL &)),
          SLOT(slotOcsResolved(const QString &, int, const KURL &)));
  connect(page->client, SIGNAL(downloadFailed(const QString &, int, const QString &)),
          SLOT(slotOcsResolveFailed(const QString &, int, const QString &)));

  for(int i = 0; i < 3; i++)
  {
    Private::OcsTab &t = page->tabs[i];
    t.list = lists[i];
    t.nextPage = 0;
    t.loaded = 0;
    t.busy = false;
    t.started = false;

    QHBoxLayout *row = new QHBoxLayout(layouts[i]);
    row->setSpacing(spacingHint());
    t.status = new QLabel(tabs[i]);
    t.status->setTextFormat(Qt::PlainText);
    t.more = new QPushButton(i18n("&More"), tabs[i]);
    t.more->setEnabled(false);
    row->addWidget(t.status, 1);
    row->addWidget(t.more);
    connect(t.more, SIGNAL(clicked()), SLOT(slotOcsMore()));
  }

  d->m_ocs[frame] = page;

  if(!d->m_ocsResized)
  {
    d->m_ocsResized = true;
    resize(QMAX(width(), 960), QMAX(height(), 640));
  }

  ocsFetch(frame, 0);
}

void DownloadDialog::ocsFetch(QWidget *frame, int tab)
{
  if(!d->m_ocs.contains(frame) || tab < 0 || tab > 2) return;
  Private::OcsPage *page = d->m_ocs[frame];
  Private::OcsTab &t = page->tabs[tab];

  t.started = true;
  t.busy = true;
  t.more->setEnabled(false);
  t.status->setText(i18n("Loading..."));
  page->client->listContents(tab, t.nextPage);
}

void DownloadDialog::slotOcsListed(int sortMode, int pageNumber, const QValueList<KNS::OcsContent> &items, int total, bool more)
{
  Private::OcsPage *page = d->ocsPageFor(const_cast<QObject*>(sender()));
  if(!page || sortMode < 0 || sortMode > 2) return;
  Private::OcsTab &t = page->tabs[sortMode];
  QWidget *frame = 0;
  for(QMap<QWidget*, Private::OcsPage*>::Iterator it = d->m_ocs.begin(); it != d->m_ocs.end(); ++it)
    if(it.data() == page) frame = it.key();

  t.busy = false;
  t.nextPage = pageNumber + 1;

  QColor background = t.list->viewport()->colorGroup().base();

  for(QValueList<OcsContent>::ConstIterator it = items.begin(); it != items.end(); ++it)
  {
    const OcsContent &c = *it;

    bool isNew = !page->entries.contains(c.id);
    Entry *e = isNew ? new Entry : page->entries[c.id];
    e->setName(c.name);
    e->setName(c.name, QString::null);
    e->setType(m_filter);
    e->setAuthor(c.author);
    e->setLicence(c.license);
    e->setSummary(c.summary);
    e->setVersion(c.version);
    e->setReleaseDate(c.changed.date());
    e->setRating(c.score);
    e->setDownloads(c.downloads);
    e->setPreview(c.preview);
    page->content[e] = c;
    if(isNew)
    {
      page->entries[c.id] = e;
      d->m_entryPage[e] = frame;
      m_entries.append(e);

      if(c.preview.isValid())
      {
        int key = d->m_nextPreviewKey++;
        page->previewKeys[key] = e;
        page->client->fetchPreview(ocsPreviewUrl(c.preview), key);
      }
    }

    KListViewItem *item = new KListViewItem(t.list, t.list->lastItem(),
      c.name, c.author, QString("%1%").arg(c.score),
      KGlobal::locale()->formatNumber(c.downloads, 0),
      KGlobal::locale()->formatDate(c.changed.date(), true));
    item->setPixmap(0, ocsItemPixmap(e, background));
    d->m_itemEntries[item] = e;
    t.loaded++;
  }

  t.more->setText(i18n("&More"));
  t.more->setEnabled(more);
  if(t.loaded == 0)
    t.status->setText(i18n("No items found."));
  else if(total >= t.loaded)
    t.status->setText(i18n("%1 of %2 items").arg(t.loaded).arg(total));
  else
    t.status->setText(i18n("%1 items").arg(t.loaded));
}

void DownloadDialog::slotOcsListingFailed(int sortMode, const QString &message)
{
  Private::OcsPage *page = d->ocsPageFor(const_cast<QObject*>(sender()));
  if(!page || sortMode < 0 || sortMode > 2) return;
  Private::OcsTab &t = page->tabs[sortMode];

  t.busy = false;
  t.status->setText(i18n("Could not load the list: %1").arg(message));
  t.more->setText(i18n("&Retry"));
  t.more->setEnabled(true);
}

void DownloadDialog::slotOcsMore()
{
  const QObject *button = sender();
  for(QMap<QWidget*, Private::OcsPage*>::Iterator it = d->m_ocs.begin(); it != d->m_ocs.end(); ++it)
    for(int i = 0; i < 3; i++)
      if(it.data()->tabs[i].more == button)
      {
        if(!it.data()->tabs[i].busy) ocsFetch(it.key(), i);
        return;
      }
}

void DownloadDialog::slotOcsPreview(int key, const QImage &image)
{
  Private::OcsPage *page = d->ocsPageFor(const_cast<QObject*>(sender()));
  if(!page || !page->previewKeys.contains(key)) return;
  Entry *e = page->previewKeys[key];
  page->previewKeys.remove(key);

  QImage preview = image;
  if(preview.width() > 280 || preview.height() > 200)
    preview = preview.smoothScale(280, 200, QImage::ScaleMin);
  page->previews[e] = preview;

  ocsUpdateItems(e);
  if(getEntry() == e) ocsShowDetails(e);
}

QPixmap DownloadDialog::ocsItemPixmap(Entry *e, const QColor &background)
{
  QPixmap pm(ocsThumbWidth, ocsThumbHeight);
  pm.fill(background);
  QPainter painter(&pm);

  Private::OcsPage *page = d->m_entryPage.contains(e) ? d->m_ocs[d->m_entryPage[e]] : 0;
  if(page && page->previews.contains(e))
  {
    QImage thumb = page->previews[e].smoothScale(ocsThumbWidth, ocsThumbHeight, QImage::ScaleMin);
    painter.drawImage((ocsThumbWidth - thumb.width()) / 2, (ocsThumbHeight - thumb.height()) / 2, thumb);
  }

  int installed = installStatus(e);
  if(installed != 0)
  {
    QPixmap icon = KGlobal::iconLoader()->loadIcon(installed > 0 ? "ok" : "history", KIcon::Small);
    painter.drawPixmap(ocsThumbWidth - icon.width(), ocsThumbHeight - icon.height(), icon);
  }
  painter.end();
  return pm;
}

void DownloadDialog::ocsUpdateItems(Entry *e)
{
  QPixmap pm;
  bool done = false;
  for(QMap<QListViewItem*, Entry*>::Iterator it = d->m_itemEntries.begin(); it != d->m_itemEntries.end(); ++it)
  {
    if(it.data() != e) continue;
    if(!done)
    {
      pm = ocsItemPixmap(e, it.key()->listView()->viewport()->colorGroup().base());
      done = true;
    }
    it.key()->setPixmap(0, pm);
  }
}

QString DownloadDialog::ocsStatusKey(Entry *e)
{
  if(!e || !d->m_entryPage.contains(e)) return QString::null;
  Private::OcsPage *page = d->m_ocs[d->m_entryPage[e]];
  return QString("ocs:%1:%2").arg(page->host).arg(page->content[e].id);
}

void DownloadDialog::ocsShowDetails(Entry *e)
{
  Private::OcsPage *page = d->m_ocs[d->m_entryPage[e]];
  const OcsContent &c = page->content[e];

  QString desc = "<qt><b>" + QStyleSheet::escape(c.name) + "</b>";
  if(!c.version.isEmpty())
    desc += " " + QStyleSheet::escape(c.version);
  if(!c.author.isEmpty())
    desc += "<br>" + i18n("by %1").arg(QStyleSheet::escape(c.author));
  desc += "<br><br>";
  if(page->previews.contains(e))
  {
    QString name = QString("ocs-preview-%1").arg(c.id);
    page->factory->setImage(name, page->previews[e]);
    desc += "<img src=\"" + name + "\">";
  }
  else if(c.preview.isValid())
    desc += i18n("Loading preview...");
  else
    desc += i18n("Preview not available.");
  if(!c.summary.isEmpty())
    desc += "<br><br><i>" + QStyleSheet::escape(c.summary) + "</i>";
  desc += "<br><br>" + i18n("Rating: %1%").arg(c.score);
  desc += "<br>" + i18n("Downloads: %1").arg(KGlobal::locale()->formatNumber(c.downloads, 0));
  if(c.changed.isValid())
    desc += "<br>" + i18n("Updated: %1").arg(KGlobal::locale()->formatDate(c.changed.date()));
  if(!c.license.isEmpty())
    desc += "<br>" + i18n("License: %1").arg(QStyleSheet::escape(c.license));
  desc += "</qt>";

  m_rt->setText(desc);

  bool busy = d->m_ocsEntry != 0;
  QPushButton *in = *(m_buttons[d->m_page]->at(0));
  QPushButton *de = *(m_buttons[d->m_page]->at(1));
  if(in) in->setEnabled(!busy && installStatus(e) != 1 && !c.files.isEmpty());
  if(de) de->setEnabled(true);
}

void DownloadDialog::ocsInstall(Entry *e)
{
  if(d->m_ocsEntry) return;

  QWidget *frame = d->m_entryPage[e];
  Private::OcsPage *page = d->m_ocs[frame];
  // a copy: listings may update the content while the file dialog is open
  const OcsContent c = page->content[e];

  if(c.files.isEmpty())
  {
    KMessageBox::sorry(this, i18n("This item has no files that can be downloaded."));
    return;
  }

  int index = 0;
  if(c.files.count() > 1)
  {
    QStringList choices;
    int n = 1;
    for(QValueList<OcsDownload>::ConstIterator it = c.files.begin(); it != c.files.end(); ++it)
    {
      QString s = QString("%1. %2").arg(n++).arg((*it).name);
      if((*it).sizeKiB > 0)
        s += " (" + KIO::convertSizeFromKB((*it).sizeKiB) + ")";
      choices << s;
    }
    bool ok = false;
    QString choice = KInputDialog::getItem(i18n("Choose File"),
      "<qt>" + i18n("\"%1\" offers several files. Which one do you want to install?").arg(QStyleSheet::escape(c.name)) + "</qt>",
      choices, 0, false, &ok, this);
    if(!ok) return;
    index = choices.findIndex(choice);
    if(index < 0) return;
  }

  if(!m_engine)
  {
    bool wallpaper;
    if(OcsInstaller::targetDirectory(&wallpaper).isEmpty())
    {
      KMessageBox::error(this, i18n("The application does not say where to install this item."));
      return;
    }
  }

  d->m_ocsEntry = e;
  d->m_ocsPage = frame;
  d->m_ocsFile = c.files[index];
  d->m_ocsTooLarge = false;
  d->m_ocsBadRedirect = false;

  m_entryitem = currentEntryItem();
  m_entryname = m_entryitem ? m_entryitem->text(0) : c.name;

  d->m_lvtmp_r->setEnabled(false);
  d->m_lvtmp_l->setEnabled(false);
  d->m_lvtmp_d->setEnabled(false);
  QPushButton *in = *(m_buttons[d->m_page]->at(0));
  if(in) in->setEnabled(false);

  page->client->resolveDownload(c.id, d->m_ocsFile.number);
}

void DownloadDialog::slotOcsResolveFailed(const QString &, int, const QString &message)
{
  if(!d->m_ocsEntry) return;
  KMessageBox::error(this, "<qt>" + i18n("Could not get the download link: %1").arg(QStyleSheet::escape(message)) + "</qt>");
  ocsFinish();
}

void DownloadDialog::slotOcsResolved(const QString &id, int number, const KURL &link)
{
  Entry *e = d->m_ocsEntry;
  if(!e || !d->m_ocs.contains(d->m_ocsPage)) return;
  Private::OcsPage *page = d->m_ocs[d->m_ocsPage];
  if(page->content[e].id != id || d->m_ocsFile.number != number) return;

  kdDebug() << "OCS: downloading " << link.url() << endl;

  delete d->m_ocsTemp;
  d->m_ocsTemp = new KTempFile(locateLocal("tmp", "knewstuff-ocs-"), ".download");
  d->m_ocsTemp->setAutoDelete(true);
  d->m_ocsWritten = 0;
  if(d->m_ocsTemp->status() != 0 || !d->m_ocsTemp->file())
  {
    KMessageBox::error(this, i18n("Could not create a temporary file."));
    ocsFinish();
    return;
  }

  // a plain GET, so that the transfer can be anonymous (no cookies) and
  // limited in size; the server redirects to its file storage
  KIO::TransferJob *job = KIO::get(link, true, true);
  job->addMetaData("errorPage", "false");
  job->addMetaData("cookies", "none");
  job->addMetaData("ssl_militant", "TRUE");
  d->m_ocsJob = job;
  connect(job, SIGNAL(redirection(KIO::Job*, const KURL&)),
          SLOT(slotOcsRedirection(KIO::Job*, const KURL&)));
  connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
          SLOT(slotOcsDownloadData(KIO::Job*, const QByteArray&)));
  connect(job, SIGNAL(result(KIO::Job*)), SLOT(slotOcsDownloaded(KIO::Job*)));
}

void DownloadDialog::slotOcsRedirection(KIO::Job *job, const KURL &url)
{
  if(job != d->m_ocsJob || !d->m_ocs.contains(d->m_ocsPage)) return;
  if(d->m_ocs[d->m_ocsPage]->client->isAllowedUrl(url)) return;
  // reported by slotOcsDownloaded()
  d->m_ocsBadRedirect = true;
  job->kill(false);
}

void DownloadDialog::slotOcsDownloadData(KIO::Job *job, const QByteArray &data)
{
  if(job != d->m_ocsJob || !d->m_ocsTemp || d->m_ocsTooLarge || data.isEmpty()) return;

  d->m_ocsWritten += data.size();
  QFile *f = d->m_ocsTemp->file();
  if(d->m_ocsWritten > OcsInstaller::maxDownloadSize ||
     !f || f->writeBlock(data.data(), data.size()) != (Q_LONG)data.size())
  {
    // reported by slotOcsDownloaded()
    d->m_ocsTooLarge = true;
    job->kill(false);
  }
}

void DownloadDialog::slotOcsDownloaded(KIO::Job *job)
{
  if(job != d->m_ocsJob) return;
  d->m_ocsJob = 0;
  Entry *e = d->m_ocsEntry;
  if(!e || !d->m_ocs.contains(d->m_ocsPage))
  {
    ocsFinish();
    return;
  }

  if(job->error() || d->m_ocsTooLarge || d->m_ocsBadRedirect || !d->m_ocsTemp->close())
  {
    if(d->m_ocsBadRedirect)
      KMessageBox::error(this, i18n("The server redirected the download to a location that is not allowed."));
    else if(d->m_ocsTooLarge)
      KMessageBox::error(this, i18n("The download is too large, or it could not be saved."));
    else if(!job->error())
      KMessageBox::error(this, i18n("The download could not be saved."));
    else if(job->error() != KIO::ERR_USER_CANCELED)
      job->showErrorDialog(this);
    ocsFinish();
    return;
  }

  if(m_engine)
  {
    // The application installs the file: the engine copies it to the
    // application's destination. The copy runs after this returns, so the
    // file is kept until the next download or until the dialog closes.
    delete d->m_ocsEngineTemp;
    d->m_ocsEngineTemp = d->m_ocsTemp;
    d->m_ocsTemp = 0;
    e->setPayload(KURL::fromPathOrURL(d->m_ocsEngineTemp->name()));
    m_engine->download(e);
    install(e);
    ocsFinish();
    return;
  }

  Private::OcsPage *page = d->m_ocs[d->m_ocsPage];
  bool wallpaper;
  QString dir = OcsInstaller::targetDirectory(&wallpaper);
  QStringList installed;
  QString error = dir.isEmpty() ?
    i18n("The application does not say where to install this item.") :
    OcsInstaller::install(d->m_ocsTemp->name(), dir, wallpaper, page->content[e],
                          d->m_ocsFile, this, &installed);

  if(!error.isEmpty())
    KMessageBox::error(this, "<qt>" + QStyleSheet::escape(error) + "</qt>", i18n("Installation"));
  else if(!installed.isEmpty())
  {
    install(e);
    KMessageBox::information(this, i18n("Installation successful."), i18n("Installation"));
  }

  ocsFinish();
}

void DownloadDialog::ocsFinish()
{
  Entry *e = d->m_ocsEntry;
  d->m_ocsEntry = 0;
  d->m_ocsPage = 0;
  d->m_ocsJob = 0;
  delete d->m_ocsTemp;
  d->m_ocsTemp = 0;

  d->m_lvtmp_r->setEnabled(true);
  d->m_lvtmp_l->setEnabled(true);
  d->m_lvtmp_d->setEnabled(true);

  if(e && getEntry() == e) ocsShowDetails(e);
}

// END Open Collaboration Services
