// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "rssidebuildconfiguration.h"

#include "rustyconstants.h"
#include "rustproject.h"
#include "rusttr.h"

#include <projectexplorer/buildinfo.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/environmentaspect.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>

#include <utils/commandline.h>
#include <utils/process.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

const char RssideBuildStep[] = "Rust.RssideBuildStep";

RsSideBuildStepFactory::RsSideBuildStepFactory()
{
    registerStep<RsSideBuildStep>(RssideBuildStep);
    setSupportedProjectType(RustProjectId);
    setDisplayName(Tr::tr("Run Cargo project tool"));
    setFlags(BuildStep::UniqueStep);
}

RsSideBuildStep::RsSideBuildStep(BuildStepList *bsl, Id id)
    : AbstractProcessStep(bsl, id)
{
    m_cargoProject.setSettingsKey("Rust.CargoProjectTool");
    m_cargoProject.setLabelText(Tr::tr("Cargo project tool:"));
    m_cargoProject.setToolTip(Tr::tr("Enter location of Cargo project tool."));
    m_cargoProject.setExpectedKind(PathChooser::Command);
    m_cargoProject.setHistoryCompleter("Rust.CargoProjectTool.History");

    const FilePath cargoProjectPath = FilePath("cargo").searchInPath();
    if (cargoProjectPath.isExecutableFile())
        m_cargoProject.setValue(cargoProjectPath);

    QString str(m_cargoProject.value());

    setCommandLineProvider([this] { return CommandLine(m_cargoProject(), {"build"}); });
    setWorkingDirectoryProvider([this] {
        return m_cargoProject().withNewMappedPath(project()->projectDirectory()); // FIXME: new path needed?
    });
    setEnvironmentModifier([this](Environment &env) {
        env.prependOrSetPath(m_cargoProject().parentDir());
    });
}

void RsSideBuildStep::updateRsSideProjectPath(const FilePath &rsSideProjectPath)
{
    m_cargoProject.setValue(rsSideProjectPath);
}

Tasking::GroupItem RsSideBuildStep::runRecipe()
{
    using namespace Tasking;

    const auto onSetup = [this] {
        if (!processParameters()->effectiveCommand().isExecutableFile())
            return SetupResult::StopWithDone;
        return SetupResult::Continue;
    };

    return Group { onGroupSetup(onSetup), defaultProcessTask() };
}

// RsSideBuildConfiguration

class RsSideBuildConfiguration : public BuildConfiguration
{
public:
    RsSideBuildConfiguration(Target *target, Id id)
        : BuildConfiguration(target, id)
    {
        setConfigWidgetDisplayName(Tr::tr("General"));

        setInitializer([this](const BuildInfo &) {
            buildSteps()->appendStep(RssideBuildStep);
            updateCacheAndEmitEnvironmentChanged();
        });

        updateCacheAndEmitEnvironmentChanged();
    }
};

RsSideBuildConfigurationFactory::RsSideBuildConfigurationFactory()
{
    registerBuildConfiguration<RsSideBuildConfiguration>("Rust.PySideBuildConfiguration");
    setSupportedProjectType(RustProjectId);
    setSupportedProjectMimeTypeName(Constants::C_RS_MIMETYPE);
    setBuildGenerator([](const Kit *, const FilePath &projectPath, bool) {
        BuildInfo info;
        info.displayName = "build";
        info.typeName = "build";
        info.buildDirectory = projectPath.parentDir();
        return QList<BuildInfo>{info};
    });
}

} // Python::Internal
