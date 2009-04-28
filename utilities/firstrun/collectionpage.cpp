/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2009-28-04
 * Description : first run assistant dialog
 *
 * Copyright (C) 2009 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "collectionpage.h"
#include "collectionpage.moc"

// Qt includes

#include <QLabel>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QVBoxLayout>

// KDE includes

#include <kdialog.h>
#include <kconfig.h>
#include <kvbox.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kurlrequester.h>
#include <kglobalsettings.h>
#include <kmessagebox.h>

// Local includes

#include "version.h"

namespace Digikam
{

class CollectionPagePriv
{
public:

    CollectionPagePriv()
    {
        rootAlbumPathRequester = 0;
        dbPathRequester        = 0;
        dbPathEdited           = false;
    }

    bool           dbPathEdited;

    QString        rootAlbum;
    QString        dbPath;

    KUrlRequester *rootAlbumPathRequester;
    KUrlRequester *dbPathRequester;
};

CollectionPage::CollectionPage(KAssistantDialog* dlg)
              : AssistantDlgPage(dlg, i18n("<b>Configure where images and meta-data are stored</b>")), 
                d(new CollectionPagePriv)
{
    QWidget *widget      = new QWidget(this);
    QVBoxLayout *vlayout = new QVBoxLayout(widget);

    QString picturesPath;
#if KDE_IS_VERSION(4,1,61)
    picturesPath = KGlobalSettings::picturesPath();
#else
#if QT_VERSION >= 0x040400
    picturesPath = QDesktopServices::storageLocation(QDesktopServices::PicturesLocation);
#endif
#endif
    kDebug() << picturesPath;
    if (picturesPath.isEmpty())
    {
        picturesPath = QDir::homePath() + i18nc("This is a path name so you should "
                                                "include the slash in the translation", "/Pictures");
    }

    QLabel *textLabel1 = new QLabel(widget);
    textLabel1->setWordWrap(true);
#ifndef _WIN32
    textLabel1->setText(i18n("<p>Please enter a location where you want to store your images.</p> "
                             "<p>You can choose any local folder, even one that already contains images."
                             "<br/> "
                             "More folders can be added later under the <i>Settings</i> menu. "
                             "</p> "
                             "<p>Note: Removable media (such as USB drives or DVDs) and remote file systems "
                             "(such as NFS, or Samba mounted with cifs/smbfs) are supported.</p>") );
#else
    textLabel1->setText(i18n("<p>Please enter a location where you want to store your images.</p> "
                             "<p>You can choose any local folder, even one that already contains images."
                             "<br/> "
                             "More folders can be added later under the <i>Settings</i> menu. "
                             "</p> ") );
#endif

    d->rootAlbumPathRequester = new KUrlRequester(widget);
    d->rootAlbumPathRequester->setMode(KFile::Directory | KFile::LocalOnly);
    d->rootAlbumPathRequester->setUrl(picturesPath);

    QLabel *textLabel3 = new QLabel(widget);
    textLabel3->setWordWrap(true);
    textLabel3->setText(i18n("<p>digiKam stores information and meta-data about your images in a database file. "
                             "Please set the location of this file or accept the default.</p>"
                             "<p><i>Note:</i> You need to have write access to the folder used here, "
                             "and you cannot use a remote location on a networked server, using NFS or Samba.</p>"));

    d->dbPathRequester = new KUrlRequester(widget);
    d->dbPathRequester->setMode(KFile::Directory | KFile::LocalOnly);
    d->dbPathRequester->setUrl(picturesPath);

    vlayout->addWidget(textLabel1);
    vlayout->addWidget(d->rootAlbumPathRequester);
    vlayout->addWidget(textLabel3);
    vlayout->addWidget(d->dbPathRequester);
    vlayout->setMargin(0);
    vlayout->setSpacing(KDialog::spacingHint());

    widget->setMinimumSize(450, sizeHint().height());
    setContentsWidget(widget);
    setDigiKamLogo();

    connect(d->rootAlbumPathRequester, SIGNAL(urlSelected(const KUrl&)),
            this, SLOT(slotAlbumRootChanged(const KUrl&)));

    connect(d->dbPathRequester, SIGNAL(urlSelected(const KUrl&)),
            this, SLOT(slotDbPathChanged(const KUrl&)));
}

CollectionPage::~CollectionPage()
{
    delete d;
}

QString CollectionPage::firstAlbumPath() const
{
    return d->rootAlbum;
}

QString CollectionPage::databasePath() const
{
    return d->dbPath;
}

bool CollectionPage::applySettings()
{
    QString rootAlbumFolder;
    if (!checkRootAlbum(rootAlbumFolder))
        return false;

    QString dbFolder;
    if (!checkDatabase(dbFolder))
        return false;

    saveSettings(rootAlbumFolder, dbFolder);
    return true;
}

void CollectionPage::saveSettings(const QString& rootAlbumFolder, const QString& dbFolder)
{
    KSharedConfig::Ptr config = KGlobal::config();
    KConfigGroup group        = config->group("General Settings");
    group.writeEntry("Version", digikam_version);

    group = config->group("Album Settings");
    group.writeEntry("Database File Path", dbFolder);

    d->rootAlbum = rootAlbumFolder;
    d->dbPath    = dbFolder;

    config->sync();
}

bool CollectionPage::checkRootAlbum(QString& rootAlbumFolder)
{
    rootAlbumFolder = d->rootAlbumPathRequester->url().toLocalFile();
    kDebug(50003) << "Root album is : " << rootAlbumFolder << endl;

    if (rootAlbumFolder.isEmpty())
    {
        KMessageBox::sorry(this, i18n("You must select a folder for digiKam to "
                                      "use as the root album. All of your images will go there."));
        return false;
    }

#ifndef _WIN32
    if (!QDir::isAbsolutePath(rootAlbumFolder))
    {
        rootAlbumFolder.prepend(QDir::homePath());
    }
#endif

    /*
    if (KUrl(rootAlbumFolder).equals(KUrl(QDir::homePath()), KUrl::CompareWithoutFragment))
    {
        KMessageBox::sorry(this, i18n("digiKam will not use your home folder as the "
                                      "root album. Please select another location."));
        return false;
    }
    */

    QDir targetPath(rootAlbumFolder);

    if (!targetPath.exists())
    {
        int rc = KMessageBox::questionYesNo(this,
                                   i18n("The folder to use as the root album path does not exist: "
                                        "<p><b>%1</b></p>"
                                        "Would you like digiKam to create it for you?",
                                        rootAlbumFolder),
                                   i18n("Create Root Album Folder?"));

        if (rc == KMessageBox::No)
        {
            return false;
        }

        if (!targetPath.mkdir(rootAlbumFolder))
        {
            KMessageBox::sorry(this,
                               i18n("digiKam could not create the folder to use as the root album.\n"
                                    "Please select a different location."
                                    "<p><b>%1</b></p>", rootAlbumFolder),
                               i18n("Create Root Album Folder Failed"));
            return false;
        }
    }

    QFileInfo path(rootAlbumFolder);

    if (!path.isWritable())
    {
        KMessageBox::information(this, i18n("You do not seem to have write access for the folder selected to be the root album.\n"
                                            "Warning: Without write access, the comment and tag features "
                                            "will not work."));
    }

    return true;
}

bool CollectionPage::checkDatabase(QString& dbFolder)
{
    dbFolder = d->dbPathRequester->url().toLocalFile();
    kDebug(50003) << "DB folder is : " << dbFolder << endl;

    if (dbFolder.isEmpty())
    {
        KMessageBox::sorry(this, i18n("You must select a folder for digiKam to "
                                      "store information and meta-data in a database file."));
        return false;
    }

#ifndef _WIN32
    if (!QDir::isAbsolutePath(dbFolder))
    {
        dbFolder.prepend(QDir::homePath());
    }
#endif

    /*
    if (KUrl(dbFolder).equals(KUrl(QDir::homePath()), KUrl::CompareWithoutFragment))
    {
        KMessageBox::sorry(this, i18n("digiKam cannot use your home folder as "
                                      "database file path."));
        return false;
    }
    */

    QDir targetPath(dbFolder);

    if (!targetPath.exists())
    {
        int rc = KMessageBox::questionYesNo(this,
                                   i18n("The folder to put your database in does not seem to exist: "
                                        "<p><b>%1</b></p>"
                                        "Would you like digiKam to create it for you?",
                                        dbFolder),
                                   i18n("Create Database Folder?"));

        if (rc == KMessageBox::No)
        {
            return false;
        }

        if (!targetPath.mkdir(dbFolder))
        {
            KMessageBox::sorry(this,
                               i18n("digiKam could not create the folder to host your database file.\n"
                                    "Please select a different location."
                                    "<p><b>%1</b></p>", dbFolder),
                               i18n("Create Database Folder Failed"));
            return false;
        }
    }

    QFileInfo path(dbFolder);

    if (!path.isWritable())
    {
        KMessageBox::information(this, i18n("<p>You do not seem to have write access for the folder to host the database file.<br/>"
                                            "Please select a different location.</p>"
                                            "<p><b>%1</b></p>", dbFolder),
                                 i18n("No Database Write Access"));
        return false;
    }

    return true;
}

void CollectionPage::slotAlbumRootChanged(const KUrl& url)
{
    if (!d->dbPathEdited)
        d->dbPathRequester->setUrl(url);
}

void CollectionPage::slotDbPathChanged(const KUrl&)
{
    d->dbPathEdited = true;
}

}   // namespace Digikam
