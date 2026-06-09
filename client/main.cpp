//#include "maindialog.h"
#include"ckernel.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    MainDialog w;
//    w.show();
    CKernel::GetInstance();//走构造
    return a.exec();
}
