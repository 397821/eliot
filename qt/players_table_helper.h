/*****************************************************************************
 * Eliot
 * Copyright (C) 2012 Olivier Teulière
 * Authors: Olivier Teulière <ipkiss @@ gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *****************************************************************************/

#ifndef PLAYERS_TABLE_HELPER_H_
#define PLAYERS_TABLE_HELPER_H_

#include <QtGui/QWidget>
#include <QtGui/QItemDelegate>
#include <QtCore/QString>
#include <QtCore/QList>

#include "logging.h"


class QTableWidget;
class QPushButton;

class PlayersTableHelper : public QObject
{
    Q_OBJECT;
    DEFINE_LOGGER();

public:
    struct PlayerDef
    {
        QString name;
        QString type;
        int level;
    };

    /// Possible values for the player type
    static const char * kHUMAN;
    static const char * kAI;

    PlayersTableHelper(QObject *parent,
                       QTableWidget *tableWidget,
                       QPushButton *addButton = 0,
                       QPushButton *removeButton = 0);

    QList<PlayerDef> getPlayers(bool onlySelected) const;
    void addPlayers(const QList<PlayerDef> &iList);

    void fillWithFavPlayers();
    void saveAsFavPlayers();

    int getRowCount() const;
    void addRow(QString iName, QString iType, QString iLevel);

signals:
    void rowCountChanged();

private slots:
    void enableRemoveButton();
    void removeSelectedRows();
    void addRow();

private:
    QTableWidget *m_tablePlayers;
    QPushButton *m_buttonAdd;
    QPushButton *m_buttonRemove;

};


/// Delegate used for the edition of the players type
class PlayersTypeDelegate: public QItemDelegate
{
    Q_OBJECT;

public:
    explicit PlayersTypeDelegate(QObject *parent = 0);
    virtual ~PlayersTypeDelegate() {}

    // Implement the needed methods
    virtual QWidget *createEditor(QWidget *parent,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;
    virtual void setEditorData(QWidget *editor,
                               const QModelIndex &index) const;
    virtual void setModelData(QWidget *editor,
                              QAbstractItemModel *model,
                              const QModelIndex &index) const;

    virtual void updateEditorGeometry(QWidget *editor,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const;
};


/// Delegate used for the edition of the players level
class PlayersLevelDelegate: public QItemDelegate
{
    Q_OBJECT;

public:
    explicit PlayersLevelDelegate(QObject *parent = 0);
    virtual ~PlayersLevelDelegate() {}

    // Implement the needed methods
    virtual QWidget *createEditor(QWidget *parent,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;
    virtual void setEditorData(QWidget *editor,
                               const QModelIndex &index) const;
    virtual void setModelData(QWidget *editor,
                              QAbstractItemModel *model,
                              const QModelIndex &index) const;

    virtual void updateEditorGeometry(QWidget *editor,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const;
};


/// Event filter used for the edition of the players display
class PlayersEventFilter: public QObject
{
    Q_OBJECT;

public:
    explicit PlayersEventFilter(QObject *parent = 0);
    virtual ~PlayersEventFilter() {}

signals:
    /// As its name indicates...
    void deletePressed();

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event);
};

#endif
