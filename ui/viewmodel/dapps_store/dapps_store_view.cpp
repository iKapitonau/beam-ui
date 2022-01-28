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
#include <QObject>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineProfile>
#include <QFileDialog>
#include <qbuffer.h>
#include "dapps_store_view.h"
#include "utility/logger.h"
#include "model/app_model.h"
#include "version.h"
#include "quazip/quazip.h"
#include "quazip/quazipfile.h"
#include "quazip/JlCompress.h"
#include "viewmodel/qml_globals.h"
#include "wallet/client/apps_api/apps_utils.h"

namespace
{
    const char* kDappStoreShaderPath = "d:/work/dapps-store/beam-dapps-store/shaders/dapps_store_app.wasm";

    auto getDappStoreCID()
    {
        return "c7bfd39e04ab9ff2f21615e52d973867f9c70b43ffb4f6f7f086b5ba1de08567";
    }
}

DappsStoreViewModel::DappsStoreViewModel()
{
    LOG_INFO() << "DappsStoreViewModel created";

    auto defaultProfile = QWebEngineProfile::defaultProfile();
    defaultProfile->setHttpCacheType(QWebEngineProfile::HttpCacheType::DiskHttpCache);
    defaultProfile->setPersistentCookiesPolicy(QWebEngineProfile::PersistentCookiesPolicy::AllowPersistentCookies);
    defaultProfile->setCachePath(AppSettings().getAppsCachePath());
    defaultProfile->setPersistentStoragePath(AppSettings().getAppsStoragePath());

    _serverAddr = QString("127.0.0.1:") + QString::number(AppSettings().getAppsServerPort());

    loadApps();
}

DappsStoreViewModel::~DappsStoreViewModel()
{
    LOG_INFO() << "DappsStoreViewModel destroyed";
}

void DappsStoreViewModel::onCompleted(QObject *webView)
{
    assert(webView != nullptr);
}

QMap<QString, QVariant> DappsStoreViewModel::parseAppManifest(QTextStream& in, const QString& appFolder)
{
    QMap<QString, QVariant> app;

    const auto content = in.readAll();
    if (content.isEmpty())
    {
        throw std::runtime_error("Failed to read the manifest file");
    }

    const auto utf = content.toUtf8();

    // do not make json const or it will throw on missing keys
    auto json = nlohmann::json::parse(utf.begin(), utf.end());
    if (!json.is_object() || json.empty())
    {
        throw std::runtime_error("Invalid manifest file");
    }

    const auto& guid = json["guid"];
    if (!guid.is_string() || guid.empty())
    {
        throw std::runtime_error("Invalid GUID in the manifest file");
    }
    app.insert("guid", QString::fromStdString(guid.get<std::string>()));

    const auto& desc = json["description"];
    if (!desc.is_string() || desc.empty())
    {
        throw std::runtime_error("Invalid description in the manifest file");
    }
    app.insert("description", QString::fromStdString(desc.get<std::string>()));

    const auto& name = json["name"];
    if (!name.is_string() || name.empty())
    {
        throw std::runtime_error("Invalid app name in the manifest file");
    }

    const auto sname = name.get<std::string>();
    app.insert("name", QString::fromStdString(sname));

    const auto& url = json["url"];
    if (!url.is_string() || url.empty())
    {
        throw std::runtime_error("Invalid url in the manifest file");
    }

    const auto surl = url.get<std::string>();
    app.insert("url", expandLocalUrl(appFolder, surl));

    const auto& icon = json["icon"];
    if (!icon.empty())
    {
        if (!icon.is_string())
        {
            throw std::runtime_error("Invalid icon in the manifest file");
        }

        const auto ipath = expandLocalFile(appFolder, icon.get<std::string>());
        app.insert("icon", ipath);
        LOG_INFO() << "App: " << sname << ", icon: " << ipath.toStdString();
    }

    const auto& av = json["api_version"];
    if (!av.empty())
    {
        if (!av.is_string())
        {
            throw std::runtime_error("Invalid api_version in the manifest file");
        }
        app.insert("api_version", QString::fromStdString(av.get<std::string>()));
    }

    const auto& mav = json["min_api_version"];
    if (!mav.empty())
    {
        if (!mav.is_string())
        {
            app.insert("min_api_version", QString::fromStdString(mav.get<std::string>()));
        }
        throw std::runtime_error("Invalid min_api_version in the manifest file");
    }

    app.insert("local", true);
    // TODO: check why we used surl instead of extended url - app["url"]
    const auto appid = beam::wallet::GenerateAppID(sname, app["url"].toString().toStdString());
    app.insert("appid", QString::fromStdString(appid));

    return app;
}

void DappsStoreViewModel::loadApps()
{
    // settings ? - load whitelist of the publishers
    // callShader - getAppsByPublisher
    // add Apps to AppList
    // install app from IPFS

    std::string args = "role=manager,action=view_dapps,cid=";
    args += getDappStoreCID();

    QPointer<DappsStoreViewModel> guard(this);

    AppModel::getInstance().getWalletModel()->getAsync()->callShader(kDappStoreShaderPath, args,
        [this, guard](const std::string& err, const std::string& output, const beam::wallet::TxID& id)
        {
            if (!guard)
            {
                return;
            }

            if (!err.empty())
            {
                LOG_WARNING() << "Failed to load dapps list" << ", " << err;
                return;
            }

            try
            {
                auto json = nlohmann::json::parse(output);

                auto parseStringField = [](nlohmann::json& json, const char* fieldName) {
                    const auto& field = json[fieldName];
                    if (!field.is_string())
                    {
                        std::stringstream ss;
                        ss << "Invalid " << fieldName << " of the dapp";
                        throw std::runtime_error(ss.str());
                    }
                    return QString::fromStdString(field.get<std::string>());
                };

                if (json.empty() || !json.is_object() || json["dapps"].empty() || !json["dapps"].is_array())
                {
                    throw std::runtime_error("Invalid response of the view_dapps method");
                }

                LOG_INFO() << json.dump(4);

                QList<QMap<QString, QVariant>> apps;

                for (auto& item : json["dapps"].items())
                {
                    // TODO: add publisherKey to error messages
                    if (!item.value().is_object())
                    {
                        throw std::runtime_error("Invalid body of the dapp " + item.key());
                    }
                    auto guid = parseStringField(item.value(), "id");
                    auto publisher = parseStringField(item.value(), "publisher");

                    LOG_DEBUG() << "Parsing DApp from contract, guid - " << guid.toStdString() << ", publisher - " << publisher.toStdString();

                    QMap<QString, QVariant> app;
                    app.insert("description", parseStringField(item.value(), "description"));
                    app.insert("name", parseStringField(item.value(), "name"));
                    app.insert("ipfs_id", parseStringField(item.value(), "ipfs_id"));
                    // TODO: check if empty url is allowed for not installed app
                    app.insert("url", "");
                    app.insert("api_version", parseStringField(item.value(), "api_ver"));
                    app.insert("min_api_version", parseStringField(item.value(), "min_api_ver"));
                    app.insert("guid", guid);
                    app.insert("publisher", publisher);

                    // TODO: check
                    app.insert("supported", true);

                    auto localApp = loadLocalDapp(guid);

                    if (localApp.isEmpty())
                    {
                        app.insert("notInstalled", true);
                    }
                    else
                    {
                        app.insert("appid", localApp["appid"]);
                        // TODO:  add version comparison
                        app.insert("hasUpdate", false);
                    }

                    apps.push_back(app);
                }

                _apps = apps;
                emit appsChanged();
            }
            catch (std::runtime_error& err)
            {
                LOG_WARNING() << "Error while parsing app from contract" << ", " << err.what();
            }
        }
    );
}

QMap<QString, QVariant> DappsStoreViewModel::loadLocalDapp(const QString& guid)
{
    const auto appsPath = AppSettings().getLocalAppsPath();
    const auto appFolder = QDir(appsPath).filePath(guid);
    const auto manifestPath = QDir(appFolder).absoluteFilePath("manifest.json");

    try
    {
        if (QDir(appFolder).exists())
        {
            QFile file(manifestPath);
            if (!file.open(QFile::ReadOnly | QFile::Text))
            {
                throw std::runtime_error("Cannot open file");
            }

            QTextStream in(&file);
            auto app = parseAppManifest(in, guid);
            app.insert("full_path", appFolder);
            return app;
        }
    }
    catch (std::runtime_error& err)
    {
        LOG_WARNING() << "Error while reading local app from " << manifestPath.toStdString() << ", " << err.what();
    }
    return {};
}

QList<QMap<QString, QVariant>> DappsStoreViewModel::getApps()
{
    return _apps;
}

QString DappsStoreViewModel::getPublisherKey()
{
    if (_publisherKey.isEmpty())
    {
        std::string args = "role=manager,action=get_pk,cid=";
        args += getDappStoreCID();
        QPointer<DappsStoreViewModel> guard(this);

        AppModel::getInstance().getWalletModel()->getAsync()->callShader(kDappStoreShaderPath, args,
            [this, guard](const std::string& err, const std::string& output, const beam::wallet::TxID& id)
            {
                if (!guard)
                {
                    return;
                }

                if (!err.empty())
                {
                    LOG_WARNING() << "Failed to get publisherKey" << ", " << err;
                    return;
                }

                try
                {
                    auto json = nlohmann::json::parse(output);

                    if (json.empty() || !json.is_object() || !json["pk"].is_string())
                    {
                        throw std::runtime_error("Invalid response of the get_pk method");
                    }

                    _publisherKey = QString::fromStdString(json["pk"].get<std::string>());
                    emit publisherKeyChanged();
                }
                catch (std::runtime_error& err)
                {
                    LOG_WARNING() << "Failed to parse publisherKey from contract" << ", " << err.what();
                }
            }
        );
    }
    return _publisherKey;
}

QString DappsStoreViewModel::expandLocalUrl(const QString& folder, const std::string& url) const
{
    QString result = QString::fromStdString(url);
    result.replace("localapp", QString("http://") + _serverAddr + "/" + folder);
    return result;
}

QString DappsStoreViewModel::expandLocalFile(const QString& folder, const std::string& url) const
{
    auto path = QDir(AppSettings().getLocalAppsPath()).filePath(folder);
    auto result = QString::fromStdString(url);
    result.replace("localapp", QString("file:///") + path);
    return result;
}

QString DappsStoreViewModel::chooseFile()
{
    QFileDialog dialog(nullptr,
                        //% "Select application to install"
                        qtTrId("applications-install-title"),
                        "",
                        "BEAM DApp files (*.dapp)");

    dialog.setWindowModality(Qt::WindowModality::ApplicationModal);
    if (!dialog.exec())
    {
        return "";
    }
    return dialog.selectedFiles().value(0);
}

QString DappsStoreViewModel::installFromFile(const QString& rawFname)
{
    try
    {
        QString fname = rawFname;

        // Some shells/systems provide incorrect count of '/' after file:
        // For example in gnome on linux one '/' is missing. So this ugly code is necessary
        if (fname.startsWith("file:"))
        {
            fname = fname.remove(0, 5);
            while(fname.startsWith("/"))
            {
                fname = fname.remove(0, 1);
            }

            #ifndef WIN32
            fname = QString("/") + fname;
            #endif
        }

        LOG_DEBUG() << "Installing DApp from file " << rawFname.toStdString() << " | " << fname.toStdString();

        QuaZip zip(fname);
        if(!zip.open(QuaZip::Mode::mdUnzip))
        {
            throw std::runtime_error("Failed to open the DApp file");
        }

        QString guid, appName;
        for (bool ok = zip.goToFirstFile(); ok; ok = zip.goToNextFile())
        {
            const auto zipFname = zip.getCurrentFileName();
            if (zipFname == "manifest.json")
            {
                QuaZipFile mfile(zip.getZipName(), zipFname);
                if (!mfile.open(QIODevice::ReadOnly))
                {
                    throw std::runtime_error("Failed to read the DApp file");
                }

                QTextStream in(&mfile);
                const auto app = parseAppManifest(in, "");
                guid = app["guid"].value<QString>();
                appName = app["name"].value<QString>();
            }
        }

        if (guid.isEmpty())
        {
            throw std::runtime_error("Invalid DApp file");
        }

        const auto appsPath = AppSettings().getLocalAppsPath();
        const auto appFolder = QDir(appsPath).filePath(guid);

        if (QDir(appFolder).exists())
        {
            if(!QDir(appFolder).removeRecursively())
            {
                throw std::runtime_error("Failed to prepare folder");
            }
        }

        QDir(appsPath).mkdir(guid);
        if(JlCompress::extractDir(fname, appFolder).isEmpty())
        {
            //cleanupFolder(appFolder)
            throw std::runtime_error("DApp Installation failed");
        }

        return appName;
    }
    catch(std::exception& err)
    {
        LOG_ERROR() << "Failed to install DApp: " << err.what();
        return "";
    }
}

void DappsStoreViewModel::uploadApp()
{
    try
    {
        // select file
        const auto appPath = chooseFile();

        // unpack and verify
        QuaZip zip(appPath);
        if (!zip.open(QuaZip::Mode::mdUnzip))
        {
            throw std::runtime_error("Failed to open the DApp file");
        }

        QMap<QString, QVariant> app;
        for (bool ok = zip.goToFirstFile(); ok; ok = zip.goToNextFile())
        {
            const auto zipFname = zip.getCurrentFileName();
            if (zipFname == "manifest.json")
            {
                QuaZipFile mfile(zip.getZipName(), zipFname);
                if (!mfile.open(QIODevice::ReadOnly))
                {
                    throw std::runtime_error("Failed to read the DApp file");
                }

                QTextStream in(&mfile);
                app = parseAppManifest(in, "");
            }
        }

        if (app["guid"].value<QString>().isEmpty())
        {
            throw std::runtime_error("Invalid DApp file");
        }

        // add to IPFS
        QFile file(appPath);

        if (!file.open(QFile::ReadOnly))
        {
            throw std::runtime_error("Failed to read the DApp file");
        }

        QDataStream in(&file);
        auto buffer = file.readAll();
        auto ipfs = AppModel::getInstance().getWalletModel()->getIPFS();
        QPointer<DappsStoreViewModel> guard(this);

        ipfs->AnyThread_add(beam::ByteBuffer(buffer.cbegin(), buffer.cend()),
            [this, guard, app=std::move(app)] (std::string&& ipfsID) mutable
            {
                if (!guard)
                {
                    return;
                }

                LOG_INFO() << "IPFS_ID: " << ipfsID;

                // save result to contract
                addAppToStore(std::move(app), ipfsID);
            },
            [] (std::string&& err) {
                LOG_ERROR() << "Failed to add to ipfs: " << err;
            }
        );
    }
    catch (std::runtime_error& err)
    {
        LOG_ERROR() << "Failed to upload DApp: " << err.what();
    }
}

void DappsStoreViewModel::addAppToStore(QMap<QString, QVariant>&& app, const std::string& ipfsID)
{
    QString guid = app["guid"].value<QString>();
    QString appName = app["name"].value<QString>();
    QString description = app["description"].value<QString>();

    QString args;
    QTextStream textStream(&args);

    textStream << "role=manager,action=add_dapp,cid=" << getDappStoreCID() << ",ipfs_id=" << QString::fromStdString(ipfsID);
    textStream << ",name=" << appName << ",id=" << guid << ",description=" << description;
    textStream << ",api_ver=" << app["api_version"].value<QString>() << ",min_api_ver=" << app["min_api_version"].value<QString>();

    QPointer<DappsStoreViewModel> guard(this);

    AppModel::getInstance().getWalletModel()->getAsync()->callShader(kDappStoreShaderPath, args.toStdString(),
        [this, guard] (const std::string& err, const std::string& output, const beam::wallet::TxID& id)
        {
            if (!guard)
            {
                return;
            }

            try
            {
                if (!err.empty())
                {
                    LOG_WARNING() << "Failed to publish app" << ", " << err;
                }
                // TODO: check TX status

                LOG_INFO() << "App added!";
            }
            catch (std::runtime_error& err)
            {
                LOG_WARNING() << "Failed to publish app" << ", " << err.what();
            }
        }
    );
}

void DappsStoreViewModel::registerPublisher()
{
    std::string args = "role=manager,action=add_publisher,cid=";
    args += getDappStoreCID();
    // TODO:
    args += ",name=test publisher";

    QPointer<DappsStoreViewModel> guard(this);

    AppModel::getInstance().getWalletModel()->getAsync()->callShader(kDappStoreShaderPath, args,
        [this, guard](const std::string& err, const std::string& output, const beam::wallet::TxID& id)
        {
            if (!guard)
            {
                return;
            }
            try
            {
                if (!err.empty())
                {
                    LOG_WARNING() << "Failed to add publisher" << ", " << err;
                }

                // TODO: check TX status
                LOG_INFO() << "publisher registered!";
            }
            catch (std::runtime_error& err)
            {
                LOG_WARNING() << "Failed to add publishert" << ", " << err.what();
            }
        }
    );
}

void DappsStoreViewModel::installApp(const QString& guid)
{
    // find app in _apps by guid
    const auto it = std::find_if(_apps.cbegin(), _apps.cend(),
        [guid] (const auto& app) -> bool {
            const auto appIt = app.find("guid");
            if (appIt == app.end())
            {
                assert(false);
                return false;
            }
            return appIt->toString() == guid;
    });

    if (it == _apps.end())
    {
        assert(false);
        return;
    }

    try
    {
        const auto ipfsID = (*it)["ipfs_id"].toString();
        const auto appName = (*it)["name"].toString();

        // get dapp binary data from ipfs
        QPointer<DappsStoreViewModel> guard(this);
        auto ipfs = AppModel::getInstance().getWalletModel()->getIPFS();

        // TODO: check timeout value
        ipfs->AnyThread_get(ipfsID.toStdString(), 0,
            [this, guard, appName](beam::ByteBuffer&& data) mutable
            {
                if (!guard)
                {
                    return;
                }

                try
                {
                    // unpack & verify & install
                    LOG_DEBUG() << "Installing DApp " << appName.toStdString() << " from ipfs";

                    QByteArray qData;
                    std::copy(data.cbegin(), data.cend(), std::back_inserter(qData));

                    QBuffer buffer(&qData);
                    const auto result = installFromBuffer(&buffer);

                    // TODO: probably not the best place to check
                    if (result != appName)
                    {
                        assert(false);
                        LOG_WARNING() << "Mismatched DApp names, expected - "
                            << appName.toStdString() << ", resulting - " << result.toStdString();
                    }

                    emit appInstallOK(appName);
                }
                catch (std::runtime_error& err)
                {
                    LOG_ERROR() << "Failed to install DApp: " << err.what();
                    emit appInstallFail(appName);
                }
            },
            [this, guard, appName](std::string&& err)
            {
                LOG_ERROR() << "Failed to get app from ipfs: " << err;
                emit appInstallFail(appName);
            }
        );
    }
    catch (const std::runtime_error& err)
    {
        assert(false);
        LOG_WARNING() << "Failed to get properties for " << guid.toStdString() << ", " << err.what();
        return;
    }
}

QString DappsStoreViewModel::installFromBuffer(QIODevice* ioDevice)
{
    QuaZip zip(ioDevice);
    if (!zip.open(QuaZip::Mode::mdUnzip))
    {
        throw std::runtime_error("Failed to open the DApp archive");
    }

    QString guid, appName;
    for (bool ok = zip.goToFirstFile(); ok; ok = zip.goToNextFile())
    {
        const auto zipFname = zip.getCurrentFileName();
        if (zipFname == "manifest.json")
        {
            QuaZipFile mfile(&zip);
            if (!mfile.open(QIODevice::ReadOnly))
            {
                throw std::runtime_error("Failed to read the DApp archive");
            }
                
            QTextStream in(&mfile);
            const auto app = parseAppManifest(in, "");
            guid = app["guid"].value<QString>();
            appName = app["name"].value<QString>();
        }
    }

    if (guid.isEmpty())
    {
        throw std::runtime_error("Invalid DApp archive");
    }

    const auto appsPath = AppSettings().getLocalAppsPath();
    const auto appFolder = QDir(appsPath).filePath(guid);

    if (QDir(appFolder).exists())
    {
        if (!QDir(appFolder).removeRecursively())
        {
            throw std::runtime_error("Failed to prepare folder");
        }
    }

    QDir(appsPath).mkdir(guid);
    if (JlCompress::extractDir(ioDevice, appFolder).isEmpty())
    {
        //cleanupFolder(appFolder)
        throw std::runtime_error("DApp Installation failed");
    }

    return appName;
}