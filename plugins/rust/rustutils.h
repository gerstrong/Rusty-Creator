#ifndef RUSTUTILS_H
#define RUSTUTILS_H

#include <utils/filepath.h>

namespace Rusty::Internal {

enum class ReplType { Unmodified, Import, ImportToplevel };
void openPythonRepl(QObject *parent, const Utils::FilePath &file, ReplType type);
Utils::FilePath detectRust(const Utils::FilePath &documentPath);
void definePythonForDocument(const Utils::FilePath &documentPath, const Utils::FilePath &python);
QString pythonName(const Utils::FilePath &pythonPath);

class RustProject;
RustProject *rustProjectForFile(const Utils::FilePath &pythonFile);

void createVenv(const Utils::FilePath &python,
                const Utils::FilePath &venvPath,
                const std::function<void(bool)> &callback);

} // Rusty::Internal

#endif // RUSTUTILS_H
