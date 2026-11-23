#pragma once

#include <QString>

namespace napp {

// Installs a message handler that writes "timestamp level message" lines to
// `filePath` (UTF-8) and mirrors them to stderr. Replaces any previous handler.
void initLogging(const QString& filePath);

QString logFilePath();

} // namespace napp
