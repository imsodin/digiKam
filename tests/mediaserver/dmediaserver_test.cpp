/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2011-12-28
 * Description : stand alone test for DMediaServer
 *
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

// Qt includes

#include <QString>
#include <QStringList>
#include <QApplication>
#include <QStandardPaths>
#include <QUrl>
#include <QMap>
#include <QDebug>

// Local includes

#include "dmediaservermngr.h"
#include "dfiledialog.h"

using namespace Digikam;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QList<QUrl> list;

    if (argc <= 1)
    {
        QStringList files = DFileDialog::getOpenFileNames(0, QString::fromLatin1("Select Files to Share With Media Server"),
                                                          QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).first(),
                                                          QLatin1String("*.jpg"));

        foreach(const QString& f, files)
            list.append(QUrl::fromLocalFile(f));
    }
    else
    {
        for (int i = 1 ; i < argc ; i++)
            list.append(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[i])));
    }

    if (!list.isEmpty())
    {
        QMap<QString, QList<QUrl> > map;
        map.insert(QLatin1String("Test Collection"), list);
        DMediaServerMngr::instance()->setCollectionMap(map);
        app.exec();
    }

    return 0;
}
