#include "GitHubUpdateController.h"

#include "PackageKitBackend.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QVersionNumber>

#include <algorithm>

namespace {
constexpr qsizetype maxReleaseResponseBytes = 1024 * 1024;
constexpr qint64 maxPackageBytes = 512LL * 1024 * 1024;
constexpr auto apiUrl = "https://api.github.com/repos/Maxdemyd0/Plaza/releases/latest";

QString debianArchitecture()
{
    return GitHubUpdateController::debianArchitectureForCpu(QSysInfo::buildCpuArchitecture());
}

QString normalizedVersion(const QString& tag)
{
    QString version = tag.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) version.remove(0, 1);
    static const QRegularExpression validVersion(QStringLiteral("^\\d+(?:\\.\\d+){1,2}$"));
    return validVersion.match(version).hasMatch() ? version : QString();
}

QNetworkRequest githubRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "Plaza-Software-Center");
    return request;
}
}

GitHubUpdateController::GitHubUpdateController(
    QString currentVersion,
    PackageKitBackend* packageKit,
    QObject* parent
)
    : QObject(parent),
      currentVersion_(std::move(currentVersion)),
      packageKit_(packageKit)
{
    restartArguments_ = QCoreApplication::arguments();
    if (!restartArguments_.isEmpty()) restartArguments_.removeFirst();

    if (packageKit_ == nullptr) {
        errorText_ = tr("The system package installer is unavailable.");
        return;
    }

    connect(packageKit_, &PackageKitBackend::filesInstalled, this, [this] {
        installing_ = false;
        restartRequired_ = true;
        statusText_ = tr("Plaza %1 is installed. Restart Plaza to finish updating.").arg(latestVersion_);
        if (!downloadPath_.isEmpty()) QFile::remove(downloadPath_);
        downloadPath_.clear();
        emit changed();
    });

    connect(packageKit_, &PackageKitBackend::changed, this, [this] {
        if (!installing_ || packageKit_->busy() || packageKit_->errorText().isEmpty()) return;
        installing_ = false;
        setError(packageKit_->errorText());
    });
}

GitHubUpdateController::~GitHubUpdateController()
{
    if (downloadReply_ != nullptr) downloadReply_->abort();
    if (downloadFile_ != nullptr) {
        downloadFile_->cancelWriting();
        delete downloadFile_;
    }
}

bool GitHubUpdateController::checking() const { return checking_; }
bool GitHubUpdateController::updateAvailable() const { return updateAvailable_; }
bool GitHubUpdateController::downloadReady() const { return downloadReady_; }
bool GitHubUpdateController::busy() const { return checking_ || downloading_ || installing_; }
bool GitHubUpdateController::restartRequired() const { return restartRequired_; }
int GitHubUpdateController::progress() const
{
    if (downloading_) return downloadProgress_;
    if (installing_ && packageKit_ != nullptr) return packageKit_->progress();
    return 0;
}
QString GitHubUpdateController::latestVersion() const { return latestVersion_; }
QString GitHubUpdateController::statusText() const { return statusText_; }
QString GitHubUpdateController::errorText() const { return errorText_; }

bool GitHubUpdateController::isNewerVersion(const QString& candidate, const QString& current)
{
    const QString candidateVersion = normalizedVersion(candidate);
    const QString currentVersion = normalizedVersion(current);
    if (candidateVersion.isEmpty() || currentVersion.isEmpty()) return false;
    return QVersionNumber::compare(QVersionNumber::fromString(candidateVersion),
                                   QVersionNumber::fromString(currentVersion)) > 0;
}

QString GitHubUpdateController::debianArchitectureForCpu(const QString& cpuArchitecture)
{
    const QString architecture = cpuArchitecture.trimmed().toLower();
    if (architecture == QStringLiteral("x86_64") || architecture == QStringLiteral("x86-64")
        || architecture == QStringLiteral("amd64")) {
        return QStringLiteral("amd64");
    }
    if (architecture == QStringLiteral("i386") || architecture == QStringLiteral("i486")
        || architecture == QStringLiteral("i586") || architecture == QStringLiteral("i686")
        || architecture == QStringLiteral("x86")) {
        return QStringLiteral("i386");
    }
    if (architecture == QStringLiteral("aarch64") || architecture == QStringLiteral("arm64")) {
        return QStringLiteral("arm64");
    }
    if (architecture == QStringLiteral("arm") || architecture == QStringLiteral("armhf")
        || architecture == QStringLiteral("armv7") || architecture == QStringLiteral("armv7l")) {
        return QStringLiteral("armhf");
    }
    return {};
}

void GitHubUpdateController::checkForUpdates()
{
    if (busy()) return;
    checking_ = true;
    updateAvailable_ = false;
    downloadReady_ = false;
    restartRequired_ = false;
    assetUrl_.clear();
    expectedSha256_.clear();
    errorText_.clear();
    statusText_ = tr("Checking GitHub for Plaza updates…");
    emit changed();

    QNetworkReply* reply = network_.get(githubRequest(QUrl(QString::fromLatin1(apiUrl))));
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > maxReleaseResponseBytes) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray data = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        checking_ = false;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError || httpStatus != 200
            || data.size() > maxReleaseResponseBytes) {
            setError(tr("Could not check GitHub releases: %1").arg(reply->errorString()));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            setError(tr("GitHub returned invalid release information."));
            return;
        }

        const QJsonObject release = document.object();
        const QString version = normalizedVersion(release.value(QStringLiteral("tag_name")).toString());
        if (version.isEmpty() || release.value(QStringLiteral("draft")).toBool(true)
            || release.value(QStringLiteral("prerelease")).toBool(true)) {
            setError(tr("The latest GitHub release does not contain a valid stable Plaza version."));
            return;
        }

        latestVersion_ = version;
        updateAvailable_ = isNewerVersion(version, currentVersion_);
        if (!updateAvailable_) {
            statusText_ = tr("Plaza is up to date.");
            emit changed();
            return;
        }

        const QString architecture = debianArchitecture();
        const QString expectedAsset = QStringLiteral("plaza_%1_%2.deb").arg(version, architecture);
        for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString() != expectedAsset) continue;

            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            static const QRegularExpression digestPattern(QStringLiteral("^sha256:([A-Fa-f0-9]{64})$"));
            const auto digestMatch = digestPattern.match(asset.value(QStringLiteral("digest")).toString());
            if (url.scheme() != QStringLiteral("https") || url.host() != QStringLiteral("github.com")
                || !url.path().startsWith(QStringLiteral("/Maxdemyd0/Plaza/releases/download/"))
                || !digestMatch.hasMatch()
                || asset.value(QStringLiteral("size")).toInteger() <= 0
                || asset.value(QStringLiteral("size")).toInteger() > maxPackageBytes) {
                setError(tr("The Plaza release package is missing valid download or checksum information."));
                return;
            }

            assetUrl_ = url.toString();
            expectedSha256_ = digestMatch.captured(1).toLower();
            downloadReady_ = true;
            statusText_ = tr("Plaza %1 is available.").arg(latestVersion_);
            emit changed();
            return;
        }

        setError(architecture.isEmpty()
            ? tr("Plaza %1 is available, but this device's architecture is not supported yet.").arg(version)
            : tr("Plaza %1 is available, but GitHub has no matching Debian package for this device.").arg(version));
    });
}

void GitHubUpdateController::downloadAndInstall()
{
    if (busy() || !updateAvailable_ || !downloadReady_ || packageKit_ == nullptr) return;
    if (!packageKit_->supports(QStringLiteral("apt"))) {
        setError(tr("Installing Plaza updates requires a Debian-based system with PackageKit."));
        return;
    }

    const QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDirectory.isEmpty() || !QDir().mkpath(cacheDirectory)) {
        setError(tr("Could not create a folder for the Plaza update download."));
        return;
    }

    const QString architecture = debianArchitecture();
    downloadPath_ = QDir(cacheDirectory).filePath(
        QStringLiteral("plaza_%1_%2.deb").arg(latestVersion_, architecture));
    downloadFile_ = new QSaveFile(downloadPath_);
    if (!downloadFile_->open(QIODevice::WriteOnly)) {
        delete downloadFile_;
        downloadFile_ = nullptr;
        setError(tr("Could not create the Plaza update download file."));
        return;
    }

    hash_.reset();
    downloading_ = true;
    downloadProgress_ = 0;
    errorText_.clear();
    statusText_ = tr("Downloading Plaza %1…").arg(latestVersion_);
    emit changed();

    downloadReply_ = network_.get(githubRequest(QUrl(assetUrl_)));
    connect(downloadReply_, &QIODevice::readyRead, this, [this] {
        if (downloadReply_ == nullptr || downloadFile_ == nullptr) return;
        const QByteArray chunk = downloadReply_->readAll();
        if (downloadFile_->size() + chunk.size() > maxPackageBytes
            || downloadFile_->write(chunk) != chunk.size()) {
            setError(tr("The Plaza update download is too large or could not be saved."));
            downloadReply_->abort();
            return;
        }
        hash_.addData(chunk);
    });
    connect(downloadReply_, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) downloadProgress_ = std::clamp(static_cast<int>(received * 100 / total), 0, 100);
        emit changed();
    });
    connect(downloadReply_, &QNetworkReply::finished, this,
            [this, reply = downloadReply_] { finishDownload(reply); });
}

void GitHubUpdateController::finishDownload(QNetworkReply* reply)
{
    if (reply != downloadReply_) return;
    const QByteArray remainder = reply->readAll();
    if (!remainder.isEmpty() && downloadFile_ != nullptr) {
        if (downloadFile_->size() + remainder.size() > maxPackageBytes
            || downloadFile_->write(remainder) != remainder.size()) {
            setError(tr("The Plaza update download could not be saved."));
        } else {
            hash_.addData(remainder);
        }
    }

    const QString networkError = reply->errorString();
    const bool networkSucceeded = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
    downloadReply_ = nullptr;
    reply->deleteLater();
    downloading_ = false;

    if (!networkSucceeded || !errorText_.isEmpty() || downloadFile_ == nullptr) {
        if (errorText_.isEmpty()) setError(tr("Could not download the Plaza update: %1").arg(networkError));
        cleanupDownload();
        return;
    }

    const QString actualDigest = QString::fromLatin1(hash_.result().toHex());
    if (actualDigest != expectedSha256_) {
        setError(tr("The downloaded Plaza update failed its SHA-256 integrity check."));
        cleanupDownload();
        return;
    }

    if (!downloadFile_->commit()) {
        setError(tr("Could not finish saving the Plaza update."));
        cleanupDownload();
        return;
    }
    delete downloadFile_;
    downloadFile_ = nullptr;

    installing_ = true;
    statusText_ = tr("Requesting system authorization to install the update…");
    emit changed();
    if (!packageKit_->installFiles({downloadPath_})) {
        installing_ = false;
        setError(packageKit_->errorText().isEmpty()
            ? tr("The system package installer could not start the Plaza update.")
            : packageKit_->errorText());
        cleanupDownload();
    }
}

void GitHubUpdateController::restartPlaza()
{
    if (!restartRequired_) return;
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), restartArguments_)) {
        setError(tr("The update is installed, but Plaza could not restart automatically."));
        return;
    }
    QCoreApplication::quit();
}

void GitHubUpdateController::setError(const QString& error)
{
    errorText_ = error;
    statusText_.clear();
    emit changed();
    qWarning().noquote() << "Plaza update:" << error;
}

void GitHubUpdateController::cleanupDownload()
{
    if (downloadFile_ != nullptr) {
        downloadFile_->cancelWriting();
        delete downloadFile_;
        downloadFile_ = nullptr;
    }
    if (!downloadPath_.isEmpty()) QFile::remove(downloadPath_);
    downloadPath_.clear();
}
