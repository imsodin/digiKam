/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2009-05-28
 * Description : database statistics dialog
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

#ifndef DBSTATDLG_H
#define DBSTATDLG_H

// Qt includes

#include <QMap>

// KDE includes

#include <kdialog.h>

// Local includes

#include "digikam_export.h"
#include "infodlg.h"

namespace Digikam
{

class DIGIKAM_EXPORT DBStatDlg : public InfoDlg
{
public:

    DBStatDlg(QWidget* parent);
    ~DBStatDlg();
};

}  // namespace Digikam

#endif  // DBSTATDLG_H
