#ifndef RUSTEDITORFACTORY_H
#define RUSTEDITORFACTORY_H

#include <texteditor/texteditor.h>

namespace Rusty::Internal {

class RustEditorFactory : public TextEditor::TextEditorFactory
{
public:
    RustEditorFactory();
};

} // Rusty::Internal

#endif // RUSTEDITORFACTORY_H
