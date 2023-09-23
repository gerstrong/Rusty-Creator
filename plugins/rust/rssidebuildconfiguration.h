#ifndef RSSIDEBUILDSTEP_H
#define RSSIDEBUILDSTEP_H

#include <projectexplorer/abstractprocessstep.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildstep.h>

namespace Rusty::Internal {

class RsSideBuildStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT
public:
    RsSideBuildStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    void updatePySideProjectPath(const Utils::FilePath &pySideProjectPath);

private:
    Tasking::GroupItem runRecipe() final;

    Utils::FilePathAspect m_cargoProject{this};
};

class RsSideBuildStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    RsSideBuildStepFactory();
};

class RsSideBuildConfigurationFactory : public ProjectExplorer::BuildConfigurationFactory
{
public:
    RsSideBuildConfigurationFactory();
};

} // Rusty::Internal
#endif // RSSIDEBUILDSTEP_H
