#ifndef RUSTPROJECT_H
#define RUSTPROJECT_H

#include <projectexplorer/project.h>

namespace Rusty::Internal {

const char CrateMimeType[] = "text/plain";
const char RustMimeType[] = "text/x-rust-project";
const char RustMimeTypeLegacy[] = "text/x-rsqt-project";
const char RustProjectId[] = "RustProject";
const char RustErrorTaskCategory[] = "Task.Category.Rust";

class RustProject : public ProjectExplorer::Project
{
    Q_OBJECT
public:
    explicit RustProject(const Utils::FilePath &filename);

    bool needsConfiguration() const final { return false; }

private:
    RestoreResult fromMap(const QVariantMap &map, QString *errorMessage) override;
};

}

#endif // RUSTPROJECT_H
