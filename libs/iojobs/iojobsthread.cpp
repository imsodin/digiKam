/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2015-06-15
 * Description : IO Jobs thread for file system jobs
 *
 * Copyright (C) 2015 by Mohamed Anwer <m dot anwer at gmx dot com>
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

// Qt includes

#include <QFileInfo>
#include <QDir>

// Local includes

#include "iojobsthread.h"
#include "iojob.h"
#include "digikam_debug.h"
#include "coredb.h"
#include "coredbaccess.h"

namespace Digikam
{

class IOJobsThread::Private
{

public:

    Private()
        : jobsCount(0),
          isCanceled(false),
          keepErrors(true)
    {
    }

    int            jobsCount;
    bool           isCanceled;

    bool           keepErrors;
    QList<QString> errorsList;
};

IOJobsThread::IOJobsThread(QObject* const parent)
    : ActionThreadBase(parent),
      d(new Private)
{
}

IOJobsThread::~IOJobsThread()
{
    delete d;
}

void IOJobsThread::copy(const QList<QUrl>& srcFiles, const QUrl destAlbum)
{
    ActionJobCollection collection;

    foreach (const QUrl& url, srcFiles)
    {
        CopyJob* const j = new CopyJob(url, destAlbum, false);

        connectOneJob(j);

        collection.insert(j, 0);
        d->jobsCount++;
    }

    appendJobs(collection);
}

void IOJobsThread::move(const QList<QUrl>& srcFiles, const QUrl destAlbum)
{
    ActionJobCollection collection;

    foreach (const QUrl& url, srcFiles)
    {
        qCDebug(DIGIKAM_IOJOB_LOG) << "move: url" << url;
        CopyJob* const j = new CopyJob(url, destAlbum, true);

        connectOneJob(j);

        collection.insert(j, 0);
        d->jobsCount++;
    }

    appendJobs(collection);
}

void IOJobsThread::deleteFiles(const QList<QUrl>& srcsToDelete, bool useTrash)
{
    ActionJobCollection collection;

    foreach (const QUrl& url, srcsToDelete)
    {
        DeleteJob* const j = new DeleteJob(url, useTrash,true);

        connectOneJob(j);

        collection.insert(j, 0);
        d->jobsCount++;
    }

    appendJobs(collection);
}

void IOJobsThread::listDTrashItems(const QString& collectionPath)
{
    ActionJobCollection collection;

    DTrashItemsListingJob* const j = new DTrashItemsListingJob(collectionPath);

    connect(j, SIGNAL(trashItemInfo(DTrashItemInfo)),
            this, SIGNAL(collectionTrashItemInfo(DTrashItemInfo)));

    connect(j, SIGNAL(signalDone()),
            this, SIGNAL(finished()));

    collection.insert(j, 0);
    d->jobsCount++;

    appendJobs(collection);
}

void IOJobsThread::restoreDTrashItems(const DTrashItemInfoList& items)
{
    QList<QUrl> listOfJsonFilesToRemove;
    QList<QUrl> listOfUsedUrls;

    foreach (const DTrashItemInfo& item, items)
    {
        QUrl srcToRename = QUrl::fromLocalFile(item.trashPath);
        QUrl newName     = getAvailableQUrlToRestoreInCollection(item.collectionPath, listOfUsedUrls);

        listOfUsedUrls << newName;

        QFileInfo fi(item.collectionPath);

        if (!fi.dir().exists())
        {
            fi.dir().mkpath(fi.dir().path());
        }

        renameFile(srcToRename, newName);
        listOfJsonFilesToRemove << QUrl::fromLocalFile(item.jsonFilePath);
    }

    deleteFiles(listOfJsonFilesToRemove, false);
}

void IOJobsThread::deleteDTrashItems(const DTrashItemInfoList& items)
{
    QList<QUrl> urlsToDelete;
    CoreDbAccess access;

    foreach (const DTrashItemInfo& item, items)
    {
        urlsToDelete << QUrl::fromLocalFile(item.trashPath);
        urlsToDelete << QUrl::fromLocalFile(item.jsonFilePath);
        // Set the status of the image id to obsolete, i.e. to remove.
        access.db()->setItemStatus(item.imageId, DatabaseItem::Status::Obsolete);
    }

    deleteFiles(urlsToDelete, false);
}

void IOJobsThread::renameFile(const QUrl& srcToRename, const QUrl& newName)
{
    ActionJobCollection collection;
    RenameFileJob* const j = new RenameFileJob(srcToRename, newName);

    connectOneJob(j);

    connect(j, SIGNAL(signalRenamed(QUrl,QUrl)),
            this, SIGNAL(renamed(QUrl,QUrl)));

    connect(j, SIGNAL(signalRenamedFailed(QUrl)),
            this, SIGNAL(renameFailed(QUrl)));

    collection.insert(j, 0);
    d->jobsCount++;

    appendJobs(collection);
}

void IOJobsThread::cancel()
{
    d->isCanceled = true;
    ActionThreadBase::cancel();
}

bool IOJobsThread::isCanceled()
{
    return d->isCanceled;
}

bool IOJobsThread::hasErrors()
{
    return !d->errorsList.isEmpty();
}

void IOJobsThread::setKeepErrors(bool keepErrors)
{
    d->keepErrors = keepErrors;
}

bool IOJobsThread::isKeepingErrors()
{
    return d->keepErrors;
}

QList<QString>& IOJobsThread::errorsList()
{
    return d->errorsList;
}

void IOJobsThread::connectOneJob(IOJob* const j)
{
    connect(j, SIGNAL(error(QString)),
            this, SLOT(error(QString)));

    connect(j, SIGNAL(signalDone()),
            this, SLOT(oneJobFinished()));
}

QUrl IOJobsThread::getAvailableQUrlToRestoreInCollection(const QString& fileColPath, QList<QUrl>& usedUrls, int version)
{
    QFileInfo fileInfo(fileColPath);

    if (version != 0)
    {
        QString dir      = fileInfo.dir().path() + QLatin1Char('/');
        QString baseName = fileInfo.baseName()   + QString::number(version);
        QString suffix   = QLatin1String(".")    + fileInfo.completeSuffix();
        fileInfo.setFile(dir + baseName + suffix);
    }

    QUrl url = QUrl::fromLocalFile(fileInfo.filePath());
    qCDebug(DIGIKAM_IOJOB_LOG) << "URL USED BEFORE: " << usedUrls.contains(url);

    if (!fileInfo.exists() && !usedUrls.contains(url))
    {
        return url;
    }
    else
    {
        return getAvailableQUrlToRestoreInCollection(fileColPath, usedUrls, ++version);
    }
}

void IOJobsThread::oneJobFinished()
{
    d->jobsCount--;

    if (d->jobsCount == 0)
    {
        emit finished();
        qCDebug(DIGIKAM_IOJOB_LOG) << "Thread Finished";
    }
}

void IOJobsThread::error(const QString& errString)
{
    d->errorsList.append(errString);
}

} // namespace Digikam
