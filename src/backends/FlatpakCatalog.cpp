#include "FlatpakCatalog.h"

#ifdef signals
#pragma push_macro("signals")
#undef signals
#define PLAZA_RESTORE_SIGNALS_MACRO
#endif
#include <flatpak.h>
#ifdef PLAZA_RESTORE_SIGNALS_MACRO
#pragma pop_macro("signals")
#undef PLAZA_RESTORE_SIGNALS_MACRO
#endif

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrentRun>
#include <zlib.h>

#include <algorithm>

namespace {
QByteArray readCatalog(const QString& directory)
{
    QFile xml(QDir(directory).filePath(QStringLiteral("appstream.xml")));
    if (xml.open(QIODevice::ReadOnly)) {
        return xml.readAll();
    }
    const QByteArray path = QFile::encodeName(QDir(directory).filePath(QStringLiteral("appstream.xml.gz")));
    gzFile compressed = gzopen(path.constData(), "rb");
    if (!compressed) return {};
    QByteArray data;
    char buffer[32768];
    int bytes = 0;
    while ((bytes = gzread(compressed, buffer, sizeof(buffer))) > 0) {
        data.append(buffer, bytes);
        if (data.size() > 128 * 1024 * 1024) {
            qWarning() << "Flatpak catalog exceeds the size limit:" << directory;
            data.clear();
            break;
        }
    }
    if (bytes < 0) {
        qWarning() << "Could not decompress Flatpak catalog:" << directory;
        data.clear();
    }
    gzclose(compressed);
    return data;
}

void readComponent(QXmlStreamReader& xml, const QString& directory,
                   const QString& remote, const QString& architecture,
                   QVector<CatalogApp>& apps)
{
    const QString type = xml.attributes().value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("desktop") && type != QStringLiteral("desktop-application")) {
        xml.skipCurrentElement();
        return;
    }
    CatalogApp app;
    app.repository = remote;
    QString cachedIcon;
    while (xml.readNextStartElement()) {
        const QStringView name = xml.name();
        const auto attributes = xml.attributes();
        const bool defaultLanguage = !attributes.hasAttribute(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("lang"));
        if (name == u"id") app.componentId = xml.readElementText().trimmed();
        else if (name == u"name" && defaultLanguage) app.displayName = xml.readElementText().trimmed();
        else if (name == u"summary" && defaultLanguage) app.summary = xml.readElementText().trimmed();
        else if (name == u"description" && defaultLanguage) app.description = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
        else if (name == u"bundle" && attributes.value(QStringLiteral("type")) == u"flatpak") {
            const QStringList ref = xml.readElementText().trimmed().split(QLatin1Char('/'));
            if (ref.size() == 4 && ref[0] == QStringLiteral("app") && ref[2] == architecture && ref[3] == QStringLiteral("stable"))
                app.packageId = ref[1];
        } else if (name == u"icon") {
            const QString iconType = attributes.value(QStringLiteral("type")).toString();
            const QString icon = xml.readElementText().trimmed();
            if (iconType == QStringLiteral("cached") && QFileInfo(icon).fileName() == icon) cachedIcon = icon;
            else if (iconType == QStringLiteral("stock") && app.iconName.isEmpty()) app.iconName = icon;
        } else if (name == u"custom") {
            while (xml.readNextStartElement()) {
                if (xml.name() == u"value" && xml.attributes().value(QStringLiteral("key")) == u"flathub::verification::verified")
                    app.verified = xml.readElementText().trimmed() == QStringLiteral("true");
                else xml.skipCurrentElement();
            }
        } else xml.skipCurrentElement();
    }
    if (app.componentId.isEmpty() || app.displayName.isEmpty() || app.packageId.isEmpty()) return;
    if (!cachedIcon.isEmpty()) {
        for (const QString& size : {QStringLiteral("128x128"), QStringLiteral("64x64"), QStringLiteral("48x48")}) {
            const QString path = QDir(directory).filePath(QStringLiteral("icons/") + size + QLatin1Char('/') + cachedIcon);
            if (QFileInfo::exists(path)) {
                app.iconName = QUrl::fromLocalFile(path).toString();
                break;
            }
        }
    }
    apps.append(std::move(app));
}

void readInstallation(FlatpakInstallation* installation, const QString& architecture, QVector<CatalogApp>& apps)
{
    if (!installation) return;
    GError* error = nullptr;
    GPtrArray* remotes = flatpak_installation_list_remotes(installation, nullptr, &error);
    if (!remotes) {
        if (error) { qWarning() << "Could not list Flatpak remotes:" << error->message; g_error_free(error); }
        return;
    }
    QVector<FlatpakRemote*> sorted;
    for (guint i = 0; i < remotes->len; ++i) sorted.append(static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, i)));
    std::stable_sort(sorted.begin(), sorted.end(), [](FlatpakRemote* a, FlatpakRemote* b) {
        return QString::fromUtf8(flatpak_remote_get_name(a)) == QStringLiteral("flathub") &&
               QString::fromUtf8(flatpak_remote_get_name(b)) != QStringLiteral("flathub");
    });
    for (FlatpakRemote* remote : sorted) {
        if (flatpak_remote_get_disabled(remote)) continue;
        GFile* dir = flatpak_remote_get_appstream_dir(remote, architecture.toUtf8().constData());
        if (!dir) continue;
        char* rawPath = g_file_get_path(dir);
        const QString path = QString::fromUtf8(rawPath ? rawPath : "");
        g_free(rawPath);
        g_object_unref(dir);
        const QByteArray data = readCatalog(path);
        if (data.isEmpty()) continue;
        const QString remoteName = QString::fromUtf8(flatpak_remote_get_name(remote));
        apps += parseFlatpakAppstream(data, path, remoteName, architecture);
    }
    g_ptr_array_unref(remotes);
}

QVector<CatalogApp> readCatalogs()
{
    QVector<CatalogApp> apps;
    const QString arch = QString::fromUtf8(flatpak_get_default_arch());
    GError* error = nullptr;
    FlatpakInstallation* user = flatpak_installation_new_user(nullptr, &error);
    if (error) { qWarning() << "Could not open user Flatpak installation:" << error->message; g_clear_error(&error); }
    readInstallation(user, arch, apps);
    if (user) g_object_unref(user);
    FlatpakInstallation* system = flatpak_installation_new_system(nullptr, &error);
    if (error) { qWarning() << "Could not open system Flatpak installation:" << error->message; g_clear_error(&error); }
    readInstallation(system, arch, apps);
    if (system) g_object_unref(system);
    return apps;
}
}

QVector<CatalogApp> parseFlatpakAppstream(const QByteArray& data, const QString& directory,
                                          const QString& remote, const QString& architecture)
{
    QVector<CatalogApp> apps;
    QXmlStreamReader xml(data);
    if (xml.readNextStartElement() && xml.name() == u"components") {
        while (xml.readNextStartElement()) {
            if (xml.name() == u"component") readComponent(xml, directory, remote, architecture, apps);
            else xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        qWarning() << "Invalid Flatpak catalog" << remote << xml.errorString();
        return {};
    }
    return apps;
}

FlatpakCatalog::FlatpakCatalog(QObject* parent) : QObject(parent)
{
    connect(&watcher_, &QFutureWatcherBase::finished, this, [this] {
        emit loaded(watcher_.result());
        if (reloadPending_) {
            reloadPending_ = false;
            watcher_.setFuture(QtConcurrent::run(readCatalogs));
        }
        emit loadingChanged();
    });
}
FlatpakCatalog::~FlatpakCatalog() { watcher_.waitForFinished(); }
bool FlatpakCatalog::loading() const { return watcher_.isRunning(); }
void FlatpakCatalog::load()
{
    if (loading()) {
        reloadPending_ = true;
        return;
    }
    watcher_.setFuture(QtConcurrent::run(readCatalogs));
    emit loadingChanged();
}
