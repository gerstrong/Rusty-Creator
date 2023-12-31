
#include "rustutils.h"

#include "rustproject.h"
#include "rustsettings.h"
#include "rusttr.h"

#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/processprogress.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <utils/algorithm.h>
#include <utils/mimeutils.h>
#include <utils/process.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

static QHash<FilePath, FilePath> &userDefinedPythonsForDocument()
{
    static QHash<FilePath, FilePath> userDefines;
    return userDefines;
}

FilePath detectCargo(const FilePath &documentPath)
{
    Project *project = documentPath.isEmpty() ? nullptr
                                              : ProjectManager::projectForFile(documentPath);
    if (!project)
        project = ProjectManager::startupProject();

    FilePaths dirs = Environment::systemEnvironment().path();

    if (project) {
        if (auto target = project->activeTarget()) {
            if (auto runConfig = target->activeRunConfiguration()) {
                if (auto interpreter = runConfig->aspect<InterpreterAspect>())
                    return interpreter->currentInterpreter().command;
                if (auto environmentAspect = runConfig->aspect<EnvironmentAspect>())
                    dirs = environmentAspect->environment().path();
            }
        }
    }

    const FilePath userDefined = userDefinedPythonsForDocument().value(documentPath);
    if (userDefined.exists())
        return userDefined;

    // check whether this file is inside a python virtual environment
    const QList<Interpreter> venvInterpreters = RustSettings::detectRustVenvs(documentPath);
    if (!venvInterpreters.isEmpty())
        return venvInterpreters.first().command;

    auto defaultInterpreter = RustSettings::defaultInterpreter().command;
    if (defaultInterpreter.exists())
        return defaultInterpreter;

    auto pythonFromPath = [dirs](const FilePath &toCheck) {
        const FilePaths found = toCheck.searchAllInDirectories(dirs);
        for (const FilePath &python : found) {
            // Windows creates empty redirector files that may interfere
            if (python.exists() && python.osType() == OsTypeWindows && python.fileSize() != 0)
                return python;
        }
        return FilePath();
    };

    const FilePath fromPath = pythonFromPath("cargo");
    if (fromPath.exists())
        return fromPath;

    return RustSettings::interpreters().value(0).command;
}

void defineRustForDocument(const FilePath &documentPath, const FilePath &python)
{
    userDefinedPythonsForDocument()[documentPath] = python;
}

static QStringList replImportArgs(const FilePath &pythonFile, ReplType type)
{
    using MimeTypes = QList<MimeType>;
    const MimeTypes mimeTypes = pythonFile.isEmpty() || type == ReplType::Unmodified
                                    ? MimeTypes()
                                    : mimeTypesForFileName(pythonFile.toString());
    const bool isPython = Utils::anyOf(mimeTypes, [](const MimeType &mt) {
        return mt.inherits("text/x-python") || mt.inherits("text/x-python3");
    });
    if (type == ReplType::Unmodified || !isPython)
        return {};
    const auto import = type == ReplType::Import
                            ? QString("import %1").arg(pythonFile.completeBaseName())
                            : QString("from %1 import *").arg(pythonFile.completeBaseName());
    return {"-c", QString("%1; print('Running \"%1\"')").arg(import)};
}

void openRustRepl(QObject *parent, const FilePath &file, ReplType type)
{
    Q_UNUSED(parent)

    static const auto workingDir = [](const FilePath &file) {
        if (file.isEmpty()) {
            if (Project *project = ProjectManager::startupProject())
                return project->projectDirectory();
            return FilePath::currentWorkingPath();
        }
        return file.absolutePath();
    };

    const auto args = QStringList{"-i"} + replImportArgs(file, type);
    const FilePath pythonCommand = detectCargo(file);

    Process process;
    process.setCommand({pythonCommand, args});
    process.setWorkingDirectory(workingDir(file));
    process.setTerminalMode(TerminalMode::Detached);
    process.start();

    if (process.error() != QProcess::UnknownError) {
        Core::MessageManager::writeDisrupting(
            Tr::tr((process.error() == QProcess::FailedToStart)
                       ? "Failed to run Python (%1): \"%2\"."
                       : "Error while running Python (%1): \"%2\".")
                .arg(process.commandLine().toUserOutput(), process.errorString()));
    }
}

QString rustName(const FilePath &pythonPath)
{
    static QHash<FilePath, QString> nameForPython;
    if (!pythonPath.exists())
        return {};
    QString name = nameForPython.value(pythonPath);
    if (name.isEmpty()) {
        Process pythonProcess;
        pythonProcess.setTimeoutS(2);
        pythonProcess.setCommand({pythonPath, {"--version"}});
        pythonProcess.runBlocking();
        if (pythonProcess.result() != ProcessResult::FinishedWithSuccess)
            return {};
        name = pythonProcess.allOutput().trimmed();
        nameForPython[pythonPath] = name;
    }
    return name;
}

RustProject *rustProjectForFile(const FilePath &pythonFile)
{
    for (Project *project : ProjectManager::projects()) {
        if (auto rustProject = qobject_cast<RustProject *>(project)) {
            if (rustProject->isKnownFile(pythonFile))
                return rustProject;
        }
    }
    return nullptr;
}

void createVenv(const Utils::FilePath &python,
                const Utils::FilePath &venvPath,
                const std::function<void(bool)> &callback)
{
    QTC_ASSERT(python.isExecutableFile(), callback(false); return);
    QTC_ASSERT(!venvPath.exists() || venvPath.isDir(), callback(false); return);

    const CommandLine command(python, QStringList{"-m", "venv", venvPath.toUserOutput()});

    auto process = new Process;
    auto progress = new Core::ProcessProgress(process);
    progress->setDisplayName(Tr::tr("Create Python venv"));
    QObject::connect(process, &Process::done, [process, callback](){
        callback(process->result() == ProcessResult::FinishedWithSuccess);
        process->deleteLater();
    });
    process->setCommand(command);
    process->start();
}

} // Rusty::Internal
