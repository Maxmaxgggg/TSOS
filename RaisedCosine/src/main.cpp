#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("RCosGenerator");
    a.setOrganizationName("Eshenko");
    Widget w;
    w.show();
    return a.exec();
}
