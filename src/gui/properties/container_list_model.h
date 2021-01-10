#pragma once

#include <QAbstractListModel>

#include "container_model.h"

namespace PropertiesUI
{
    class ContainerListModel : public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int size READ size NOTIFY sizeChanged)

      public:
        enum class Role
        {
            ItemModel = Qt::UserRole + 1
        };

        ContainerListModel(QObject *parent = 0);

        void clear();
        void refresh(int index);
        void refresh(ContainerModel *model);

        void addItemModel(ContainerModel *model);
        void remove(int index);
        void remove(ContainerModel *model);

        std::vector<ContainerModel *>::iterator find(const ContainerModel *model);

        int rowCount(const QModelIndex &parent = QModelIndex()) const;
        int size();

        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

      signals:
        void sizeChanged(int size);

      protected:
        QHash<int, QByteArray> roleNames() const;

      private:
        std::vector<ContainerModel *> itemModels;
    };
} // namespace PropertiesUI