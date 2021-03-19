#include <QDebug>
#include <QString>
#include <QMessageBox>
#include <QThread>
#include <QPainter>

#include "hkdecpaly.h"
#include "ui_hkdecpaly.h"
#include <Windows.h>
#include "PlayM4.h"

#define mycout (qDebug()<<__FILE__<<__LINE__)

LONG nPort = -1;        //海康播放库的端口


HkDecPaly::HkDecPaly(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HkDecPaly)
{
    ui->setupUi(this);

    //mycout<<pthread_self();测试线程号

#ifdef CALLBACK_PLAY
    //不能指定父对象,故他要手动delete.线程不能处理图形界面即对话框这种框,但可以利用他利用他带参数的信号传出来到主线程
    m_workerThread = new WorkerThread;
    m_thread= new QThread(this);
    //将自定义类对象m_workerThread移动到子线程中，自定义类就相当于一个子线程。
    m_workerThread->moveToThread(m_thread);
    //开启线程，一般在开始和结束的按钮处理,但与Qt4版本与区别，这里只是启动了线程，线程函数还没启动。
    m_thread->start();

    /*
     * 参1：参2的类对象
     * 参2：信号
     * 参3：参4的类对象
     * 参4：槽函数
    */
    connect(m_workerThread, &WorkerThread::HandleImage, this, &HkDecPaly::getImageAndUpdate);//参数13不熟,可由参数24得出
#endif
    connect(this, &HkDecPaly::destroyed, this, &HkDecPaly::on_ExitButton_clicked);//处理右上角的关闭按钮

    m_isCallClean = false;

    m_userId = -1;
    m_lRealPlayHandle = -1;

    //1 初始化海康网络设备
    NET_DVR_Init();

    ui->IpLine->setText("192.168.1.196");
    ui->PortLine->setText("8000");
    ui->UserLine->setText("admin");
    ui->PwdLine->setText("Runone2016");
    ui->IpChannelsLine->setText("33");//有效Ip通道，有的从1开始，有点从33开始，与DVR,NVR设备类型有关

    //ui->LoginButton->setEnabled(false);
    ui->StartButton->setEnabled(false);

}

HkDecPaly::~HkDecPaly()
{
    delete ui;
}

//yv12转RGB888
static bool yv12ToRGB888(const unsigned char *yv12, unsigned char *rgb888, int width, int height)
{
    if ((width < 1) || (height < 1) || (yv12 == NULL) || (rgb888 == NULL)) {
        return false;
    }

    int len = width * height;
    unsigned char const *yData = yv12;
    unsigned char const *vData = &yData[len];
    unsigned char const *uData = &vData[len >> 2];

    int rgb[3];
    int yIdx, uIdx, vIdx, idx;

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            yIdx = i * width + j;
            vIdx = (i / 2) * (width / 2) + (j / 2);
            uIdx = vIdx;

            rgb[0] = static_cast<int>(yData[yIdx] + 1.370705 * (vData[uIdx] - 128));
            rgb[1] = static_cast<int>(yData[yIdx] - 0.698001 * (uData[uIdx] - 128) - 0.703125 * (vData[vIdx] - 128));
            rgb[2] = static_cast<int>(yData[yIdx] + 1.732446 * (uData[vIdx] - 128));

            for (int k = 0; k < 3; ++k) {
                idx = (i * width + j) * 3 + k;
                if ((rgb[k] >= 0) && (rgb[k] <= 255)) {
                    rgb888[idx] = static_cast<unsigned char>(rgb[k]);
                } else {
                    rgb888[idx] = (rgb[k] < 0) ? (0) : (255);
                }
            }
        }
    }
    return true;
}

void HkDecPaly::getImageAndUpdate(QImage temp){
    m_hkImage = temp;
    update();//必须使用该函数来间接调用绘图事件
}

#ifdef CALLBACK_PLAY
/*
 * 重写窗口绘图事件
 * 当需要绘图时，系统会自动调用。当然，这里会不断在getImageAndUpdate触发更新也可以
*/
void HkDecPaly::paintEvent(QPaintEvent *event){
    //QPainter p(ui->listView);
    QPainter p(this);

    int x = ui->widget->x();
    int y = ui->widget->y();
    int w = ui->widget->width();
    int h = ui->widget->height();
    //mycout<<x<<y<<w<<h<<m_hkImage.width()<<m_hkImage.height();

    //QRect rec(x,y);
    p.drawImage(QPoint(x,y), m_hkImage.scaled(QSize(w,h)));
}
#endif

/*
 * 解码回调。视频为YUV数据(YV12)，音频为PCM数据。
 * 参1：播放库端口
 * 参2：码流数据
 * 参3：码流数据大小
 * 参4：解码帧
 * 参5,6：保留位，一般可以将指针强转来获取到自定义的用户数据
*/
void CALLBACK DecCBFun(long nPort,char * pBuf,long nSize,FRAME_INFO *pFrameInfo, long nReserved1,long nReserved2)
{

    //YV12(平面格式plane)：连续存储Y,V,U的1:4:4.例(yyyy yyyy yyyy yyyy):vvvv:uuuu
    //YU12(平面格式plane)：连续存储Y,U,V的1:4:4.例(yyyy yyyy yyyy yyyy):uuuu:vvvv.两者只是互相调换uv顺序

    long lFrameType = pFrameInfo->nType;
    WorkerThread *thread = (WorkerThread *)nReserved1;
    //视频数据是 T_YV12 音频数据是 T_AUDIO16
    if (lFrameType == T_YV12) {
        int width = pFrameInfo->nWidth;
        int height = pFrameInfo->nHeight;
        QImage image(width, height, QImage::Format_RGB888);
        if (yv12ToRGB888((unsigned char *)pBuf, image.bits(), width, height)) {
              thread->GetImageFromMainThread(image);//若想作为线程函数，只能通过信号和槽的方式调用。但若直接调用，也可以，相当于每次自己调用该线程
        }
    } else if (lFrameType == T_AUDIO16) {
        //mycout<<"Audio nStamp:%d",pFrameInfo->nStamp;
        //mycout<<"test_DecCb_Write Audio16";
    }

}


/*
 * 回调函数中可以处理自己想要处理的内容，并非固定的，以下调用播放库播放只是其中一种。
 * lRealHandle：播放句柄。有的情况可以根据播放句柄分辨不同的视频，从而达到多分屏显示。
 * dwDataType：数据类型。
 * pBuffer：回调传回的码流数据，传出类型。
 * dwBufSize：码流大小。
 * pUser：播放时设置的自定义用户参数。
*/
void CALLBACK fRealDataCallBack(LONG lRealHandle,DWORD dwDataType,BYTE *pBuffer,DWORD dwBufSize,void *pUser){

    BOOL ret = FALSE;

    /*
     * 不能直接使用窗口播放，否则出现：
     * Cannot set parent, new parent is in a different thread错误
     * 原因：函数被调用在海康dll的线程中，所以再使用ui的成员报错，即子控件widget属于ui，而再在海康的回调线程使用就会导致工作在不同的线程中导致保错
    */
    //HWND hw = (HWND)this->ui->widget->winId();

    switch (dwDataType)
    {
        //首先在系统头中初始化并打开海康播放库.系统头中的步骤：1）打开流->2)开始播放.中间可以设置一些内容.
        case NET_DVR_SYSHEAD:
        {
            if(nPort >= 0){
                break;//同一路码流不需要多次调用开流接口
            }

            ret = PlayM4_GetPort(&nPort);
            if(ret == FALSE){
                mycout<<NET_DVR_GetErrorMsg();
                break;
            }
            //1 打开流----->参4为回调码流的缓冲区,不能太小，否则码率太快的话缓冲区溢出，用户在下面的case需要自行暂停一下再input
            ret = PlayM4_OpenStream(nPort,pBuffer,dwBufSize,2048*2048);
            if(ret == FALSE){
                mycout<<NET_DVR_GetErrorMsg();
                break;
            }

            //设置解码回调函数,在PlayM4_Play前调用
//            if (!PlayM4_SetDecCallBackEx(nPort,DecCBFun,NULL,NULL))//执行回调
//            {
//                mycout<<NET_DVR_GetErrorMsg();
//                break;
//            }

            //设置解码回调函数 只解码不显示
            if (!PlayM4_SetDecCallBackMend(nPort,DecCBFun,(long)pUser)) {
                mycout<<PlayM4_GetLastError(nPort);
                break;
            }

            //2 开始播放(系统头实际就是为了准备播放)，只解码不显示，参2传NULL
            ret = PlayM4_Play(nPort, NULL);
            if(ret == FALSE){
                mycout<<NET_DVR_GetErrorMsg();
                break;
            }
            // 打开音频播放(独占方式)
            if (!PlayM4_PlaySound(nPort))
            {
                mycout<<PlayM4_GetLastError(nPort);
                break;
            }

            break;
        }

        //码流数据，海康回调出来的就是PS流。这里的处理方法是将数据传回去pBuffer，让上面的播放库处理；也可以自己解码ps成264交给ffmpeg处理
        case NET_DVR_STREAMDATA:
        {
            if(dwBufSize > 0 && nPort != -1){
                ret = PlayM4_InputData(nPort,pBuffer,dwBufSize);
                //inData返回false的话是由于上面的缓存1024*1024满了，放不下去,用户需要自行暂停一下再input
                while (ret == FALSE) {
                    Sleep(20);
                    ret = PlayM4_InputData(nPort, pBuffer, dwBufSize);//一直满的话继续Sleep
                    mycout<<"PlayM4_InputData Buf too more.";
                }
            }
            break;
        }

        //其它数据
        default:
        {
            //这里同理
            while (ret == FALSE){
                Sleep(10);
                ret = PlayM4_InputData(nPort,pBuffer,dwBufSize);
                mycout<<"PlayM4_InputData Buf too more.";
            }

            break;
        }

    }

}


//2 登录槽函数
void HkDecPaly::on_LoginButton_clicked()
{
    //已经在登录的不让它在登录
    if(m_userId >= 0){
        return;
    }

    //获取登录的相应内容
    QString ip = ui->IpLine->text();
    QString port = ui->PortLine->text();
    QString user = ui->UserLine->text();
    QString pwd = ui->PwdLine->text();

#ifdef NVR_V40
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40;//V40的传出参数,存放着设备信息参数的结构体
    NET_DVR_USER_LOGIN_INFO struLoginInfo;//将登录时的密码信息存进登录，避免调用登陆函数时参数过多
    memset(&struDeviceInfoV40, 0, sizeof(NET_DVR_DEVICEINFO_V40));
    memset(&struLoginInfo, 0, sizeof(NET_DVR_USER_LOGIN_INFO));

    struLoginInfo.bUseAsynLogin = 0;                          //同步登录方式
    strcpy(struLoginInfo.sDeviceAddress, ip.toUtf8().data()); //设备IP地址
    struLoginInfo.wPort = port.toUtf8().toInt();			  //设备服务端口
    strcpy(struLoginInfo.sUserName, user.toUtf8().data());    //设备登录用户名
    strcpy(struLoginInfo.sPassword, pwd.toUtf8().data());     //设备登录密码

    m_userId = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if(m_userId < 0){
        QMessageBox::information(this, "登录", "登录出错，请检查账号密码等内容");
        return;
    }

    QMessageBox::information(this, "登录", "登录成功");
    ui->StartButton->setEnabled(true);
    ui->LoginButton->setEnabled(false);

#else
    NET_DVR_DEVICEINFO_V30 dev30Info;//V30的传出参数,存放着设备信息参数的结构体
    memset(&dev30Info, 0, sizeof(NET_DVR_DEVICEINFO_V30));
    m_userId = NET_DVR_Login_V30(ip.toUtf8().data(),port.toUtf8().toInt(),user.toUtf8().data(),pwd.toUtf8().data(),&dev30Info);
    if(m_userId < 0){
        QMessageBox::information(this, "登录", "登录出错，请检查账号密码等内容");
        return;
    }

    QMessageBox::information(this, "登录", "登录成功");
    ui->StartButton->setEnabled(true);
    ui->LoginButton->setEnabled(false);
#endif

}


//3 预览/停止函数槽函数
void HkDecPaly::on_StartButton_clicked()
{
    //没有登录不给它播放
    if(m_userId < 0){
        QMessageBox::information(this,"提醒","请先登录设备");
        return;
    }

    if(m_lRealPlayHandle < 0){
#ifdef NVR_V40
        int ipChannels = ui->IpChannelsLine->text().toInt();

        NET_DVR_PREVIEWINFO struPlayInfo;
        memset(&struPlayInfo, 0, sizeof(NET_DVR_PREVIEWINFO));

#ifdef CALLBACK_PLAY
        struPlayInfo.hPlayWnd = NULL;                           //播放方法2，置空通过回调中调用播放库播放。
#else
        struPlayInfo.hPlayWnd = (HWND)ui->widget->winId();      //播放方法1，直接使用窗口播放。需要转成对应的HWND句柄。窗口为空，设备SDK不解码只取流,但可以通过在回调调用播放库播放。
#endif
        struPlayInfo.lChannel = ipChannels;                     //lChannel 设备通道号,nPort不一样,nPort是针对于海康播放库的通道
        struPlayInfo.dwStreamType = 0;
        struPlayInfo.dwLinkMode = 0;
        struPlayInfo.bBlocked = 0;
        struPlayInfo.byVideoCodingType = 0;
        struPlayInfo.dwDisplayBufNum = 0;
        struPlayInfo.byProtoType = 0;

#ifdef CALLBACK_PLAY
        m_lRealPlayHandle = NET_DVR_RealPlay_V40(m_userId, &struPlayInfo, fRealDataCallBack, m_workerThread);//方法2：通过回调调用播放库,参4不能是NULL
#else
        m_lRealPlayHandle = NET_DVR_RealPlay_V40(m_userId, &struPlayInfo, NULL, NULL);//方法1：直接播放
#endif
        if (m_lRealPlayHandle < 0){
            NET_DVR_Logout(m_userId);
            QMessageBox::information(this, "播放", "播放失败");
            return;
        }

        ui->StartButton->setText("Stop");

#else
        int ipChannels = ui->IpChannelsLine->text().toInt();

        NET_DVR_CLIENTINFO clientInfo;
        clientInfo.lChannel = ipChannels;                       //lChannel 设备通道号
#ifdef CALLBACK_PLAY
        clientInfo.hPlayWnd = NULL;                             //播放方法2，置空通过回调中调用播放库播放。
#else
        clientInfo.hPlayWnd = (HWND)ui->widget->winId();        //播放方法1，直接使用窗口播放。需要转成对应的HWND句柄。窗口为空，设备SDK不解码只取流。
#endif

        clientInfo.lLinkMode = 0;                               //Main Stream
        clientInfo.sMultiCastIP = NULL;

#ifdef CALLBACK_PLAY
        m_lRealPlayHandle = NET_DVR_RealPlay_V30(m_userId, &clientInfo, fRealDataCallBack, m_workerThread, TRUE);//方法2

#else
        m_lRealPlayHandle = NET_DVR_RealPlay_V30(m_userId, &clientInfo, NULL, NULL, TRUE);//方法1
#endif
        if (m_lRealPlayHandle < 0){
            NET_DVR_Logout(m_userId);
            QMessageBox::information(this, "播放", "播放失败");
            return;
        }

        ui->StartButton->setText("Stop");
#endif
    }else{
        BOOL ret = NET_DVR_StopRealPlay(m_lRealPlayHandle);
        if(ret == false){
            QMessageBox::information(this, "停止", "视频停止失败");
            return;
        }
        m_lRealPlayHandle = -1; //重置为未播放

#ifdef CALLBACK_PLAY
        //2 停止用播放库播放(必须停止，否则按下停止播放画面仍在播放数秒才停)
        if (nPort>-1)
        {
            if (!PlayM4_StopSound())//1)停止音频播放(独占)(因为上面也播放了音频,这里可选,下面3步若用了播放库是必要的)
            {
                mycout<<PlayM4_GetLastError(nPort);
            }
            if (!PlayM4_Stop(nPort))//停止播放
            {
                mycout<<PlayM4_GetLastError(nPort);
            }
            if (!PlayM4_CloseStream(nPort))//2)关闭通道流
            {
                mycout<<PlayM4_GetLastError(nPort);
            }
            PlayM4_FreePort(nPort);//3)释放通道端口并置为原始值-1
            nPort=-1;
        }
#endif
        ui->StartButton->setText("Start");

        //将视频停止的画面清掉
        //ui->widget->reset();
        //ui->widget->update();
    }

}


/*
 * 主动按下按钮退出槽函数(参考例子的做法是要求我们停止预览才能退出，我这里直接做成一键退出不管你是否在预览)
*/
void HkDecPaly::on_ExitButton_clicked()
{
    //m_isCallClean是防止两次调用on_ExitButton_clicked
    if(m_isCallClean == false){
        if(m_lRealPlayHandle >= 0){
            BOOL ret = NET_DVR_StopRealPlay(m_lRealPlayHandle);
            if(ret == false){
                QMessageBox::information(this, "退出", "退出失败，在停止播放时");
                return;
            }
            m_lRealPlayHandle = -1;
        }

        if(m_userId >= 0){
            if(NET_DVR_Logout(m_userId) == FALSE){
                QMessageBox::information(this, "退出", "退出失败，在退出登录时");
                //失败也是调用下面的清理函数
            }
            m_userId = -1;
        }

        if(HkCleanUp() == false){
            //项目中清理失败最好是打印日志
            qDebug()<<"HkCleanUp Failed======================================="<<endl;
        }

    #ifdef CALLBACK_PLAY
        if(m_thread!=NULL)
            threadJoin();
    #endif

        //关闭窗口
        this->close();
        m_isCallClean = true;
    }
}


/*
 * 清理海康资源
*/
bool HkDecPaly::HkCleanUp(){
    if(NET_DVR_Cleanup() == FALSE){
        return false;
    }
    return true;
}

/*
 * 子线程相关资源回收
*/
void HkDecPaly::threadJoin()
{
    //退出子线程
    m_thread->quit();
    //回收资源
    m_thread->wait();
    delete m_workerThread;
    m_workerThread = NULL;
    delete m_thread;
    m_thread = NULL;
}
