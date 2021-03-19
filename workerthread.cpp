#include "workerthread.h"
#include <QDebug>

WorkerThread::WorkerThread(QObject *parent) : QObject(parent)
{

}

void WorkerThread::GetImageFromMainThread(QImage image){
    qDebug() << "GetImageFromMainThread" << pthread_self();
    emit HandleImage(image);
}
