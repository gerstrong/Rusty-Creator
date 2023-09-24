#include "rustlanguageclient.h"


#include "cratesupport.h"
#include "rssideuicextracompiler.h"

#include "rustyconstants.h"

#include "rusty.h"

#include "rustproject.h"
#include "rustrunconfiguration.h"
#include "rustsettings.h"

#include "rusttr.h"
#include "rustutils.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>

#include <languageclient/languageclientinterface.h>
#include <languageclient/languageclientmanager.h>
#include <languageserverprotocol/textsynchronization.h>
#include <languageserverprotocol/workspace.h>

#include <projectexplorer/extracompiler.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <utils/async.h>
#include <utils/infobar.h>
#include <utils/process.h>
#include <utils/variablechooser.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QJsonDocument>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

using namespace LanguageClient;
using namespace LanguageServerProtocol;
using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

static constexpr char installPylsInfoBarId[] = "Python::InstallPyls";

class PythonLanguageServerState
{
public:
    enum {
        CanNotBeInstalled,
        CanBeInstalled,
        AlreadyInstalled
    } state;
    FilePath pylsModulePath;
};

static QHash<FilePath, RsLSClient*> &pythonClients()
{
    static QHash<FilePath, RsLSClient*> clients;
    return clients;
}

FilePath getPylsModulePath(CommandLine pylsCommand)
{
    static QMutex mutex; // protect the access to the cache
    QMutexLocker locker(&mutex);
    static QMap<FilePath, FilePath> cache;
    const FilePath &modulePath = cache.value(pylsCommand.executable());
    if (!modulePath.isEmpty())
        return modulePath;

    pylsCommand.addArg("-h");

    Process pythonProcess;
    Environment env = pythonProcess.environment();
    env.set("PYTHONVERBOSE", "x");
    pythonProcess.setEnvironment(env);
    pythonProcess.setCommand(pylsCommand);
    pythonProcess.runBlocking();

    static const QString pylsInitPattern = "(.*)"
                                           + QRegularExpression::escape(
                                               QDir::toNativeSeparators("/pylsp/__init__.py"))
                                           + '$';
    static const QRegularExpression regexCached(" matches " + pylsInitPattern,
                                                QRegularExpression::MultilineOption);
    static const QRegularExpression regexNotCached(" code object from " + pylsInitPattern,
                                                   QRegularExpression::MultilineOption);

    const QString output = pythonProcess.allOutput();
    for (const auto &regex : {regexCached, regexNotCached}) {
        const QRegularExpressionMatch result = regex.match(output);
        if (result.hasMatch()) {
            const FilePath &modulePath = FilePath::fromUserInput(result.captured(1));
            cache[pylsCommand.executable()] = modulePath;
            return modulePath;
        }
    }
    return {};
}

static PythonLanguageServerState checkPythonLanguageServer(const FilePath &python)
{
    using namespace LanguageClient;
    const CommandLine pythonLShelpCommand(python, {"-m", "pylsp", "-h"});
    const FilePath &modulePath = getPylsModulePath(pythonLShelpCommand);

    Process pythonProcess;
    pythonProcess.setTimeoutS(2);
    pythonProcess.setCommand(pythonLShelpCommand);
    pythonProcess.runBlocking();
    if (pythonProcess.allOutput().contains("Python Language Server"))
        return {PythonLanguageServerState::AlreadyInstalled, modulePath};

    pythonProcess.setCommand({python, {"-m", "pip", "-V"}});
    pythonProcess.runBlocking();
    if (pythonProcess.allOutput().startsWith("pip "))
        return {PythonLanguageServerState::CanBeInstalled, FilePath()};
    else
        return {PythonLanguageServerState::CanNotBeInstalled, FilePath()};
}


class RsLSInterface : public StdIOClientInterface
{
public:
    RsLSInterface()
        : m_extraPythonPath("QtCreator-pyls-XXXXXX")
    { }

    TemporaryDirectory m_extraPythonPath;
protected:
    void startImpl() override
    {
        if (!m_cmd.executable().needsDevice()) {
            // todo check where to put this tempdir in remote setups
            Environment env = Environment::systemEnvironment();
            env.appendOrSet("PYTHONPATH",
                            m_extraPythonPath.path().toString(),
                            OsSpecificAspects::pathListSeparator(env.osType()));
            setEnvironment(env);
        }
        StdIOClientInterface::startImpl();
    }
};

RsLSClient *clientForPython(const FilePath &rust)
{
    if (auto client = pythonClients()[rust])
        return client;
    auto interface = new RsLSInterface;
    interface->setCommandLine(CommandLine(rust, {"-m", "pylsp"}));
    auto client = new RsLSClient(interface);
    client->setName(Tr::tr("Rust Language Server (%1)").arg(rust.toUserOutput()));
    client->setActivateDocumentAutomatically(true);
    client->updateConfiguration();
    LanguageFilter filter;
    filter.mimeTypes = QStringList() << Constants::C_RS_MIMETYPE << Constants::C_TOML_MIMETYPE;
    client->setSupportedLanguage(filter);
    client->start();
    pythonClients()[rust] = client;
    return client;
}

RsLSClient::RsLSClient(RsLSInterface *interface)
    : Client(interface)
    , m_extraCompilerOutputDir(interface->m_extraPythonPath.path())
{

    connect(this, &Client::initialized, this, &RsLSClient::updateConfiguration);
    connect(RustSettings::instance(), &RustSettings::pylsConfigurationChanged,
            this, &RsLSClient::updateConfiguration);
    connect(RustSettings::instance(), &RustSettings::pylsEnabledChanged,
            this, [this](const bool enabled){
                if (!enabled)
                    LanguageClientManager::shutdownClient(this);
            });

}

RsLSClient::~RsLSClient()
{
    pythonClients().remove(pythonClients().key(this));
}

void RsLSClient::updateConfiguration()
{

    const auto doc = QJsonDocument::fromJson(RustSettings::pylsConfiguration().toUtf8());
    if (doc.isArray())
        Client::updateConfiguration(doc.array());
    else if (doc.isObject())
        Client::updateConfiguration(doc.object());

}

void RsLSClient::openDocument(TextEditor::TextDocument *document)
{

    using namespace LanguageServerProtocol;
    if (reachable()) {
        const FilePath documentPath = document->filePath();
        if (RustProject *project = rustProjectForFile(documentPath)) {
            if (Target *target = project->activeTarget()) {
                if (RunConfiguration *rc = target->activeRunConfiguration())
                    if (auto aspect = rc->aspect<RustInterpreterAspect>())
                        updateExtraCompilers(project, aspect->extraCompilers());
            }
        } else if (isSupportedDocument(document)) {
            const FilePath workspacePath = documentPath.parentDir();
            if (!m_extraWorkspaceDirs.contains(workspacePath)) {
                WorkspaceFoldersChangeEvent event;
                event.setAdded({WorkSpaceFolder(hostPathToServerUri(workspacePath),
                                                workspacePath.fileName())});
                DidChangeWorkspaceFoldersParams params;
                params.setEvent(event);
                DidChangeWorkspaceFoldersNotification change(params);
                sendMessage(change);
                m_extraWorkspaceDirs.append(workspacePath);
            }
        }
    }
    Client::openDocument(document);

}

void RsLSClient::projectClosed(ProjectExplorer::Project *project)
{
    for (ProjectExplorer::ExtraCompiler *compiler : m_extraCompilers.value(project))
        closeExtraCompiler(compiler);
    Client::projectClosed(project);
}

void RsLSClient::updateExtraCompilers(ProjectExplorer::Project *project,
                                      const QList<RsSideUicExtraCompiler *> &extraCompilers)
{

    auto oldCompilers = m_extraCompilers.take(project);
    for (RsSideUicExtraCompiler *extraCompiler : extraCompilers) {
        QTC_ASSERT(extraCompiler->targets().size() == 1 , continue);
        int index = oldCompilers.indexOf(extraCompiler);
        if (index < 0) {
            m_extraCompilers[project] << extraCompiler;
            connect(extraCompiler,
                    &ExtraCompiler::contentsChanged,
                    this,
                    [this, extraCompiler](const FilePath &file) {
                        updateExtraCompilerContents(extraCompiler, file);
                    });
            if (extraCompiler->isDirty())
                extraCompiler->compileFile();
        } else {
            m_extraCompilers[project] << oldCompilers.takeAt(index);
        }
    }
    for (ProjectExplorer::ExtraCompiler *compiler : oldCompilers)
        closeExtraCompiler(compiler);

}

void RsLSClient::updateExtraCompilerContents(ExtraCompiler *compiler, const FilePath &file)
{
    const FilePath target = m_extraCompilerOutputDir.pathAppended(file.fileName());

    target.writeFileContents(compiler->content(file));
}

void RsLSClient::closeExtraCompiler(ProjectExplorer::ExtraCompiler *compiler)
{
    const FilePath file = compiler->targets().constFirst();
    m_extraCompilerOutputDir.pathAppended(file.fileName()).removeFile();
    compiler->disconnect(this);
}

RsLSClient *RsLSClient::clientForRust(const FilePath &python)
{
    return pythonClients()[python];
}

RsLSConfigureAssistant *RsLSConfigureAssistant::instance()
{
    static auto *instance = new RsLSConfigureAssistant(RustyPlugin::instance());
    return instance;
}

void RsLSConfigureAssistant::installPythonLanguageServer(const FilePath &python,
                                                         QPointer<TextEditor::TextDocument> document)
{

    document->infoBar()->removeInfo(installPylsInfoBarId);

    // Hide all install info bar entries for this python, but keep them in the list
    // so the language server will be setup properly after the installation is done.
    for (TextEditor::TextDocument *additionalDocument : m_infoBarEntries[python])
        additionalDocument->infoBar()->removeInfo(installPylsInfoBarId);

    auto install = new CrateInstallTask(python);

    connect(install, &CrateInstallTask::finished, this, [=](const bool success) {
        if (success) {
            if (document) {
                if (RsLSClient *client = clientForPython(python))
                    LanguageClientManager::openDocumentWithClient(document, client);
            }
        }
        install->deleteLater();
    });

    install->setPackages({CratePackage{"python-lsp-server[all]", "Python Language Server"}});
    install->run();

}

void RsLSConfigureAssistant::openDocumentWithPython(const FilePath &python,
                                                    TextEditor::TextDocument *document)
{

    instance()->resetEditorInfoBar(document);
    if (!RustSettings::pylsEnabled())
        return;

    if (auto client = pythonClients().value(python)) {
        LanguageClientManager::openDocumentWithClient(document, client);
        return;
    }

    using CheckPylsWatcher = QFutureWatcher<PythonLanguageServerState>;
    QPointer<CheckPylsWatcher> watcher = new CheckPylsWatcher();

    // cancel and delete watcher after a 10 second timeout
    QTimer::singleShot(10000, instance(), [watcher]() {
        if (watcher) {
            watcher->cancel();
            watcher->deleteLater();
        }
    });

    connect(watcher,
            &CheckPylsWatcher::resultReadyAt,
            instance(),
            [=, document = QPointer<TextEditor::TextDocument>(document)]() {
                if (!document || !watcher)
                    return;
                instance()->handlePyLSState(python, watcher->result(), document);
                watcher->deleteLater();
            });
    watcher->setFuture(Utils::asyncRun(&checkPythonLanguageServer, python));

}

void RsLSConfigureAssistant::handlePyLSState(const FilePath &rust,
                                             const PythonLanguageServerState &state,
                                             TextEditor::TextDocument *document)
{
    if (state.state == PythonLanguageServerState::CanNotBeInstalled)
        return;

    Utils::InfoBar *infoBar = document->infoBar();
    if (state.state == PythonLanguageServerState::CanBeInstalled
        && infoBar->canInfoBeAdded(installPylsInfoBarId)) {
        auto message = Tr::tr("Install Python language server (PyLS) for %1 (%2). "
                              "The language server provides Python specific completion and annotation.")
                           .arg(pythonName(rust), rust.toUserOutput());
        Utils::InfoBarEntry info(installPylsInfoBarId,
                                 message,
                                 Utils::InfoBarEntry::GlobalSuppression::Enabled);
        info.addCustomButton(Tr::tr("Install"),
                             [=]() { installPythonLanguageServer(rust, document); });
        infoBar->addInfo(info);
        m_infoBarEntries[rust] << document;
    } else if (state.state == PythonLanguageServerState::AlreadyInstalled) {
        if (auto client = clientForPython(rust))
            LanguageClientManager::openDocumentWithClient(document, client);
    }
}

void RsLSConfigureAssistant::updateEditorInfoBars(const FilePath &python, Client *client)
{
    for (TextEditor::TextDocument *document : instance()->m_infoBarEntries.take(python)) {
        instance()->resetEditorInfoBar(document);
        if (client)
            LanguageClientManager::openDocumentWithClient(document, client);
    }
}

void RsLSConfigureAssistant::resetEditorInfoBar(TextEditor::TextDocument *document)
{
    for (QList<TextEditor::TextDocument *> &documents : m_infoBarEntries)
        documents.removeAll(document);
    document->infoBar()->removeInfo(installPylsInfoBarId);
}

RsLSConfigureAssistant::RsLSConfigureAssistant(QObject *parent)
    : QObject(parent)
{
    Core::EditorManager::instance();

    connect(Core::EditorManager::instance(),
            &Core::EditorManager::documentClosed,
            this,
            [this](Core::IDocument *document) {
                if (auto textDocument = qobject_cast<TextEditor::TextDocument *>(document))
                    resetEditorInfoBar(textDocument);
            });
}

} // Rusty::Internal
