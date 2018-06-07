#include "measuring.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    measuring w;
    w.show();

    return a.exec();
}
