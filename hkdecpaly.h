#ifndef HKDECPALY_H
#define HKDECPALY_H

#include <QWidget>
#include "workerthread.h"
#include <QImage>

//#include "HCNetSDK.h" //Qt下的头文件包含不包含最后一个文件夹,必须加上include
#include "include/HCNetSDK.h"

//#define NVR_V40         //该宏决定是否使用V40接口播放
#define CALLBACK_PLAY   //是否使用回调播放视频

QT_BEGIN_NAMESPACE
namespace Ui { class HkDecPaly; }
QT_END_NAMESPACE

class HkDecPaly : public QWidget
{
    Q_OBJECT

public:
    HkDecPaly(QWidget *parent = nullptr);
    ~HkDecPaly();

    //C++不支持成员函数作回调,但是我们可以通过海康回调的函数中的pUser调用C++的函数，这样我们就可以间接在普通回调中使用C++的私有成员
    //由于这里传本身不适合，所以被我弃掉
    void HkfRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser);

private slots:
    void on_LoginButton_clicked();

    void on_ExitButton_clicked();

    void on_StartButton_clicked();

private:
    Ui::HkDecPaly *ui;

private:
    bool HkCleanUp();
    void getImageAndUpdate(QImage temp);
#ifdef CALLBACK_PLAY
    void paintEvent(QPaintEvent *event) override;
#endif
    void threadJoin();
private:
    LONG m_userId;
    LONG m_lRealPlayHandle;

    WorkerThread *m_workerThread;      //自定义线程对象
    QThread      *m_thread;            //线程对象
    QImage        m_hkImage;           //海康的码流数据转成的image图片
    bool          m_isCallClean;       //防止两次调用HkCleanUp，原因是按下退出按钮调用一次，但是最终系统也会调用一次右上角的关闭按钮，而我让该信号也调用退出按钮的槽函数
/*
函数的返回值是void类型
函数只能声明不能定义
信号必须使用signals关键字进行声明
函数的访问属性自动被设置为protected
只能通过emit关键字调用函数（发射信号）
*/
signals:

};
#endif // HKDECPALY_H
