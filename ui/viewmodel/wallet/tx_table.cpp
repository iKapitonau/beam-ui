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
#include "tx_table.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <vector>
#include "model/app_model.h"

namespace
{
    const char kTxHistoryFileNamePrefix[] = "transactions_history_";
    const char kTxHistoryFileFormatDesc[] = "Comma-Separated Values (*.csv)";
    const char kTxHistoryFileNameFormat[] = "yyyy_MM_dd_HH_mm_ss";
}

TxTableViewModel::TxTableViewModel()
    : _model(AppModel::getInstance().getWalletModel())
    , _rates(AppModel::getInstance().getRates())
    , _settings{AppModel::getInstance().getSettings()}
{
    connect(_model.get(), SIGNAL(transactionsChanged(beam::wallet::ChangeAction, const std::vector<beam::wallet::TxDescription>&)), SLOT(onTransactionsChanged(beam::wallet::ChangeAction, const std::vector<beam::wallet::TxDescription>&)));
    connect(_model.get(), SIGNAL(txHistoryExportedToCsv(const QString&)), this, SLOT(onTxHistoryExportedToCsv(const QString&)));
    connect(_rates.get(), &ExchangeRatesManager::rateUnitChanged, this, &TxTableViewModel::rateChanged);
    connect(_rates.get(), &ExchangeRatesManager::activeRateChanged, this, &TxTableViewModel::rateChanged);

    _showInProgress = _settings.getShowInProgress();
    _showCompleted = _settings.getShowCompleted();
    _showCanceled = _settings.getShowCanceled();
    _showFailed = _settings.getShowFailed();

    _model->getAsync()->getTransactions();
}

void TxTableViewModel::exportTxHistoryToCsv()
{
    QDateTime now = QDateTime::currentDateTime();
    QString path = QFileDialog::getSaveFileName(
        nullptr,
        //: transactions history screen, export button tooltip and open file dialog
        //% "Export transactions history"
        qtTrId("wallet-export-tx-history"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))
            .filePath(kTxHistoryFileNamePrefix + now.toString(kTxHistoryFileNameFormat)), kTxHistoryFileFormatDesc);

    if (!path.isEmpty())
    {
        _txHistoryToCsvPaths.enqueue(path);
        _model->getAsync()->exportTxHistoryToCsv();
    }
}

void TxTableViewModel::onTxHistoryExportedToCsv(const QString& data)
{
    if (!_txHistoryToCsvPaths.isEmpty())
    {
        const auto& path = _txHistoryToCsvPaths.dequeue();
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextCodec *codec = QTextCodec::codecForName("UTF8");
            QTextStream out(&file);
            out.setCodec(codec);
            out << data;
        }
    }
}

QAbstractItemModel* TxTableViewModel::getTransactions()
{
    return &_transactionsList;
}

void TxTableViewModel::onTransactionsChanged(beam::wallet::ChangeAction action, const std::vector<beam::wallet::TxDescription>& transactions)
{
    using namespace beam::wallet;

    std::vector<std::shared_ptr<TxObject>> modifiedTransactions;
    modifiedTransactions.reserve(transactions.size());
    const auto secondCurrency = _rates->getRateCurrency();

    for (const auto& t : transactions)
    {
        if(const auto txType = t.GetParameter<TxType>(TxParameterID::TransactionType))
        {
            switch(*txType)
            {
            case TxType::AtomicSwap:
            case TxType::AssetIssue:
            case TxType::AssetConsume:
            case TxType::AssetReg:
            case TxType::AssetUnreg:
            case TxType::AssetInfo:
            case TxType::PullTransaction:
            case TxType::UnlinkFunds:
            case TxType::VoucherRequest:
            case TxType::VoucherResponse:
                continue;

            case TxType::ALL:
                assert(!"This should not happen");
                continue;

            case TxType::Contract:
            case TxType::PushTransaction:
            case TxType::Simple:
            case TxType::DexSimpleSwap:
                modifiedTransactions.push_back(std::make_shared<TxObject>(t, secondCurrency));
                break;
            }
        }
    }

    switch (action)
    {
        case ChangeAction::Reset:
            {
                std::vector<std::shared_ptr<TxObject>> modifiedTransactionsFiltered;
                modifiedTransactionsFiltered.reserve(modifiedTransactions.size());
                for (const auto& tx: modifiedTransactions)
                {
                    if (!_showInProgress && tx->isInProgress())
                        continue;

                    if (!_showCompleted && tx->isCompleted())
                        continue;

                    if (!_showCanceled && tx->isCanceled())
                        continue;

                    if (!_showFailed && tx->isFailed())
                        continue;

                    modifiedTransactionsFiltered.push_back(tx);
                }
                _transactionsList.reset(modifiedTransactionsFiltered);
                break;
            }

        case ChangeAction::Removed:
            {
                _transactionsList.remove(modifiedTransactions);
                break;
            }

        case ChangeAction::Added:
            {
                _transactionsList.insert(modifiedTransactions);
                break;
            }

        case ChangeAction::Updated:
            {
                _transactionsList.update(modifiedTransactions);
                break;
            }

        default:
            assert(false && "Unexpected action");
            break;
    }

    emit transactionsChanged();
}

QString TxTableViewModel::getRateUnit() const
{
    return beamui::getCurrencyUnitName(_rates->getRateCurrency());
}

QString TxTableViewModel::getRate() const
{
    auto rate = _rates->getRate(beam::wallet::Currency::BEAM());
    return beamui::AmountToUIString(rate);
}

bool TxTableViewModel::getShowInProgress() const
{
    return _showInProgress;
}

void TxTableViewModel::setShowInProgress(bool value)
{
    _showInProgress = value;
    _settings.setShowInProgress(value);
    emit showInProgressChanged();
    _model->getAsync()->getTransactions();
}

bool TxTableViewModel::getShowCompleted() const
{
    return _showCompleted;
}

void TxTableViewModel::setShowCompleted(bool value)
{
    _showCompleted = value;
    _settings.setShowCompleted(value);
    emit showCompletedChanged();
    _model->getAsync()->getTransactions();
}

bool TxTableViewModel::getShowCanceled() const
{
    return _showCanceled;
}

void TxTableViewModel::setShowCanceled(bool value)
{
    _showCanceled = value;
    _settings.setShowCanceled(value);
    emit showCanceledChanged();
    _model->getAsync()->getTransactions();
}

bool TxTableViewModel::getShowFailed() const
{
    return _showFailed;
}

void TxTableViewModel::setShowFailed(bool value)
{
    _showFailed = value;
    _settings.setShowFailed(value);
    emit showFailedCanged();
    _model->getAsync()->getTransactions();
}

void TxTableViewModel::cancelTx(const QVariant& variantTxID)
{
    if (!variantTxID.isNull() && variantTxID.isValid())
    {
        auto txId = variantTxID.value<beam::wallet::TxID>();
        _model->getAsync()->cancelTx(txId);
    }
}

void TxTableViewModel::deleteTx(const QVariant& variantTxID)
{
    if (!variantTxID.isNull() && variantTxID.isValid())
    {
        auto txId = variantTxID.value<beam::wallet::TxID>();
        _model->getAsync()->deleteTx(txId);
    }
}

PaymentInfoItem* TxTableViewModel::getPaymentInfo(const QVariant& variantTxID)
{
    if (!variantTxID.isNull() && variantTxID.isValid())
    {
        auto txId = variantTxID.value<beam::wallet::TxID>();
        return new MyPaymentInfoItem(txId, this);
    }
    else return Q_NULLPTR;
}
