#include <QtGui/QApplication>
#include "Med.h"


int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Med foo;
    foo.show();
    return app.exec();
}
