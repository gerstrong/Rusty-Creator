#ifndef RUSTINDENTER_H
#define RUSTINDENTER_H

#include <texteditor/textindenter.h>

namespace Rusty {

class RustIndenter : public TextEditor::TextIndenter
{
public:
    explicit RustIndenter(QTextDocument *doc);
private:
    bool isElectricCharacter(const QChar &ch) const override;
    int indentFor(const QTextBlock &block,
                  const TextEditor::TabSettings &tabSettings,
                  int cursorPositionInEditor = -1) override;

    bool isElectricLine(const QString &line) const;
    int getIndentDiff(const QString &previousLine,
                      const TextEditor::TabSettings &tabSettings) const;
};

} // namespace Rusty

#endif // RUSTINDENTER_H
