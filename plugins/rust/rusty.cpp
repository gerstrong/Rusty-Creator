// Copyright (C) 2016 The Qt Company Ltd.
// Copyright (C) 2023 Steinzone
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "rusty.h"
#include "rustyconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>

#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>

#include "rssidebuildconfiguration.h"
#include "rusteditor.h"
#include "rustproject.h"
#include "rustrunconfiguration.h"
#include "rustsettings.h"
#include "rusttr.h"
#include "rustwizardpagefactory.h"

#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/jsonwizard/jsonwizardfactory.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/taskhub.h>


#include <utils/fsengine/fileiconprovider.h>
#include <utils/theme/theme.h>

using namespace ProjectExplorer;
using namespace Utils;


namespace Rusty::Internal {

static RustyPlugin *m_instance = nullptr;

class RustyPluginPrivate
{
public:
    RustEditorFactory editorFactory;
    RustOutputFormatterFactory outputFormatterFactory;
    RustRunConfigurationFactory runConfigFactory;
    RsSideBuildStepFactory buildStepFactory;
    RsSideBuildConfigurationFactory buildConfigFactory;
    SimpleTargetRunnerFactory runWorkerFactory{{runConfigFactory.runConfigurationId()}};
    RustSettings settings;
};


RustyPlugin::RustyPlugin()
{
    m_instance = this;
}

RustyPlugin::~RustyPlugin()
{
    m_instance = nullptr;
    delete d;
}

RustyPlugin *RustyPlugin::instance()
{
    return m_instance;
}

bool RustyPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    // Register objects in the plugin manager's object pool
    // Load settings
    // Add actions to menus
    // Connect to other plugins' signals
    // In the initialize function, a plugin can be sure that the plugins it
    // depends on have initialized their members.

    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    auto action = new QAction(tr("Rusty Action"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::ACTION_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+Meta+A")));
    connect(action, &QAction::triggered, this, &RustyPlugin::triggerAction);

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("Rusty"));
    menu->addAction(cmd);
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    d = new RustyPluginPrivate;        


    ProjectManager::registerProjectType<Rusty::Internal::RustProject>(Rusty::Internal::RustMimeType);
    ProjectManager::registerProjectType<Rusty::Internal::RustProject>(Rusty::Internal::RustMimeTypeLegacy);
    ProjectManager::registerProjectType<Rusty::Internal::RustProject>(Rusty::Internal::CrateMimeType);
    JsonWizardFactory::registerPageFactory(new Rusty::Internal::RustWizardPageFactory);


    return true;
}

void RustyPlugin::extensionsInitialized()
{
    // Add MIME overlay icons (these icons displayed at Project dock panel)
    const QString imageFile = Utils::creatorTheme()->imageFile(Theme::IconOverlayPro,
                                                               ::Constants::FILEOVERLAY_PY);
    FileIconProvider::registerIconOverlayForSuffix(imageFile, "py");

    TaskHub::addCategory({Rusty::Internal::RustErrorTaskCategory,
                          "Rust",
                          Rusty::Internal::Tr::tr("Issues parsed from Rust runtime output."),
                          true});
}

ExtensionSystem::IPlugin::ShutdownFlag RustyPlugin::aboutToShutdown()
{
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
    return SynchronousShutdown;
}

void RustyPlugin::triggerAction()
{
    QMessageBox::information(Core::ICore::mainWindow(),
                             tr("Action Triggered"),
                             tr("This is an action from Rusty."));
}

RustyPlugin *instance()
{
    return m_instance;
}

} // namespace Rusty::Internal
