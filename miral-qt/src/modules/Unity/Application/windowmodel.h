/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WINDOWMODEL_H
#define WINDOWMODEL_H

#include <QAbstractListModel>

#include "mirsurfaceinterface.h"
#include "windowmodelinterface.h"

namespace qtmir {

class WindowModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        SurfaceRole = Qt::UserRole
    };

    WindowModel();

    // QAbstractItemModel methods
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;

    int count() const { return rowCount(); }

Q_SIGNALS:
    void countChanged();

private Q_SLOTS:
    void onWindowAdded(const NumberedWindow);
    void onWindowRemoved(const unsigned int index);
    void onWindowChanged(const DirtiedWindow);

private:
    QVector<MirSurfaceInterface *> m_windowModel;
};

} // namespace qtmir
#endif // WINDOWMODEL_H