#ifndef RUSTEDITORFACTORY_H
#define RUSTEDITORFACTORY_H

#include <texteditor/texteditor.h>

namespace Rusty::Internal {

class RustEditorFactory : public TextEditor::TextEditorFactory
{
public:
    RustEditorFactory();
private:
    QObject m_guard;
};

} // Rusty::Internal

#endif // RUSTEDITORFACTORY_H
