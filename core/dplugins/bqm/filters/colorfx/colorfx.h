/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2012-11-08
 * Description : a batch tool to apply color effects to images.
 *
 * Copyright (C) 2012 by Alexander Dymo <adymo at develop dot org>
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

#ifndef DIGIKAM_BQM_COLOR_FX_H
#define DIGIKAM_BQM_COLOR_FX_H

// Local includes

#include "batchtool.h"
#include "colorfxsettings.h"

using namespace Digikam;

namespace DigikamBqmColorFXPlugin
{

class ColorFX : public BatchTool
{
    Q_OBJECT

public:

    explicit ColorFX(QObject* const parent = 0);
    ~ColorFX();

    BatchToolSettings defaultSettings();

    BatchTool* clone(QObject* const parent=0) const { return new ColorFX(parent); };

    void registerSettingsWidget();

private:

    bool toolOperations();

private Q_SLOTS:

    void slotAssignSettings2Widget();
    void slotSettingsChanged();

private:

    ColorFXSettings* m_settingsView;
};

} // namespace DigikamBqmColorFXPlugin

#endif // DIGIKAM_BQM_COLOR_FX_H/