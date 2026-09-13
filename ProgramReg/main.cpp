// ProgramReg — 安装注册程序（requireAdministrator 清单提权）
#include "MainWindow.h"

#include "../Shared/Theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>

using namespace esp;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(globalStyleSheet());
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/ProgramReg.ico")));

    QFont f(QStringLiteral("Segoe UI"), 10);
    app.setFont(f);

    MainWindow w;
    w.show();
    return app.exec();
}
