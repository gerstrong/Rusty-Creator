#ifndef RUSTYPLUGIN_H
#define RUSTYPLUGIN_H

#include "rusty_global.h"

#include <extensionsystem/iplugin.h>

namespace Rusty::Internal {

class RustyPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Rusty.json")

public:
    RustyPlugin();
    virtual ~RustyPlugin() override;

    bool initialize(const QStringList &arguments, QString *errorString) override;
    void extensionsInitialized() override;

    ShutdownFlag aboutToShutdown() override;

    static RustyPlugin *instance();

private:
    void triggerAction();

    class RustyPluginPrivate *d = nullptr;
};

} // Internal::Rusty

#endif // RUSTYPLUGIN_H
