#include "cratesupport.h"

#include "rusty.h"
#include "rusttr.h"

#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/progressmanager.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <utils/algorithm.h>
#include <utils/async.h>
#include <utils/mimeutils.h>
#include <utils/process.h>

using namespace Utils;

namespace Rusty::Internal {

const char crateInstallTaskId[] = "Rust::crateInstallTask";

CrateInstallTask::CrateInstallTask(const FilePath &python)
    : m_rust(python)
{
    connect(&m_process, &Process::done, this, &CrateInstallTask::handleDone);
    connect(&m_process, &Process::readyReadStandardError, this, &CrateInstallTask::handleError);
    connect(&m_process, &Process::readyReadStandardOutput, this, &CrateInstallTask::handleOutput);
    connect(&m_killTimer, &QTimer::timeout, this, &CrateInstallTask::cancel);
    connect(&m_watcher, &QFutureWatcher<void>::canceled, this, &CrateInstallTask::cancel);
    m_watcher.setFuture(m_future.future());
}

void CrateInstallTask::addPackage(const CratePackage &package)
{
    m_packages << package;
}

void CrateInstallTask::setPackages(const QList<CratePackage> &packages)
{
    m_packages = packages;
}

void CrateInstallTask::run()
{
    if (m_packages.isEmpty()) {
        emit finished(false);
        return;
    }
    const QString taskTitle = Tr::tr("Install Python Packages");
    Core::ProgressManager::addTask(m_future.future(), taskTitle, crateInstallTaskId);
    QStringList arguments = {"-m", "pip", "install"};
    for (const CratePackage &package : m_packages) {
        QString pipPackage = package.packageName;
        if (!package.version.isEmpty())
            pipPackage += "==" + package.version;
        arguments << pipPackage;
    }

    // add --user to global rusts, but skip it for venv rusts
    if (!QDir(m_rust.parentDir().toString()).exists("activate"))
        arguments << "--user";

    m_process.setCommand({m_rust, arguments});
    m_process.start();

    Core::MessageManager::writeDisrupting(
        Tr::tr("Running \"%1\" to install %2.")
            .arg(m_process.commandLine().toUserOutput(), packagesDisplayName()));

    m_killTimer.setSingleShot(true);
    m_killTimer.start(5 /*minutes*/ * 60 * 1000);
}

void CrateInstallTask::cancel()
{
    m_process.stop();
    m_process.waitForFinished();
    Core::MessageManager::writeFlashing(
        m_killTimer.isActive()
            ? Tr::tr("The installation of \"%1\" was canceled by timeout.").arg(packagesDisplayName())
            : Tr::tr("The installation of \"%1\" was canceled by the user.")
                  .arg(packagesDisplayName()));
}

void CrateInstallTask::handleDone()
{
    m_future.reportFinished();
    const bool success = m_process.result() == ProcessResult::FinishedWithSuccess;
    if (!success) {
        Core::MessageManager::writeFlashing(Tr::tr("Installing \"%1\" failed with exit code %2.")
                                                .arg(packagesDisplayName())
                                                .arg(m_process.exitCode()));
    }
    emit finished(success);
}

void CrateInstallTask::handleOutput()
{
    const QString &stdOut = QString::fromLocal8Bit(m_process.readAllRawStandardOutput().trimmed());
    if (!stdOut.isEmpty())
        Core::MessageManager::writeSilently(stdOut);
}

void CrateInstallTask::handleError()
{
    const QString &stdErr = QString::fromLocal8Bit(m_process.readAllRawStandardError().trimmed());
    if (!stdErr.isEmpty())
        Core::MessageManager::writeSilently(stdErr);
}

QString CrateInstallTask::packagesDisplayName() const
{
    return Utils::transform(m_packages, &CratePackage::displayName).join(", ");
}

void CratePackageInfo::parseField(const QString &field, const QStringList &data)
{
    if (field.isEmpty())
        return;
    if (field == "Name") {
        name = data.value(0);
    } else if (field == "Version") {
        version = data.value(0);
    } else if (field == "Summary") {
        summary = data.value(0);
    } else if (field == "Home-page") {
        homePage = QUrl(data.value(0));
    } else if (field == "Author") {
        author = data.value(0);
    } else if (field == "Author-email") {
        authorEmail = data.value(0);
    } else if (field == "License") {
        license = data.value(0);
    } else if (field == "Location") {
        location = FilePath::fromUserInput(data.value(0)).normalizedPathName();
    } else if (field == "Requires") {
        requiresPackage = data.value(0).split(',', Qt::SkipEmptyParts);
    } else if (field == "Required-by") {
        requiredByPackage = data.value(0).split(',', Qt::SkipEmptyParts);
    } else if (field == "Files") {
        for (const QString &fileName : data) {
            if (!fileName.isEmpty())
                files.append(FilePath::fromUserInput(fileName.trimmed()));
        }
    }
}

Crate *Crate::instance(const FilePath &rust)
{
    static QMap<FilePath, Crate *> pips;
    auto it = pips.find(rust);
    if (it == pips.end())
        it = pips.insert(rust, new Crate(rust));
    return it.value();
}

static CratePackageInfo infoImpl(const CratePackage &package, const FilePath &rust)
{
    CratePackageInfo result;

    Process crate;
    crate.setCommand(CommandLine(rust, {"-m", "pip", "show", "-f", package.packageName}));
    crate.runBlocking();
    QString fieldName;
    QStringList data;
    const QString crateOutput = crate.allOutput();
    for (const QString &line : crateOutput.split('\n')) {
        if (line.isEmpty())
            continue;
        if (line.front().isSpace()) {
            data.append(line.trimmed());
        } else {
            result.parseField(fieldName, data);
            if (auto colonPos = line.indexOf(':'); colonPos >= 0) {
                fieldName = line.left(colonPos);
                data = QStringList(line.mid(colonPos + 1).trimmed());
            } else {
                fieldName.clear();
                data.clear();
            }
        }
    }
    result.parseField(fieldName, data);
    return result;
}

QFuture<CratePackageInfo> Crate::info(const CratePackage &package)
{
    return Utils::asyncRun(infoImpl, package, m_rust);
}

Crate::Crate(const Utils::FilePath &rust)
    : QObject(RustyPlugin::instance())
    , m_rust(rust)
{}

} // Rust::Internal
