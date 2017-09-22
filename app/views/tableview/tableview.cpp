/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2013-02-11
 * Description : Table view
 *
 * Copyright (C) 2013 by Michael G. Hansen <mike at mghansen dot de>
 * Copyright (C) 2017 by Simon Frei <freisim93 at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "tableview.h"

// Qt includes

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QApplication>
#include <QPointer>

// Local includes

#include "advancedrenamedialog.h"
#include "advancedrenameprocessdialog.h"
#include "album.h"
#include "contextmenuhelper.h"
#include "digikam_debug.h"
#include "fileactionmngr.h"
#include "imageinfo.h"
#include "imageviewutilities.h"
#include "tableview_columnfactory.h"
#include "tableview_model.h"
#include "tableview_selection_model_syncer.h"
#include "tableview_shared.h"
#include "tableview_treeview.h"

namespace Digikam
{

class ImageAlbumModel;
class ImageFilterModel;

class TableView::Private
{
public:

    Private()
      : columnProfiles(),
        thumbnailSize(),
        imageViewUtilities(0),
        headerContextMenuActiveColumn(-1),
        actionHeaderContextMenuRemoveColumn(0),
        actionHeaderContextMenuConfigureColumn(0),
        dragDropThumbnailSize()
    {
    }

    QList<TableViewColumnProfile> columnProfiles;
    ThumbnailSize                 thumbnailSize;
    ImageViewUtilities*           imageViewUtilities;

    int           headerContextMenuActiveColumn;
    QAction*      actionHeaderContextMenuRemoveColumn;
    QAction*      actionHeaderContextMenuConfigureColumn;
    ThumbnailSize dragDropThumbnailSize;
};

TableView::TableView(QItemSelectionModel* const selectionModel,
                     DCategorizedSortFilterProxyModel* const imageFilterModel,
                     QWidget* const parent)
    : QTreeView(parent),
      StateSavingObject(this),
      d(new Private()),
      s(new TableViewShared())
{
    s->isActive                      = false;
    s->tableView                     = this;
    s->thumbnailLoadThread           = new ThumbnailLoadThread(this);
    s->imageFilterModel              = dynamic_cast<ImageFilterModel*>(imageFilterModel);
    s->imageModel                    = dynamic_cast<ImageModel*>(imageFilterModel->sourceModel());
    s->imageFilterSelectionModel     = selectionModel;
    s->columnFactory                 = new TableViewColumnFactory(s.data(), this);

    QVBoxLayout* const vbox1         = new QVBoxLayout();
    s->tableViewModel                = new TableViewModel(s.data(), this);
    s->tableViewSelectionModel       = new QItemSelectionModel(s->tableViewModel);
    s->tableViewSelectionModelSyncer = new TableViewSelectionModelSyncer(s.data(), this);
    setModel(s->tableViewModel);
    setSelectionModel(s->tableViewSelectionModel);

    s->itemDelegate = new TableViewItemDelegate(s, this);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemDelegate(s->itemDelegate);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    setAllColumnsShowFocus(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setWordWrap(true);
//     viewport()->setAcceptDrops(true);

    d->actionHeaderContextMenuRemoveColumn = new QAction(QIcon::fromTheme(QLatin1String("edit-table-delete-column")), i18n("Remove this column"), this);

    connect(d->actionHeaderContextMenuRemoveColumn, SIGNAL(triggered(bool)),
            this, SLOT(slotHeaderContextMenuActionRemoveColumnTriggered()));

    d->actionHeaderContextMenuConfigureColumn = new QAction(QIcon::fromTheme(QLatin1String("configure")), i18n("Configure this column"), this);

    connect(d->actionHeaderContextMenuConfigureColumn, SIGNAL(triggered(bool)),
            this, SLOT(slotHeaderContextMenuConfigureColumn()));

    header()->installEventFilter(this);

    slotModelGroupingModeChanged();

    connect(s->tableViewModel, SIGNAL(signalGroupingModeChanged()),
            this, SLOT(slotModelGroupingModeChanged()));
    installEventFilter(this);

    d->imageViewUtilities            = new ImageViewUtilities(this);

    connect(this, SIGNAL(activated(QModelIndex)),
            this, SLOT(slotItemActivated(QModelIndex)));

    connect(this, SIGNAL(signalZoomInStep()),
            this, SIGNAL(signalZoomInStep()));

    connect(this, SIGNAL(signalZoomOutStep()),
            this, SIGNAL(signalZoomOutStep()));

    connect(s->tableViewSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SIGNAL(signalItemsChanged()));

    connect(this, SIGNAL(collapsed(QModelIndex)),
            this, SIGNAL(signalItemsChanged()));

    connect(this, SIGNAL(expanded(QModelIndex)),
            this, SIGNAL(signalItemsChanged()));

    connect(s->tableViewModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SIGNAL(signalItemsChanged()));

    connect(s->tableViewModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SIGNAL(signalItemsChanged()));

    connect(s->tableViewModel, SIGNAL(layoutChanged()),
            this, SIGNAL(signalItemsChanged()));

    connect(s->tableViewModel, SIGNAL(modelReset()),
            this, SIGNAL(signalItemsChanged()));

    vbox1->addWidget(this);

    setLayout(vbox1);
}

TableView::~TableView()
{
}

void TableView::doLoadState()
{
    const KConfigGroup group                        = getConfigGroup();

    TableViewColumnProfile profile;
    const KConfigGroup groupCurrentProfile          = group.group("Current Profile");
    profile.loadSettings(groupCurrentProfile);
    s->tableViewModel->loadColumnProfile(profile);

    const TableViewModel::GroupingMode groupingMode = TableViewModel::GroupingMode(group.readEntry<int>("Grouping mode",
                                                      int(TableViewModel::GroupingShowSubItems)));
    s->tableViewModel->setGroupingMode(groupingMode);

    if (!profile.headerState.isEmpty())
    {
        header()->restoreState(profile.headerState);
    }
}

void TableView::doSaveState()
{
    KConfigGroup group = getConfigGroup();

    TableViewColumnProfile profile   = s->tableViewModel->getColumnProfile();
    profile.headerState              = header()->saveState();
    KConfigGroup groupCurrentProfile = group.group("Current Profile");
    profile.saveSettings(groupCurrentProfile);
    group.writeEntry("Grouping mode", int(s->tableViewModel->groupingMode()));
}

AbstractItemDragDropHandler* TableViewTreeView::dragDropHandler() const
{
    qCDebug(DIGIKAM_GENERAL_LOG)<<s->imageModel->dragDropHandler();
    return s->imageModel->dragDropHandler();
}

QModelIndex TableViewTreeView::mapIndexForDragDrop(const QModelIndex& index) const
{
    // "index" is a TableViewModel index.
    // We are using the drag-drop-handler of ImageModel, thus
    // we have to convert it to an index of ImageModel.

    // map to ImageModel
    return s->tableViewModel->toImageModelIndex(index);
}

QPixmap TableViewTreeView::pixmapForDrag(const QList< QModelIndex >& indexes) const
{
    const QModelIndex& firstIndex = indexes.at(0);
    const ImageInfo info          = s->tableViewModel->imageInfo(firstIndex);
    const QString path            = info.filePath();

    QPixmap thumbnailPixmap;
    /// @todo The first thumbnail load always fails. We have to add thumbnail pre-generation
    ///       like in ImageModel. Getting thumbnails from ImageModel does not help, because it
    ///       does not necessarily prepare them the same way.
    /// @todo Make a central drag-drop thumbnail generator?
    if (!s->thumbnailLoadThread->find(info.thumbnailIdentifier(), thumbnailPixmap, d->dragDropThumbnailSize.size()))
    {
        /// @todo better default pixmap?
        thumbnailPixmap.fill();
    }

    /// @todo Decorate the pixmap like the other drag-drop implementations?
    /// @todo Write number of images onto the pixmap
    return thumbnailPixmap;

//     const QModelIndex& firstIndex = indexes.at(0);
//     const QModelIndex& imageModelIndex = s->sortModel->toImageModelIndex(firstIndex);
//     ImageModel* const imageModel = s->imageFilterModel->sourceImageModel();
//
//     /// @todo Determine how other views choose the size
//     const QSize thumbnailSize(60, 60);
//
//     imageModel->setData(imageModelIndex, qMax(thumbnailSize.width(), thumbnailSize.height()), ImageModel::ThumbnailRole);
//     QVariant thumbnailData = imageModel->data(imageModelIndex, ImageModel::ThumbnailRole);
//     imageModel->setData(imageModelIndex, QVariant(), ImageModel::ThumbnailRole);
//
//     QPixmap thumbnailPixmap = thumbnailData.value<QPixmap>();
//
//     /// @todo Write number of images onto the pixmap
//     return thumbnailPixmap;
}

void TableViewTreeView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        const int delta = event->delta();

        if (delta > 0)
        {
            emit(signalZoomInStep());
        }
        else if (delta < 0)
        {
            emit(signalZoomOutStep());
        }

        event->accept();
        return;
    }

    QTreeView::wheelEvent(event);
}

void TableView::slotItemActivated(const QModelIndex& tableViewIndex)
{
    const ImageInfo info = s->tableViewModel->imageInfo(tableViewIndex);

    if (info.isNull())
    {
        return;
    }

    if (qApp->queryKeyboardModifiers() != Qt::MetaModifier)
    {
        if (ApplicationSettings::instance()->getItemLeftClickAction() == ApplicationSettings::ShowPreview)
        {
            emit signalPreviewRequested(info);
        }
        else
        {
            d->imageViewUtilities->openInfos(info, allInfo(), currentAlbum());
        }
    }
    else
    {
        d->imageViewUtilities->openInfosWithDefaultApplication(QList<ImageInfo>() << info);
    }
}

void TableView::slotGroupingModeActionTriggered()
{
    const QAction* const senderAction = qobject_cast<QAction*>(sender());

    if (!senderAction)
    {
        return;
    }

    const TableViewModel::GroupingMode newGroupingMode = senderAction->data().value<TableViewModel::GroupingMode>();
    s->tableViewModel->setGroupingMode(newGroupingMode);
}

bool TableView::eventFilter(QObject* watched, QEvent* event)
{
    QHeaderView* const headerView = header();

    if ((watched == headerView) && (event->type() == QEvent::ContextMenu))
    {
        showHeaderContextMenu(event);

        return true;
    }

    // we are looking for context menu events for the table view
    if ((watched == this) && (event->type() == QEvent::ContextMenu))
    {
        QContextMenuEvent* const e = static_cast<QContextMenuEvent*>(event);
        e->accept();

        const QModelIndex contextMenuIndex = indexAt(e->pos());

        if (contextMenuIndex.isValid())
        {
            emit signalShowContextMenuOnInfo(
                        e, s->tableViewModel->imageInfo(contextMenuIndex),
                        getExtraGroupingActions());
        }
        else
        {
            emit signalShowContextMenu(e, getExtraGroupingActions());
        }

        // event has been filtered by us
        return true;
    }

    return QObject::eventFilter(watched, event);
}

void TableView::setThumbnailSize(const ThumbnailSize& size)
{
    d->thumbnailSize                            = size;
    const QList<TableViewColumn*> columnObjects = s->tableViewModel->getColumnObjects();

    Q_FOREACH(TableViewColumn* const iColumn, columnObjects)
    {
        iColumn->updateThumbnailSize();
    }
}

ThumbnailSize TableView::getThumbnailSize() const
{
    return d->thumbnailSize;
}

Album* TableView::currentAlbum() const
{
    ImageAlbumModel* const albumModel = qobject_cast<ImageAlbumModel*>(s->imageModel);

    if (!albumModel)
    {
        return 0;
    }

    if (albumModel->currentAlbums().isEmpty())
    {
        return 0;
    }

    return albumModel->currentAlbums().first();
}

void TableView::slotPaste()
{
    DragDropViewImplementation* const dragDropViewImplementation = s->treeView;
    dragDropViewImplementation->paste();
}

ImageInfo TableView::currentInfo() const
{
    return s->tableViewModel->imageInfo(s->tableViewSelectionModel->currentIndex());
}

ImageInfoList TableView::allInfo(bool grouping) const
{
    ImageInfoList infos = s->tableViewModel->imageInfos(s->tableViewSelectionModel->selectedRows());

    if (grouping)
    {
        return resolveGrouping(infos);
    }

    return infos;
}

void TableView::slotDeleteSelected(const ImageViewUtilities::DeleteMode deleteMode)
{
    const ImageInfoList infoList = selectedImageInfos(true);

    /// @todo Update parameter naming for deleteImages
    if (d->imageViewUtilities->deleteImages(infoList, deleteMode))
    {
        slotAwayFromSelection();
    }
}

void TableView::slotDeleteSelectedWithoutConfirmation(const ImageViewUtilities::DeleteMode deleteMode)
{
    const ImageInfoList infoList = selectedImageInfos(true);

    d->imageViewUtilities->deleteImagesDirectly(infoList, deleteMode);
    slotAwayFromSelection();
}

QList<QAction*> TableView::getExtraGroupingActions()
{
    QList<QAction*> actionList;

    const TableViewModel::GroupingMode currentGroupingMode = s->tableViewModel->groupingMode();

    QAction* const actionHideGrouped = new QAction(i18n("Hide grouped items"), this);
    actionHideGrouped->setCheckable(true);
    actionHideGrouped->setChecked(currentGroupingMode == TableViewModel::GroupingHideGrouped);
    actionHideGrouped->setData(QVariant::fromValue<TableViewModel::GroupingMode>(TableViewModel::GroupingHideGrouped));

    connect(actionHideGrouped, SIGNAL(triggered(bool)),
            this, SLOT(slotGroupingModeActionTriggered()));

    actionList << actionHideGrouped;

    QAction* const actionIgnoreGrouping = new QAction(i18n("Ignore grouping"), this);
    actionIgnoreGrouping->setCheckable(true);
    actionIgnoreGrouping->setChecked(currentGroupingMode == TableViewModel::GroupingIgnoreGrouping);
    actionIgnoreGrouping->setData(QVariant::fromValue<TableViewModel::GroupingMode>(TableViewModel::GroupingIgnoreGrouping));

    connect(actionIgnoreGrouping, SIGNAL(triggered(bool)),
            this, SLOT(slotGroupingModeActionTriggered()));

    actionList << actionIgnoreGrouping;

    QAction* const actionShowSubItems = new QAction(i18n("Show grouping in tree"), this);
    actionShowSubItems->setCheckable(true);
    actionShowSubItems->setChecked(currentGroupingMode == TableViewModel::GroupingShowSubItems);
    actionShowSubItems->setData(QVariant::fromValue<TableViewModel::GroupingMode>(TableViewModel::GroupingShowSubItems));

    connect(actionShowSubItems, SIGNAL(triggered(bool)),
            this, SLOT(slotGroupingModeActionTriggered()));

    actionList << actionShowSubItems;

    return actionList;
}

QList<QUrl> TableView::allUrls(bool grouping) const
{
    const ImageInfoList infos = allInfo(grouping);
    QList<QUrl> resultList;

    Q_FOREACH(const ImageInfo& info, infos)
    {
        resultList << info.fileUrl();
    }

    return resultList;
}

int TableView::numberOfSelectedItems() const
{
    return s->tableViewSelectionModel->selectedRows().count();
}

void TableView::slotGoToRow(const int rowNumber, const bool relativeMove)
{
    int nextDeepRowNumber = rowNumber;

    if (relativeMove)
    {
        const QModelIndex currentTableViewIndex = s->tableViewSelectionModel->currentIndex();
        const int currentDeepRowNumber          = s->tableViewModel->indexToDeepRowNumber(currentTableViewIndex);
        nextDeepRowNumber                      += currentDeepRowNumber;
    }

    const QModelIndex nextIndex = s->tableViewModel->deepRowIndex(nextDeepRowNumber);

    if (nextIndex.isValid())
    {
        const QItemSelection rowSelection = s->tableViewSelectionModelSyncer->targetIndexToRowItemSelection(nextIndex);
        s->tableViewSelectionModel->select(rowSelection, QItemSelectionModel::ClearAndSelect);
        s->tableViewSelectionModel->setCurrentIndex(nextIndex, QItemSelectionModel::Select);
    }
}

ImageInfo TableView::deepRowImageInfo(const int rowNumber, const bool relative) const
{
    int targetRowNumber = rowNumber;

    if (relative)
    {
        const QModelIndex& currentTableViewIndex = s->tableViewSelectionModel->currentIndex();

        if (!currentTableViewIndex.isValid())
        {
            return ImageInfo();
        }

        const int currentDeepRowNumber = s->tableViewModel->indexToDeepRowNumber(currentTableViewIndex);
        targetRowNumber               += currentDeepRowNumber;
    }

    const QModelIndex targetIndex = s->tableViewModel->deepRowIndex(targetRowNumber);
    return s->tableViewModel->imageInfo(targetIndex);
}

ImageInfo TableView::nextInfo() const
{
    const QModelIndex cIndex       = s->tableViewSelectionModel->currentIndex();
    const int currentDeepRowNumber = s->tableViewModel->indexToDeepRowNumber(cIndex);
    const int nextDeepRowNumber    = currentDeepRowNumber + 1;

    if (nextDeepRowNumber>=s->tableViewModel->deepRowCount())
    {
        return ImageInfo();
    }

    const QModelIndex nextDeepRowIndex = s->tableViewModel->deepRowIndex(nextDeepRowNumber);
    return s->tableViewModel->imageInfo(nextDeepRowIndex);
}

ImageInfo TableView::previousInfo() const
{
    const QModelIndex cIndex        = s->tableViewSelectionModel->currentIndex();
    const int currentDeepRowNumber  = s->tableViewModel->indexToDeepRowNumber(cIndex);
    const int previousDeepRowNumber = currentDeepRowNumber - 1;

    if (previousDeepRowNumber < 0)
    {
        return ImageInfo();
    }

    const QModelIndex previousDeepRowIndex = s->tableViewModel->deepRowIndex(previousDeepRowNumber);
    return s->tableViewModel->imageInfo(previousDeepRowIndex);
}

void TableView::slotSetCurrentWhenAvailable(const qlonglong id)
{
    const QModelIndex idx = s->tableViewModel->indexFromImageId(id, 0);

    if (!idx.isValid())
    {
        /// @todo Actually buffer this request until the model is fully populated
        return;
    }

    s->tableViewSelectionModel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
}

/**
 * @brief Unselects the current selection and changes the current item
 *
 * @todo This may not work correctly if grouped items are deleted, but are not selected
 */
void TableView::slotAwayFromSelection()
{
    QModelIndexList selection = s->tableViewSelectionModel->selectedRows(0);

    if (selection.isEmpty())
    {
        return;
    }

    const QModelIndex firstIndex = s->tableViewModel->deepRowIndex(0);
    const QModelIndex lastIndex  = s->tableViewModel->deepRowIndex(-1);

    if (selection.contains(firstIndex) && selection.contains(lastIndex))
    {
        // both the first and the last index are selected, we have to
        // select an index inbetween
        const int nextFreeDeepRow = s->tableViewModel->firstDeepRowNotInList(selection);

        if (nextFreeDeepRow < 0)
        {
            s->tableViewSelectionModel->clearSelection();
            s->tableViewSelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::ClearAndSelect);
        }
        else
        {
            const QModelIndex nextFreeIndex         = s->tableViewModel->deepRowIndex(nextFreeDeepRow);
            s->tableViewSelectionModel->setCurrentIndex(nextFreeIndex, QItemSelectionModel::ClearAndSelect);
            const QItemSelection nextFreeIndexAsRow = s->tableViewSelectionModelSyncer->targetIndexToRowItemSelection(nextFreeIndex);
            s->tableViewSelectionModel->select(nextFreeIndexAsRow, QItemSelectionModel::ClearAndSelect);
        }
    }
    else if (selection.contains(lastIndex))
    {
        const int firstSelectedRowNumber   = s->tableViewModel->indexToDeepRowNumber(selection.first());
        const QModelIndex newIndex         = s->tableViewModel->deepRowIndex(firstSelectedRowNumber-1);
        s->tableViewSelectionModel->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect);
        const QItemSelection newIndexAsRow = s->tableViewSelectionModelSyncer->targetIndexToRowItemSelection(newIndex);
        s->tableViewSelectionModel->select(newIndexAsRow, QItemSelectionModel::ClearAndSelect);
    }
    else
    {
        const int lastSelectedRowNumber    = s->tableViewModel->indexToDeepRowNumber(selection.last());
        const QModelIndex newIndex         = s->tableViewModel->deepRowIndex(lastSelectedRowNumber+1);
        s->tableViewSelectionModel->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect);
        const QItemSelection newIndexAsRow = s->tableViewSelectionModelSyncer->targetIndexToRowItemSelection(newIndex);
        s->tableViewSelectionModel->select(newIndexAsRow, QItemSelectionModel::ClearAndSelect);
    }
}

void TableView::clearSelection()
{
    s->tableViewSelectionModel->clearSelection();
}

void TableView::invertSelection()
{
    const int deepRowCount = s->tableViewModel->deepRowCount();
    QList<int> rowsToSelect;
    int lastSelectedRow    = -1;

    /// @todo Create a DeepRowIterator because there is a lot of overhead here

    for (int i = 0; i < deepRowCount; ++i)
    {
        const QModelIndex iIndex = s->tableViewModel->deepRowIndex(i);

        if (s->tableViewSelectionModel->isSelected(iIndex))
        {
            if (i - 1 > lastSelectedRow)
            {
                for (int j = lastSelectedRow + 1; j < i; ++j)
                {
                    rowsToSelect << j;
                }
            }

            lastSelectedRow = i;
        }
    }

    if (lastSelectedRow + 1 < deepRowCount)
    {
        for (int j = lastSelectedRow + 1; j < deepRowCount; ++j)
        {
            rowsToSelect << j;
        }
    }

    s->tableViewSelectionModel->clearSelection();

    Q_FOREACH(const int i, rowsToSelect)
    {
        const QModelIndex iIndex = s->tableViewModel->deepRowIndex(i);
        const QItemSelection is  = s->tableViewSelectionModelSyncer->targetIndexToRowItemSelection(iIndex);
        s->tableViewSelectionModel->select(is, QItemSelectionModel::Select);
    }
}

void TableView::selectAll()
{
    /// @todo This only selects expanded items.
    selectAll();
}

void TableView::slotSetActive(const bool isActive)
{
    if (s->isActive != isActive)
    {
        s->isActive = isActive;
        s->tableViewModel->slotSetActive(isActive);
        s->tableViewSelectionModelSyncer->slotSetActive(isActive);
    }
}

ImageInfoList TableView::selectedImageInfos(bool grouping) const
{
    ImageInfoList infos = s->tableViewModel->imageInfos(s->tableViewSelectionModel->selectedRows());

    if (grouping) {
        return resolveGrouping(infos);
    }

    return infos;
}

ImageInfoList TableView::selectedImageInfos(ApplicationSettings::OperationType type) const
{
    return selectedImageInfos(needGroupResolving(type));
}

QModelIndexList TableView::selectedIndexesCurrentFirst() const
{
    QModelIndexList indexes   = s->tableViewSelectionModel->selectedRows();
    const QModelIndex current = s->tableViewModel->toCol0(s->tableViewSelectionModel->currentIndex());

    if (!indexes.isEmpty())
    {
        if (indexes.first() != current)
        {
            if (indexes.removeOne(current))
            {
                indexes.prepend(current);
            }
        }
    }

    return indexes;
}

ImageInfoList TableView::selectedImageInfosCurrentFirst(bool grouping) const
{
    ImageInfoList infos = s->tableViewModel->imageInfos(selectedIndexesCurrentFirst());

    if (grouping) {
        return resolveGrouping(infos);
    }

    return infos;
}

QList<qlonglong> TableView::selectedImageIdsCurrentFirst(bool grouping) const
{
    return selectedImageInfosCurrentFirst(grouping).toImageIdList();
}

QList<QUrl> TableView::selectedUrls(bool grouping) const
{
    return selectedImageInfos(grouping).toImageUrlList();
}

ImageInfoList TableView::resolveGrouping(const ImageInfoList& infos) const
{
    ImageInfoList out;

    foreach(const ImageInfo& info, infos)
    {
        QModelIndex index = s->tableViewModel->indexFromImageId(info.id(), 0);

        out << info;

        if (info.hasGroupedImages()
            && (s->tableViewModel->groupingMode() == s->tableViewModel->GroupingMode::GroupingHideGrouped
                || (s->tableViewModel->groupingMode() == s->tableViewModel->GroupingMode::GroupingShowSubItems
                    && !isExpanded(index))))
        {
            out << info.groupedImages();
        }
    }

    return out;
}

bool TableView::needGroupResolving(ApplicationSettings::OperationType type, bool all) const
{
    ApplicationSettings::ApplyToEntireGroup applyAll =
            ApplicationSettings::instance()->getGroupingOperateOnAll(type);

    if (applyAll == ApplicationSettings::No)
    {
        return false;
    }
    else if (applyAll == ApplicationSettings::Yes)
    {
        return true;
    }

    ImageInfoList infos;

    if (all)
    {
        infos = s->tableViewModel->allImageInfo();
    }
    else
    {
        infos = s->tableViewModel->imageInfos(s->tableViewSelectionModel->selectedRows());
    }

    foreach(const ImageInfo& info, infos)
    {
        QModelIndex index = s->tableViewModel->indexFromImageId(info.id(), 0);

        if (info.hasGroupedImages()
            && (s->tableViewModel->groupingMode() == s->tableViewModel->GroupingMode::GroupingHideGrouped
                || (s->tableViewModel->groupingMode() == s->tableViewModel->GroupingMode::GroupingShowSubItems
                    && !isExpanded(index))))
        {
            // Ask whether should be performed on all and return info if no
            return ApplicationSettings::instance()->askGroupingOperateOnAll(type);
        }
    }

    return false;
}

void TableView::rename()
{
    bool grouping     = needGroupResolving(ApplicationSettings::Rename);
    QList<QUrl>  urls = selectedUrls(grouping);
    NewNamesList newNamesList;

    qCDebug(DIGIKAM_GENERAL_LOG) << "Selected URLs to rename: " << urls;

    QPointer<AdvancedRenameDialog> dlg = new AdvancedRenameDialog(this);
    dlg->slotAddImages(urls);

    if (dlg->exec() == QDialog::Accepted)
    {
        newNamesList = dlg->newNames();

        slotAwayFromSelection();
    }

    delete dlg;

    if (!newNamesList.isEmpty())
    {
        QPointer<AdvancedRenameProcessDialog> dlg = new AdvancedRenameProcessDialog(newNamesList);
        dlg->exec();
        delete dlg;
    }
}

void TableView::addColumnDescriptionsToMenu(const QList<TableViewColumnDescription>& columnDescriptions, QMenu* const menu)
{
    for (int i = 0; i < columnDescriptions.count(); ++i)
    {
        const TableViewColumnDescription& desc = columnDescriptions.at(i);
        QAction* const action                  = new QAction(desc.columnTitle, menu);

        if (!desc.columnIcon.isEmpty())
        {
            action->setIcon(QIcon::fromTheme(desc.columnIcon));
        }

        if (desc.subColumns.isEmpty())
        {
            connect(action, SIGNAL(triggered(bool)),
                    this, SLOT(slotHeaderContextMenuAddColumn()));

            action->setData(QVariant::fromValue<TableViewColumnDescription>(desc));
        }
        else
        {
            QMenu* const subMenu = new QMenu(menu);
            addColumnDescriptionsToMenu(desc.subColumns, subMenu);

            action->setMenu(subMenu);
        }

        menu->addAction(action);
    }
}

void TableView::showHeaderContextMenu(QEvent* const event)
{
    QContextMenuEvent* const e                = static_cast<QContextMenuEvent*>(event);
    QHeaderView* const headerView             = header();
    d->headerContextMenuActiveColumn          = headerView->logicalIndexAt(e->pos());
    const TableViewColumn* const columnObject = s->tableViewModel->getColumnObject(d->headerContextMenuActiveColumn);
    QMenu* const menu                         = new QMenu(this);

    d->actionHeaderContextMenuRemoveColumn->setEnabled(s->tableViewModel->columnCount(QModelIndex())>1);
    menu->addAction(d->actionHeaderContextMenuRemoveColumn);
    const bool columnCanConfigure = columnObject->getColumnFlags().testFlag(TableViewColumn::ColumnHasConfigurationWidget);
    d->actionHeaderContextMenuConfigureColumn->setEnabled(columnCanConfigure);
    menu->addAction(d->actionHeaderContextMenuConfigureColumn);
    menu->addSeparator();

    // add actions for all columns
    QList<TableViewColumnDescription> columnDescriptions = s->columnFactory->getColumnDescriptionList();
    addColumnDescriptionsToMenu(columnDescriptions, menu);

    menu->exec(e->globalPos());
}

void TableView::slotHeaderContextMenuAddColumn()
{
    QAction* const triggeredAction = qobject_cast<QAction*>(sender());

    const QVariant actionData = triggeredAction->data();

    if (!actionData.canConvert<TableViewColumnDescription>())
    {
        return;
    }

    const TableViewColumnDescription desc = actionData.value<TableViewColumnDescription>();
    qCDebug(DIGIKAM_GENERAL_LOG) << "clicked: " << desc.columnTitle;
    const int newColumnLogicalIndex       = d->headerContextMenuActiveColumn+1;
    s->tableViewModel->addColumnAt(desc, newColumnLogicalIndex);

    // since the header column order is not the same as the model's column order, we need
    // to make sure the new column is moved directly behind the current column in the header:
    const int clickedVisualIndex   = header()->visualIndex(d->headerContextMenuActiveColumn);
    const int newColumnVisualIndex = header()->visualIndex(newColumnLogicalIndex);
    int newColumnVisualTargetIndex = clickedVisualIndex + 1;

    // If the column is inserted before the clicked column, we have to
    // subtract one from the target index because it looks like QHeaderView first removes
    // the column and then inserts it.
    if (newColumnVisualIndex < clickedVisualIndex)
    {
        newColumnVisualTargetIndex--;
    }

    if (newColumnVisualIndex!=newColumnVisualTargetIndex)
    {
        header()->moveSection(newColumnVisualIndex, newColumnVisualTargetIndex);
    }

    // Ensure that the newly created column is visible.
    // This is especially important if the new column is the last one,
    // because then it can be outside of the viewport.
    const QModelIndex topIndex = indexAt(QPoint(0, 0));
    const QModelIndex targetIndex = s->tableViewModel->index(topIndex.row(), newColumnLogicalIndex, topIndex.parent());
    scrollTo(targetIndex, EnsureVisible);
}

void TableView::slotHeaderContextMenuConfigureColumn()
{
    TableViewConfigurationDialog* const configurationDialog = new TableViewConfigurationDialog(s, d->headerContextMenuActiveColumn, this);
    const int result                                        = configurationDialog->exec();

    if (result!=QDialog::Accepted)
    {
        return;
    }

    const TableViewColumnConfiguration newConfiguration = configurationDialog->getNewConfiguration();
    s->tableViewModel->getColumnObject(d->headerContextMenuActiveColumn)->setConfiguration(newConfiguration);
}

void TableView::slotHeaderContextMenuActionRemoveColumnTriggered()
{
    qCDebug(DIGIKAM_GENERAL_LOG) << "remove column " << d->headerContextMenuActiveColumn;
    s->tableViewModel->removeColumnAt(d->headerContextMenuActiveColumn);
}

void TableView::slotModelGroupingModeChanged()
{
    setRootIsDecorated(s->tableViewModel->groupingMode()==TableViewModel::GroupingShowSubItems);
}

} // namespace Digikam
