#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Gavrilin");
    a.setApplicationName("Sinc_interpolator");
    Widget w;
    w.show();
    return a.exec();
}
