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
#include <QSettings>
#include <QDir>
#include <QStringList>
#include <chrono>
#include <mutex>
#include "model/wallet_model.h"

#ifdef BEAM_IPFS_SUPPORT
#include <asio-ipfs/include/ipfs_config.h>
#endif

class WalletSettings : public QObject
{
    Q_OBJECT
public:
    explicit WalletSettings(const QDir& appDataDir);

    QString getNodeAddress() const;
    void setNodeAddress(const QString& value);

    int getLockTimeout() const;
    void setLockTimeout(int value);

    bool isPasswordReqiredToSpendMoney() const;
    void setPasswordReqiredToSpendMoney(bool value);

    bool isAllowedBeamMWLinks() const;
    void setAllowedBeamMWLinks(bool value);

    bool showSwapBetaWarning();
    void setShowSwapBetaWarning(bool value);

    #if defined(BEAM_HW_WALLET)
    std::string getTrezorWalletStorage() const;
    #endif

    std::string getWalletStorage() const;
    std::string getWalletFolder() const;
    std::string getAppDataPath() const;
    void reportProblem();

    bool getRunLocalNode() const;
    void setRunLocalNode(bool value);

    uint getLocalNodePort() const;
    void setLocalNodePort(uint port);
    std::string getLocalNodeStorage() const;
    std::string getTempDir() const;

    QStringList getLocalNodePeers();
    void setLocalNodePeers(const QStringList& qPeers);

    bool getPeersPersistent() const;

    QString getLocale() const;
    QString getLanguageName() const;
    void setLocaleByLanguageName(const QString& language);
    static QStringList getSupportedLanguages();

    // Second currency settings
    beam::wallet::Currency getRateCurrency() const;
    void setRateCurrency(const beam::wallet::Currency&);

    // Notifications settings
    bool isNewVersionActive() const;
    bool isBeamNewsActive() const;
    bool isTxStatusActive() const;
    void setNewVersionActive(bool isActive);
    void setBeamNewsActive(bool isActive);
    void setTxStatusActive(bool isActive);

    static void openFolder(const QString& path);

    //
    // DApps
    //
    QString getDevAppUrl() const;
    QString getDevAppName() const;
    QString getDevAppApiVer() const;
    QString getDevAppMinApiVer() const;

    bool getDevMode() const;
    bool getAppsAllowed() const;
    void setAppsAllowed(bool val);
    QString getLocalAppsPath() const;
    QString getAppsCachePath(const QString& name = QString()) const;
    QString getAppsStoragePath(const QString& name = QString()) const;
    int getAppsServerPort() const;
    void setAppsServerPort(int port);

    #ifdef BEAM_IPFS_SUPPORT
    asio_ipfs::config getIPFSConfig() const;
    void setIPFSPort(uint32_t port);
    void setIPFSNodeStart(const QString&);
    QString getIPFSNodeStart() const;

    enum class IPFSLaunch {
        AtStart = 0,
        AtDApps,
        Never,
    };

    IPFSLaunch getIPFSNodeLaunch() const;
    #endif

    uint8_t getMaxPrivacyAnonymitySet() const;
    void setMaxPrivacyAnonymitySet(uint8_t anonymitySet);

    void maxPrivacyLockTimeLimitInit();
    uint8_t getMaxPrivacyLockTimeLimitHours() const;
    void setMaxPrivacyLockTimeLimitHours(uint8_t lockTimeLimit);

    QString getExplorerUrl() const;
    QString getFaucetUrl() const;
    QString getAppsUrl() const;

    bool showFaucetPromo() const;
    void setShowFacetPromo(bool value);

    bool hideSeedValidationPromo() const;
    void setHideSeedValidationPromo(bool value);

    void minConfirmationsInit();
    uint32_t getMinConfirmations() const;
    void setMinConfirmations(uint32_t value);

    [[nodiscard]] QVector<beam::Asset::ID> getLastAssetSelection() const;
    void setLastAssetSelection(QVector<beam::Asset::ID> selection);

    // tx table filters
    bool getShowInProgress() const;
    void setShowInProgress(bool value);
    bool getShowCompleted() const;
    void setShowCompleted(bool value);
    bool getShowCanceled() const;
    void setShowCanceled(bool value);
    bool getShowFailed() const;
    void setShowFailed(bool value);

    bool isAppActive() const;
    void setAppActive(bool value);

public:
    static const char* WalletCfg;
    static const char* LogsFolder;
    static const char* SettingsFile;
    static const char* WalletDBFile;

    #if defined(BEAM_HW_WALLET)
    static const char* TrezorWalletDBFile;
    #endif

    static const char* NodeDBFile;
    void applyNodeChanges();

    #ifdef BEAM_IPFS_SUPPORT
    void applyIPFSChanges();
    #endif

signals:
    void nodeAddressChanged();
    void lockTimeoutChanged();
    void localNodeRunChanged();
    void localNodePortChanged();
    void localNodePeersChanged();
    void localeChanged();
    void beamMWLinksChanged();
    void secondCurrencyChanged();
    void dappsAllowedChanged();
    void IPFSSettingsChanged();

private:
    mutable QSettings m_data;
    QDir m_appDataDir;
    uint8_t m_mpLockTimeLimit = 0;
    uint32_t m_minConfirmations = 0;
    mutable std::recursive_mutex m_mutex;
    using Lock = std::unique_lock<decltype(m_mutex)>;
    bool m_isActive = false;
    uint64_t m_activateTime = 0;
};
