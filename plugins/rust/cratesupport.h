#ifndef CRATE_H
#define CRATE_H

#include <utils/filepath.h>
#include <utils/process.h>

#include <QFutureWatcher>
#include <QTimer>
#include <QUrl>

namespace Rusty::Internal {

class CratePackageInfo
{
public:
    QString name;
    QString version;
    QString summary;
    QUrl homePage;
    QString author;
    QString authorEmail;
    QString license;
    Utils::FilePath location;
    QStringList requiresPackage;
    QStringList requiredByPackage;
    Utils::FilePaths files;

    void parseField(const QString &field, const QStringList &value);
};

class CratePackage
{
public:
    explicit CratePackage(const QString &packageName = {},
                        const QString &displayName = {},
                        const QString &version = {})
        : packageName(packageName)
        , displayName(displayName.isEmpty() ? packageName : displayName)
        , version(version)
    {}
    QString packageName;
    QString displayName;
    QString version;
};

class Crate : public QObject
{
public:
    static Crate *instance(const Utils::FilePath &python);

    QFuture<CratePackageInfo> info(const CratePackage &package);

private:
    Crate(const Utils::FilePath &rust);

    Utils::FilePath m_rust;
};

class CrateInstallTask : public QObject
{
    Q_OBJECT
public:
    explicit CrateInstallTask(const Utils::FilePath &python);
    void addPackage(const CratePackage &package);
    void setPackages(const QList<CratePackage> &packages);
    void run();

signals:
    void finished(bool success);

private:
    void cancel();
    void handleDone();
    void handleOutput();
    void handleError();

    QString packagesDisplayName() const;

    const Utils::FilePath m_rust;
    QList<CratePackage> m_packages;
    Utils::Process m_process;
    QFutureInterface<void> m_future;
    QFutureWatcher<void> m_watcher;
    QTimer m_killTimer;
};

} // Python::Internal

#endif // CRATE_H
