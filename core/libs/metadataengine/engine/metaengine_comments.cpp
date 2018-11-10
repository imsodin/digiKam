/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2006-09-15
 * Description : Exiv2 library interface.
 *               Comments manipulation methods.
 *
 * Copyright (C) 2006-2018 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * Copyright (C) 2006-2013 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
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

// Local includes

#include "metaengine_p.h"
#include "metaengine.h"
#include "digikam_debug.h"

namespace Digikam
{

bool MetaEngine::canWriteComment(const QString& filePath)
{
    QMutexLocker lock(&s_metaEngineMutex);

    try
    {
        Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open((const char*)
                                      (QFile::encodeName(filePath).constData()));

        Exiv2::AccessMode mode      = image->checkMode(Exiv2::mdComment);

        return (mode == Exiv2::amWrite || mode == Exiv2::amReadWrite);
    }
    catch( Exiv2::Error& e )
    {
        std::string s(e.what());
        qCCritical(DIGIKAM_METAENGINE_LOG) << "Cannot check Comment access mode using Exiv2 (Error #"
                                           << e.code() << ": " << s.c_str() << ")";
    }
    catch(...)
    {
        qCCritical(DIGIKAM_METAENGINE_LOG) << "Default exception from Exiv2";
    }

    return false;
}

bool MetaEngine::hasComments() const
{
    return !d->itemComments().empty();
}

bool MetaEngine::clearComments() const
{
    return setComments(QByteArray());
}

QByteArray MetaEngine::getComments() const
{
    return QByteArray(d->itemComments().data(), d->itemComments().size());
}

QString MetaEngine::getCommentsDecoded() const
{
    return d->detectEncodingAndDecode(d->itemComments());
}

bool MetaEngine::setComments(const QByteArray& data) const
{
    d->itemComments() = std::string(data.data(), data.size());

    return true;
}

QString MetaEngine::detectLanguageAlt(const QString& value, QString& lang)
{
    // Ex. from an Xmp tag Xmp.tiff.copyright: "lang="x-default"

    if (value.size() > 6 && value.startsWith(QLatin1String("lang=\"")))
    {
        int pos = value.indexOf(QLatin1String("\""), 6);

        if (pos != -1)
        {
            lang = value.mid(6, pos-6);
            return (value.mid(pos+2));
        }
    }

    lang.clear();

    return value;
}

} // namespace Digikam
