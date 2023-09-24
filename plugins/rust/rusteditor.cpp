#include "rusteditor.h"

#include "rsside.h"
#include "rustyconstants.h"

#include "rusthighlighter.h"
#include "rustindenter.h"

#include "rustlanguageclient.h"

#include "rustsettings.h"
#include "rusttr.h"
#include "rustutils.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/commandbutton.h>
#include <coreplugin/coreplugintr.h>
#include <coreplugin/icore.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditoractionhandler.h>

#include <utils/stylehelper.h>

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QMenu>

using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace Rusty::Internal {

static QAction *createAction(QObject *parent, ReplType type)
{
    QAction *action = new QAction(parent);
    switch (type) {
    case ReplType::Unmodified:
        action->setText(Tr::tr("REPL"));
        action->setToolTip(Tr::tr("Open interactive Rust."));
        break;
    case ReplType::Import:
        action->setText(Tr::tr("REPL Import File"));
        action->setToolTip(Tr::tr("Open interactive Python and import file."));
        break;
    case ReplType::ImportToplevel:
        action->setText(Tr::tr("REPL Import *"));
        action->setToolTip(Tr::tr("Open interactive Python and import * from file."));
        break;
    }

    QObject::connect(action, &QAction::triggered, parent, [type, parent] {
        Core::IDocument *doc = Core::EditorManager::currentDocument();
        openPythonRepl(parent, doc ? doc->filePath() : FilePath(), type);
    });

    return action;
}

static void registerReplAction(QObject *parent)
{

    Core::ActionManager::registerAction(createAction(parent, ReplType::Unmodified),
                                        Constants::RUST_OPEN_REPL);
    Core::ActionManager::registerAction(createAction(parent, ReplType::Import),
                                        Constants::RUST_OPEN_REPL_IMPORT);
    Core::ActionManager::registerAction(createAction(parent, ReplType::ImportToplevel),
                                        Constants::RUST_OPEN_REPL_IMPORT_TOPLEVEL);

}

class RustDocument : public TextDocument
{
    Q_OBJECT
public:
    RustDocument() : TextDocument(Constants::C_RUSTEDITOR_ID)
    {

        connect(RustSettings::instance(),
                &RustSettings::pylsEnabledChanged,
                this,
                [this](const bool enabled) {
                    if (!enabled)
                        return;
                    const FilePath &rust = detectRust(filePath());
                    if (rust.exists())
                        RsLSConfigureAssistant::openDocumentWithPython(rust, this);
                });
        connect(this, &RustDocument::openFinishedSuccessfully,
                this, &RustDocument::checkForRsls);
    }

    void checkForRsls()
    {

        const FilePath &rust = detectRust(filePath());
        if (!rust.exists())
            return;

        RsLSConfigureAssistant::openDocumentWithPython(rust, this);
        RsSideInstaller::checkRsSideInstallation(rust, this);
    }
};

class RustEditorWidget : public TextEditorWidget
{
public:
    RustEditorWidget(QWidget *parent = nullptr);

protected:
    void finalizeInitialization() override;
    void setUserDefinedPython(const Interpreter &interpreter);
    void updateInterpretersSelector();

private:
    QToolButton *m_interpreters = nullptr;
    QList<QMetaObject::Connection> m_projectConnections;
};

RustEditorWidget::RustEditorWidget(QWidget *parent) : TextEditorWidget(parent)
{

    auto replButton = new QToolButton(this);
    replButton->setProperty(StyleHelper::C_NO_ARROW, true);
    replButton->setText(Tr::tr("REPL"));
    replButton->setPopupMode(QToolButton::InstantPopup);
    replButton->setToolTip(Tr::tr("Open interactive Rust. Either importing nothing, "
                                  "importing the current file, "
                                  "or importing everything (*) from the current file."));
    auto menu = new QMenu(replButton);
    replButton->setMenu(menu);
    menu->addAction(Core::ActionManager::command(Constants::RUST_OPEN_REPL)->action());
    menu->addSeparator();
    menu->addAction(Core::ActionManager::command(Constants::RUST_OPEN_REPL_IMPORT)->action());
    menu->addAction(
        Core::ActionManager::command(Constants::RUST_OPEN_REPL_IMPORT_TOPLEVEL)->action());
    insertExtraToolBarWidget(TextEditorWidget::Left, replButton);

}

void RustEditorWidget::finalizeInitialization()
{

    connect(textDocument(), &TextDocument::filePathChanged,
            this, &RustEditorWidget::updateInterpretersSelector);
    connect(RustSettings::instance(), &RustSettings::interpretersChanged,
            this, &RustEditorWidget::updateInterpretersSelector);
    connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::fileListChanged,
            this, &RustEditorWidget::updateInterpretersSelector);

}

void RustEditorWidget::setUserDefinedPython(const Interpreter &interpreter)
{

    const auto rustDocument = qobject_cast<RustDocument *>(textDocument());
    QTC_ASSERT(rustDocument, return);
    FilePath documentPath = rustDocument->filePath();
    QTC_ASSERT(!documentPath.isEmpty(), return);
    if (Project *project = ProjectManager::projectForFile(documentPath)) {
        if (Target *target = project->activeTarget()) {
            if (RunConfiguration *rc = target->activeRunConfiguration()) {
                if (auto interpretersAspect= rc->aspect<InterpreterAspect>()) {
                    interpretersAspect->setCurrentInterpreter(interpreter);
                    return;
                }
            }
        }
    }
    definePythonForDocument(textDocument()->filePath(), interpreter.command);
    updateInterpretersSelector();
    rustDocument->checkForRsls();

}

void RustEditorWidget::updateInterpretersSelector()
{

    if (!m_interpreters) {
        m_interpreters = new QToolButton(this);
        insertExtraToolBarWidget(TextEditorWidget::Left, m_interpreters);
        m_interpreters->setMenu(new QMenu(m_interpreters));
        m_interpreters->setPopupMode(QToolButton::InstantPopup);
        m_interpreters->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_interpreters->setProperty(StyleHelper::C_NO_ARROW, true);
    }

    QMenu *menu = m_interpreters->menu();
    QTC_ASSERT(menu, return);
    menu->clear();
    for (const QMetaObject::Connection &connection : m_projectConnections)
        disconnect(connection);
    m_projectConnections.clear();
    const FilePath documentPath = textDocument()->filePath();
    if (Project *project = ProjectManager::projectForFile(documentPath)) {
        m_projectConnections << connect(project,
                                        &Project::activeTargetChanged,
                                        this,
                                        &RustEditorWidget::updateInterpretersSelector);
        if (Target *target = project->activeTarget()) {
            m_projectConnections << connect(target,
                                            &Target::activeRunConfigurationChanged,
                                            this,
                                            &RustEditorWidget::updateInterpretersSelector);
            if (RunConfiguration *rc = target->activeRunConfiguration()) {
                if (auto interpreterAspect = rc->aspect<InterpreterAspect>()) {
                    m_projectConnections << connect(interpreterAspect,
                                                    &InterpreterAspect::changed,
                                                    this,
                                                    &RustEditorWidget::updateInterpretersSelector);
                }
            }
        }
    }

    auto setButtonText = [this](QString text) {
        constexpr int maxTextLength = 25;
        if (text.size() > maxTextLength)
            text = text.left(maxTextLength - 3) + "...";
        m_interpreters->setText(text);
    };

    const FilePath currentInterpreterPath = detectRust(textDocument()->filePath());
    const QList<Interpreter> configuredInterpreters = RustSettings::interpreters();
    auto interpretersGroup = new QActionGroup(menu);
    interpretersGroup->setExclusive(true);
    std::optional<Interpreter> currentInterpreter;
    for (const Interpreter &interpreter : configuredInterpreters) {
        QAction *action = interpretersGroup->addAction(interpreter.name);
        connect(action, &QAction::triggered, this, [this, interpreter]() {
            setUserDefinedPython(interpreter);
        });
        action->setCheckable(true);
        if (!currentInterpreter && interpreter.command == currentInterpreterPath) {
            currentInterpreter = interpreter;
            action->setChecked(true);
            setButtonText(interpreter.name);
            m_interpreters->setToolTip(interpreter.command.toUserOutput());
        }
    }
    menu->addActions(interpretersGroup->actions());
    if (!currentInterpreter) {
        if (currentInterpreterPath.exists())
            setButtonText(currentInterpreterPath.toUserOutput());
        else
            setButtonText(Tr::tr("No Python Selected"));
    }
    if (!interpretersGroup->actions().isEmpty()) {
        menu->addSeparator();
        auto venvAction = menu->addAction(Tr::tr("Create Virtual Environment"));
        connect(venvAction,
                &QAction::triggered,
                this,
                [self = QPointer<RustEditorWidget>(this), currentInterpreter]() {
                    if (!currentInterpreter)
                        return;
                    auto callback = [self](const std::optional<Interpreter> &venvInterpreter) {
                        if (self && venvInterpreter)
                            self->setUserDefinedPython(*venvInterpreter);
                    };
                    RustSettings::createVirtualEnvironmentInteractive(self->textDocument()
                                                                            ->filePath()
                                                                            .parentDir(),
                                                                        *currentInterpreter,
                                                                        callback);
                });
    }
    auto settingsAction = menu->addAction(Tr::tr("Manage Python Interpreters"));
    connect(settingsAction, &QAction::triggered, this, []() {
        Core::ICore::showOptionsDialog(Constants::C_RUSTOPTIONS_PAGE_ID);
    });
}


RustEditorFactory::RustEditorFactory()
{

    registerReplAction(this);

    setId(Constants::C_RUSTEDITOR_ID);
    setDisplayName(::Core::Tr::tr(Constants::C_EDITOR_DISPLAY_NAME));
    addMimeType(Constants::C_RS_MIMETYPE);
    addMimeType(Constants::C_TOML_MIMETYPE);           

    setEditorActionHandlers(TextEditorActionHandler::Format
                            | TextEditorActionHandler::UnCommentSelection
                            | TextEditorActionHandler::UnCollapseAll
                            | TextEditorActionHandler::FollowSymbolUnderCursor);

    setDocumentCreator([]() { return new RustDocument; });
    setEditorWidgetCreator([]() { return new RustEditorWidget; });
    setIndenterCreator([](QTextDocument *doc) { return new RustIndenter(doc); });
    setSyntaxHighlighterCreator([] { return new RustHighlighter; });
    setCommentDefinition(CommentDefinition::HashStyle);
    setParenthesesMatchingEnabled(true);
    setCodeFoldingSupported(true);
}

} // Rusty::Internal

#include "rusteditor.moc"
