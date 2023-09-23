#ifndef RSSIDEUICEXTRACOMPILER_H
#define RSSIDEUICEXTRACOMPILER_H

#include <projectexplorer/extracompiler.h>

namespace Rusty::Internal {

class RsSideUicExtraCompiler : public ProjectExplorer::ProcessExtraCompiler
{
public:
    RsSideUicExtraCompiler(const Utils::FilePath &pySideUic,
                           const ProjectExplorer::Project *project,
                           const Utils::FilePath &source,
                           const Utils::FilePaths &targets,
                           QObject *parent = nullptr);

    Utils::FilePath pySideUicPath() const;

private:
    Utils::FilePath command() const override;
    ProjectExplorer::FileNameToContentsHash handleProcessFinished(
        Utils::Process *process) override;

    Utils::FilePath m_pySideUic;
};

} // Rusty::Internal


#endif // RSSIDEUICEXTRACOMPILER_H
