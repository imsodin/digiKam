/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2012-01-13
 * Description : Multithreaded worker objects, working in parallel
 *
 * Copyright (C) 2010-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
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

#ifndef PARALLELWORKERS_H
#define PARALLELWORKERS_H

// Qt includes

#include <QObject>

// Local includes

#include "digikam_export.h"
#include "workerobject.h"

namespace Digikam
{

class DIGIKAM_EXPORT ParallelWorkers
{

public:

    /** ParallelWorkers is a helper class to distribute work over
     *  several identical workers objects.
     *  See ParallelAdapter for guidance how to use it.
     */
    ParallelWorkers();
    virtual ~ParallelWorkers();

    /// The corresponding methods of all added worker objects will be called

    void schedule();
    void deactivate(WorkerObject::DeactivatingMode mode = WorkerObject::FlushSignals);

    void add(WorkerObject* const worker);
    void setPriority(QThread::Priority priority);

    /// Returns true if the current number of added workers has reached the optimalWorkerCount()
    bool optimalWorkerCountReached() const;

    /** Regarding the number of logical CPUs on the current machine,
        returns the optimal count of concurrent workers */
    static int  optimalWorkerCount();

public:

    /// Connects signals outbound from all workers to a given receiver

    bool connect(const char* signal, const QObject* receiver, const char* method,
                 Qt::ConnectionType type = Qt::AutoConnection) const;

protected:

    // Internal implementation

    int ParallelWorkers_qt_metacall(QMetaObject::Call _c, int _id, void** _a);
    virtual QObject* asQObject() = 0;
    virtual int WorkerObject_qt_metacall(QMetaObject::Call _c, int _id, void** _a) = 0;

protected:

    QList<WorkerObject*> m_workers;
    int                  m_currentIndex;
};

// -------------------------------------------------------------------------------------------------

template <class A>

class ParallelAdapter : public A, public ParallelWorkers
{
public:

    /**
     * Instead of using a single WorkerObject, create a ParallelAdapter for
     * your worker object subclass, and add() individual WorkerObjects.
     * The load will be evenly distributed.
     * Note: unlike with WorkerObject directly, there is no need to call schedule().
     * For inbound connections (signals connected to a WorkerObject's slot, to be processed,
     * use a Qt::DirectConnection on the adapter.
     * For outbound connections (signals emitted from the WorkerObject),
     * use ParallelAdapter's connect to have a connection from all added WorkerObjects.
     */

    ParallelAdapter()  {}
    ~ParallelAdapter() {}

    // Internal Implentation

    virtual int qt_metacall(QMetaObject::Call _c, int _id, void** _a)
        { return ParallelWorkers::ParallelWorkers_qt_metacall(_c, _id, _a); }

    int WorkerObject_qt_metacall(QMetaObject::Call _c, int _id, void** _a)
        { return WorkerObject::qt_metacall(_c, _id, _a); }

    virtual QObject* asQObject() { return this; }

    void schedule() { ParallelWorkers::schedule(); }
    void deactivate(WorkerObject::DeactivatingMode mode = WorkerObject::FlushSignals)
        { ParallelWorkers::deactivate(mode); }

    bool connect(const char* signal, const QObject* receiver, const char* method,
                 Qt::ConnectionType type = Qt::AutoConnection) const
        { return ParallelWorkers::connect(signal, receiver, method, type); }
};

} // namespace Digikam

#endif

