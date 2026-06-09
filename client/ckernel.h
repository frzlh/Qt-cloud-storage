#ifndef CKERNEL_H
#define CKERNEL_H
#include"maindialog.h"
#include <QObject>
#include"INetMediator.h"
#include"packdef.h"
#include"logindialog.h"
#include"common.h"
#include"csqlite.h"


//#define USER_SERVER 1

//核心处理类
//单例模式
//1、构造 拷贝构造 析构 私有化  2、提供静态的公有的获取对象的方法

//协议映射表

//类成员函数指针
class CKernel;
typedef void (CKernel::*PFUN)(uint lSendIP , char* buf , int nlen);

class CKernel : public QObject
{
    Q_OBJECT
private:
    explicit CKernel(QObject *parent = nullptr);
    explicit CKernel(const CKernel& kernel){}
    ~CKernel(){}

    void loadIniFile();
    void setNetPackMap();
    void setSystemPath();

signals:
    void SIG_updateUploadFileProgress(int timestamp ,int pos );
    void SIG_updateDownloadFileProgress(int timestamp ,int pos );

public:
    static CKernel* GetInstance(){
        static CKernel kernel;
        return &kernel;
    }

private slots:
    //普通槽函数
    void slot_destroy();
    void slot_registerCommit(QString tel,QString password,QString name);
    void slot_loginCommit(QString tel,QString password);
    //什么绝对路径的文件，上传到什么目录下
    void slot_uploadFile(QString path,QString dir);
    //上传什么路径的文件夹，到什么目录下面
    void slot_uploadFolder(QString path,QString dir);

    void slot_getCurDirFileList();
    //什么文件id，什么目录下的文件 下载
    void slot_downloadFile(int fileid,QString dir);
    //什么文件id，什么目录下的文件夹 下载
    void slot_downloadFolder(int fileid,QString dir);
     //什么路径下创建什么名字的文件夹
    void slot_addFolder(QString  name,QString dir);
    //改变路径
    void slot_changeDir(QString dir);
    //分享 什么目录下面的文件列表
    void slot_shareFile(QVector<int>fileidArray,QString dir);
    //获取个人所有分享
    void slot_getMyShare();
    //获取什么分享码的文件 添加到什么目录
    void slot_getShareByLink(int code,QString dir);
    //删除什么目录下的 一系列文件（文件id 数组）
    void slot_deleteFile(QVector<int>fileidArray,QString dir);
    //设置上传暂停 0 开始 1 暂停
    void slot_setUploadPause(int timestamp,int isPause);
    //设置下载暂停 0 开始 1 暂停
    void slot_setDownloadPause(int timestamp,int isPause);




    //网络响应槽函数
    void slot_dealClientData(uint lSendIP , char* buf , int nlen);
    void slot_dealRegisterRs(uint lSendIP,char* buf,int len);
    void slot_dealLoginRs(uint lSendIP,char* buf,int len);
    void slot_dealUploadFileRs(uint lSendIP,char* buf,int len);
    void slot_dealFileContentRs(uint lSendIP,char* buf,int len);
    void slot_dealGetFileInfoRs(uint lSendIP,char* buf,int len);
    void slot_dealFileHeaderRq(uint lSendIP,char* buf,int len);
    void slot_dealFileContentRq(uint lSendIP,char* buf,int len);
    void slot_dealAddFolderRs(uint lSendIP,char* buf,int len);
    void slot_dealQuickUploadRs(uint lSendIP,char* buf,int len);
    void slot_dealShareFileRs(uint lSendIP,char* buf,int len);
    void slot_dealMyShareRs(uint lSendIP,char* buf,int len);
    void slot_dealGetShareRs(uint lSendIP,char* buf,int len);
    void slot_dealFolderHeadRq(uint lSendIP,char* buf,int len);
    void slot_dealDeleteFileRs(uint lSendIP,char* buf,int len);
    void slot_dealContinueUploadRs(uint lSendIP,char* buf,int len);

#ifdef USER_SERVER
    void slot_dealServerData(uint lSendIP , char* buf , int nlen);
#endif

private:
    void SendData(char * buf,int len);

private:
    MainDialog * m_mainDialog;

    QString m_ip;
    QString m_port;

    QString m_name;
    int m_id;
    QString m_curDir;  //网盘当前的目录
    QString m_sysPath; //默认存储的系统路径（绝对路径）exe同级下NetDisk文件夹下

    INetMediator* m_tcpClient;
    LoginDialog* m_loginDialog;
    //key 时间戳 hhmmsszzz int 21 xxxx xxxx value 文件信息
    std::map<int,FileInfo> m_mapTimestampToFileInfo;
    PFUN m_netPackMap[_DEF_PACK_COUNT];

    //退出标志
    bool m_quit;
    //数据库
    CSqlite *m_sql;

private:
    //登录之后，初始化数据
    void InitDatabase(int id);
    //缓存上传的任务
    void slot_writeUploadTask(FileInfo* info);
    //缓存下载的任务
    void slot_writeDownloadTask(FileInfo&info);
    //完成任务，删除上传记录
    void slot_deleteUploadTask(FileInfo* info);
    //完成任务，删除下载任务
    void slot_deleteDownloadTask(FileInfo* info);
    //加载上传任务
    void slot_getUploadTask(QList<FileInfo>&infoList);
    //加载下载任务
    void slot_getDownloadTask(QList<FileInfo>&infoList);

 #ifdef USER_SERVER
    INetMediator* m_tcpServer;
 #endif


};

#endif // CKERNEL_H
