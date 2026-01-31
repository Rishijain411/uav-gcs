// Qt UI standalone - QGroundControl style

#include "ui/MainWindow_QGC.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
