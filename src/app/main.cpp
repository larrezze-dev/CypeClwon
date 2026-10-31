#include <QApplication>
#include <QMainWindow>

#include "core/version.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;
    window.setWindowTitle(QString::fromUtf8(cc::core::productName().data(),
                                            static_cast<qsizetype>(cc::core::productName().size())));
    window.resize(1280, 800);
    window.show();
    return app.exec();
}

