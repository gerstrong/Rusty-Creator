
#include "rssideuicextracompiler.h"

#include <utils/process.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

RsSideUicExtraCompiler::RsSideUicExtraCompiler(const FilePath &pySideUic,
                                               const Project *project,
                                               const FilePath &source,
                                               const FilePaths &targets,
                                               QObject *parent)
    : ProcessExtraCompiler(project, source, targets, parent)
    , m_pySideUic(pySideUic)
{
}

FilePath RsSideUicExtraCompiler::pySideUicPath() const
{
    return m_pySideUic;
}

FilePath RsSideUicExtraCompiler::command() const
{
    return m_pySideUic;
}

FileNameToContentsHash RsSideUicExtraCompiler::handleProcessFinished(Process *process)
{
    FileNameToContentsHash result;
    if (process->exitStatus() != QProcess::NormalExit && process->exitCode() != 0)
        return result;

    const FilePaths targetList = targets();
    if (targetList.size() != 1)
        return result;
    // As far as I can discover in the UIC sources, it writes out local 8-bit encoding. The
    // conversion below is to normalize both the encoding, and the line terminators.
    result[targetList.first()] = QString::fromLocal8Bit(process->readAllRawStandardOutput()).toUtf8();
    return result;
}

} // Rusty::Internal
