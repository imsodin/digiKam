/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2009-04-16
 * Description : Qt Model for Albums - drag and drop handling
 *
 * Copyright (C) 2002-2005 by Renchi Raju <renchi dot raju at gmail dot com>
 * Copyright (C) 2002-2017 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * Copyright (C) 2006-2011 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * Copyright (C) 2009      by Andi Clemens <andi dot clemens at gmail dot com>
 * Copyright (C) 2013      by Michael G. Hansen <mike at mghansen dot de>
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

#include "imagedragdrop.h"

// Qt includes

#include <QDropEvent>
#include <QMenu>
#include <QIcon>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "albummanager.h"
#include "importui.h"
#include "importiconview.h"
#include "imagethumbnailbar.h"
#include "ddragobjects.h"
#include "dio.h"
#include "imagecategorizedview.h"
#include "imageinfo.h"
#include "imageinfolist.h"
#include "tableview_treeview.h"

namespace Digikam
{

enum DropAction
{
    NoAction,
    CopyAction,
    MoveAction,
    GroupAction,
    GroupAndMoveAction,
    AssignTagAction
};

static QAction* addGroupAction(QMenu* const menu)
{
    return menu->addAction( QIcon::fromTheme(QLatin1String("go-bottom")), i18nc("@action:inmenu Group images with this image", "Group here"));
}

static QAction* addGroupAndMoveAction(QMenu* const menu)
{
    return menu->addAction( QIcon::fromTheme(QLatin1String("go-bottom")), i18nc("@action:inmenu Group images with this image and move them to its album", "Group here and move to album"));
}

static QAction* addCancelAction(QMenu* const menu)
{
    return menu->addAction( QIcon::fromTheme(QLatin1String("dialog-cancel")), i18n("C&ancel") );
}

static DropAction copyOrMove(const QDropEvent* const e, QWidget* const view, bool allowMove = true, bool askForGrouping = false)
{
    if (e->keyboardModifiers() & Qt::ControlModifier)
    {
        return CopyAction;
    }
    else if (e->keyboardModifiers() & Qt::ShiftModifier)
    {
        return MoveAction;
    }

    if (!allowMove && !askForGrouping)
    {
        switch (e->proposedAction())
        {
            case Qt::CopyAction:
                return CopyAction;
            case Qt::MoveAction:
                return MoveAction;
            default:
                return NoAction;
        }
    }

    QMenu popMenu(view);

    QAction* moveAction = 0;

    if (allowMove)
    {
        moveAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("go-jump")), i18n("&Move Here"));
    }

    QAction* const copyAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("edit-copy")), i18n("&Copy Here"));
    popMenu.addSeparator();

    QAction* groupAction        = 0;
    QAction* groupAndMoveAction = 0;

    if (askForGrouping)
    {
        groupAction        = addGroupAction(&popMenu);
        groupAndMoveAction = addGroupAndMoveAction(&popMenu);
        popMenu.addSeparator();
    }

    addCancelAction(&popMenu);

    popMenu.setMouseTracking(true);
    QAction* const choice = popMenu.exec(QCursor::pos());

    if (moveAction && choice == moveAction)
    {
        return MoveAction;
    }
    else if (choice == copyAction)
    {
        return CopyAction;
    }
    else if (groupAction && choice == groupAction)
    {
        return GroupAction;
    }
    else if (groupAndMoveAction && choice == groupAndMoveAction)
    {
        return GroupAndMoveAction;
    }

    return NoAction;
}

static DropAction tagAction(const QDropEvent* const, QWidget* const view, bool askForGrouping)
{
    QMenu popMenu(view);
    QAction* const tagAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("tag")), i18n("Assign Tag to Dropped Items"));
    QAction* groupAction     = 0;

    if (askForGrouping)
    {
        popMenu.addSeparator();
        groupAction = addGroupAction(&popMenu);
    }

    popMenu.addSeparator();
    addCancelAction(&popMenu);

    popMenu.setMouseTracking(true);
    QAction* const choice = popMenu.exec(QCursor::pos());

    if (groupAction && choice == groupAction)
    {
        return GroupAction;
    }
    else if (tagAction && choice == tagAction)
    {
        return AssignTagAction;
    }

    return NoAction;
}

static DropAction groupAction(const QDropEvent* const, QWidget* const view)
{
    QMenu popMenu(view);
    QAction* const groupAction = addGroupAction(&popMenu);
    popMenu.addSeparator();
    addCancelAction(&popMenu);

    QAction* const choice      = popMenu.exec(QCursor::pos());

    if (groupAction && choice == groupAction)
    {
        return GroupAction;
    }

    return NoAction;
}

// -------------------------------------------------------------------------------------

ImageDragDropHandler::ImageDragDropHandler(ImageModel* const model)
    : AbstractItemDragDropHandler(model),
      m_readOnly(false)
{
}

ImageModel* ImageDragDropHandler::model() const
{
    return static_cast<ImageModel*>(m_model);
}

ImageAlbumModel* ImageDragDropHandler::albumModel() const
{
    return qobject_cast<ImageAlbumModel*>(model());
}

void ImageDragDropHandler::setReadOnlyDrop(bool readOnly)
{
    m_readOnly = readOnly;
}

bool ImageDragDropHandler::dropEvent(QAbstractItemView* abstractview, const QDropEvent* e, const QModelIndex& droppedOn)
{
    Album* album = 0;

    // Note that the drop event does not have to be in an ImageCategorizedView.
    // It can also be a TableViewTreeView.
    ImageCategorizedView* const view = qobject_cast<ImageCategorizedView*>(abstractview);

    if (view)
    {
        album = view->albumAt(e->pos());
    }
    else
    {
        TableViewTreeView* const tableViewTreeView = qobject_cast<TableViewTreeView*>(abstractview);

        if (tableViewTreeView)
        {
            album = tableViewTreeView->albumAt(e->pos());
        }
    }

    // unless we are readonly anyway, we always want an album
    if ( !m_readOnly && (!album || album->isRoot()) )
    {
        return false;
    }

    PAlbum* palbum = 0;
    TAlbum* talbum = 0;

    if (album)
    {
        Album* currentAlbum = 0;

        if(albumModel() && !(albumModel()->currentAlbums().isEmpty()))
            currentAlbum = albumModel()->currentAlbums().first();

        if (album->type() == Album::PHYSICAL)
        {
            palbum = static_cast<PAlbum*>(album);
        }
        else if (currentAlbum && currentAlbum->type() == Album::PHYSICAL)
        {
            palbum = static_cast<PAlbum*>(currentAlbum);
        }

        if (album->type() == Album::TAG)
        {
            talbum = static_cast<TAlbum*>(album);
        }
        else if (currentAlbum && currentAlbum->type() == Album::TAG)
        {
            talbum = static_cast<TAlbum*>(currentAlbum);
        }
    }

    if (DItemDrag::canDecode(e->mimeData()))
    {
        // Drag & drop inside of digiKam
        QList<QUrl>      urls;
        QList<int>       albumIDs;
        QList<qlonglong> imageIDs;

        if (!DItemDrag::decode(e->mimeData(), urls, albumIDs, imageIDs))
        {
            return false;
        }

        if (m_readOnly)
        {
            emit imageInfosDropped(ImageInfoList(imageIDs));
            return true;
        }

        // ---------------------------------------------------------------------
        // Determine ImageInfo from the index where the drop happened

        ImageInfo droppedOnInfo;

        if (droppedOn.isValid())
        {
            ImageThumbnailBar* const thumbBar = qobject_cast<ImageThumbnailBar*>(abstractview);

            if (thumbBar)
            {
                droppedOnInfo = model()->imageInfo(thumbBar->imageFilterModel()->mapToSourceImageModel(droppedOn));
            }
            else
            {
                droppedOnInfo = model()->imageInfo(droppedOn);
            }
        }

        // ---------------------------------------------------------------------
        // Determine which action (if any) is to be performed and return early
        // if nothing else is to be done

        DropAction action = NoAction;

        // Check if items dropped come from outside current album.
        QList<ImageInfo> extImages, intImages;
        bool onlyInternal;

        if (palbum)
        {
            // Check for drop of image on itself
            if (imageIDs.size() == 1 && ImageInfo(imageIDs.first()) == droppedOnInfo)
            {
                return false;
            }

            for (QList<qlonglong>::const_iterator it = imageIDs.constBegin(); it != imageIDs.constEnd(); ++it)
            {
                ImageInfo info(*it);

                if (info.albumId() != album->id())
                {
                    extImages << info;
                }
                else
                {
                    intImages << info;
                }
            }

            // Check for drop of image on itself
            if (intImages.size() == 1 && intImages.first() == droppedOnInfo)
            {
                return false;
            }

            onlyInternal = (!intImages.isEmpty() && extImages.isEmpty());

            if (droppedOn.isValid() && onlyInternal)
            {
                action = groupAction(e, view);
            }
            else
            {
                // Determine action. Show Menu only if there are any album-external items.
                // Ask for grouping if dropped-on is valid (gives LinkAction)
                action = copyOrMove(e, view, !extImages.isEmpty(), !droppedOnInfo.isNull());
            }
        }
        else if (talbum)
        {
            action = tagAction(e, view, droppedOn.isValid());
        }
        else
        {
            if (droppedOn.isValid())
            {
                // Ask if the user wants to group
                action = groupAction(e, view);
            }
        }

        // ---------------------------------------------------------------------
        // Perform whatever action has been chosen above

        switch (action)
        {
            case AssignTagAction:
                emit assignTags(ImageInfoList(imageIDs), QList<int>() << talbum->id());
                return true;
            case GroupAction:
                if (droppedOnInfo.isNull())
                {
                    return false;
                }
                emit addToGroup(droppedOnInfo, ImageInfoList(imageIDs));
                return true;
            case GroupAndMoveAction:
                if (droppedOnInfo.isNull())
                {
                    return false;
                }
                emit addToGroup(droppedOnInfo, ImageInfoList(imageIDs));
                DIO::move(ImageInfoList(imageIDs), palbum);
                return true;
            case CopyAction:
                // We can copy regardless of where the files are from
                DIO::copy(extImages+intImages, palbum);
                return true;
            case MoveAction:
                if (onlyInternal)
                {
                    // Only items from the current album:
                    // Move is a no-op.
                    return false;
                }
                // Ignored internal images when moving
                DIO::move(extImages, palbum);
                return true;
        }

        return false;
    }

    if (e->mimeData()->hasUrls())
    {
        if (!palbum && !m_readOnly)
        {
            return false;
        }

        // Drag & drop outside of digiKam

        QList<QUrl> srcURLs = e->mimeData()->urls();

        if (m_readOnly)
        {
            emit urlsDropped(srcURLs);
            return true;
        }

        DropAction action = copyOrMove(e, view);

        if (action == MoveAction)
        {
            DIO::move(srcURLs, palbum);
        }
        else if (action == CopyAction)
        {
            DIO::copy(srcURLs, palbum);
        }
        else
        {
            return false;
        }

        return true;
    }

    if (DTagListDrag::canDecode(e->mimeData()))
    {
        QList<int> tagIDs;
        bool isDecoded = DTagListDrag::decode(e->mimeData(), tagIDs);

        if (!isDecoded)
        {
            qCDebug(DIGIKAM_GENERAL_LOG) << "Error: Decoding failed!";
            return false;
        }

        QMenu popMenu(view);

        QList<ImageInfo> selectedInfos  = view->selectedImageInfosCurrentFirst();
        QAction* assignToSelectedAction = 0;

        if (selectedInfos.count() > 1)
        {
            assignToSelectedAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("tag")), i18n("Assign Tags to &Selected Items"));
        }

        QAction* assignToThisAction = 0;

        if (droppedOn.isValid())
        {
            assignToThisAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("tag")), i18n("Assign Tags to &This Item"));
        }

        QAction* const assignToAllAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("tag")), i18n("Assign Tags to &All Items"));

        popMenu.addSeparator();
        popMenu.addAction(QIcon::fromTheme(QLatin1String("dialog-cancel")), i18n("&Cancel"));

        popMenu.setMouseTracking(true);
        QAction* const choice = popMenu.exec(view->mapToGlobal(e->pos()));

        if (choice)
        {
            if (choice == assignToSelectedAction)    // Selected Items
            {
                emit assignTags(selectedInfos, tagIDs);
            }
            else if (choice == assignToAllAction)    // All Items
            {
                emit assignTags(view->allImageInfos(), tagIDs);
            }
            else if (choice == assignToThisAction)    // Dropped item only.
            {
                emit assignTags(QList<ImageInfo>() << model()->imageInfo(droppedOn), tagIDs);
            }
        }

        return true;
    }

    if (DCameraItemListDrag::canDecode(e->mimeData()))
    {
        if (!palbum)
        {
            return false;
        }

        ImportIconView* const iconView = dynamic_cast<ImportIconView*>(e->source());

        if (!iconView)
        {
            return false;
        }

        QMenu popMenu(view);
        popMenu.addSection(QIcon::fromTheme(QLatin1String("digikam")), i18n("Importing"));
        QAction* const downAction    = popMenu.addAction(QIcon::fromTheme(QLatin1String("get-hot-new-stuff")),
                                                         i18n("Download From Camera"));
        QAction* const downDelAction = popMenu.addAction(QIcon::fromTheme(QLatin1String("get-hot-new-stuff")),
                                                         i18n("Download && Delete From Camera"));
        popMenu.addSeparator();
        popMenu.addAction(QIcon::fromTheme(QLatin1String("dialog-cancel")), i18n("C&ancel"));
        popMenu.setMouseTracking(true);
        QAction* const choice        = popMenu.exec(view->mapToGlobal(e->pos()));

        if (choice)
        {
            if (choice == downAction)
            {
                ImportUI::instance()->slotDownload(true, false, palbum);
            }
            else if (choice == downDelAction)
            {
                ImportUI::instance()->slotDownload(true, true, palbum);
            }
        }

        return true;
    }

    return false;
}

Qt::DropAction ImageDragDropHandler::accepts(const QDropEvent* e, const QModelIndex& /*dropIndex*/)
{
    if (albumModel() && albumModel()->currentAlbums().isEmpty())
    {
        return Qt::IgnoreAction;
    }

    if (DItemDrag::canDecode(e->mimeData()) || e->mimeData()->hasUrls())
    {
        if (e->keyboardModifiers() & Qt::ControlModifier)
        {
            return Qt::CopyAction;
        }
        else if (e->keyboardModifiers() & Qt::ShiftModifier)
        {
            return Qt::MoveAction;
        }

        return Qt::MoveAction;
    }

    if (DTagListDrag::canDecode(e->mimeData())        ||
        DCameraItemListDrag::canDecode(e->mimeData()) ||
        DCameraDragObject::canDecode(e->mimeData()))
    {
        return Qt::MoveAction;
    }

    return Qt::IgnoreAction;
}

QStringList ImageDragDropHandler::mimeTypes() const
{
    QStringList mimeTypes;

    mimeTypes << DItemDrag::mimeTypes()
              << DTagListDrag::mimeTypes()
              << DCameraItemListDrag::mimeTypes()
              << DCameraDragObject::mimeTypes()
              << QLatin1String("text/uri-list");

    return mimeTypes;
}

QMimeData* ImageDragDropHandler::createMimeData(const QList<QModelIndex>& indexes)
{
    QList<ImageInfo> infos = model()->imageInfos(indexes);

    QList<QUrl>      urls;
    QList<int>       albumIDs;
    QList<qlonglong> imageIDs;

    foreach(const ImageInfo& info, infos)
    {
        urls.append(info.fileUrl());
        albumIDs.append(info.albumId());
        imageIDs.append(info.id());
    }

    if (urls.isEmpty())
    {
        return 0;
    }

    return new DItemDrag(urls, albumIDs, imageIDs);
}

} // namespace Digikam
