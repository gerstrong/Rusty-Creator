#ifndef RSSIDE_H
#define RSSIDE_H

#include <utils/filepath.h>

#include <QCoreApplication>
#include <QTextDocument>

namespace TextEditor { class TextDocument; }
namespace ProjectExplorer { class RunConfiguration; }

namespace Rusty::Internal {

class RsSideInstaller : public QObject
{
    Q_OBJECT

public:
    static RsSideInstaller *instance();
    static void checkRsSideInstallation(const Utils::FilePath &python,
                                        TextEditor::TextDocument *document);

signals:
    void rsSideInstalled(const Utils::FilePath &python, const QString &pySide);

private:
    RsSideInstaller();

    void installRsside(const Utils::FilePath &python,
                       const QString &rsSide, TextEditor::TextDocument *document);
    void handleRsSideMissing(const Utils::FilePath &python,
                             const QString &pySide,
                             TextEditor::TextDocument *document);

    void runRsSideChecker(const Utils::FilePath &python,
                          const QString &pySide,
                          TextEditor::TextDocument *document);
    static bool missingRsSideInstallation(const Utils::FilePath &python, const QString &pySide);
    static QString importedRsSide(const QString &text);

    QHash<Utils::FilePath, QList<TextEditor::TextDocument *>> m_infoBarEntries;
};

} // Rusty::Internal


#endif // RSSIDE_H
