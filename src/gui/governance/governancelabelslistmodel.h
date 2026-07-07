/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GOVERNANCELABELLISTMODEL_H
#define GOVERNANCELABELLISTMODEL_H

#include "governancetypes.h"
#include "governancelabelinfo.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QList>

namespace OCC
{

class GovernanceLabelsListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Governance::LabelType labelType READ labelType WRITE setLabelType NOTIFY labelTypeChanged FINAL)

    Q_PROPERTY(QString entityId READ entityId WRITE setEntityId NOTIFY entityIdChanged FINAL)

    Q_PROPERTY(LabelBehavior labelBehavior READ labelBehavior WRITE setLabelBehavior NOTIFY labelBehaviorChanged FINAL)

public:
    enum class LabelsListModelRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PriorityRole,
        DescriptionRole,
        ColorRole,
        ScopesRole,
        SelectedRole,
    };

    Q_ENUM(LabelsListModelRoles)

    enum class LabelBehavior {
        UniqueLabel,
        MultipleLabels,
        UnknownLabelbehavior,
    };

    Q_ENUM(LabelBehavior)

    explicit GovernanceLabelsListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int,QByteArray> roleNames() const override;

    [[nodiscard]] Governance::LabelType labelType() const;

    void setLabelType(Governance::LabelType newLabelType);

    [[nodiscard]] QString entityId() const;

    void setEntityId(const QString &newEntityId);

    [[nodiscard]] LabelBehavior labelBehavior() const;

    void setLabelBehavior(LabelBehavior newLabelBehavior);

public Q_SLOTS:
    void setAvailableLabelsJsonData(const QJsonDocument &reply);

    void setExistingLabelsJsonData(const QJsonDocument &reply);

    void toggleLabel(int index, const QString &labelId);

    void toggleLabel(const QString &labelId);

    void etagChanged();

    void labelWasModified();

Q_SIGNALS:
    void labelTypeChanged();

    void entityIdChanged();

    void refreshData(OCC::Governance::LabelType labelType, const QString &entityId);

    void addLabel(const QString &labelId);

    void removeLabel(const QString &labelId);

    void labelBehaviorChanged();

private:
    void emitRefreshData();

    QJsonValue readOcsReply(const QJsonDocument &reply);

    Governance::LabelType _labelType = Governance::LabelType::InvalidLabelType;

    QString _entityId;

    QList<GovernanceLabelInfo> _data;

    LabelBehavior _labelBehavior = LabelBehavior::UnknownLabelbehavior;
};

} // namespace OCC

#endif // GOVERNANCELABELLISTMODEL_H
