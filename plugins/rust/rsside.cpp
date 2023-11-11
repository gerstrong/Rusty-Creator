#include "rsside.h"

#include "cratesupport.h"
#include "rusty.h"
#include "rusttr.h"
#include "rustutils.h"

#include <coreplugin/icore.h>

#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>

#include <texteditor/textdocument.h>

#include <utils/algorithm.h>
#include <utils/async.h>
#include <utils/infobar.h>
#include <utils/process.h>
#include <utils/qtcassert.h>

#include <QRegularExpression>
#include <QTextCursor>

using namespace Utils;
using namespace ProjectExplorer;

namespace Rusty::Internal {

const char installPySideInfoBarId[] = "Python::InstallPySide";

RsSideInstaller *RsSideInstaller::instance()
{
    static RsSideInstaller *instance = new RsSideInstaller; // FIXME: Leaks.
    return instance;
}

void RsSideInstaller::checkRsSideInstallation(const FilePath &rustc,
                                              TextEditor::TextDocument *document)
{
    document->infoBar()->removeInfo(installPySideInfoBarId);
    const QString pySide = importedRsSide(document->plainText());
    if (pySide == "PySide2" || pySide == "PySide6")
        instance()->runRsSideChecker(rustc, pySide, document);
}

bool RsSideInstaller::missingRsSideInstallation(const FilePath &pythonPath,
                                                const QString &pySide)
{
    QTC_ASSERT(!pySide.isEmpty(), return false);
    static QMap<FilePath, QSet<QString>> pythonWithPyside;
    if (pythonWithPyside[pythonPath].contains(pySide))
        return false;

    Process pythonProcess;
    pythonProcess.setCommand({pythonPath, {"-c", "import " + pySide}});
    pythonProcess.runBlocking();
    const bool missing = pythonProcess.result() != ProcessResult::FinishedWithSuccess;
    if (!missing)
        pythonWithPyside[pythonPath].insert(pySide);
    return missing;
}

QString RsSideInstaller::importedRsSide(const QString &text)
{
    static QRegularExpression importScanner("^\\s*(import|from)\\s+(PySide\\d)",
                                            QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = importScanner.match(text);
    return match.captured(2);
}

RsSideInstaller::RsSideInstaller()
    : QObject(RustyPlugin::instance())
{}

void RsSideInstaller::installRsside(const FilePath &python,
                                    const QString &pySide,
                                    TextEditor::TextDocument *document)
{
    document->infoBar()->removeInfo(installPySideInfoBarId);

    auto install = new CrateInstallTask(python);
    connect(install, &CrateInstallTask::finished, install, &QObject::deleteLater);
    connect(install, &CrateInstallTask::finished, this, [=](bool success){
        if (success)
            emit rsSideInstalled(python, pySide);
    });
    install->setPackages({CratePackage(pySide)});
    install->run();
}

void RsSideInstaller::handleRsSideMissing(const FilePath &python,
                                          const QString &pySide,
                                          TextEditor::TextDocument *document)
{
    if (!document || !document->infoBar()->canInfoBeAdded(installPySideInfoBarId))
        return;
    const QString message = Tr::tr("%1 installation missing for %2 (%3)")
                                .arg(pySide, rustName(python), python.toUserOutput());
    InfoBarEntry info(installPySideInfoBarId, message, InfoBarEntry::GlobalSuppression::Enabled);
    auto installCallback = [=]() { installRsside(python, pySide, document); };
    const QString installTooltip = Tr::tr("Install %1 for %2 using pip package installer.")
                                       .arg(pySide, python.toUserOutput());
    info.addCustomButton(Tr::tr("Install"), installCallback, installTooltip);
    document->infoBar()->addInfo(info);
}

void RsSideInstaller::runRsSideChecker(const FilePath &rust,
                                       const QString &pySide,
                                       TextEditor::TextDocument *document)
{
    using CheckRsSideWatcher = QFutureWatcher<bool>;

    QPointer<CheckRsSideWatcher> watcher = new CheckRsSideWatcher();

    // cancel and delete watcher after a 10 second timeout
    QTimer::singleShot(10000, this, [watcher]() {
        if (watcher) {
            watcher->cancel();
            watcher->deleteLater();
        }
    });
    connect(watcher,
            &CheckRsSideWatcher::resultReadyAt,
            this,
            [=, document = QPointer<TextEditor::TextDocument>(document)]() {
                if (watcher->result())
                    handleRsSideMissing(rust, pySide, document);
                watcher->deleteLater();
            });
    watcher->setFuture(Utils::asyncRun(&missingRsSideInstallation, rust, pySide));
}

} // Rusty::Internal
