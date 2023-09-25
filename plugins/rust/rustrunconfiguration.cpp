#include "rustrunconfiguration.h"

#include "cratesupport.h"
#include "rsside.h"
#include "rssidebuildconfiguration.h"
#include "rssideuicextracompiler.h"
#include "rustyconstants.h"
#include "rustlanguageclient.h"
#include "rustproject.h"
#include "rustsettings.h"
#include "rusttr.h"

#include <coreplugin/icore.h>
#include <coreplugin/editormanager/editormanager.h>

#include <extensionsystem/pluginmanager.h>

#include <languageclient/languageclientmanager.h>

#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <texteditor/textdocument.h>

#include <utils/aspects.h>
#include <utils/fileutils.h>
#include <utils/futuresynchronizer.h>
#include <utils/layoutbuilder.h>
#include <utils/outputformatter.h>
#include <utils/theme/theme.h>

#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>

using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

class PythonOutputLineParser : public OutputLineParser
{
public:
    PythonOutputLineParser()
        // Note that moc dislikes raw string literals.
        : filePattern("^(\\s*)(File \"([^\"]+)\", line (\\d+), .*$)")
    {
        TaskHub::clearTasks(RustErrorTaskCategory);
    }

private:
    Result handleLine(const QString &text, OutputFormat format) final
    {
        if (!m_inTraceBack) {
            m_inTraceBack = format == StdErrFormat
                    && text.startsWith("Traceback (most recent call last):");
            if (m_inTraceBack)
                return Status::InProgress;
            return Status::NotHandled;
        }

        const Id category(RustErrorTaskCategory);
        const QRegularExpressionMatch match = filePattern.match(text);
        if (match.hasMatch()) {
            const LinkSpec link(match.capturedStart(2), match.capturedLength(2), match.captured(2));
            const auto fileName = FilePath::fromString(match.captured(3));
            const int lineNumber = match.captured(4).toInt();
            m_tasks.append({Task::Warning, QString(), fileName, lineNumber, category});
            return {Status::InProgress, {link}};
        }

        Status status = Status::InProgress;
        if (text.startsWith(' ')) {
            // Neither traceback start, nor file, nor error message line.
            // Not sure if that can actually happen.
            if (m_tasks.isEmpty()) {
                m_tasks.append({Task::Warning, text.trimmed(), {}, -1, category});
            } else {
                Task &task = m_tasks.back();
                if (!task.summary.isEmpty())
                    task.summary += ' ';
                task.summary += text.trimmed();
            }
        } else {
            // The actual exception. This ends the traceback.
            TaskHub::addTask({Task::Error, text, {}, -1, category});
            for (auto rit = m_tasks.crbegin(), rend = m_tasks.crend(); rit != rend; ++rit)
                TaskHub::addTask(*rit);
            m_tasks.clear();
            m_inTraceBack = false;
            status = Status::Done;
        }
        return status;
    }

    bool handleLink(const QString &href) final
    {
        const QRegularExpressionMatch match = filePattern.match(href);
        if (!match.hasMatch())
            return false;
        const QString fileName = match.captured(3);
        const int lineNumber = match.captured(4).toInt();
        Core::EditorManager::openEditorAt({FilePath::fromString(fileName), lineNumber});
        return true;
    }

    const QRegularExpression filePattern;
    QList<Task> m_tasks;
    bool m_inTraceBack;
};

////////////////////////////////////////////////////////////////

class RustInterpreterAspectPrivate : public QObject
{
public:
    RustInterpreterAspectPrivate(RustInterpreterAspect *parent, RunConfiguration *rc)
        : q(parent), rc(rc)
    {
        connect(q, &InterpreterAspect::changed,
                this, &RustInterpreterAspectPrivate::currentInterpreterChanged);
        currentInterpreterChanged();

        connect(RsSideInstaller::instance(), &RsSideInstaller::rsSideInstalled, this,
                [this](const FilePath &python) {
                    if (python == q->currentInterpreter().command)
                        checkForPySide(python);
                }
        );

        connect(rc->target(), &Target::buildSystemUpdated,
                this, &RustInterpreterAspectPrivate::updateExtraCompilers);
    }

    ~RustInterpreterAspectPrivate() { qDeleteAll(m_extraCompilers); }

    void checkForPySide(const FilePath &python);
    void checkForPySide(const FilePath &rust, const QString &rsSidePackageName);
    void handlePySidePackageInfo(const CratePackageInfo &pySideInfo,
                                 const FilePath &python,
                                 const QString &requestedPackageName);
    void updateExtraCompilers();
    void currentInterpreterChanged();

    FilePath m_rsSideUicPath;

    RustInterpreterAspect *q;
    RunConfiguration *rc;
    QList<RsSideUicExtraCompiler *> m_extraCompilers;
    QFutureWatcher<CratePackageInfo> m_watcher;
    QMetaObject::Connection m_watcherConnection;
};

RustInterpreterAspect::RustInterpreterAspect(AspectContainer *container, RunConfiguration *rc)
    : InterpreterAspect(container), d(new RustInterpreterAspectPrivate(this, rc))
{
    setSettingsKey("RustEditor.RunConfiguation.Interpreter");
    setSettingsDialogId(Constants::C_RUSTOPTIONS_PAGE_ID);

    updateInterpreters(RustSettings::interpreters());

    const QList<Interpreter> interpreters = RustSettings::detectRustVenvs(
        rc->project()->projectDirectory());
    Interpreter defaultInterpreter = interpreters.isEmpty() ? RustSettings::defaultInterpreter()
                                                            : interpreters.first();
    if (!defaultInterpreter.command.isExecutableFile())
        defaultInterpreter = RustSettings::interpreters().value(0);
    if (defaultInterpreter.command.isExecutableFile()) {
        const IDeviceConstPtr device = DeviceKitAspect::device(rc->kit());
        if (device && !device->handlesFile(defaultInterpreter.command)) {
            defaultInterpreter = Utils::findOr(RustSettings::interpreters(),
                                               defaultInterpreter,
                                               [device](const Interpreter &interpreter) {
                                                   return device->handlesFile(interpreter.command);
                                               });
        }
    }
    setDefaultInterpreter(defaultInterpreter);

    connect(RustSettings::instance(), &RustSettings::interpretersChanged,
            this, &InterpreterAspect::updateInterpreters);
}

RustInterpreterAspect::~RustInterpreterAspect()
{
    delete d;
}

void RustInterpreterAspectPrivate::checkForPySide(const FilePath &python)
{
    checkForPySide(python, "PySide6-Essentials");
}

void RustInterpreterAspectPrivate::checkForPySide(const FilePath &rust,
                                                    const QString &rsSidePackageName)
{
    const CratePackage package(rsSidePackageName);
    QObject::disconnect(m_watcherConnection);
    m_watcherConnection = QObject::connect(&m_watcher, &QFutureWatcherBase::finished, q, [=] {
        handlePySidePackageInfo(m_watcher.result(), rust, rsSidePackageName);
    });
    const auto future = Crate::instance(rust)->info(package);
    m_watcher.setFuture(future);
    ExtensionSystem::PluginManager::futureSynchronizer()->addFuture(future);
}

void RustInterpreterAspectPrivate::handlePySidePackageInfo(const CratePackageInfo &pySideInfo,
                                                             const FilePath &python,
                                                             const QString &requestedPackageName)
{
    struct PythonTools
    {
        FilePath pySideProjectPath;
        FilePath pySideUicPath;
    };

    BuildStepList *buildSteps = nullptr;
    if (Target *target = rc->target()) {
        if (auto buildConfiguration = target->activeBuildConfiguration())
            buildSteps = buildConfiguration->buildSteps();
    }
    if (!buildSteps)
        return;

    const auto findPythonTools = [](const FilePaths &files,
                                    const FilePath &location,
                                    const FilePath &rust) -> PythonTools {
        PythonTools result;
        const QString pySide6ProjectName
            = OsSpecificAspects::withExecutableSuffix(rust.osType(), "pyside6-project");
        const QString pySide6UicName
            = OsSpecificAspects::withExecutableSuffix(rust.osType(), "pyside6-uic");
        for (const FilePath &file : files) {
            if (file.fileName() == pySide6ProjectName) {
                result.pySideProjectPath = rust.withNewMappedPath(location.resolvePath(file));
                result.pySideProjectPath = result.pySideProjectPath.cleanPath();
                if (!result.pySideUicPath.isEmpty())
                    return result;
            } else if (file.fileName() == pySide6UicName) {
                result.pySideUicPath = rust.withNewMappedPath(location.resolvePath(file));
                result.pySideUicPath = result.pySideUicPath.cleanPath();
                if (!result.pySideProjectPath.isEmpty())
                    return result;
            }
        }
        return {};
    };

    PythonTools rustTools = findPythonTools(pySideInfo.files, pySideInfo.location, python);
    if (!rustTools.pySideProjectPath.isExecutableFile() && requestedPackageName != "PySide6") {
        checkForPySide(python, "PySide6");
        return;
    }

    m_rsSideUicPath = rustTools.pySideUicPath;

    updateExtraCompilers();

    if (auto RssideBuildStep = buildSteps->firstOfType<RsSideBuildStep>())
        RssideBuildStep->updateRsSideProjectPath(rustTools.pySideProjectPath);
}

void RustInterpreterAspectPrivate::currentInterpreterChanged()
{
    const FilePath rust = q->currentInterpreter().command;
    checkForPySide(rust);

    for (FilePath &file : rc->project()->files(Project::AllFiles)) {
        if (auto document = TextEditor::TextDocument::textDocumentForFilePath(file)) {
            if (document->mimeType() == Constants::C_RS_MIMETYPE
                || document->mimeType() == Constants::C_TOML_MIMETYPE) {
                RsLSConfigureAssistant::openDocumentWithPython(rust, document);
                RsSideInstaller::checkRsSideInstallation(rust, document);
            }
        }
    }
}

QList<RsSideUicExtraCompiler *> RustInterpreterAspect::extraCompilers() const
{
    return d->m_extraCompilers;
}

void RustInterpreterAspectPrivate::updateExtraCompilers()
{
    QList<RsSideUicExtraCompiler *> oldCompilers = m_extraCompilers;
    m_extraCompilers.clear();

    if (m_rsSideUicPath.isExecutableFile()) {
        auto uiMatcher = [](const Node *node) {
            if (const FileNode *fileNode = node->asFileNode())
                return fileNode->fileType() == FileType::Form;
            return false;
        };
        const FilePaths uiFiles = rc->project()->files(uiMatcher);
        for (const FilePath &uiFile : uiFiles) {
            FilePath generated = uiFile.parentDir();
            generated = generated.pathAppended("/ui_" + uiFile.baseName() + ".py");
            int index = Utils::indexOf(oldCompilers, [&](RsSideUicExtraCompiler *oldCompiler) {
                return oldCompiler->pySideUicPath() == m_rsSideUicPath
                       && oldCompiler->project() == rc->project() && oldCompiler->source() == uiFile
                       && oldCompiler->targets() == FilePaths{generated};
            });
            if (index < 0) {
                m_extraCompilers << new RsSideUicExtraCompiler(m_rsSideUicPath,
                                                               rc->project(),
                                                               uiFile,
                                                               {generated},
                                                               this);
            } else {
                m_extraCompilers << oldCompilers.takeAt(index);
            }
        }
    }
    for (LanguageClient::Client *client : LanguageClient::LanguageClientManager::clients())
    {
        if (auto rslsClient = qobject_cast<RsLSClient *>(client))
            rslsClient->updateExtraCompilers(rc->project(), m_extraCompilers);
    }
    qDeleteAll(oldCompilers);
}

// RunConfiguration


const char RUST_EXECUTABLE_RUNCONFIG_ID[] = "ProjectExplorer.RustRunConfiguration";


class RustRunConfiguration : public RunConfiguration
{
public:

    explicit RustRunConfiguration(Target *target)
        : RustRunConfiguration(target, RUST_EXECUTABLE_RUNCONFIG_ID)
    {}

    RustRunConfiguration(Target *target, Id id)
        : RunConfiguration(target, id)
    {
        buffered.setSettingsKey("RustEditor.RunConfiguation.Buffered");
        buffered.setLabelText(Tr::tr("Buffered output"));
        buffered.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);
        buffered.setToolTip(Tr::tr("Enabling improves output performance, "
                                   "but results in delayed output."));

        mainScript.setSettingsKey("RustEditor.RunConfiguation.Script");
        mainScript.setLabelText(Tr::tr("Script:"));
        mainScript.setReadOnly(true);

        environment.setSupportForBuildEnvironment(target);

        executable.setDeviceSelector(target, ExecutableAspect::HostDevice);
        executable.setSettingsKey("ProjectExplorer.RustRunConfiguration");
        executable.setReadOnly(false);
        executable.setHistoryCompleter("Qt.CustomExecutable.History");
        executable.setExpectedKind(PathChooser::ExistingCommand);
        executable.setEnvironment(environment.environment());

        arguments.setMacroExpander(macroExpander());

        workingDir.setMacroExpander(macroExpander());

        x11Forwarding.setMacroExpander(macroExpander());
        x11Forwarding.setVisible(HostOsInfo::isAnyUnixHost());

        setCommandLineGetter([this] {
            CommandLine cmd{interpreter.currentInterpreter().command};
            if (!buffered())
                cmd.addArg("-u");
            cmd.addArg(mainScript().fileName());
            cmd.addArgs(arguments(), CommandLine::Raw);
            return cmd;
        });

        setUpdater([this] {
            const BuildTargetInfo bti = buildTargetInfo();
            setDefaultDisplayName(Tr::tr("Run %1").arg(bti.targetFilePath.toUserOutput()));
            mainScript.setValue(bti.targetFilePath);
            workingDir.setDefaultWorkingDirectory(bti.targetFilePath.parentDir());
        });

        connect(target, &Target::buildSystemUpdated, this, &RunConfiguration::update);
    }

    RustInterpreterAspect interpreter{this, this};
    BoolAspect buffered{this};
    MainScriptAspect mainScript{this};
    EnvironmentAspect environment{this};
    ExecutableAspect executable{this};
    ArgumentsAspect arguments{this};
    WorkingDirectoryAspect workingDir{this};

    TerminalAspect terminal{this};
    X11ForwardingAspect x11Forwarding{this};
};

static CommandLine rustRunCommand(const Target *target,
                                const QString &buildKey)
{
    if (BuildConfiguration *bc = target->activeBuildConfiguration()) {
        const Environment env = bc->environment();
        const FilePath emrun = env.searchInPath("cargo");
        const FilePath emrunPy = emrun.absolutePath().pathAppended(emrun.baseName() + ".py");

        QStringList args(emrunPy.path());
        args.append("--port");
        args.append("--no_emrun_detect");
        args.append("--serve_after_close");

        return CommandLine("test.exe", args);
    }
    return {};
}

class RustRunWorker : public SimpleTargetRunner
{
public:
    RustRunWorker(RunControl *runControl)
        : SimpleTargetRunner(runControl)
    {
        /*auto portsGatherer = new PortsGatherer(runControl);
        addStartDependency(portsGatherer);

        setStartModifier([this, runControl, portsGatherer] {
            const QString browserId =
                    runControl->aspect<WebBrowserSelectionAspect>()->currentBrowser;
            setCommandLine(emrunCommand(runControl->target(),
                                        runControl->buildKey(),
                                        browserId,
                                        QString::number(portsGatherer->findEndPoint().port())));
            setEnvironment(runControl->buildEnvironment());
        });
        */

        setStartModifier([this, runControl] {
            setCommandLine(rustRunCommand(runControl->target(),
                                          runControl->buildKey()));

            setEnvironment(runControl->buildEnvironment());
        });

    }
};

// Factories

RustRunConfigurationFactory::RustRunConfigurationFactory()
{
    registerRunConfiguration<RustRunConfiguration>(Constants::C_RUSTRUNCONFIGURATION_ID);
    addSupportedProjectType(RustProjectId);
}

RustOutputFormatterFactory::RustOutputFormatterFactory()
{
    setFormatterCreator([](Target *t) -> QList<OutputLineParser *> {
        if (t && t->project()->mimeType() == Constants::C_RS_MIMETYPE)
            return {new PythonOutputLineParser};
        return {};
    });
}

RustRunWorkerFactory::RustRunWorkerFactory()
{
    setProduct<RustRunWorker>();
    addSupportedRunMode(ProjectExplorer::Constants::NORMAL_RUN_MODE);
    addSupportedRunConfig(Constants::C_RUSTRUNCONFIGURATION_ID);
}

} // Rusty::Internal
