#include "rustwizardpagefactory.h"

#include "rustyconstants.h"
#include "rustsettings.h"
#include "rusttr.h"

#include "rustrunconfiguration.h"

#include <projectexplorer/runconfiguration.h>


#include <coreplugin/generatedfile.h>

#include <utils/algorithm.h>
#include <utils/layoutbuilder.h>
#include <utils/mimeutils.h>
#include <utils/qtcassert.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Rusty::Internal {

RustWizardPageFactory::RustWizardPageFactory()
{
    setTypeIdsSuffix("RustConfiguration");
}

WizardPage *RustWizardPageFactory::create(JsonWizard *wizard, Id typeId, const QVariant &data)
{
    Q_UNUSED(wizard)

    QTC_ASSERT(canCreate(typeId), return nullptr);

    return new RustWizardPage();
}

static bool validItem(const QVariant &item)
{
    QMap<QString, QVariant> map = item.toMap();
    if (!map.value("trKey").canConvert<QString>())
        return false;
    map = map.value("value").toMap();
    return map.value("PySideVersion").canConvert<QString>();
}

bool RustWizardPageFactory::validateData(Id typeId, const QVariant &data, QString *errorMessage)
{
    QTC_ASSERT(canCreate(typeId), return false);
    const QList<QVariant> items = data.toMap().value("items").toList();

    if (items.isEmpty()) {
        if (errorMessage) {
            *errorMessage = Tr::tr("\"data\" of a Python wizard page expects a map with \"items\" "
                                   "containing a list of objects.");
        }
        return false;
    }

    if (!Utils::allOf(items, &validItem)) {
        if (errorMessage) {
            *errorMessage = Tr::tr(
                "An item of Python wizard page data expects a \"trKey\" field containing the UI "
                "visible string for that Python version and a \"value\" field containing an object "
                "with a \"PySideVersion\" field used for import statements in the Python files.");
        }
        return false;
    }
    return true;
}

RustWizardPage::RustWizardPage()
{
    using namespace Layouting;
    m_interpreter.setSettingsDialogId(Rusty::Constants::C_RUSTOPTIONS_PAGE_ID);
    connect(RustSettings::instance(),
            &RustSettings::interpretersChanged,
            this,
            &RustWizardPage::updateInterpreters);

    m_stateLabel = new InfoLabel();
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setFilled(true);
    m_stateLabel->setType(InfoLabel::Error);

    Form {
        m_interpreter, st, br,
        m_stateLabel, br
    }.attachTo(this);
}

void RustWizardPage::initializePage()
{
    auto wiz = qobject_cast<JsonWizard *>(wizard());
    QTC_ASSERT(wiz, return);
    connect(wiz, &JsonWizard::filesPolished,
            this, &RustWizardPage::setupProject,
            Qt::UniqueConnection);

    const FilePath projectDir = FilePath::fromString(wiz->property("ProjectDirectory").toString());

    updateInterpreters();
    updateStateLabel();
}

bool RustWizardPage::validatePage()
{
    return true;
}

void RustWizardPage::setupProject(const JsonWizard::GeneratorFiles &files)
{
    for (const JsonWizard::GeneratorFile &f : files) {
        if (f.file.attributes() & Core::GeneratedFile::OpenProjectAttribute) {
            Interpreter interpreter = m_interpreter.currentInterpreter();
            Project *project = ProjectManager::openProject(Utils::mimeTypeForFile(f.file.filePath()),
                                                           f.file.filePath().absoluteFilePath());

            if (project) {
                project->addTargetForDefaultKit();
                if (Target *target = project->activeTarget()) {
                    if (RunConfiguration *rc = target->activeRunConfiguration()) {
                        if (auto interpreters = rc->aspect<InterpreterAspect>()) {
                            interpreters->setCurrentInterpreter(interpreter);
                            project->saveSettings();
                        }
                    }
                }
                delete project;
            }
        }
    }
}

void RustWizardPage::updateInterpreters()
{
    m_interpreter.setDefaultInterpreter(RustSettings::defaultInterpreter());
    m_interpreter.updateInterpreters(RustSettings::interpreters());
}

void RustWizardPage::updateStateLabel()
{
    QTC_ASSERT(m_stateLabel, return);    
    m_stateLabel->hide();
}

} // namespace Rusty::Internal

