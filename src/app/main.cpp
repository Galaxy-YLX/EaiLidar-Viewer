#include <QApplication>

#include "app/mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EaiLidar Viewer");
    app.setOrganizationName("EaiLidar");

    eai::MainWindow window;
    window.show();
    return app.exec();
}
