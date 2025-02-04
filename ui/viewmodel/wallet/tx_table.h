// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <QObject>
#include <QQueue>
#include <QAbstractItemModel>
#include "model/wallet_model.h"
#include "tx_object_list.h"
#include "model/exchange_rates_manager.h"
#include "model/settings.h"

class TxTableViewModel: public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel*  transactions READ   getTransactions     NOTIFY transactionsChanged)
    Q_PROPERTY(QString rateUnit     READ getRateUnit    NOTIFY rateChanged)
    Q_PROPERTY(bool showInProgress  READ getShowInProgress WRITE setShowInProgress NOTIFY showInProgressChanged)
    Q_PROPERTY(bool showCompleted   READ getShowCompleted  WRITE setShowCompleted  NOTIFY showCompletedChanged)
    Q_PROPERTY(bool showCanceled    READ getShowCanceled   WRITE setShowCanceled   NOTIFY showCanceledChanged)
    Q_PROPERTY(bool showFailed      READ getShowFailed     WRITE setShowFailed     NOTIFY showFailedCanged)

public:
    TxTableViewModel();
    ~TxTableViewModel() override = default;

    QAbstractItemModel* getTransactions();
    QString getRateUnit() const;
    QString getRate() const;

    bool getShowInProgress() const;
    void setShowInProgress(bool value);
    bool getShowCompleted() const;
    void setShowCompleted(bool value);
    bool getShowCanceled() const;
    void setShowCanceled(bool value);
    bool getShowFailed() const;
    void setShowFailed(bool value);

    Q_INVOKABLE void exportTxHistoryToCsv();
    Q_INVOKABLE void cancelTx(const QVariant& variantTxID);
    Q_INVOKABLE void deleteTx(const QVariant& variantTxID);
    Q_INVOKABLE PaymentInfoItem* getPaymentInfo(const QVariant& variantTxID);

public slots:
    void onTxHistoryExportedToCsv(const QString& data);
    void onTransactionsChanged(beam::wallet::ChangeAction action, const std::vector<beam::wallet::TxDescription>& items);

signals:
    void transactionsChanged();
    void rateChanged();
    void showInProgressChanged();
    void showCompletedChanged();
    void showCanceledChanged();
    void showFailedCanged();

private:
    WalletModel::Ptr     _model;
    QQueue<QString>      _txHistoryToCsvPaths;
    TxObjectList         _transactionsList;
    ExchangeRatesManager::Ptr _rates;
    WalletSettings&      _settings;

    bool _showInProgress = true;
    bool _showCompleted = true;
    bool _showCanceled = true;
    bool _showFailed = true;
};
