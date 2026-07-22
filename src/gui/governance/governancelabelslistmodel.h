/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GOVERNANCELABELLISTMODEL_H
#define GOVERNANCELABELLISTMODEL_H

#include "governancetypes.h"
#include "governancelabelinfo.h"
#include "accountfwd.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QList>
#include <QJsonObject>

namespace OCC
{

class GovernanceLabelsListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged FINAL)

    Q_PROPERTY(Governance::ApiVersion apiVersion READ apiVersion WRITE setApiVersion NOTIFY apiVersionChanged FINAL)

    Q_PROPERTY(Governance::EntityType entityType READ entityType WRITE setEntityType NOTIFY entityTypeChanged FINAL)

    Q_PROPERTY(Governance::LabelType labelType READ labelType WRITE setLabelType NOTIFY labelTypeChanged FINAL)

    Q_PROPERTY(QString entityId READ entityId WRITE setEntityId NOTIFY entityIdChanged FINAL)

    Q_PROPERTY(LabelBehavior labelBehavior READ labelBehavior WRITE setLabelBehavior NOTIFY labelBehaviorChanged FINAL)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)

    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged FINAL)

    Q_PROPERTY(bool hasPendingChanges READ hasPendingChanges NOTIFY hasPendingChangesChanged FINAL)

public:
    enum class LabelsListModelRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PriorityRole,
        DescriptionRole,
        ColorRole,
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

    [[nodiscard]] AccountPtr account() const;

    void setAccount(AccountPtr newAccount);

    [[nodiscard]] Governance::ApiVersion apiVersion() const;

    void setApiVersion(Governance::ApiVersion newApiVersion);

    [[nodiscard]] Governance::EntityType entityType() const;

    void setEntityType(Governance::EntityType newEntityType);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QHash<int,QByteArray> roleNames() const override;

    [[nodiscard]] Governance::LabelType labelType() const;

    void setLabelType(Governance::LabelType newLabelType);

    [[nodiscard]] QString entityId() const;

    void setEntityId(const QString &newEntityId);

    [[nodiscard]] LabelBehavior labelBehavior() const;

    void setLabelBehavior(LabelBehavior newLabelBehavior);

    [[nodiscard]] bool busy() const;

    [[nodiscard]] bool isEmpty() const;

    [[nodiscard]] bool hasPendingChanges() const;

public Q_SLOTS:
    void setAvailableLabelsJsonData(const QJsonDocument &reply);

    void toggleLabel(int row);

    void etagChanged();

    void labelsWereModified();

    void reset();

    void apply();

Q_SIGNALS:
    void accountChanged();

    void apiVersionChanged();

    void entityTypeChanged();

    void labelTypeChanged();

    void entityIdChanged();

    void refreshAvailableLabelsData(OCC::Governance::LabelType labelType, const QString &entityId);

    void labelBehaviorChanged();

    void displayError(const QString &errorMessage);

    void busyChanged();

    void isEmptyChanged();

    void hasPendingChangesChanged();

private:
    void emitRefreshData();

    static QJsonValue readOcsReply(const QJsonObject &reply);

    [[nodiscard]] bool hasReceivedAllData() const;

    [[nodiscard]] bool validState() const;

    void refreshModel();

    void addLabel(const QString &labelId);

    void removeLabel(const QString &labelId);

    void setHasPendingChanges(bool newValue);

    [[nodiscard]] bool checksForPendingChanges() const;

    AccountPtr _account;

    Governance::ApiVersion _apiVersion = Governance::ApiVersion::Version_1;

    Governance::EntityType _entityType = Governance::EntityType::Files;

    Governance::LabelType _labelType = Governance::LabelType::InvalidLabelType;

    QString _entityId;

    QList<GovernanceLabelInfo> _data;

    LabelBehavior _labelBehavior = LabelBehavior::UnknownLabelbehavior;

    QJsonObject _availableLabelsReply;

    QSet<QString> _pendingRefreshLabelIds;

    bool _busy = false;

    bool _applyingChanges = false;

    bool _hasPendingChanges = false;
};

} // namespace OCC

#endif // GOVERNANCELABELLISTMODEL_H
