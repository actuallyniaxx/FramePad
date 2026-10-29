#pragma once

#include <QString>

class QApplication;

namespace fp::language {

// "" = follow the system, otherwise a language code such as "es" or "en".
QString preferred();
void setPreferred(const QString &code);

// Installs the translator for the preferred language. Call once before creating any window.
void install(QApplication &app);

} // namespace fp::language
