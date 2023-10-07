#ifndef RUSTWIZARDPAGEFACTORY_H
#define RUSTWIZARDPAGEFACTORY_H

#include <projectexplorer/jsonwizard/jsonwizard.h>
#include <projectexplorer/jsonwizard/jsonwizardpagefactory.h>
#include <projectexplorer/runconfigurationaspects.h>

#include <utils/aspects.h>
#include <utils/wizardpage.h>

namespace Rusty::Internal {

class RustWizardPageFactory : public ProjectExplorer::JsonWizardPageFactory
{
public:
    RustWizardPageFactory();

    Utils::WizardPage *create(ProjectExplorer::JsonWizard *wizard,
                              Utils::Id typeId,
                              const QVariant &data) override;
    bool validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) override;
};

class RustWizardPage : public Utils::WizardPage
{
public:
    RustWizardPage();
    void initializePage() override;
    bool validatePage() override;

private:
    void setupProject(const ProjectExplorer::JsonWizard::GeneratorFiles &files);
    void updateInterpreters();
    void updateStateLabel();

    ProjectExplorer::InterpreterAspect m_interpreter;
    Utils::InfoLabel *m_stateLabel = nullptr;
};

} // namespace Rusty::Internal


#endif // RUSTWIZARDPAGEFACTORY_H
