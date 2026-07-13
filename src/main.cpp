#include "ptapro/ui/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("二维码/条形码工具"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    ptapro::MainWindow window;
    window.resize(960, 640);
    window.show();

    return QApplication::exec();
}
