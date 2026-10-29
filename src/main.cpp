#include "ui/Language.h"
#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FramePad"));
    QApplication::setOrganizationName(QStringLiteral("FramePad"));
    QApplication::setApplicationVersion(QStringLiteral(FRAMEPAD_VERSION));
    QApplication::setStyle(QStringLiteral("fusion"));

    fp::language::install(app);

    fp::MainWindow window;
    window.show();
    // arguments() rather than argv: it keeps non-ASCII paths intact on Windows.
    if (const QStringList args = QApplication::arguments(); args.size() > 1)
        window.openProject(args.at(1));

    return app.exec();
}
