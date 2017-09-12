/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2012-05-28
 * Description : Media server manager
 *
 * Copyright (C) 2012      by Smit Mehta <smit dot meh at gmail dot com>
 * Copyright (C) 2012-2017 by Gilles Caulier <caulier dot gilles at gmail dot com>
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

#include "dmediaservermngr.h"

// Qt includes

#include <QList>
#include <QMap>
#include <QStringList>
#include <QUrl>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>
#include <QTextCodec>
#include <QStandardPaths>

// KDE includes

#include <ksharedconfig.h>
#include <kconfiggroup.h>

// Local includes

#include "dmediaserver.h"
#include "digikam_debug.h"

namespace Digikam
{

class DMediaServerMngrCreator
{
public:

    DMediaServerMngr object;
};

Q_GLOBAL_STATIC(DMediaServerMngrCreator, creator)

// ---------------------------------------------------------------------------------------------

class DMediaServerMngr::Private
{
public:

    Private()
    {
        server = 0;
    }

    QString                    file;
    DMediaServer*              server;
    QMap<QString, QList<QUrl>> collectionMap;
};

DMediaServerMngr* DMediaServerMngr::instance()
{
    return &creator->object;
}

DMediaServerMngr::DMediaServerMngr()
    : d(new Private)
{
    d->file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
              QLatin1String("/digikam/mediaserver.xml");
}

DMediaServerMngr::~DMediaServerMngr()
{
    delete d;
}

void DMediaServerMngr::cleanUp()
{
    slotTurnOff();
}

void DMediaServerMngr::slotTurnOff()
{
    delete d->server;
    d->server = 0;
}

void DMediaServerMngr::checkLoadAtStartup()
{
    // Load mediaserver at startup ?

    KSharedConfig::Ptr config    = KSharedConfig::openConfig();
    KConfigGroup dlnaConfigGroup = config->group(QLatin1String("DLNA Settings"));
    bool startServerOnStartup    = dlnaConfigGroup.readEntry(QLatin1String("Start Server On Startup"), false);

    if (startServerOnStartup)
    {
        // TODO: restore old configuration.
        slotTurnOn();
    }
}

void DMediaServerMngr::slotTurnOn()
{
    startMediaServer();
}

void DMediaServerMngr::setCollectionMap(const QMap<QString, QList<QUrl>>& collectionMap)
{
    d->collectionMap = collectionMap;
}

void DMediaServerMngr::startMediaServer()
{
    if (!d->server)
    {
        d->server = new DMediaServer();
    }

    d->server->addImagesOnServer(d->collectionMap);
}

bool DMediaServerMngr::save()
{
    // If not modified don't save the file
/*    if (!d->modified)
    {
        return true;
    }
*/
    QDomDocument doc(QString::fromLatin1("mediaserverlist"));
    doc.setContent(QString::fromUtf8("<!DOCTYPE XMLQueueList><mediaserverlist version=\"1.0\" client=\"digikam\" encoding=\"UTF-8\"/>"));
    QDomElement docElem = doc.documentElement();

    auto end = d->collectionMap.cend();

    for (auto it = d->collectionMap.cbegin() ; it != end ; ++it)
    {
        QDomElement elm = doc.createElement(QString::fromLatin1("album"));
        elm.setAttribute(QString::fromLatin1("title"), it.key());

        // ----------------------

        QDomElement data;

        foreach(const QUrl& url, it.value())
        {
            data = doc.createElement(QString::fromLatin1("path"));
            data.setAttribute(QString::fromLatin1("value"), url.toLocalFile());
            elm.appendChild(data);
        }

        docElem.appendChild(elm);
    }
   
    QFile file(d->file);

    if (!file.open(QIODevice::WriteOnly))
    {
        qCDebug(DIGIKAM_MEDIASRV_LOG) << "Cannot open XML file to store MediaServer list";
        qCDebug(DIGIKAM_MEDIASRV_LOG) << file.fileName();
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    stream.setAutoDetectUnicode(true);
    stream << doc.toString(4);
    file.close();

    return true;
}

bool DMediaServerMngr::load()
{
    //d->modified = false;

    QFile file(d->file);

    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            qCDebug(DIGIKAM_MEDIASRV_LOG) << "Cannot open XML file to load MediaServer list";
            return false;
        }

        QDomDocument doc(QString::fromLatin1("mediaserverlist"));

        if (!doc.setContent(&file))
        {
            qCDebug(DIGIKAM_MEDIASRV_LOG) << "Cannot load MediaServer list XML file";
            return false;
        }

        QDomElement                docElem = doc.documentElement();
        QMap<QString, QList<QUrl>> map;
        QList<QUrl>                urls;
        QString                    album;

        for (QDomNode n = docElem.firstChild() ; !n.isNull() ; n = n.nextSibling())
        {
            QDomElement e = n.toElement();

            if (e.isNull())
            {
                continue;
            }

            if (e.tagName() != QString::fromLatin1("album"))
            {
                continue;
            }

            album = e.attribute(QString::fromLatin1("title"));
            urls.clear();

            for (QDomNode n2 = e.firstChild() ; !n2.isNull() ; n2 = n2.nextSibling())
            {
                QDomElement e2 = n2.toElement();

                if (e2.isNull())
                {
                    continue;
                }

                QString name2 = e2.tagName();
                QString val2  = e2.attribute(QString::fromLatin1("value"));

                if (name2 == QString::fromLatin1("path"))
                {
                    urls << QUrl::fromLocalFile(val2);
                }
            }

            map.insert(album, urls);
        }

        setCollectionMap(map);

        return true;
    }
    else
    {
        return false;
    }
}

} // namespace Digikam