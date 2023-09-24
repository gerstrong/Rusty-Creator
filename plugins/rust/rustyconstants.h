#ifndef RUSTYCONSTANTS_H
#define RUSTYCONSTANTS_H

namespace Rusty::Constants {

const char ACTION_ID[] = "Rusty.Action";
const char MENU_ID[] = "Rusty.Menu";

const char RUST_LANGUAGE_ID[] = "Rust";

const char C_RUSTEDITOR_ID[] = "RustEditor.RustEditor";
const char C_RUSTRUNCONFIGURATION_ID[] = "RustEditor.RunConfiguration.";

const char C_EDITOR_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("QtC::Core", "Rust Editor");

const char C_RUSTOPTIONS_PAGE_ID[] = "RustEditor.OptionsPage";
const char C_PYLSCONFIGURATION_PAGE_ID[] = "RustEditor.RustLanguageServerConfiguration";
const char C_RUST_SETTINGS_CATEGORY[] = "R.Rust";

const char RUST_OPEN_REPL[] = "Rust.OpenRepl";
const char RUST_OPEN_REPL_IMPORT[] = "Rust.OpenReplImport";
const char RUST_OPEN_REPL_IMPORT_TOPLEVEL[] = "Rust.OpenReplImportToplevel";

const char RSLS_SETTINGS_ID[] = "Rust.RsLSSettingsID";

/*******************************************************************************
 * MIME type
 ******************************************************************************/
const char C_RS_MIMETYPE[] = "text/rust";
const char C_TOML_MIMETYPE[] = "text/plain";
const char C_PY_MIME_ICON[] = "text-x-rust";


} // namespace Rusty::Constants

#endif // RUSTYCONSTANTS_H
