#ifndef RUSTLANGUAGECLIENT_H
#define RUSTLANGUAGECLIENT_H


#include <utils/fileutils.h>
#include <utils/temporarydirectory.h>

#include <languageclient/client.h>
#include <languageclient/languageclientsettings.h>

namespace Core { class IDocument; }
namespace ProjectExplorer { class ExtraCompiler; }
namespace TextEditor { class TextDocument; }

namespace Rusty::Internal {

class RsSideUicExtraCompiler;
class PythonLanguageServerState;
class RsLSInterface;

class RsLSClient : public LanguageClient::Client
{
    Q_OBJECT
public:
    explicit RsLSClient(RsLSInterface *interface);
    ~RsLSClient();

    void openDocument(TextEditor::TextDocument *document) override;
    void projectClosed(ProjectExplorer::Project *project) override;

    void updateExtraCompilers(ProjectExplorer::Project *project,
                              const QList<RsSideUicExtraCompiler *> &extraCompilers);

    static RsLSClient *clientForRust(const Utils::FilePath &python);
    void updateConfiguration();

private:
    void updateExtraCompilerContents(ProjectExplorer::ExtraCompiler *compiler,
                                     const Utils::FilePath &file);
    void closeExtraDoc(const Utils::FilePath &file);
    void closeExtraCompiler(ProjectExplorer::ExtraCompiler *compiler);

    Utils::FilePaths m_extraWorkspaceDirs;
    Utils::FilePath m_extraCompilerOutputDir;

    QHash<ProjectExplorer::Project *, QList<ProjectExplorer::ExtraCompiler *>> m_extraCompilers;
};

class RsLSConfigureAssistant : public QObject
{
    Q_OBJECT
public:
    static RsLSConfigureAssistant *instance();

    static void updateEditorInfoBars(const Utils::FilePath &python,
                                     LanguageClient::Client *client);
    static void openDocumentWithPython(const Utils::FilePath &python,
                                       TextEditor::TextDocument *document);

private:
    explicit RsLSConfigureAssistant(QObject *parent);

    void handlePyLSState(const Utils::FilePath &rust,
                         const PythonLanguageServerState &state,
                         TextEditor::TextDocument *document);
    void resetEditorInfoBar(TextEditor::TextDocument *document);
    void installPythonLanguageServer(const Utils::FilePath &python,
                                     QPointer<TextEditor::TextDocument> document);

    QHash<Utils::FilePath, QList<TextEditor::TextDocument *>> m_infoBarEntries;
};

} // Rusty::Internal

#endif // RUSTLANGUAGECLIENT_H
