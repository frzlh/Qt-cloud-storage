#include "ckernel.h"
#include<QDebug>
#include"TcpClientMediator.h"
#include"TcpServerMediator.h"
#include<QMessageBox>
#include<QCoreApplication>
#include<QThread>


#define NetMap(a) m_netPackMap[a-_DEF_PACK_BASE]

void CKernel::setNetPackMap()
{
    memset(m_netPackMap,0,sizeof(PFUN)*_DEF_PACK_COUNT);
    //协议映射表 key 协议头偏移量 value 函数指针
    //通过 协议头 找到 对应的处理函数
    NetMap(_DEF_PACK_LOGIN_RS)=&CKernel::slot_dealLoginRs;
    NetMap(_DEF_PACK_REGISTER_RS)=&CKernel::slot_dealRegisterRs;
    NetMap(_DEF_PACK_UPLOAD_FILE_RS)=&CKernel::slot_dealUploadFileRs;
    NetMap(_DEF_PACK_FILE_CONTENT_RS)=&CKernel::slot_dealFileContentRs;
    NetMap(_DEF_PACK_GET_FILE_INFO_RS)=&CKernel::slot_dealGetFileInfoRs;
    NetMap(_DEF_PACK_FILE_HEADER_RQ)=&CKernel::slot_dealFileHeaderRq;
    NetMap(_DEF_PACK_FILE_CONTENT_RQ)=&CKernel::slot_dealFileContentRq;
    NetMap(_DEF_PACK_ADD_FOLDER_RS)=&CKernel::slot_dealAddFolderRs;
    NetMap(_DEF_PACK_QUICK_UPLOAD_RS)=&CKernel::slot_dealQuickUploadRs;
    NetMap(_DEF_PACK_SHARE_FILE_RS)=&CKernel::slot_dealShareFileRs;
    NetMap(_DEF_PACK_MY_SHARE_RS)=&CKernel::slot_dealMyShareRs;
    NetMap(_DEF_PACK_GET_SHARE_RS)=&CKernel::slot_dealGetShareRs;
    NetMap(_DEF_PACK_FOLDER_HEADER_RQ)=&CKernel::slot_dealFolderHeadRq;
    NetMap(_DEF_PACK_DELETE_FILE_RS)=&CKernel::slot_dealDeleteFileRs;
    NetMap(_DEF_PACK_CONTINUE_UPLOAD_RS)=&CKernel::slot_dealContinueUploadRs;
}

//系统路径组成：exe同级 ./NetDisk todo 后续考虑逐层dir
#include<QDir>
void CKernel::setSystemPath()
{
    QString path=QCoreApplication::applicationDirPath()+"/NetDisk";
    QDir dir;
    //没有文件夹 创建
    if(!dir.exists(path)){
        dir.mkdir(path);//只能创建一层
    }
    //默认路径
    qDebug()<<"path"<<path<<endl;
    m_sysPath=path;
}

#include"md5.h"
#define MD5_KEY "1234"
//password_1234
//生成MD5函数
static std::string getMD5(QString val){ //static 函数 当前文件可用
    QString str=QString("%1_%2").arg(val).arg(MD5_KEY);
    MD5 md(str.toStdString());
    qDebug()<<str<<" md5: "<<md.toString().c_str();
    return md.toString();
}

#include<QTextCodec>
// QString -> char* gb2312
void Utf8ToGB2312( char* gbbuf , int nlen ,QString& utf8)
{
    //转码的对象
    QTextCodec * gb2312code = QTextCodec::codecForName( "gb2312");//编码器
    //QByteArray char 类型数组的封装类 里面有很多关于转码 和 写IO的操作
    QByteArray ba = gb2312code->fromUnicode( utf8 );// Unicode -> 转码对象的字符集

    strcpy_s ( gbbuf , nlen , ba.data() );
}

//获取文件MD5
static std::string getFileMD5(QString path){//path是utf8编码 fopen需要用ANSI的编码 需要转码
    //打开文件，读取文件内容，读到md5类，生成md5
    FILE * pFile=nullptr;
    //fopen 如果有中文 支持ANSI 编码 使用ascii码
    //path里面是utf8（qt默认的） 编码
    char buf[1000]="";
    Utf8ToGB2312(buf,1000,path);
    pFile=fopen(buf,"rb");//二进制只读
    if(!pFile){
        qDebug()<<"file md5 open fail";
        return string();
    }
    MD5 md;
    int len=0;
    do{
        //缓冲区，一次读多少，读多少次，文件指针 返回值为读成功的次数
        len=fread(buf,1,1000,pFile);
        md.update(buf,len);//不断拼接文本，不断更新md5

        //为了避免阻塞窗口线程，影响事件循环，加入下面的处理 将信号取出并执行
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);

    }while(len>0);
    fclose(pFile);
    //file md5:  be10eabb469db3e0823385be73755ac8
//    在线生成：   be10eabb469db3e0823385be73755ac8

    qDebug()<<"file md5: "<<md.toString().c_str();

    return md.toString();
}

void CKernel::SendData(char *buf, int len)
{
    m_tcpClient->SendData(0,buf,len);
}

CKernel::CKernel(QObject *parent) : QObject(parent),m_id(0),m_curDir("/"),m_quit(false)
{

    //设置网络映射
    setNetPackMap();
    //加载配置文件
    loadIniFile();



    setSystemPath();

#ifdef USER_SERVER

    m_tcpServer=new TcpServerMediator;
    connect(m_tcpServer,SIGNAL(SIG_ReadyData(uint,char*,int))
            ,this,SLOT(slot_dealServerData(uint,char*,int)));
    m_tcpServer->OpenNet();

#endif

    m_tcpClient=new TcpClientMediator;
    connect(m_tcpClient,SIGNAL(SIG_ReadyData(uint,char*,int))
            ,this,SLOT(slot_dealClientData(uint,char*,int)));

    //客户端连接真实地址
    m_tcpClient->OpenNet(m_ip.toStdString().c_str(),m_port.toInt());
    m_loginDialog=new LoginDialog;
    m_mainDialog = new MainDialog;

    connect(m_loginDialog,SIGNAL(SIG_registerCommit(QString ,QString ,QString )),this,SLOT(slot_registerCommit(QString ,QString ,QString )));
    connect(m_loginDialog,SIGNAL(SIG_loginCommit(QString ,QString )),this,SLOT(slot_loginCommit(QString ,QString )));
    m_loginDialog->show();
    connect(m_mainDialog,SIGNAL(SIG_close()),this,SLOT(slot_destroy()));
    connect(m_mainDialog,SIGNAL(SIG_uploadFile(QString,QString)),this,SLOT(slot_uploadFile(QString,QString)));
    connect(this,SIGNAL(SIG_updateUploadFileProgress(int,int)),m_mainDialog,SLOT(slot_updateUploadFileProgress(int,int)));
    connect(m_mainDialog,SIGNAL(SIG_downloadFile(int,QString)),this,SLOT(slot_downloadFile(int,QString)));
    connect(m_mainDialog,SIGNAL(SIG_downloadFolder(int,QString)),this,SLOT(slot_downloadFolder(int,QString)));
    connect(this,SIGNAL(SIG_updateDownloadFileProgress(int,int)),m_mainDialog,SLOT(slot_updateDownloadFileProgress(int,int)));
    connect(m_mainDialog,SIGNAL(SIG_addFolder(QString,QString)),this,SLOT(slot_addFolder(QString,QString)));
    connect(m_mainDialog,SIGNAL(SIG_changeDir(QString)),this,SLOT(slot_changeDir(QString)));
    connect(m_mainDialog,SIGNAL(SIG_uploadFolder(QString,QString)),this,SLOT(slot_uploadFolder(QString,QString)));
    connect(m_mainDialog,SIGNAL(SIG_shareFile(QVector<int>,QString)),this,SLOT(slot_shareFile(QVector<int>,QString )));
    connect(m_mainDialog,SIGNAL(SIG_getShareByLink(int , QString )),this,SLOT(slot_getShareByLink(int, QString)));
    connect(m_mainDialog,SIGNAL(SIG_deleteFile(QVector<int> , QString )),this,SLOT( slot_deleteFile(QVector<int> , QString )));
    connect(m_mainDialog,SIGNAL(SIG_setUploadPause(int,int )),this,SLOT( slot_setUploadPause(int , int )));
    connect(m_mainDialog,SIGNAL(SIG_setDownloadPause(int,int )),this,SLOT( slot_setDownloadPause(int , int )));

    //    m_mainDialog->show();

#ifdef USER_SERVER
    //测试 对服务器发送数据
    char strBuf[100]="hello server";
    int len=strlen("hello serv er")+1;
    qDebug()<<"客户端发送给服务器";
    m_tcpClient->SendData(0,strBuf,len);//客户端一定是发给服务器的，套接字参数随意
#endif
    //sizeof+数组名 整个数组大小
    //strlen()+1
//    STRU_LOGIN_RQ rq;
//    m_tcpClient->SendData(0,(char*)&rq,sizeof(rq));

}

//配置文件  什么位置？ exe同级目录   思路：根据目录看文件是否存在 如存在直接加载 如不存在 则创建并且写入默认值
// .ini
//格式
//[组名]
//key=value

//例如
//[net]
//ip=192.168.5.198
//port=8004

#include<QCoreApplication>

//配置文件使用的类
#include<QSettings>
void CKernel::loadIniFile()
{
    //默认值
    m_ip="192.168.5.198";
    m_port="8004";
    //获取exe目录 c:/build
    QString path=QCoreApplication::applicationDirPath()+"/config.ini";
    //看文件是否存在 如存在直接加载 如不存在 则创建并且写入默认值
    qDebug()<<path;
    QFileInfo info(path);
    if(info.exists()){
        //存在
        QSettings setting(path,QSettings::IniFormat);
        //打开组
        setting.beginGroup("net");
        QVariant strIP=setting.value("ip","");
        QVariant strPort=setting.value("port","");
        if(!strIP.toString().isEmpty())m_ip=strIP.toString();
        if(!strPort.toString().isEmpty())m_port=strPort.toString();

        //关闭组
        setting.endGroup();
    }else{
        //不存在
        QSettings setting(path,QSettings::IniFormat);//没有会创建
        //打开组
        setting.beginGroup("net");
        //设置 key value
        setting.setValue("ip",m_ip);
        setting.setValue("port",m_port);
        //关闭组
        setting.endGroup();
    }
    qDebug()<<"ip:"<<m_ip<<" port:"<<m_port;

}

void CKernel::slot_destroy(){
    m_quit=true;
    qDebug()<<__func__;
    m_tcpClient->CloseNet();
    delete m_tcpClient;
    delete m_mainDialog;
    delete m_loginDialog;
}

void CKernel::slot_registerCommit(QString tel, QString password, QString name)
{
    STRU_REGISTER_RQ rq;
    strcpy(rq.tel,tel.toStdString().c_str());
//    strcpy(rq.password,password.toStdString().c_str());
    strcpy(rq.password,getMD5(password).c_str());
    //"aa_1234"  md5:  ab1dc0898283934b849b887a90c412df
    //名字需要兼容中文
    strcpy(rq.name,name.toStdString().c_str());
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_loginCommit(QString tel, QString password)
{
    STRU_LOGIN_RQ rq;
    strcpy(rq.tel,tel.toStdString().c_str());
//    strcpy(rq.password,password.toStdString().c_str());
    strcpy(rq.password,getMD5(password).c_str());
    SendData((char*)&rq,sizeof(rq));
}

#include<QFileInfo>
#include<QDateTime>

//上传文件槽函数
void CKernel::slot_uploadFile(QString path, QString dir)
{
    //qt带的文件类 可以通过此类很方便获取很多信息
    QFileInfo qFileInfo(path);

    //文件信息存储
    FileInfo info;
    info.absolutePath=path;
    info.dir=dir;
    info.md5=QString::fromStdString(getFileMD5(path));
    info.name=qFileInfo.fileName();
    info.size=qFileInfo.size();
    info.time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    info.type="file";

    char buf[1000]="";
    Utf8ToGB2312(buf,1000,path);
    info.pFile=fopen(buf,"rb");
    if(!info.pFile){
        qDebug()<<"file open fail";
        return;
    }
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();

    //bug修复 反复检测时间戳是否存在
    while(m_mapTimestampToFileInfo.count(timestamp)>0){
        timestamp++;
    }

    info.timestamp=timestamp;
    qDebug()<<"timestamp:"<<timestamp;
    //存储到map里面 key 时间戳 value 文件信息
    m_mapTimestampToFileInfo[timestamp]=info;//这里涉及到拷贝构造 （使用浅拷贝即可使用引用）

    //发上传文件请求
    STRU_UPLOAD_FILE_RQ rq;
    //兼容中文
    std::string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());

    std::string strName=info.name.toStdString();
    strcpy( rq.fileName,strName.c_str());

    strcpy(  rq.fileType,"file");
    strcpy( rq.md5,info.md5.toStdString().c_str());

    rq.size=info.size;
    strcpy( rq.time,info.time.toStdString().c_str());

    rq.timestamp=timestamp;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));

}

void CKernel::slot_uploadFolder(QString path, QString dir)
{
    qDebug()<<__func__;
    //当前文件夹的处理 addFolder c:/项目  下面 是有 /0314/ /0527/ 1.txt 上传到/05/
    QFileInfo info(path);
    QDir dr(path);
    qDebug()<<"folder:"<<info.fileName()<<" dir:"<<dir;
    slot_addFolder(info.fileName(),dir);
    //获取文件夹的下一层 所有文件的路径
    QFileInfoList lst=dr.entryInfoList();
    //遍历所有文件
    QString newDir=dir+info.fileName()+"/";
    for(int i=0;i<lst.size();i++){
        QFileInfo file=lst.at(i);
        //如果是 . 继续
        if(file.fileName()==".")continue;
        //如果是 .. 继续
        if(file.fileName()=="..")continue;
        //如果是文件 uploadFile ->路径 文件信息的绝对路径 传到什么目录  /05/项目
        if(file.isFile()){
            slot_uploadFile(file.absoluteFilePath(),newDir);
            qDebug()<<"file:"<<file.absoluteFilePath()<<"dir:"<<newDir<<endl;
        }
        //如果是文件夹  slot_uploadFolder递归
        if(file.isDir()){
            slot_uploadFolder(file.absoluteFilePath(),newDir);
        }
    }
}

void CKernel::slot_getCurDirFileList()
{
    qDebug()<<__func__;
    //向服务器发送获取当前目录文件列表
    STRU_GET_FILE_INFO_RQ rq;
    rq.userid=m_id;
    //兼容中文
    std::string strDir=m_curDir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_downloadFile(int fileid, QString dir)
{
    qDebug()<<__func__;
    //文件下载请求
    STRU_DOWNLOAD_FILE_RQ rq;
    //兼容中文
    std::string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    rq.fileid=fileid;
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();

    //bug修复 反复检测时间戳是否存在
    while(m_mapTimestampToFileInfo.count(timestamp)>0){
        timestamp++;
    }
    rq.timestamp=timestamp;

    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_downloadFolder(int fileid, QString dir)
{
    qDebug()<<__func__;
    STRU_DOWNLOAD_FOLDER_RQ rq;
    string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    rq.fileid=fileid;
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    if(m_mapTimestampToFileInfo.count(timestamp)>0){
        timestamp++;
    }
    rq.timestamp=timestamp;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));

}

void CKernel::slot_addFolder(QString name, QString dir)
{
    //发请求包
    STRU_ADD_FOLDER_RQ rq;
    string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    string strName=name.toStdString();
    strcpy(rq.fileName,strName.c_str());

    string strTime=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString();
    strcpy(rq.time,strTime.c_str());
    rq.timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    rq.userid=m_id;

    SendData((char*)&rq,sizeof(rq));

}

void CKernel::slot_changeDir(QString dir)
{
    //更新当前目录
    m_curDir=dir;
    //刷新列表
    m_mainDialog->slot_deleteAllFileInfo();
    slot_getCurDirFileList();
}

void CKernel::slot_shareFile(QVector<int> fileidArray, QString dir)
{
    qDebug()<<__func__<<endl;
    //打包
    int packlen=sizeof(STRUCT_SHARED_FILE_RQ)+fileidArray.size()*sizeof(int);
    STRUCT_SHARED_FILE_RQ * rq=(STRUCT_SHARED_FILE_RQ *)malloc(packlen);
    rq->init();
    rq->itemCount=fileidArray.size();
    for(int i=0;i<fileidArray.size();i++){
            rq->fileidArray[i]=fileidArray[i];
    }
    rq->userid=m_id;
    std::string strDir=dir.toStdString();
    strcpy(rq->dir,strDir.c_str());
    QString time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    strcpy(rq->shareTime,time.toStdString().c_str());
    SendData((char*)rq,packlen);
    free(rq);
}

void CKernel::slot_dealClientData(unsigned int lSendIP, char *buf, int nlen)
{
    qDebug()<<__func__;
//    QString str=QString("来自服务端：%1").arg(QString::fromStdString(buf));//标准字符串buf的内容转换为Qt字符串
//    QMessageBox::about(NULL,"提示",str);//about 阻塞的 模态窗口
    int type=*(int*)buf;
 qDebug() << "收到包类型 type = " << type;  // 加打印调试
 int idx = type - _DEF_PACK_BASE;
 qDebug() << "idx = " << idx;
    //最原始方法，如果非常多的处理函数就会很长的代码
//    switch(type){
//        case _DEF_PACK_LOGIN_RS:
//            slot_dealLoginRs(lSendIP,buf,nlen);
//        break;
//    }

    //通过协议头 拿到处理函数 并执行
    if(type>=_DEF_PACK_BASE&&type<_DEF_PACK_BASE+_DEF_PACK_COUNT){
        PFUN pf= NetMap(type);
        if(pf){
            (this->*pf)(lSendIP,buf,nlen);
        }
    }
    //回收空间
    delete[] buf;
}

void CKernel::slot_dealRegisterRs(uint lSendIP, char *buf, int len)
{
    STRU_REGISTER_RS* rs=(STRU_REGISTER_RS*)buf;
    switch (rs->result) {
        case tel_is_exist:
            QMessageBox::about(m_loginDialog,"提示","手机号已存在，注册失败");
            break;
        case name_is_exist:
            QMessageBox::about(m_loginDialog,"提示","昵称已存在，注册失败");
            break;
        case register_success:
            QMessageBox::about(m_loginDialog,"提示","注册成功");
            break;
    }
}

void CKernel::slot_dealLoginRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__;
    STRU_LOGIN_RS* rs=(STRU_LOGIN_RS*)buf;
    switch (rs->result) {
        case tel_not_exist:
        QMessageBox::about(m_loginDialog,"提示","手机号不存在，登录失败");
            break;
        case password_error:
        QMessageBox::about(m_loginDialog,"提示","密码错误，登录失败");
            break;
        case login_success:
            //前台
            m_loginDialog->hide();
            m_mainDialog->show();
            //后台
            m_name=rs->name;
            m_id=rs->userid;
            m_mainDialog->slot_setInfo(m_name);

            //获取 根目录下面文件列表
            m_curDir="/";
            slot_getCurDirFileList();

            //刷新 发获取请求
            slot_getMyShare();
            InitDatabase(m_id);
            break;
    }
}

//上传文件回复的处理
void CKernel::slot_dealUploadFileRs(uint lSendIP, char *buf, int len)
{
    //拆包
    STRU_UPLOAD_FILE_RS* rs=(STRU_UPLOAD_FILE_RS*)buf;
    rs->fileid;
    rs->result;
    rs->timestamp;

    //首先看结果是否为真
    if(!rs->result){
        qDebug()<<"上传失败";
        return;
    }

    //为真
        //获取文件信息
        if(m_mapTimestampToFileInfo.count(rs->timestamp)==0){
            qDebug()<<"not found";
            return ;
        }
        FileInfo& info=m_mapTimestampToFileInfo[rs->timestamp];
        //更新fileid（文件头存到数据库后有一个文件id返回给客户端在此保存）
        info.fileid=rs->fileid;
        //插入上传信息到“上传中”的控件里 todo
        slot_writeUploadTask(&info);
        m_mainDialog->slot_insertUploadFile(info);
        //发送文件块（内容）请求
        STRU_FILE_CONTENT_RQ rq;
        rq.fileid=rs->fileid;
        rq.timestamp=rs->timestamp;
        rq.userid=m_id;
        rq.len=fread(rq.content,1,_DEF_BUFFER,info.pFile);

        SendData((char*)&rq,sizeof(rq));


}

void CKernel::slot_dealFileContentRs(uint lSendIP, char *buf, int len)
{
    //拆包
    STRU_FILE_CONTENT_RS *rs=(STRU_FILE_CONTENT_RS*)buf;

    //找文件信息结构体
    if(m_mapTimestampToFileInfo.count(rs->timestamp)==0){
        qDebug()<<"file not found";
        return;
    }
    FileInfo &info=m_mapTimestampToFileInfo[rs->timestamp];

    //判断是否暂停
    while(info.isPause){
        //sleep();
        QThread::msleep(100);
        //为了避免阻塞窗口线程，影响事件循环，加入下面的处理 将信号取出并执行
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
        //避免程序退出一直卡在这里
        if(m_quit)return;
    }


    //结果
    if(!rs->result){
        //假 跳回
        fseek(info.pFile,-1*(rs->len),SEEK_CUR);
    }else{    //真 pos+len
        info.pos+=rs->len;
        //更新上传进度 todo
        //方案1：写信号槽 考虑多线程
        //方案2：直接调用 一定是当前函数在主线程
        Q_EMIT SIG_updateUploadFileProgress(info.timestamp,info.pos);


        //判断是否结束
        if(info.pos>=info.size){
            qDebug()<<"2"<<endl;
            slot_deleteUploadTask(&info);
            //是 关闭文件 回收 返回
            fclose(info.pFile);
            m_mapTimestampToFileInfo.erase(rs->timestamp);

            m_mainDialog->slot_deleteAllFileInfo();
            slot_getCurDirFileList();
            return ;
        }
    }
    //发文件块
    STRU_FILE_CONTENT_RQ rq;
    rq.fileid=rs->fileid;
    rq.timestamp=rs->timestamp;
    rq.userid=m_id;
    rq.len=fread(rq.content,1,_DEF_BUFFER,info.pFile);

    SendData((char*)&rq,sizeof(rq));

}

void CKernel::slot_dealGetFileInfoRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__;
    //拆包
    STRU_GET_FILE_INFO_RS* rs=(STRU_GET_FILE_INFO_RS*)buf;
    if(m_curDir!=QString::fromStdString(rs->dir))return;

    //先删除原来的
    m_mainDialog->slot_deleteAllFileInfo();

    //获取元素
    int count=rs->count;
    qDebug()<<"用户文件数量：count="<<count<<endl;
    for(int i=0;i<count;i++){
        FileInfo info;
        info.fileid=rs->fileInfo[i].fileid;
        info.type=QString::fromStdString(rs->fileInfo[i].fileType);
        info.name=QString::fromStdString(rs->fileInfo[i].name);
        info.size=rs->fileInfo[i].size;
        info.time=rs->fileInfo[i].time;
        //插入控件
        m_mainDialog->slot_inserFileInfo(info);
    }
}

void CKernel::slot_dealFileHeaderRq(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__;
    //拆包
    STRU_FILE_HEADER_RQ * rq=(STRU_FILE_HEADER_RQ*)buf;

    //创建文件信息结构体 赋值
    FileInfo  info;
    //默认路径 sysPath(不含最后的'/')+dir+name
    //dir 可能有很多层 需要循环创建目录 todo
    info.name=QString::fromStdString(rq->fileName);
    info.dir=QString::fromStdString(rq->dir);//info.dir是QString类型 不写的话也行但是不可控 最好写可控的东西
    info.absolutePath=QString("%1%2%3").arg(m_sysPath).arg(info.dir).arg(info.name); //默认路径 文件要下载到哪里

    info.fileid=rq->fileid;
    info.md5=QString::fromStdString(rq->md5);

    info.size=rq->size;
    info.time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    info.timestamp=rq->timestamp;
    info.type="file";
    //打开文件
    char pathbuf[1000]="";
    Utf8ToGB2312(pathbuf,1000,info.absolutePath);
    qDebug()<<"pathbuf:"<<pathbuf<<endl;
    info.pFile=fopen(pathbuf,"wb");
    if(!info.pFile){
       qDebug()<<"file open fail";
       return ;
    }
    //todo 保存下载信息到控件
    slot_writeDownloadTask(info);
    m_mainDialog->slot_insertDownloadFile(info);
    //保存map里面
    m_mapTimestampToFileInfo[rq->timestamp]=info;
    //写回复
    STRU_FILE_HEADER_RS rs;
    rs.fileid=rq->fileid;
    rs.result=1;
    rs.timestamp=rq->timestamp;
    rs.userid=m_id;

    SendData((char*)&rs,sizeof(rs));
}

void CKernel::slot_dealFileContentRq(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__;
    //拆包
    STRU_FILE_CONTENT_RQ * rq=(STRU_FILE_CONTENT_RQ*)buf;

    //拿到文件信息结构
    if(m_mapTimestampToFileInfo.count(rq->timestamp)==0){
        return;
    }

    FileInfo& info=m_mapTimestampToFileInfo[rq->timestamp];

    //判断是否暂停
    while(info.isPause){
        //sleep();
        QThread::msleep(100);
        //为了避免阻塞窗口线程，影响事件循环，加入下面的处理 将信号取出并执行
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
        //避免程序退出一直卡在这里
        if(m_quit)return;
    }

    STRU_FILE_CONTENT_RS rs;
    //写文件
    len=fwrite(rq->content,1,rq->len,info.pFile);
    if(len!=rq->len){
         //不成功 跳回去
         rs.result=0;
         fseek(info.pFile,-1*len,SEEK_CUR);
     }else{
        //成功 pos+=len
        rs.result=1;
        info.pos+=len;
        //更新进度 todo
        Q_EMIT SIG_updateDownloadFileProgress(rq->timestamp,info.pos);
        //要看 有没有到文件末尾 是否结束
        if(info.pos>=info.size){
            slot_deleteDownloadTask(&info);
            //结束 关闭文件 回收
            fclose(info.pFile) ;
            m_mapTimestampToFileInfo.erase(rq->timestamp);
        }
     }
    //写回复
    rs.fileid=info.fileid;
    rs.len=rq->len;
    rs.timestamp=rq->timestamp;
    rs.userid=m_id;

    //发送
    SendData((char*)&rs,sizeof(rs));

}

void CKernel::slot_dealAddFolderRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__;
    //拆包
    STRU_ADD_FOLDER_RS *rs=(STRU_ADD_FOLDER_RS*)buf;
    rs->result;
    rs->timestamp;
    rs->userid;
    //判断是否成功
    if(rs->result!=1){
        return;
    }
    //删除原来的
    m_mainDialog->slot_deleteAllFileInfo();
    //更新文件列表
    slot_getCurDirFileList();

}

void CKernel::slot_dealQuickUploadRs(uint lSendIP, char *buf, int len)
{
    //拆包
    STRUCT_QUICK_UPLOAD_RS* rs=(STRUCT_QUICK_UPLOAD_RS*)buf;
    //获取文件信息
    if(m_mapTimestampToFileInfo.count(rs->timestamp)==0)return;
    FileInfo& info=m_mapTimestampToFileInfo[rs->timestamp];

    //关闭文件
    if(info.pFile){
    fclose(info.pFile);
    }
    //写入上传已完成信息
    m_mainDialog->slot_insertUploadComplete(info);
    //发送刷新文件列表
    if(info.dir==m_curDir){
        m_mainDialog->slot_deleteAllFileInfo();
        slot_getCurDirFileList();
    }
    //删除节点
    m_mapTimestampToFileInfo.erase(rs->timestamp);
}

void CKernel::slot_dealShareFileRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__<<endl;
    //拆包
    STRUCT_SHARE_FILE_RS* rs=(STRUCT_SHARE_FILE_RS*)buf;
    //判断是否成功
    if(rs->result!=1)return ;
    slot_getMyShare();

}

void CKernel::slot_dealMyShareRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__<<endl;
    //拆包
    STRU_MY_SHARE_RS * rs=(STRU_MY_SHARE_RS*)buf;
    int count=rs->iteCount;
    qDebug()<<"用户共享的文件数量 count:"<<count<<endl;
    //遍历 分享文件的信息 添加到控件上面
    m_mainDialog->slot_deleteAllShareInfo();
    for(int i=0;i<count;++i){
        m_mainDialog->slot_insertShareFileInfo(rs->items[i].name,rs->items[i].size,rs->items[i].time,rs->items[i].shareLink);
    }
}

void CKernel::slot_dealGetShareRs(uint lSendIP, char *buf, int len)
{
    //拆包
    STRU_GET_SHARE_RS* rs=(STRU_GET_SHARE_RS*)buf;
    //根据结果
    if(rs->result==0){
        QMessageBox::about(this->m_mainDialog,"提示","获取分享失败");
    }else{
      //正确刷新
        if(QString::fromStdString(rs->dir)==m_curDir){
            slot_getCurDirFileList();
        }
    }
}

void CKernel::slot_dealFolderHeadRq(uint lSendIP, char *buf, int len)
{
    //拆包
    STRU_FOLDER_HEADER_RQ *rq=(STRU_FOLDER_HEADER_RQ *)buf;

    //创建目录
    //dir可能有很多层 需要循环创建目录
    QString tmpDir=QString::fromStdString(rq->dir);// /NetDisk/111  1.txt
    QStringList dirList=tmpDir.split("/");  //分割函数 NetDisk 111
    QString pathsum=m_sysPath;
    for(QString &node:dirList){
        if(!node.isEmpty()){
            pathsum+="/";
            pathsum+=node;

            QDir dir;
            if(!dir.exists(pathsum)){
                dir.mkdir(pathsum);
            }
        }
    }
    pathsum+="/";
    pathsum+=QString::fromStdString(rq->fileName);

    QDir dir;
    if(!dir.exists(pathsum)){
        dir.mkdir(pathsum);
    }
}

void CKernel::slot_dealDeleteFileRs(uint lSendIP, char *buf, int len)
{
    qDebug()<<__func__<<endl;
    //拆包
    STRU_DELETE_FILE_RS * rs=(STRU_DELETE_FILE_RS *)buf;

    //看是否刷新
    if(rs->result==1){
        if(QString::fromStdString(rs->dir)==m_curDir){
            m_mainDialog->slot_deleteAllFileInfo();
            slot_getCurDirFileList();
        }
    }

}

void CKernel::slot_dealContinueUploadRs(uint lSendIP, char *buf, int len)
{
    //拆包
    STRU_CONTINUE_UPLOAD_RS* rs=(STRU_CONTINUE_UPLOAD_RS* )buf;

    //通过map拿到文件信息
    if(m_mapTimestampToFileInfo.count(rs->timestamp)==0)return ;
    FileInfo &info=m_mapTimestampToFileInfo[rs->timestamp];
    //文件位置跳转 pos更新 界面显示 百分比更新
    info.pos=rs->pos;
    fseek(info.pFile,info.pos,SEEK_SET);//从起始位置跳 pos那么多
    m_mainDialog->slot_updateUploadFileProgress(info.timestamp,info.pos);
    //发送文件块请求
    STRU_FILE_CONTENT_RQ rq;
    //读文件
    int nlen=fread(rq.content,1,_DEF_BUFFER,info.pFile);
    rq.len=nlen;
    rq.fileid=info.fileid;
    rq.timestamp=info.timestamp;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));
}
void CKernel::slot_getMyShare(){
    qDebug()<<__func__<<endl;
    STRU_MY_SHARE_RQ rq;
    rq.userid=m_id;
    //刷新 发获取请求
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_getShareByLink(int code, QString dir)
{
    //发请求
    qDebug()<<__func__<<endl;
    STRU_GET_SHARE_RQ rq;
    string tmpDir=dir.toStdString();
    strcpy(rq.dir,tmpDir.c_str());
    rq.shareLink=code;
    string time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss").toStdString();
    strcpy(rq.time,time.c_str());
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_deleteFile(QVector<int> fileidArray, QString dir)
{
    //发请求
    int packlen=sizeof(STRU_DELETE_FILE_RQ)+fileidArray.size()*sizeof(int);
    STRU_DELETE_FILE_RQ* rq=(STRU_DELETE_FILE_RQ* )malloc(packlen);
    rq->init();
    string strDir=dir.toStdString();
    strcpy(rq->dir,strDir.c_str());
    rq->fileCount=fileidArray.size();
    rq->userid=m_id;
    for(int i=0;i<rq->fileCount;++i){
        rq->fileidArray[i]=fileidArray[i];
    }
    SendData((char*)rq,packlen);
    free(rq);
}

void CKernel::slot_setUploadPause(int timestamp, int isPause)
{
    qDebug()<<__func__<<endl;
    //isPause1从正在上传变为暂停 isPauseo从暂停变为开始继续上传
    //需要找到文件信息结构体
    //map里面 有 程序未退的情况 直接置位
    //map中没有证明程序退出的 断点续传 需要走协议
    if(m_mapTimestampToFileInfo.count(timestamp)>0){
        m_mapTimestampToFileInfo[timestamp].isPause=isPause;
    }else{
        //断点续传 todo
        //创建fileinfo 然后打开文件 放到map里面
        FileInfo info;
        bool res=m_mainDialog->slot_getUploadFileInfoByTimestamp(timestamp,info);
        if(!res){//没找到
            return ;
        }

         //转化 路径转为ASCII
         char pathbuf[1000]="";
         Utf8ToGB2312(pathbuf,1000,info.absolutePath);
         qDebug()<<"pathbuf"<<pathbuf<<endl;
         //打开文件 二进制追加 不能是w 因为会清空
         info.pFile=fopen(pathbuf,"rb");
         if(!info.pFile){
             qDebug()<<"打开失败"<<info.absolutePath;return ;
         }

         info.isPause=0;//避免开始 就停在循环那里
         m_mapTimestampToFileInfo[timestamp]=info;

         STRU_CONTINUE_UPLOAD_RQ rq;
         string dirstr=info.dir.toStdString();
         strcpy(rq.dir,dirstr.c_str());
         rq.fileid=info.fileid;
         rq.timestamp=info.timestamp;
         rq.userid=m_id;
         SendData((char*)&rq,sizeof(rq));
    }
}

void CKernel::slot_setDownloadPause(int timestamp, int isPause)
{
    qDebug()<<__func__<<endl;
    //isPause1从正在下载变为暂停 isPauseo从暂停变为开始继续下载
    //需要找到文件信息结构体
    //map里面 有 程序未退的情况 直接置位
    //map中没有证明程序退出的 断点续传 需要走协议
    if(m_mapTimestampToFileInfo.count(timestamp)>0){
        m_mapTimestampToFileInfo[timestamp].isPause=isPause;
    }else{
        //断点续传 todo
        //下载的信息 存到数据库，重新登录加载，然后点击开始（继续）
        if(isPause==0){
            //断点续传
            //1、创建信息结构体，装到map里面
            //信息 在哪里？可以直接从控件里面取出
            FileInfo info;
           bool res=m_mainDialog->slot_getDownloadFileInfoByTimestamp(timestamp,info);
            if(!res){//没找到
                return ;
            }
            //转化 路径转成ASCII 因为open接受的是ASCII QString 是UTF-8
            char pathbuf[1000]="";
            Utf8ToGB2312(pathbuf,1000,info.absolutePath);
            //打开文件 二进制追加 不能是w 因为会清空
            info.pFile=fopen(pathbuf,"ab");
            if(!info.pFile){
                qDebug()<<"打开失败"<<info.absolutePath;return ;
            }
            info.isPause=0;//避免开始 就停在循环那里
            m_mapTimestampToFileInfo[timestamp]=info;

            //2.发协议 告诉服务器 文件下载到哪里了，然后服务器跳转到哪里，从哪开始继续读，然后文件块发送
            //服务器接收 有两种可能 1.文件信息还在（客户端出现异常很快就好了，没有超过预定删除客户端所有信息的时间） 2.不在（超过了时间）
            STRU_CONTINUE_DOWNLOAD_RQ rq;
            rq.fileid=info.fileid;
            string dirstr=info.dir.toStdString();
            strcpy(rq.dir,dirstr.c_str());
            rq.pos=info.pos;
            rq.timestamp=info.timestamp;
            rq.userid=m_id;
            SendData((char*)&rq,sizeof(rq));
        }
    }
}

#ifdef USER_SERVER
void CKernel::slot_dealServerData(unsigned int lSendIP, char *buf, int nlen)
{
    qDebug()<<__func__;
    QString str=QString("来自客户端：%1").arg(QString::fromStdString(buf));//标准字符串buf的内容转换为Qt字符串
    QMessageBox::about(NULL,"提示",str); //about 阻塞的 模态窗口 NULL没有父窗口，会以独立窗口显示
    m_tcpServer->SendData(lSendIP,buf,nlen);
    //回收空间
    delete [] buf;
}
#endif

//客户端数据库 id.db
//表 正在上传
//create table t_upload(
//        timestamp int,
//        f_id int,
//        f_name varchar(260),
//        f_dir varchar(260),
//        f_time varchar(6o),
//        f_size int,
//        f_md5 varchar(60),
//        f_type varchar(60),
//        f_absolutePath(260)
//);


//表 正在下载
//create table t_download(
//        timestamp int,
//        f_id int,
//        f_name varchar(260),
//        f_dir varchar(260),
//        f_time varchar(6o),
//        f_size int,
//        f_md5 varchar(60),
//        f_type varchar(60),
//        f_absolutePath(260)
//);
#include<QDir>
void CKernel::InitDatabase(int id)
{
     m_sql=new CSqlite;
    //首先 找到exe 去同级目录 /database/id.db
    QString path= QCoreApplication::applicationDirPath()+"/database/";
    QDir dir(path);
    if(!dir.exists()){
        dir.mkdir(path);
    }
    path=path+QString("%1.db").arg(id);
    //查看有没有这个文件
    QFileInfo info(path);
    if(info.exists()){
        //有直接加载
        //连接
        m_sql->ConnectSql(path);
            //测试 读取数据
//        QString sqlbuf="select count(*) from t_upload;";
//        QStringList lst;
//        m_sql->SelectSql(sqlbuf,1,lst);
//        qDebug()<<"upload item count:"<<lst.front();
//        lst.clear();

//        sqlbuf="select count(*) from t_download;";
//        m_sql->SelectSql(sqlbuf,1,lst);
//        qDebug()<<"download item count:"<<lst.front();
//        lst.clear();
    QList<FileInfo> uploadTaskList;
    QList<FileInfo> downloadTaskList;
    slot_getUploadTask(uploadTaskList);
    slot_getDownloadTask(downloadTaskList);
    for(FileInfo&info:uploadTaskList){
        //如果这个文件没有了不能继续
        QFileInfo fi(info.absolutePath);
        if(!fi.exists())continue;

        //修改任务的初始状态
        info.isPause=1;
        //进行到多少 可以知道
//        info.pos=fi.size();

        m_mainDialog->slot_insertUploadFile(info);
        //上传续传  控件上看不到进行到多少
        //todo 获取当前位置

        //同步到控件的位置
//        m_mainDialog->slot_updateUploadFileProgress(info.timestamp,fi.size());

    }
    //加载下载任务
    for(FileInfo&info:downloadTaskList){
        //如果这个文件没有了不能继续
        QFileInfo fi(info.absolutePath);
        if(!fi.exists())continue;

        //修改任务的初始状态
        info.isPause=1;
        //进行到多少 可以知道
        info.pos=fi.size();

        m_mainDialog->slot_insertDownloadFile(info);

        //控件同步位置
        m_mainDialog->slot_updateDownloadFileProgress(info.timestamp,fi.size());
    }

    }else{
        //没有 创建表
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly))return ;
        file.close();
        //连接
        m_sql->ConnectSql(path);
        //创建表
        QString sqlbuf="create table t_upload(timestamp int,f_id int, f_name varchar(260),f_dir varchar(260), f_time varchar(60),f_size int,f_md5 varchar(60),f_type varchar(60),f_absolutePath varchar(260));";
        m_sql->UpdateSql(sqlbuf);

        sqlbuf="create table t_download(timestamp int,f_id int, f_name varchar(260),f_dir varchar(260), f_time varchar(60),f_size int,f_md5 varchar(60),f_type varchar(60),f_absolutePath varchar(260));";
        m_sql->UpdateSql(sqlbuf);

    }
}

void CKernel::slot_writeUploadTask(FileInfo *info)
{
    QString sqlbuf=QString("insert into t_upload values(%1,%2,'%3','%4','%5',%6,'%7','%8','%9');").arg(info->timestamp).arg(info->fileid).arg(info->name).arg(info->dir).arg(info->time).arg(info->size).arg(info->md5).arg(info->type).arg(info->absolutePath);
    m_sql->UpdateSql(sqlbuf);
}

void CKernel::slot_writeDownloadTask(FileInfo &info)
{
    QString sqlbuf=QString("insert into t_download values(%1,%2,'%3','%4','%5',%6,'%7','%8','%9');").arg(info.timestamp).arg(info.fileid).arg(info.name).arg(info.dir).arg(info.time).arg(info.size).arg(info.md5).arg(info.type).arg(info.absolutePath);
    m_sql->UpdateSql(sqlbuf);
}

void CKernel::slot_deleteUploadTask(FileInfo *info)
{
    QString sqlbuf=QString("delete from t_upload where timestamp=%1;").arg(info->timestamp);
    m_sql->UpdateSql(sqlbuf);
}

void CKernel::slot_deleteDownloadTask(FileInfo *info)
{
    QString sqlbuf=QString("delete from t_download where timestamp=%1 and f_absolutePath='%2';").arg(info->timestamp).arg(info->absolutePath);
    m_sql->UpdateSql(sqlbuf);
}

void CKernel::slot_getUploadTask(QList<FileInfo> &infoList)
{
    //获取所有任务
    QString sqlbuf="select * from t_upload;";
    QStringList lst;
    m_sql->SelectSql(sqlbuf,9,lst);
    while(lst.size()!=0){
        FileInfo info;
        info.timestamp=QString(lst.front()).toInt();lst.pop_front();
        info.fileid=QString(lst.front()).toInt();lst.pop_front();
        info.name=lst.front();lst.pop_front();
        info.dir=lst.front();lst.pop_front();
        info.time=lst.front();lst.pop_front();
        info.size=QString(lst.front()).toInt();lst.pop_front();
        info.md5=lst.front();lst.pop_front();
        info.type=lst.front();lst.pop_front();
        info.absolutePath=lst.front();lst.pop_front();
        infoList.push_back(info);
    }
}

void CKernel::slot_getDownloadTask(QList<FileInfo> &infoList)
{
    //获取所有任务
    QString sqlbuf="select * from t_download;";
    QStringList lst;
    m_sql->SelectSql(sqlbuf,9,lst);
    while(lst.size()!=0){
        FileInfo info;
        info.timestamp=QString(lst.front()).toInt();lst.pop_front();
        info.fileid=QString(lst.front()).toInt();lst.pop_front();
        info.name=lst.front();lst.pop_front();
        info.dir=lst.front();lst.pop_front();
        info.time=lst.front();lst.pop_front();
        info.size=QString(lst.front()).toInt();lst.pop_front();
        info.md5=lst.front();lst.pop_front();
        info.type=lst.front();lst.pop_front();
        info.absolutePath=lst.front();lst.pop_front();
        infoList.push_back(info);
    }
}
