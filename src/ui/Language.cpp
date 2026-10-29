#include "ui/Language.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace fp::language {

namespace {
constexpr auto Key = "ui/language";
}

QString preferred()
{
    return QSettings().value(Key).toString();
}

void setPreferred(const QString &code)
{
    QSettings().setValue(Key, code);
}

void install(QApplication &app)
{
    const QString code = preferred();
    const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);
    QLocale::setDefault(locale);

    // English is the source language, so it needs no translator.
    auto *ours = new QTranslator(&app);
    if (ours->load(locale, QStringLiteral("FramePad"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        app.installTranslator(ours);

    // Qt's own strings (standard dialog buttons and so on), shipped next to the exe by windeployqt.
    auto *qt = new QTranslator(&app);
    if (qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                 QApplication::applicationDirPath() + QStringLiteral("/translations"))
        || qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                    QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(qt);
}

} // namespace fp::language
