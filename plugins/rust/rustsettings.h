#ifndef RUSTSETTINGS_H
#define RUSTSETTINGS_H

#include <projectexplorer/runconfigurationaspects.h>

#include <utils/filepath.h>

namespace Rusty::Internal {

class RustSettings : public QObject
{
    Q_OBJECT

public:
    RustSettings();
    ~RustSettings();

    using Interpreter = ProjectExplorer::Interpreter;

    static QList<Interpreter> interpreters();
    static Interpreter defaultInterpreter();
    static Interpreter interpreter(const QString &interpreterId);
    static void setInterpreter(const QList<Interpreter> &interpreters, const QString &defaultId);
    static void addInterpreter(const Interpreter &interpreter, bool isDefault = false);
    static Interpreter addInterpreter(const Utils::FilePath &interpreterPath,
                                      bool isDefault = false,
                                      const QString &nameSuffix = {});
    static void setPyLSConfiguration(const QString &configuration);
    static bool pylsEnabled();
    static void setPylsEnabled(const bool &enabled);
    static QString pylsConfiguration();
    static RustSettings *instance();
    static void createVirtualEnvironmentInteractive(
        const Utils::FilePath &startDirectory,
        const Interpreter &defaultInterpreter,
        const std::function<void(std::optional<Interpreter>)> &callback);
    static void createVirtualEnvironment(
        const Utils::FilePath &directory,
        const Interpreter &interpreter,
        const std::function<void(std::optional<Interpreter>)> &callback,
        const QString &nameSuffix = {});
    static QList<Interpreter> detectRustVenvs(const Utils::FilePath &path);

signals:
    void interpretersChanged(const QList<Interpreter> &interpreters, const QString &defaultId);
    void pylsConfigurationChanged(const QString &configuration);
    void pylsEnabledChanged(const bool enabled);

public slots:
    void detectPythonOnDevice(const Utils::FilePaths &searchPaths,
                              const QString &deviceName,
                              const QString &detectionSource,
                              QString *logMessage);
    void removeDetectedPython(const QString &detectionSource, QString *logMessage);
    void listDetectedPython(const QString &detectionSource, QString *logMessage);

private:
    void initFromSettings(QSettings *settings);
    void writeToSettings(QSettings *settings);

    QList<Interpreter> m_interpreters;
    QString m_defaultInterpreterId;
    bool m_pylsEnabled = true;
    QString m_pylsConfiguration;

    static void saveSettings();
};

} // Python::Internal


#endif // RUSTSETTINGS_H
