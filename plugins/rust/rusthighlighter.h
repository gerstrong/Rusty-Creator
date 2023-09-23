#ifndef RUSTHIGHLIGHTER_H
#define RUSTHIGHLIGHTER_H


#include <texteditor/syntaxhighlighter.h>

namespace Rusty::Internal {

class Scanner;

class RustHighlighter : public TextEditor::SyntaxHighlighter
{
public:
    RustHighlighter();

private:
    void highlightBlock(const QString &text) override;
    int highlightLine(const QString &text, int initialState);
    void highlightImport(Internal::Scanner &scanner);

    int m_lastIndent = 0;
    bool withinLicenseHeader = false;
};

} // namespace Rusty::Internal

#endif // RUSTHIGHLIGHTER_H
