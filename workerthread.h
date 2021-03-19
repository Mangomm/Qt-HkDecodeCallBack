#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include <QObject>
#include <QImage>

class WorkerThread : public QObject
{
    Q_OBJECT
public:
    explicit WorkerThread(QObject *parent = nullptr);

    void GetImageFromMainThread(QImage image);

signals:
    void HandleImage(QImage image);
private:
    QImage m_image;
};

#endif // WORKERTHREAD_H
