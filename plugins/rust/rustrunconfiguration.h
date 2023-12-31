#ifndef RUSTRUNCONFIGURATIONFACTORY_H
#define RUSTRUNCONFIGURATIONFACTORY_H

#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/runcontrol.h>

namespace Rusty::Internal {

class RsSideUicExtraCompiler;
class RustRunConfiguration;


class RustInterpreterAspect final : public ProjectExplorer::InterpreterAspect
{
    Q_OBJECT

public:
    RustInterpreterAspect(Utils::AspectContainer *container, ProjectExplorer::RunConfiguration *rc);
    ~RustInterpreterAspect() final;

    QList<RsSideUicExtraCompiler *> extraCompilers() const;

private:
    friend class RustRunConfiguration;
    class RustInterpreterAspectPrivate *d = nullptr;
};

class RustRunConfigurationFactory : public ProjectExplorer::RunConfigurationFactory
{
public:
    RustRunConfigurationFactory();
};

class RustOutputFormatterFactory : public ProjectExplorer::OutputFormatterFactory
{
public:
    RustOutputFormatterFactory();
};

class RustRunWorkerFactory final : public ProjectExplorer::RunWorkerFactory
{
public:
    RustRunWorkerFactory();
};

} // Rusty::Internal


#endif // RUSTRUNCONFIGURATIONFACTORY_H
