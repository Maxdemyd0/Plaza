#include "FlatpakBackend.h"

#ifdef signals
#pragma push_macro("signals")
#undef signals
#define PLAZA_RESTORE_SIGNALS_MACRO
#endif

#include <flatpak.h>
#include <gio/gio.h>

#ifdef PLAZA_RESTORE_SIGNALS_MACRO
#pragma pop_macro("signals")
#undef PLAZA_RESTORE_SIGNALS_MACRO
#endif

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace {

QString errorText(const GError* error)
{
    return error != nullptr && error->message != nullptr
        ? QString::fromUtf8(error->message)
        : QStringLiteral("An unknown Flatpak error occurred.");
}

QString missingRemoteMessage(const QString& remote)
{
    if (remote.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral(
            "The Flathub software source is not configured."
        );
    }

    return QStringLiteral(
        "The %1 software source is not configured."
    ).arg(remote);
}

} // namespace

class FlatpakBackendWorker final : public QObject
{
    Q_OBJECT

public:
    explicit FlatpakBackendWorker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~FlatpakBackendWorker() override
    {
        cancel();

        if (installation_ != nullptr) {
            g_object_unref(installation_);
            installation_ = nullptr;
        }
    }

    void cancel()
    {
        QMutexLocker locker(&cancellableMutex_);
        cancelRequested_ = true;

        if (cancellable_ != nullptr) {
            g_cancellable_cancel(cancellable_);
        }
    }

public slots:
    void initialize()
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;

        const bool installationReady = ensureInstallation(
            cancellable,
            &error
        );

        QStringList appIds;
        if (installationReady) {
            appIds = readInstalledApps(cancellable, &error);
        }

        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        finishCancellable(cancellable);

        if (cancelled) {
            if (error != nullptr) {
                g_error_free(error);
            }
            return;
        }

        if (!installationReady || error != nullptr) {
            const QString message = errorText(error);

            qWarning().noquote()
                << "Could not read the user Flatpak installation cache:"
                << message;

            emit cacheLoadFailed(message);

            if (error != nullptr) {
                g_error_free(error);
            }

            return;
        }

        emit cacheLoaded(appIds);
    }

    void install(const QString& remoteName, const QString& appId)
    {
        if (remoteName.trimmed().isEmpty() || appId.trimmed().isEmpty()) {
            emit operationFailed(
                appId,
                QStringLiteral(
                    "The Flatpak source and application ID are required."
                )
            );
            return;
        }

        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        FlatpakRemote* remote = nullptr;
        FlatpakTransaction* transaction = nullptr;
        QString failureMessage;

        const bool installationReady = ensureInstallation(
            cancellable,
            &error
        );

        if (!installationReady) {
            failureMessage = errorText(error);
        }

        if (failureMessage.isEmpty()) {
            remote = flatpak_installation_get_remote_by_name(
                installation_,
                remoteName.toUtf8().constData(),
                cancellable,
                &error
            );

            if (remote == nullptr) {
                if (error != nullptr &&
                    error->domain == FLATPAK_ERROR &&
                    error->code != FLATPAK_ERROR_REMOTE_NOT_FOUND) {
                    qWarning().noquote()
                        << "Flatpak remote lookup failed for"
                        << remoteName << ":" << error->message;
                }

                failureMessage = missingRemoteMessage(remoteName);
            }
        }

        if (failureMessage.isEmpty() && flatpak_remote_get_disabled(remote)) {
            failureMessage = QStringLiteral("The %1 software source is disabled. Enable it on Sources and try again.").arg(remoteName);
        }

        if (failureMessage.isEmpty()) {
            transaction = flatpak_transaction_new_for_installation(
                installation_,
                cancellable,
                &error
            );

            if (transaction == nullptr) {
                failureMessage = errorText(error);
            }
        }

        if (failureMessage.isEmpty()) {
            flatpak_transaction_set_no_interaction(transaction, TRUE);
            flatpak_transaction_set_default_arch(
                transaction,
                flatpak_get_default_arch()
            );

            g_signal_connect(
                transaction,
                "new-operation",
                G_CALLBACK(&FlatpakBackendWorker::onNewOperation),
                this
            );

            g_signal_connect(
                transaction,
                "operation-error",
                G_CALLBACK(&FlatpakBackendWorker::onOperationError),
                this
            );

            const QString ref = QStringLiteral(
                "app/%1/%2/stable"
            ).arg(appId, QString::fromUtf8(flatpak_get_default_arch()));

            if (!flatpak_transaction_add_install(
                    transaction,
                    remoteName.toUtf8().constData(),
                    ref.toUtf8().constData(),
                    nullptr,
                    &error
                )) {
                failureMessage = errorText(error);
            }
        }

        bool transactionSucceeded = false;
        if (failureMessage.isEmpty()) {
            emit progress(0, QStringLiteral("Resolving…"));

            transactionSucceeded = flatpak_transaction_run(
                transaction,
                cancellable,
                &error
            );

            if (!transactionSucceeded) {
                if (!operationError_.isEmpty()) {
                    failureMessage = operationError_;
                } else {
                    failureMessage = errorText(error);
                }
            }
        }

        QStringList appIds;
        if (transactionSucceeded) {
            GError* cacheError = nullptr;
            appIds = readInstalledApps(cancellable, &cacheError);

            if (cacheError != nullptr) {
                qWarning().noquote()
                    << "Flatpak installation succeeded, but refreshing the"
                    << "installed-app cache failed:" << cacheError->message;
                g_error_free(cacheError);
                appIds.append(appId);
            }
        }

        const bool cancelled = g_cancellable_is_cancelled(cancellable);

        if (remote != nullptr) {
            g_object_unref(remote);
        }

        if (transaction != nullptr) {
            g_object_unref(transaction);
        }

        if (error != nullptr) {
            qWarning().noquote()
                << "Flatpak operation detail:" << error->message;
            g_error_free(error);
        }

        const QString completedError = failureMessage;
        finishCancellable(cancellable);
        operationError_.clear();

        if (cancelled) {
            emit operationFailed(
                appId,
                QStringLiteral("The Flatpak installation was cancelled.")
            );
        } else if (!transactionSucceeded) {
            emit operationFailed(
                appId,
                completedError.isEmpty()
                    ? QStringLiteral("The Flatpak installation failed.")
                    : completedError
            );
        } else {
            emit cacheLoaded(appIds);
            emit installSucceeded(appId);
        }
    }

    void refreshSources()
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        QVariantList sources;
        if (ensureInstallation(cancellable, &error)) {
            GPtrArray* remotes = flatpak_installation_list_remotes(installation_, cancellable, &error);
            if (remotes != nullptr) {
                for (guint i = 0; i < remotes->len; ++i) {
                    auto* remote = FLATPAK_REMOTE(g_ptr_array_index(remotes, i));
                    if (remote == nullptr) continue;
                    char* title = flatpak_remote_get_title(remote);
                    char* url = flatpak_remote_get_url(remote);
                    const char* name = flatpak_remote_get_name(remote);
                    sources.append(QVariantMap{
                        {QStringLiteral("remoteName"), QString::fromUtf8(name ? name : "")},
                        {QStringLiteral("title"), QString::fromUtf8(title ? title : "")},
                        {QStringLiteral("url"), QString::fromUtf8(url ? url : "")},
                        {QStringLiteral("remoteEnabled"), !flatpak_remote_get_disabled(remote)},
                        {QStringLiteral("gpgVerified"), flatpak_remote_get_gpg_verify(remote)}
                    });
                    g_free(title);
                    g_free(url);
                }
                g_ptr_array_unref(remotes);
            }
        }
        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        const bool failed = error != nullptr;
        const QString message = errorText(error);
        if (error != nullptr) g_error_free(error);
        finishCancellable(cancellable);
        if (cancelled) {
            emit operationFailed(QStringLiteral("sources"), QStringLiteral("Loading Flatpak sources was cancelled."));
            return;
        }
        if (failed) {
            emit operationFailed(QStringLiteral("sources"), message);
            return;
        }
        emit sourcesLoaded(sources, QStringLiteral("Sources refreshed"));
    }

    void addSource(const QString& name, const QString& url)
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        FlatpakRemote* remote = nullptr;
        QString failure;
        QByteArray contents;
        QNetworkAccessManager network;
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(30000);
        QNetworkReply* reply = network.get(request);
        QEventLoop eventLoop;
        QTimer sizeGuard;
        sizeGuard.setInterval(100);
        QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        QObject::connect(&sizeGuard, &QTimer::timeout, &eventLoop, [&] {
            if (reply->bytesAvailable() + contents.size() > 4 * 1024 * 1024) {
                reply->abort();
            }
            contents += reply->readAll();
        });
        sizeGuard.start();
        const gulong cancelHandler = g_cancellable_connect(
            cancellable,
            G_CALLBACK(+[](GCancellable*, gpointer data) {
                auto* activeReply = static_cast<QNetworkReply*>(data);
                QMetaObject::invokeMethod(activeReply, [activeReply] {
                    if (activeReply->isRunning()) activeReply->abort();
                }, Qt::QueuedConnection);
            }),
            reply,
            nullptr
        );
        eventLoop.exec();
        g_cancellable_disconnect(cancellable, cancelHandler);
        sizeGuard.stop();
        contents += reply->readAll();
        const QUrl finalUrl = reply->url();
        if (g_cancellable_is_cancelled(cancellable)) {
            failure = QStringLiteral("Adding the source was cancelled.");
        } else if (contents.size() > 4 * 1024 * 1024) {
            failure = QStringLiteral("The Flatpak source file is larger than the 4 MiB safety limit.");
        } else if (finalUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
            failure = QStringLiteral("The Flatpak source URL redirected to a non-HTTPS address.");
        } else if (reply->error() != QNetworkReply::NoError) {
            failure = QStringLiteral("Could not download the Flatpak source file: %1").arg(reply->errorString());
        }
        reply->deleteLater();

        if (failure.isEmpty()) {
            GBytes* bytes = g_bytes_new(contents.constData(), static_cast<gsize>(contents.size()));
            remote = flatpak_remote_new_from_file(name.toUtf8().constData(), bytes, &error);
            g_bytes_unref(bytes);
            if (remote == nullptr) failure = errorText(error);
        }

        if (failure.isEmpty() && !flatpak_remote_get_gpg_verify(remote)) {
            failure = QStringLiteral("This source does not enable GPG verification. Plaza only adds signed Flatpak sources.");
        }

        if (failure.isEmpty()) {
            flatpak_remote_set_gpg_verify(remote, TRUE);
            flatpak_remote_set_disabled(remote, FALSE);
            if (!ensureInstallation(cancellable, &error) ||
                !flatpak_installation_add_remote(installation_, remote, FALSE, cancellable, &error)) {
                failure = errorText(error);
            }
        }

        QString status = QStringLiteral("Source added");
        if (failure.isEmpty()) {
            gboolean changed = FALSE;
            GError* metadataError = nullptr;
            if (!flatpak_installation_update_appstream_sync(
                    installation_, name.toUtf8().constData(), flatpak_get_default_arch(),
                    &changed, cancellable, &metadataError)) {
                status = QStringLiteral("Source added, but its app catalog could not be refreshed: %1")
                    .arg(errorText(metadataError));
                if (metadataError != nullptr) {
                    qWarning().noquote() << "Flatpak source catalog refresh failed:" << metadataError->message;
                    g_error_free(metadataError);
                }
            }
        }

        if (remote != nullptr) g_object_unref(remote);
        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        if (error != nullptr) {
            qWarning().noquote() << "Flatpak source operation detail:" << error->message;
            g_error_free(error);
        }
        finishCancellable(cancellable);
        if (cancelled) {
            emit operationFailed(QStringLiteral("sources"), QStringLiteral("Adding the source was cancelled."));
        } else if (!failure.isEmpty()) {
            emit operationFailed(QStringLiteral("sources"), failure);
        } else {
            refreshSources();
            emit catalogSourcesChanged();
            emit sourceOperationStatus(status);
        }
    }

    void setSourceEnabled(const QString& name, const bool enabled)
    {
        modifySource(name, [enabled](FlatpakRemote* remote) {
            flatpak_remote_set_disabled(remote, enabled ? FALSE : TRUE);
        }, enabled ? QStringLiteral("Source enabled") : QStringLiteral("Source disabled"));
    }

    void removeSource(const QString& name)
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        bool removed = ensureInstallation(cancellable, &error) &&
            flatpak_installation_remove_remote(installation_, name.toUtf8().constData(), cancellable, &error);
        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        const QString message = errorText(error);
        if (error != nullptr) {
            qWarning().noquote() << "Could not remove Flatpak source" << name << ":" << error->message;
            g_error_free(error);
        }
        finishCancellable(cancellable);
        if (cancelled) emit operationFailed(QStringLiteral("sources"), QStringLiteral("Removing the source was cancelled."));
        else if (!removed) emit operationFailed(QStringLiteral("sources"), message);
        else {
            refreshSources();
            emit catalogSourcesChanged();
            emit sourceOperationStatus(QStringLiteral("Source removed. Installed apps were left in place."));
        }
    }

    void launch(const QString& appId)
    {
        if (appId.trimmed().isEmpty()) {
            emit launchFailed(
                appId,
                QStringLiteral("The Flatpak application ID is empty.")
            );
            return;
        }

        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        const bool installationReady = ensureInstallation(
            cancellable,
            &error
        );

        bool launched = false;
        if (installationReady) {
            launched = flatpak_installation_launch(
                installation_,
                appId.toUtf8().constData(),
                flatpak_get_default_arch(),
                "stable",
                nullptr,
                cancellable,
                &error
            );
        }

        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        const QString message = errorText(error);

        if (error != nullptr) {
            qWarning().noquote()
                << "Could not launch Flatpak application"
                << appId << ":" << error->message;
            g_error_free(error);
        }

        finishCancellable(cancellable);

        if (launched) {
            emit launchSucceeded(appId);
        } else {
            emit launchFailed(
                appId,
                cancelled
                    ? QStringLiteral("Launching the Flatpak application was cancelled.")
                    : message
            );
        }
    }

    void checkUpdates()
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        QVariantList updates;

        if (ensureInstallation(cancellable, &error)) {
            GPtrArray* refs = flatpak_installation_list_installed_refs_for_update(
                installation_, cancellable, &error);
            if (refs != nullptr) {
                for (guint i = 0; i < refs->len; ++i) {
                    auto* ref = FLATPAK_REF(g_ptr_array_index(refs, i));
                    if (ref == nullptr) continue;
                    char* formattedRef = flatpak_ref_format_ref(ref);
                    if (formattedRef == nullptr) continue;
                    const QString refName = QString::fromUtf8(formattedRef);
                    g_free(formattedRef);
                    updates.append(QVariantMap{
                        {QStringLiteral("appId"), QString::fromUtf8(flatpak_ref_get_name(ref))},
                        {QStringLiteral("name"), QString::fromUtf8(flatpak_ref_get_name(ref))},
                        {QStringLiteral("ref"), refName},
                        {QStringLiteral("branch"), QString::fromUtf8(flatpak_ref_get_branch(ref))},
                        {QStringLiteral("kind"), flatpak_ref_get_kind(ref) == FLATPAK_REF_KIND_APP
                            ? QStringLiteral("app") : QStringLiteral("runtime")}
                    });
                }
                g_ptr_array_unref(refs);
            }
        }

        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        const bool failed = error != nullptr;
        const QString message = errorText(error);
        if (error != nullptr) g_error_free(error);
        finishCancellable(cancellable);

        if (cancelled) {
            emit operationFailed(QStringLiteral("updates"), QStringLiteral("Flatpak update check was cancelled."));
            return;
        }
        if (failed) {
            emit operationFailed(QStringLiteral("updates"), message);
            return;
        }
        emit updatesLoaded(updates);
    }

    void updateAll(const QVariantList& updates)
    {
        if (updates.isEmpty()) {
            emit updatesInstalled({});
            return;
        }

        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        FlatpakTransaction* transaction = nullptr;
        QString failureMessage;
        QStringList updatedAppIds;
        const bool installationReady = ensureInstallation(cancellable, &error);
        if (!installationReady) failureMessage = errorText(error);

        if (failureMessage.isEmpty()) {
            transaction = flatpak_transaction_new_for_installation(installation_, cancellable, &error);
            if (transaction == nullptr) failureMessage = errorText(error);
        }

        if (failureMessage.isEmpty()) {
            flatpak_transaction_set_no_interaction(transaction, TRUE);
            flatpak_transaction_set_default_arch(transaction, flatpak_get_default_arch());
            g_signal_connect(transaction, "new-operation",
                             G_CALLBACK(&FlatpakBackendWorker::onNewOperation), this);
            g_signal_connect(transaction, "operation-error",
                             G_CALLBACK(&FlatpakBackendWorker::onOperationError), this);

            for (const QVariant& update : updates) {
                const QVariantMap entry = update.toMap();
                const QString ref = entry.value(QStringLiteral("ref")).toString();
                if (ref.isEmpty() || !flatpak_transaction_add_update(
                        transaction, ref.toUtf8().constData(), nullptr, nullptr, &error)) {
                    failureMessage = ref.isEmpty()
                        ? QStringLiteral("An update contained an invalid Flatpak reference.")
                        : errorText(error);
                    break;
                }
                const QString appId = entry.value(QStringLiteral("appId")).toString();
                if (!appId.isEmpty()) updatedAppIds.append(appId);
            }
        }

        bool succeeded = false;
        if (failureMessage.isEmpty()) {
            emit progress(0, QStringLiteral("Preparing Flatpak updates…"));
            succeeded = flatpak_transaction_run(transaction, cancellable, &error);
            if (!succeeded) failureMessage = operationError_.isEmpty()
                ? errorText(error) : operationError_;
        }

        if (succeeded) {
            GError* cacheError = nullptr;
            const QStringList appIds = readInstalledApps(cancellable, &cacheError);
            if (cacheError != nullptr) {
                qWarning().noquote() << "Flatpak updates succeeded but refreshing the installed-app cache failed:"
                                     << cacheError->message;
                g_error_free(cacheError);
            } else {
                emit cacheLoaded(appIds);
            }
        }

        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        if (transaction != nullptr) g_object_unref(transaction);
        if (error != nullptr) {
            qWarning().noquote() << "Flatpak update detail:" << error->message;
            g_error_free(error);
        }
        finishCancellable(cancellable);
        operationError_.clear();

        if (cancelled) {
            emit operationFailed(QStringLiteral("updates"), QStringLiteral("Flatpak updates were cancelled."));
        } else if (!succeeded) {
            emit operationFailed(QStringLiteral("updates"), failureMessage.isEmpty()
                ? QStringLiteral("Flatpak updates failed.") : failureMessage);
        } else {
            emit updatesInstalled(updatedAppIds);
        }
    }

signals:
    void cacheLoaded(const QStringList& appIds);
    void cacheLoadFailed(const QString& message);
    void progress(int progress, const QString& status);
    void installSucceeded(const QString& appId);
    void operationFailed(const QString& appId, const QString& message);
    void launchSucceeded(const QString& appId);
    void launchFailed(const QString& appId, const QString& message);
    void updatesLoaded(const QVariantList& updates);
    void updatesInstalled(const QStringList& appIds);
    void sourcesLoaded(const QVariantList& sources, const QString& status);
    void sourceOperationStatus(const QString& status);
    void catalogSourcesChanged();

private:
    template<typename Configure>
    void modifySource(const QString& name, Configure configure, const QString& status)
    {
        GCancellable* cancellable = beginCancellable();
        GError* error = nullptr;
        FlatpakRemote* remote = nullptr;
        bool succeeded = ensureInstallation(cancellable, &error);
        if (succeeded) {
            remote = flatpak_installation_get_remote_by_name(
                installation_, name.toUtf8().constData(), cancellable, &error);
            succeeded = remote != nullptr;
        }
        if (succeeded) {
            configure(remote);
            succeeded = flatpak_installation_modify_remote(installation_, remote, cancellable, &error);
        }
        if (remote != nullptr) g_object_unref(remote);
        const bool cancelled = g_cancellable_is_cancelled(cancellable);
        const QString message = errorText(error);
        if (error != nullptr) {
            qWarning().noquote() << "Could not update Flatpak source" << name << ":" << error->message;
            g_error_free(error);
        }
        finishCancellable(cancellable);
        if (cancelled) emit operationFailed(QStringLiteral("sources"), QStringLiteral("Updating the source was cancelled."));
        else if (!succeeded) emit operationFailed(QStringLiteral("sources"), message);
        else {
            refreshSources();
            emit catalogSourcesChanged();
            emit sourceOperationStatus(status);
        }
    }

    bool ensureInstallation(GCancellable* cancellable, GError** error)
    {
        if (installation_ != nullptr) {
            return true;
        }

        installation_ = flatpak_installation_new_user(cancellable, error);
        if (installation_ != nullptr) {
            flatpak_installation_set_no_interaction(installation_, TRUE);
        }

        return installation_ != nullptr;
    }

    QStringList readInstalledApps(
        GCancellable* cancellable,
        GError** error
    ) const
    {
        QStringList appIds;
        GPtrArray* refs = flatpak_installation_list_installed_refs(
            installation_,
            cancellable,
            error
        );

        if (refs == nullptr) {
            return appIds;
        }

        for (guint i = 0; i < refs->len; ++i) {
            auto* ref = static_cast<FlatpakRef*>(
                g_ptr_array_index(refs, i)
            );

            if (ref != nullptr &&
                flatpak_ref_get_kind(ref) == FLATPAK_REF_KIND_APP) {
                appIds.append(QString::fromUtf8(flatpak_ref_get_name(ref)));
            }
        }

        g_ptr_array_unref(refs);
        return appIds;
    }

    GCancellable* beginCancellable()
    {
        auto* cancellable = g_cancellable_new();

        QMutexLocker locker(&cancellableMutex_);
        cancellable_ = cancellable;

        if (cancelRequested_) {
            g_cancellable_cancel(cancellable_);
        }

        return cancellable;
    }

    void finishCancellable(GCancellable* cancellable)
    {
        {
            QMutexLocker locker(&cancellableMutex_);
            if (cancellable_ == cancellable) {
                cancellable_ = nullptr;
            }
            cancelRequested_ = false;
        }

        g_object_unref(cancellable);
    }

    static void onNewOperation(
        FlatpakTransaction*,
        FlatpakTransactionOperation* operation,
        FlatpakTransactionProgress* progressObject,
        gpointer userData
    )
    {
        auto* worker = static_cast<FlatpakBackendWorker*>(userData);

        flatpak_transaction_progress_set_update_frequency(
            progressObject,
            200
        );

        g_signal_connect(
            progressObject,
            "changed",
            G_CALLBACK(&FlatpakBackendWorker::onProgressChanged),
            worker
        );

        const char* ref = flatpak_transaction_operation_get_ref(operation);
        const QString status = ref != nullptr
            ? QStringLiteral("Installing %1…").arg(QString::fromUtf8(ref))
            : QStringLiteral("Installing…");

        emit worker->progress(0, status);
        worker->onProgressChanged(progressObject, worker);
    }

    static gboolean onOperationError(
        FlatpakTransaction*,
        FlatpakTransactionOperation* operation,
        const GError* error,
        FlatpakTransactionErrorDetails,
        gpointer userData
    )
    {
        auto* worker = static_cast<FlatpakBackendWorker*>(userData);
        worker->operationError_ = errorText(error);

        const char* ref = flatpak_transaction_operation_get_ref(operation);
        qWarning().noquote()
            << "Flatpak transaction operation failed"
            << (ref != nullptr ? ref : "")
            << ":" << worker->operationError_;

        return FALSE;
    }

    static void onProgressChanged(
        FlatpakTransactionProgress* progressObject,
        gpointer userData
    )
    {
        auto* worker = static_cast<FlatpakBackendWorker*>(userData);
        char* status = flatpak_transaction_progress_get_status(
            progressObject
        );

        const QString statusText = status != nullptr && status[0] != '\0'
            ? QString::fromUtf8(status)
            : QStringLiteral("Installing…");

        emit worker->progress(
            qBound(0, flatpak_transaction_progress_get_progress(progressObject), 100),
            statusText
        );

        g_free(status);
    }

    FlatpakInstallation* installation_ = nullptr;
    GCancellable* cancellable_ = nullptr;
    QMutex cancellableMutex_;
    bool cancelRequested_ = false;
    QString operationError_;
};

FlatpakBackend::FlatpakBackend(QObject* parent)
    : QObject(parent),
      worker_(new FlatpakBackendWorker),
      workerThread_(this)
{
    worker_->moveToThread(&workerThread_);

    connect(
        &workerThread_,
        &QThread::started,
        worker_,
        &FlatpakBackendWorker::initialize
    );

    connect(worker_, &FlatpakBackendWorker::cacheLoaded,
            this, &FlatpakBackend::onCacheLoaded);
    connect(worker_, &FlatpakBackendWorker::cacheLoadFailed,
            this, &FlatpakBackend::onCacheLoadFailed);
    connect(worker_, &FlatpakBackendWorker::progress,
            this, &FlatpakBackend::onProgress);
    connect(worker_, &FlatpakBackendWorker::installSucceeded,
            this, &FlatpakBackend::onInstallSucceeded);
    connect(worker_, &FlatpakBackendWorker::operationFailed,
            this, &FlatpakBackend::onOperationFailed);
    connect(worker_, &FlatpakBackendWorker::launchSucceeded,
            this, &FlatpakBackend::onLaunchSucceeded);
    connect(worker_, &FlatpakBackendWorker::launchFailed,
            this, &FlatpakBackend::onLaunchFailed);
    connect(worker_, &FlatpakBackendWorker::updatesLoaded,
            this, &FlatpakBackend::onUpdatesLoaded);
    connect(worker_, &FlatpakBackendWorker::updatesInstalled,
            this, &FlatpakBackend::onUpdatesInstalled);
    connect(worker_, &FlatpakBackendWorker::sourcesLoaded,
            this, &FlatpakBackend::onSourcesLoaded);
    connect(worker_, &FlatpakBackendWorker::sourceOperationStatus,
            this, [this](const QString& status) {
                sourceStatusText_ = status;
                emit sourceStatusTextChanged();
            });
    connect(worker_, &FlatpakBackendWorker::catalogSourcesChanged,
            this, &FlatpakBackend::sourcesChanged);

    workerThread_.start();
}

FlatpakBackend::~FlatpakBackend()
{
    worker_->cancel();
    workerThread_.quit();
    workerThread_.wait();

    delete worker_;
    worker_ = nullptr;
}

bool FlatpakBackend::busy() const
{
    return busy_;
}

QString FlatpakBackend::currentAppId() const
{
    return currentAppId_;
}

int FlatpakBackend::progress() const
{
    return progress_;
}

QString FlatpakBackend::statusText() const
{
    return statusText_;
}

bool FlatpakBackend::cacheReady() const
{
    return cacheReady_;
}

int FlatpakBackend::cacheVersion() const
{
    return cacheVersion_;
}

QString FlatpakBackend::errorText() const
{
    return errorText_;
}

QVariantList FlatpakBackend::updates() const
{
    return updates_;
}

QVariantList FlatpakBackend::remotes() const { return remotes_; }

QString FlatpakBackend::sourceStatusText() const { return sourceStatusText_; }

bool FlatpakBackend::isInstalled(const QString& appId) const
{
    return installedApps_.contains(appId);
}

void FlatpakBackend::install(
    const QString& remote,
    const QString& appId
)
{
    if (busy_) {
        return;
    }

    clearError();
    setBusy(true);
    setCurrentAppId(appId);
    setProgress(0);
    setStatusText(QStringLiteral("Preparing installation…"));

    QMetaObject::invokeMethod(
        worker_,
        "install",
        Qt::QueuedConnection,
        Q_ARG(QString, remote),
        Q_ARG(QString, appId)
    );
}

void FlatpakBackend::cancel()
{
    if (!busy_) {
        return;
    }

    setStatusText(QStringLiteral("Cancelling installation…"));
    worker_->cancel();
}

void FlatpakBackend::launch(const QString& appId)
{
    if (busy_ || !isInstalled(appId)) {
        return;
    }

    clearError();

    QMetaObject::invokeMethod(
        worker_,
        "launch",
        Qt::QueuedConnection,
        Q_ARG(QString, appId)
    );
}

void FlatpakBackend::clearError()
{
    setErrorText(QString());
}

void FlatpakBackend::checkUpdates()
{
    if (busy_) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("updates"));
    setProgress(0);
    setStatusText(QStringLiteral("Checking Flatpak updates…"));
    QMetaObject::invokeMethod(worker_, "checkUpdates", Qt::QueuedConnection);
}

void FlatpakBackend::updateAll()
{
    if (busy_ || updates_.isEmpty()) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("updates"));
    setProgress(0);
    setStatusText(QStringLiteral("Preparing Flatpak updates…"));
    QMetaObject::invokeMethod(worker_, "updateAll", Qt::QueuedConnection,
                              Q_ARG(QVariantList, updates_));
}

void FlatpakBackend::refreshSources()
{
    if (busy_) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("sources"));
    setProgress(0);
    setStatusText(QStringLiteral("Loading Flatpak sources…"));
    QMetaObject::invokeMethod(worker_, "refreshSources", Qt::QueuedConnection);
}

void FlatpakBackend::addSource(const QString& name, const QString& url)
{
    const QString normalizedName = name.trimmed();
    const QUrl parsedUrl(url.trimmed());
    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$"));
    if (!validName.match(normalizedName).hasMatch()) {
        setErrorText(QStringLiteral("Source names must start with a letter or number and contain only letters, numbers, underscores, or hyphens."));
        emit failed(QStringLiteral("sources"), errorText_);
        return;
    }
    if (!parsedUrl.isValid() || parsedUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
        parsedUrl.host().isEmpty() || !parsedUrl.userInfo().isEmpty() || !parsedUrl.fragment().isEmpty()) {
        setErrorText(QStringLiteral("Enter a valid HTTPS .flatpakrepo URL without embedded credentials."));
        emit failed(QStringLiteral("sources"), errorText_);
        return;
    }
    if (busy_) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("sources"));
    setProgress(0);
    setStatusText(QStringLiteral("Adding Flatpak source…"));
    QMetaObject::invokeMethod(worker_, "addSource", Qt::QueuedConnection,
                              Q_ARG(QString, normalizedName),
                              Q_ARG(QString, parsedUrl.toString()));
}

void FlatpakBackend::setSourceEnabled(const QString& name, const bool enabled)
{
    if (busy_) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("sources"));
    setStatusText(enabled ? QStringLiteral("Enabling source…") : QStringLiteral("Disabling source…"));
    QMetaObject::invokeMethod(worker_, "setSourceEnabled", Qt::QueuedConnection,
                              Q_ARG(QString, name), Q_ARG(bool, enabled));
}

void FlatpakBackend::removeSource(const QString& name)
{
    if (busy_) return;
    clearError();
    setBusy(true);
    setCurrentAppId(QStringLiteral("sources"));
    setStatusText(QStringLiteral("Removing source…"));
    QMetaObject::invokeMethod(worker_, "removeSource", Qt::QueuedConnection,
                              Q_ARG(QString, name));
}

void FlatpakBackend::onCacheLoaded(const QStringList& appIds)
{
    installedApps_ = QSet<QString>(appIds.cbegin(), appIds.cend());

    const bool wasReady = cacheReady_;
    cacheReady_ = true;
    ++cacheVersion_;

    if (!wasReady) {
        emit cacheReadyChanged();
    }
    emit cacheVersionChanged();
}

void FlatpakBackend::onCacheLoadFailed(const QString& message)
{
    qWarning().noquote()
        << "Flatpak installed-app cache is unavailable:" << message;
}

void FlatpakBackend::onProgress(int progressValue, const QString& status)
{
    if (!busy_) {
        return;
    }

    setProgress(progressValue);
    setStatusText(status);
}

void FlatpakBackend::onInstallSucceeded(const QString& appId)
{
    setProgress(100);
    setStatusText(QStringLiteral("Installed"));
    setBusy(false);
    setCurrentAppId(QString());
    emit installed(appId);
}

void FlatpakBackend::onOperationFailed(
    const QString& appId,
    const QString& message
)
{
    qWarning().noquote()
        << "Flatpak installation failed for" << appId << ":" << message;

    setProgress(0);
    setStatusText(appId == QStringLiteral("updates")
        ? QStringLiteral("Update operation failed")
        : QStringLiteral("Ready to install"));
    setErrorText(message);
    setBusy(false);
    setCurrentAppId(QString());
    emit failed(appId, message);
}

void FlatpakBackend::onLaunchSucceeded(const QString& appId)
{
    emit launched(appId);
}

void FlatpakBackend::onLaunchFailed(
    const QString& appId,
    const QString& message
)
{
    const QString usefulMessage = message.isEmpty()
        ? QStringLiteral("Could not launch the Flatpak application.")
        : message;

    setErrorText(usefulMessage);
    emit failed(appId, usefulMessage);
}

void FlatpakBackend::onUpdatesLoaded(const QVariantList& updates)
{
    updates_ = updates;
    emit updatesChanged();
    setProgress(100);
    setStatusText(updates_.isEmpty()
        ? QStringLiteral("Flatpak is up to date")
        : QStringLiteral("Flatpak updates are available"));
    setBusy(false);
    setCurrentAppId(QString());
}

void FlatpakBackend::onUpdatesInstalled(const QStringList& appIds)
{
    Q_UNUSED(appIds)
    updates_.clear();
    emit updatesChanged();
    setProgress(100);
    setStatusText(QStringLiteral("Flatpak updates installed"));
    setBusy(false);
    setCurrentAppId(QString());
    emit updatesInstalled();
}

void FlatpakBackend::onSourcesLoaded(const QVariantList& sources, const QString& status)
{
    remotes_ = sources;
    emit remotesChanged();
    sourceStatusText_ = status;
    emit sourceStatusTextChanged();
    setProgress(100);
    setStatusText(status);
    setBusy(false);
    setCurrentAppId(QString());
}

void FlatpakBackend::setBusy(const bool busyValue)
{
    if (busy_ == busyValue) {
        return;
    }

    busy_ = busyValue;
    emit busyChanged();
}

void FlatpakBackend::setCurrentAppId(const QString& appId)
{
    if (currentAppId_ == appId) {
        return;
    }

    currentAppId_ = appId;
    emit currentAppIdChanged();
}

void FlatpakBackend::setProgress(const int progressValue)
{
    const int boundedProgress = qBound(0, progressValue, 100);
    if (progress_ == boundedProgress) {
        return;
    }

    progress_ = boundedProgress;
    emit progressChanged();
}

void FlatpakBackend::setStatusText(const QString& status)
{
    if (statusText_ == status) {
        return;
    }

    statusText_ = status;
    emit statusTextChanged();
}

void FlatpakBackend::setErrorText(const QString& error)
{
    if (errorText_ == error) {
        return;
    }

    errorText_ = error;
    emit errorTextChanged();
}

#include "FlatpakBackend.moc"
