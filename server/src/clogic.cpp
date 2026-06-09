#include "clogic.h"

void CLogic::setNetPackMap()
{
    NetPackMap(_DEF_PACK_REGISTER_RQ)    = &CLogic::RegisterRq;
    NetPackMap(_DEF_PACK_LOGIN_RQ)       = &CLogic::LoginRq;
    NetPackMap(_DEF_PACK_UPLOAD_FILE_RQ) = &CLogic::UploadFileRq;
    NetPackMap(_DEF_PACK_FILE_CONTENT_RQ) = &CLogic::FileContentRq;
    NetPackMap(_DEF_PACK_GET_FILE_INFO_RQ) = &CLogic::GetFileInfoRq;
    NetPackMap(_DEF_PACK_DOWNLOAD_FILE_RQ) = &CLogic::DownloadFileRq;
    NetPackMap(_DEF_PACK_DOWNLOAD_FOLDER_RQ) = &CLogic::DownloadFolderRq;
    NetPackMap(_DEF_PACK_FILE_HEADER_RS) = &CLogic::FileHeaderRs;
    NetPackMap(_DEF_PACK_FILE_CONTENT_RS) = &CLogic::FileContentRs;
    NetPackMap(_DEF_PACK_ADD_FOLDER_RQ) = &CLogic::AddFolderRq;
    NetPackMap(_DEF_PACK_SHARE_FILE_RQ) = &CLogic::ShareFileRq;
    NetPackMap(_DEF_PACK_MY_SHARE_RQ) = &CLogic::MyShareRq;
    NetPackMap(_DEF_PACK_GET_SHARE_RQ) = &CLogic::GetShareRq;
    NetPackMap(_DEF_PACK_DELETE_FILE_RQ) = &CLogic::DeleteFileRq;
    NetPackMap(_DEF_PACK_CONTINUE_DOWNLOAD_RQ) = &CLogic::ContinueDownloadRq;
    NetPackMap(_DEF_PACK_CONTINUE_UPLOAD_RQ) = &CLogic::ContinueUploadRq;
}

#define _DEF_COUT_FUNC_    cout << "clientfd:"<< clientfd << __func__ << endl;

#define DEF_PATH  "/home/colin/Netdisk/NetDisk/"
//注册
void CLogic::RegisterRq(sock_fd clientfd,char* szbuf,int nlen)
{
    //cout << "clientfd:"<< clientfd << __func__ << endl;
    _DEF_COUT_FUNC_
    STRU_REGISTER_RQ *rq=(STRU_REGISTER_RQ*)szbuf;
    STRU_REGISTER_RS rs;
    //根据tel查看手机号是否存在
    char sqlstr[1000]="";
    sprintf(sqlstr,"select u_tel from t_user where u_tel='%s';",rq->tel);
    list<string> lstRes;
    bool res=m_sql->SelectMysql(sqlstr,1,lstRes);
    if(!res){
        std::cout<<"select fail:"<<sqlstr<<std::endl;
    }
    if(lstRes.size()!=0){
        //存在 返回
        rs.result=tel_is_exist;

    }else{
        //不存在 查看 查看昵称是否存在
        sprintf(sqlstr,"select u_tel from t_user where u_name='%s';",rq->name);
        list<string> lstRes;
        bool res=m_sql->SelectMysql(sqlstr,1,lstRes);
        if(!res){
            std::cout<<"select fail:"<<sqlstr<<std::endl;
        }
        if(lstRes.size()!=0){
            //存在返回
            rs.result=name_is_exist;

        }else{
            //不存在
            rs.result=register_success;
            sprintf(sqlstr,"insert into t_user(u_tel,u_password,u_name) values('%s','%s','%s');",rq->tel,rq->password,rq->name);
            m_sql->UpdataMysql(sqlstr);
            //取出该人的id
            sprintf(sqlstr,"select u_id from t_user where u_tel='%s' and u_password='%s';",rq->tel,rq->password);
            lstRes.clear();
            bool res=m_sql->SelectMysql(sqlstr,1,lstRes);
            if(!res){
                std::cout<<"select fail:"<<sqlstr<<std::endl;
            }
            if(lstRes.size()!=0){
                int id=stoi(lstRes.front());
                lstRes.pop_front();

                //网盘：创建该人对应的目录 id 命名
                //默认路径 DEF_PATH  "/home/colin/Netdisk/NetDisk/"
                char pathbuf[_MAX_PATH]="";
                sprintf(pathbuf,"%s%d/",DEF_PATH,id);

                //创建路径
                umask(0);
                mkdir(pathbuf,0777);
            }
        }

    }
    SendData(clientfd,(char*)&rs,sizeof (rs));


}

//登录
void CLogic::LoginRq(sock_fd clientfd ,char* szbuf,int nlen)
{
//    cout << "clientfd:"<< clientfd << __func__ << endl;
    _DEF_COUT_FUNC_

    //拆包
      STRU_LOGIN_RQ * rq=( STRU_LOGIN_RQ*)szbuf;
      STRU_LOGIN_RS rs;
      //根据tel查id password name
      char sqlstr[1000]="";
      sprintf(sqlstr,"select u_id,u_password,u_name from t_user where u_tel='%s';",rq->tel);
      list<string> lstRes;
      bool res=m_sql->SelectMysql(sqlstr,3,lstRes);
      if(!res){
          std::cout<<"select fail: "<<sqlstr<<std::endl;
      }
      //没有
      if(lstRes.size()==0){
        //没有
         rs.result=tel_not_exist;
      }else{
          int id=stoi(lstRes.front());
          lstRes.pop_front();
        //有
          string strPassword=lstRes.front();
          lstRes.pop_front();

          string strName=lstRes.front();
          lstRes.pop_front();
        //密码是否一致
          if(strcmp(strPassword.c_str(),rq->password)!=0){
              //不一致 返回
              rs.result=password_error;
          }else{
              //一致
              rs.result=login_success;
              rs.userid=id;
              strcpy(rs.name,strName.c_str());

              //用户身份
              //需要创建用户身份结构
              //首先看是否已经在map中，如果不在直接创建
              UserInfo * info=nullptr;
              if(!m_mapIDToUserInfo.find(id,info)){
                  info=new UserInfo;
              }else{
                   //如果存在考虑让其下线 todo
              }
              //赋值
              info->name=strName;
              info->userid=id;
              info->clientfd=clientfd;
              m_mapIDToUserInfo.insert(id,info);
          }
          SendData(clientfd,(char*)&rs,sizeof(rs));
      }



//    STRU_LOGIN_RS rs;
      //    SendData( clientfd , (char*)&rs , sizeof rs );
}

//上传文件请求
void CLogic::UploadFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_UPLOAD_FILE_RQ * rq=(STRU_UPLOAD_FILE_RQ*)szbuf;

    //查看是否秒传 todo
    {
        //判断这个文件是否已经上传过
        char sqlbuf[1000]="";
        sprintf(sqlbuf,"select f_id from t_file where f_MD5='%s' and f_state=1;",rq->md5);
        //根据 md5 state=1 查数据库  得到id
        //（如果 state 是0 怎么处理？ 客户端 给拒绝 或者挂起这个请求）
        list<string>lstRes;
        bool res=m_sql->SelectMysql(sqlbuf,1,lstRes);
        if(!res){
            cout<<"select fail:"<<sqlbuf<<endl;
            return ;
        }
        if(lstRes.size()>0){
            //已上传 查到了
            int fileid=stoi(lstRes.front());lstRes.pop_front();
            //写入用户文件关系 由于有触发器 文件引用计数自动+1
            sprintf(sqlbuf,"insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) values(%d,%d,'%s','%s','%s');",rq->userid,fileid,rq->dir,rq->fileName,rq->time);
            res=m_sql->UpdataMysql(sqlbuf);
            if(!res){
                printf("update fail:%s\n",sqlbuf);
            }
            //写回复包 客户端收到之后 就更新列表
            STRUCT_QUICK_UPLOAD_RS rs;
            rs.result=1;
            rs.userid=rq->userid;
            rs.timestamp=rq->timestamp;
            //发送
            SendData(clientfd,(char*)&rs,sizeof(rs));
        }


    }
    //不是秒传
    //文件信息创建 打开文件
    FileInfo *info=new FileInfo;
    char strpath[1000]="";
    sprintf(strpath,"%s%d%s%s",DEF_PATH,rq->userid,rq->dir,rq->md5);
    info->absolutePath=strpath;//通过这个 写数据库 打开文件 文件名字 md5
    info->dir=rq->dir;
    info->md5=rq->md5;
    info->name=rq->fileName;
    info->size=rq->size;
    info->time=rq->time;
    info->type=rq->type;
    info->timestamp=rq->timestamp;
    cout<<"shangchuan file rq->timestamp"<<rq->timestamp<<endl;
    //使用linux文件io操作
    info->fileFd=open(strpath,O_CREAT|O_WRONLY|O_TRUNC,0777);
    if(info->fileFd<0){
        std::cout<<"file open fail"<<std::endl;
        return;
    }
    //map存储文件信息
    int64_t user_time=rq->userid*getNumber()+rq->timestamp;
    m_mapTimestampToFileInfo.insert(user_time,info);

    //数据库记录
        //插入文件信息（引用计数 0 ，状态 0-> 上传结束后改为1）
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"insert into t_file(f_size,f_path,f_count,f_MD5,f_state,f_type) values(%d,'%s',0,'%s',0,'file');",rq->size,strpath,rq->md5);
    bool res=m_sql->UpdataMysql(sqlbuf);
    if(!res){
        printf("update fail:%s\n",sqlbuf);
    }
        //查文件id
        list<string>lstRes;
        sprintf(sqlbuf,"select f_id from t_file where f_path='%s' and f_MD5='%s';",strpath,rq->md5);
        res=m_sql->SelectMysql(sqlbuf,1,lstRes);
        if(!res){
            printf("select fail:%s\n",sqlbuf);
        }
        if(lstRes.size()>0){
            info->fid=stoi(lstRes.front());
        }
        lstRes.clear();

        //插入用户文件关系（由于触发器 引用计数 ->1 ）
        sprintf(sqlbuf,"insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) values(%d,%d,'%s','%s','%s');",rq->userid,info->fid,rq->dir,rq->fileName,rq->time);
        res=m_sql->UpdataMysql(sqlbuf);
        if(!res){
            printf("update fail:%s\n",sqlbuf);
        }

    //写回复包
    STRU_UPLOAD_FILE_RS rs;
    rs.fileid=info->fid;
    rs.result=1;
    rs.userid=rq->userid;
    rs.timestamp=rq->timestamp;

    SendData(clientfd,(char*)&rs,sizeof(rs));




}

void CLogic::FileContentRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_FILE_CONTENT_RQ* rq=(STRU_FILE_CONTENT_RQ*)szbuf;
    //获取文件信息
    int64_t user_time=rq->userid*getNumber()+rq->timestamp;
    FileInfo * info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info)){
        cout<<"file not found"<<endl;
        return ;
    }
    STRU_FILE_CONTENT_RS rs;
     //写入
    int len=write(info->fileFd,rq->content,rq->len);
    if(len!=rq->len){
          //失败 跳回到读取之前
        rs.result=0;
        lseek(info->fileFd,-1*len,SEEK_CUR);
         }else{
          //成功  pos更新位置
            rs.result=1;
            info->pos+=len;
                //看是否到达末尾
               if(info->pos==info->size){
                    //是 关闭文件
                    close(info->fileFd);
                    //回收map节点
                    m_mapTimestampToFileInfo.erase(user_time);
                    delete info;
                    info=nullptr;
                    //更新数据库 把文件信息的状态更新为1 表示已完成
                    char sqlbuf[1000]="";
                    sprintf(sqlbuf,"update t_file set f_state=1 where f_id=%d;",rq->fileid);
                    bool res=m_sql->UpdataMysql(sqlbuf);
                    if(!res){
                        cout<<"update fail: "<<sqlbuf<<endl;
                        }
                    }
                }

          //返回结果
        rs.fileid=rq->fileid;
        rs.len=rq->len;
        rs.timestamp=rq->timestamp;
        rs.userid=rq->userid;

        SendData(clientfd,(char*)&rs,sizeof(rs));

}

void CLogic::GetFileInfoRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_GET_FILE_INFO_RQ* rq=(STRU_GET_FILE_INFO_RQ*)szbuf;
   char sqlbuf[1000]="";
   sprintf(sqlbuf,"select f_id,f_name,f_size,f_uploadtime,f_type from user_file_info where u_id=%d and f_dir='%s' and f_state=1;",rq->userid,rq->dir);
   list<string>lstRes;
   bool res=m_sql->SelectMysql(sqlbuf,5,lstRes);
   if(!res){
        cout<<"select fail:"<<sqlbuf<<endl;
       return ;
   }
   if(lstRes.size()==0) return ;
   int count=lstRes.size()/5;
   //写回复包
    int packlen=sizeof(STRU_GET_FILE_INFO_RS)+count*sizeof(STRU_FILE_INFO);
    STRU_GET_FILE_INFO_RS *rs=(STRU_GET_FILE_INFO_RS*)malloc(packlen);
    rs->init();
    rs->count=count;
    strcpy(rs->dir,rq->dir);

    for(int i=0;i<count;++i){
        int f_id=stoi(lstRes.front());lstRes.pop_front();
        string name=lstRes.front();lstRes.pop_front();
        int f_size=stoi(lstRes.front());lstRes.pop_front();
        string time=lstRes.front();lstRes.pop_front();
        string f_type=lstRes.front();lstRes.pop_front();

        rs->fileInfo[i].fileid=f_id;
        strcpy(rs->fileInfo[i].fileType,f_type.c_str());
        strcpy(rs->fileInfo[i].name,name.c_str());
        strcpy(rs->fileInfo[i].time,time.c_str());
        rs->fileInfo[i].size=f_size;
    }
    cout<<"count="<<rs->count;
    //发送
    SendData(clientfd,(char*)rs,packlen);
    //回收
    free(rs);
}

void CLogic::DownloadFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_DOWNLOAD_FILE_RQ * rq=(STRU_DOWNLOAD_FILE_RQ*)szbuf;

    //查数据库 查什么？ 如果没有 返回
    char sqlbuf[1000]="";
    list<string>lstRes;
    sprintf(sqlbuf,"select f_name,f_path,f_MD5,f_size from user_file_info where u_id=%d and f_dir='%s' and f_id=%d;",rq->userid,rq->dir,rq->fileid);
    bool res=m_sql->SelectMysql(sqlbuf,4,lstRes);
    if(!res){
        cout<<"select fail:"<<sqlbuf<<endl;
        return;
    }
    //如果没有返回
    if(lstRes.size()==0)return ;

    string strName=lstRes.front(); lstRes.pop_front();
    string strPath=lstRes.front();lstRes.pop_front();
    string strMD5=lstRes.front();lstRes.pop_front();
    int size=stoi(lstRes.front());lstRes.pop_front();


    //有 写文件信息
    FileInfo *info=new FileInfo();
    info->absolutePath=strPath;
    info->dir=rq->dir;
    info->fid=rq->fileid;
    info->md5=strMD5;
    info->name=strName;
    info->size=size;
    info->type="file";
    cout<<"info->absolutePath"<<info->absolutePath<<endl;
    cout<<"info->dir"<<info->dir<<endl;
    info->fileFd=open(info->absolutePath.c_str(),O_RDONLY);
    if(info->fileFd<=0){
        cout<<"file open fail"<<endl;
        return;
    }
    //key 求出来
    int64_t user_time=rq->userid*getNumber()+rq->timestamp;
    //存到map里面
    m_mapTimestampToFileInfo.insert(user_time,info);
    //发送文件头请求
    STRU_FILE_HEADER_RQ headrq;
    strcpy(headrq.dir,rq->dir);
    strcpy(headrq.md5,info->md5.c_str());
    headrq.size=info->size;
    headrq.fileid=rq->fileid;
    strcpy(headrq.fileName,info->name.c_str());
    headrq.timestamp=rq->timestamp;
    strcpy(headrq.fileType,"file");

    SendData(clientfd,(char*)&headrq,sizeof(headrq));

}

void CLogic::DownloadFolderRq(sock_fd clientfd, char *szbuf, int nlen)
{
     _DEF_COUT_FUNC_
    //拆包
     STRU_DOWNLOAD_FOLDER_RQ *rq=(STRU_DOWNLOAD_FOLDER_RQ *)szbuf;
     //查数据库表　拿到信息　查得属性：
     char sqlbuf[1000]="";
     list<string>lstRes;
     sprintf(sqlbuf,"select f_type,f_id,f_name,f_path,f_MD5,f_size,f_dir from user_file_info where u_id=%d and f_dir='%s' and f_id=%d;",rq->userid,rq->dir,rq->fileid);
     bool res=m_sql->SelectMysql(sqlbuf,7,lstRes);
     if(!res){
         cout<<"select fail:"<<sqlbuf<<endl;
         return;
     }
     if(lstRes.size()==0)return;
     string type=lstRes.front();lstRes.pop_front();
     int timestamp=rq->timestamp;
     DownloadFolder(rq->userid,timestamp,clientfd,lstRes);
}

void CLogic::DownloadFolder(int userid,int& timestamp,sock_fd clientfd,list<string>&lstRes){
    int fileid=stoi(lstRes.front());lstRes.pop_front();
    string strName=lstRes.front();lstRes.pop_front();
    string strPath=lstRes.front();lstRes.pop_front();
    string strMd5=lstRes.front();lstRes.pop_front();
    int  size=stoi(lstRes.front());lstRes.pop_front();
    string dir=lstRes.front();lstRes.pop_front();

    //发送创建文件夹请求
    STRU_FOLDER_HEADER_RQ rq;
    rq.timestamp=++timestamp;//时间戳处理
    strcpy(rq.dir,dir.c_str());
    rq.fileid=fileid;
    strcpy(rq.fileName,strName.c_str());

    SendData(clientfd,(char*)&rq,sizeof(rq));

    //拼接路径
    string newDir=dir+strName+"/";
    //查询　newdir userid 所有文件信息（包含type） 列表　３个文件　２１项
    char sqlbuf[1000]="";
    list<string>newlstRes;
    sprintf(sqlbuf,"select f_type,f_id,f_name,f_path,f_MD5,f_size,f_dir from user_file_info where u_id=%d and f_dir='%s';",userid,newDir.c_str());
    bool res=m_sql->SelectMysql(sqlbuf,7,newlstRes);
    if(!res){
        cout<<"select fail:"<<sqlbuf<<endl;
        return;
    }
    while(newlstRes.size()!=0){
    string type=newlstRes.front();newlstRes.pop_front();
    if(type=="file"){
        //如果是文件　下载文件流程
        DownloadFile(userid,timestamp,clientfd,newlstRes);
    }else{
        //如果是文件夹　递归
        DownloadFolder(userid,timestamp,clientfd,newlstRes);
    }
    }
}

void CLogic::DownloadFile(int userid, int &timestamp, sock_fd clientfd, list<string> &lstRes)
{
    int fileid=stoi(lstRes.front());lstRes.pop_front();
    string strName=lstRes.front();lstRes.pop_front();
    string strPath=lstRes.front();lstRes.pop_front();
    string strMd5=lstRes.front();lstRes.pop_front();
    int  size=stoi(lstRes.front());lstRes.pop_front();
    string dir=lstRes.front();lstRes.pop_front();
    //有 写文件信息
    FileInfo *info=new FileInfo();
    info->absolutePath=strPath;
    info->dir=dir;
    info->fid=fileid;
    info->md5=strMd5;
    info->name=strName;
    info->size=size;
    info->type="file";
    cout<<"info->absolutePath"<<info->absolutePath<<endl;
    cout<<"info->dir"<<info->dir<<endl;
    info->fileFd=open(info->absolutePath.c_str(),O_RDONLY);
    if(info->fileFd<=0){
        cout<<"file open fail"<<endl;
        return;
    }
    //key 求出来
    int64_t user_time=userid*getNumber()+(++timestamp);
    //存到map里面
    m_mapTimestampToFileInfo.insert(user_time,info);
    //发送文件头请求
    STRU_FILE_HEADER_RQ headrq;
    strcpy(headrq.dir,dir.c_str());
    strcpy(headrq.md5,info->md5.c_str());
    headrq.size=info->size;
    headrq.fileid=fileid;
    strcpy(headrq.fileName,info->name.c_str());
    headrq.timestamp=timestamp;
    strcpy(headrq.fileType,"file");

    SendData(clientfd,(char*)&headrq,sizeof(headrq));


}
void CLogic::FileHeaderRs(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_FILE_HEADER_RS * rs=(STRU_FILE_HEADER_RS*)szbuf;

    //拿到文件信息
    int64_t user_time=rs->userid*getNumber()+rs->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info))return;

    //发文件内容请求
    STRU_FILE_CONTENT_RQ rq;
    //读文件
    cout<<"info->fileFd"<<info->fileFd<<endl;
    rq.len=read(info->fileFd,rq.content,_DEF_BUFFER);
    if(rq.len==0)return;
    if(rq.len<0){
        perror("read file error");
        return ;
    }
    rq.userid=rs->userid;
    rq.timestamp=rs->timestamp;
    rq.fileid=rs->fileid;
    SendData(clientfd,(char*)&rq,sizeof(rq));
}

void CLogic::FileContentRs(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_FILE_CONTENT_RS * rs=(STRU_FILE_CONTENT_RS*)szbuf;

    //文件信息结构
    int64_t user_time=rs->userid*getNumber()+rs->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info)){
        return;
    }

    //判断是否成功
    if(rs->result!=1){
        //不成功 跳回去
        lseek(info->fileFd,-1*(rs->len),SEEK_CUR);

}else{
        //成功 pos+len
        info->pos+=rs->len;
        //判断是否结束
        if(info->size<=info->pos){
           //是 关闭文件 回收  退出
            close(info->fileFd);
            m_mapTimestampToFileInfo.erase(user_time);
            delete info;
            info=nullptr;
            return;
            }
    }
    //写文件内容请求
    STRU_FILE_CONTENT_RQ rq;
    //读文件
    rq.len=read(info->fileFd,rq.content,_DEF_BUFFER);
    if(rq.len==0)return;
    if(rq.len<0){
        perror("read fail");
        return;
    }
    rq.fileid=rs->fileid;
    rq.userid=rs->userid;
    rq.timestamp=rs->timestamp;

    //发送
    SendData(clientfd,(char*)&rq,sizeof(rq));

}

void CLogic::AddFolderRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    //拆包
    STRU_ADD_FOLDER_RQ* rq=(STRU_ADD_FOLDER_RQ*)szbuf;

    //数据库写表 插入文件信息
    char pathBuf[1000]="";
    sprintf(pathBuf,"%s%d%s%s",DEF_PATH,rq->userid,rq->dir,rq->fileName);

    char sqlbuf[1000]="";
    sprintf(sqlbuf,"insert into t_file(f_size,f_path,f_count,f_MD5,f_state,f_type) values(0,'%s',1,'?',1,'folder');",pathBuf);
    bool res =m_sql->UpdataMysql(sqlbuf);
    if(!res){
        cout<<"update fail:"<<sqlbuf<<endl;
        return ;
    }
    //查询id
    sprintf(sqlbuf,"select f_id from t_file where f_path='%s';",pathBuf);
    list<string>lstRes;
    res =m_sql->SelectMysql(sqlbuf,1,lstRes);
    if(!res){
        cout<<" SelectMysql fail:"<<sqlbuf<<endl;
        return ;
    }
    if(lstRes.size()==0){
        return;
    }
    int id=stoi(lstRes.front());lstRes.pop_front();
    //写入用户文件关系 -- 隐藏 触发器引用计数会+1
    sprintf(sqlbuf,"insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) values(%d,%d,'%s','%s','%s');",rq->userid,id,rq->dir,rq->fileName,rq->time);
    res =m_sql->UpdataMysql(sqlbuf);
    if(!res){
        cout<<" UpdataMysql fail:"<<sqlbuf<<endl;
        return ;
    }
    //创建目录
    umask(0);
    mkdir(pathBuf,0777);

    //写回复
    STRU_ADD_FOLDER_RS rs;
    rs.result=1;
    rs.userid=rq->userid;
    rs.timestamp=rq->timestamp;
    //发送
   SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::ShareFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    //拆包
    STRUCT_SHARED_FILE_RQ *rq=(STRUCT_SHARED_FILE_RQ*)szbuf;
    //随机生成分享链接
    //分享码规则　９位分享码
    int link=0;
     do{
        link=1+random()%9;//随机出　１－９
        link*=100000000;
        link+=random()%100000000;
        //去重　查链接是否已经存在
        char sqlbuf[1000]="";
        sprintf(sqlbuf,"select s_link from t_user_file where s_link=%d;",link);
        list<string>lstRes;
        bool res=m_sql->SelectMysql(sqlbuf,1,lstRes);
        if(!res){
            cout<<"select fail"<<sqlbuf<<endl;
            return;
        }
        if(lstRes.size()>0){
            link=0;
        }
    }while(link==0);
    //遍历　所有文件，将其分享链接设置
    int itemCount=rq->itemCount;
     for(int i=0;i<itemCount;i++){
         ShareItem(rq->userid,rq->fileidArray[i],rq->dir,rq->shareTime,link);
     }
     //写回复
     STRUCT_SHARE_FILE_RS rs;
      rs.result=1;

      SendData(clientfd,(char *)&rs,sizeof(rs));
}

void CLogic::MyShareRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    //拆包
    STRU_MY_SHARE_RQ *rq=(STRU_MY_SHARE_RQ*)szbuf;
    //rq->userid;
    //根据id查询获得分享文件列表
    //查的内容f_namef_sizes_linkTimes_link
    /*select f_name ,f_size,s_linkTime ,s_link from user_file_info where
    u_id = 6 and s_link is not null and s_linkTime is not null;*/
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"select f_name,f_size,s_linkTime,s_link from user_file_info where u_id=%d and s_link is not null and s_linkTime is not null;",rq->userid);
    list<string>lst;
    bool res=m_sql->SelectMysql(sqlbuf,4,lst);
    if(!res){
        cout<<"select fail:"<<sqlbuf<<endl;return ;
    }
    int count=lst.size();
    if((count/4==0)||(count%4!=0))return ;
    count/=4;
    //写回复
    int packlen=sizeof(STRU_MY_SHARE_RS)+count*sizeof(STRU_MY_SHARE_FILE);
    STRU_MY_SHARE_RS * rs=(STRU_MY_SHARE_RS*)malloc(packlen);
    rs->init();
    rs->iteCount=count;
    for(int i=0;i<count;i++){
        string name=lst.front();lst.pop_front();
        int size=stoi(lst.front());lst.pop_front();
        string time=lst.front();lst.pop_front();
        int link=stoi(lst.front());lst.pop_front();

        strcpy(rs->items[i].name,name.c_str());
        rs->items[i].size=size;
        strcpy(rs->items[i].time,time.c_str());
        rs->items[i].shareLink=link;
    }
    cout<<endl;
    cout<<"1"<<endl;
     //发送
     SendData(clientfd,(char *)rs,packlen);
     cout<<"2"<<endl;
     free(rs);
}

void CLogic::GetShareRq(sock_fd clientfd, char *szbuf, int nlen)
{
    __func__;
    //拆包
    STRU_GET_SHARE_RQ *rq=(STRU_GET_SHARE_RQ*)szbuf;
    //根据分享码查询到一系列文件
    //根据分享码　查询到一系列文件
    //查询属性：f_idf_namef_dir(分享人的）f_type u_id(分享人的）
    /*select f_id ,f_name,f_dir,f_type,u_id from user_file_info where s_link=111111111;*/
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"select f_id ,f_name,f_dir,f_type,u_id from user_file_info where s_link=%d;",rq->shareLink);
    list<string>lst;
    bool res=m_sql->SelectMysql(sqlbuf,5,lst);
    if(!res){
        cout<<"select fail:"<<sqlbuf<<endl;return ;
    }
    STRU_GET_SHARE_RS rs;
    if(lst.size()==0){
        rs.result=0;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return ;
    }
    rs.result=1;
    if(lst.size()%5!=0)return ;
    //遍历文件列表
    while(lst.size()!=0){
        int fileid=stoi(lst.front());lst.pop_front();
        string name=lst.front();lst.pop_front();
        string fromdir=lst.front();lst.pop_front();
        string type=lst.front();lst.pop_front();
        int fromuserid=stoi(lst.front());lst.pop_front();
        //如果是文件
        if(type=="file"){
            //插入信息到用户文件关系表
            GetShareByFile(rq->userid,fileid,rq->dir,name,rq->time);
        }else{
    //如果是文件夹
        //插入信息到用户文件关系表
        //拼接路径 获取人目录/->/06/ 分享人的目录/->/06/
        //根据新路径在分享人那边查询文件夹下的文件
        //遍历列表-—>递归
        GetShareByFolder(rq->userid,fileid,rq->dir,name,rq->time,fromuserid,fromdir);
        }
    }
    //写回复包
    strcpy(rs.dir,rq->dir);
    //发送
    SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::DeleteFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    //拆包
    STRU_DELETE_FILE_RQ *rq=(STRU_DELETE_FILE_RQ *)szbuf;
    //id列表
    for(int i=0;i<rq->fileCount;i++){
        int fileid=rq->fileidArray[i];
        //删除每一项
        DeleteOneItem(rq->userid,fileid,rq->dir);
    }
    //写回复
    STRU_DELETE_FILE_RS rs;
    rs.result=1;
    strcpy(rs.dir,rq->dir);
    SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::ContinueDownloadRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    //拆包
    STRU_CONTINUE_DOWNLOAD_RQ* rq=(STRU_CONTINUE_DOWNLOAD_RQ*)szbuf;
    //看看是否存在　文件信息
    int64_t user_time=rq->userid*getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info)){
        //没有　创建文件信息　－－由查表而来　添加到map
        info=new FileInfo;
        //查数据库 查什么？ 如果没有 返回
        char  sqlbuf[1000]="";
        list<string>lstRes;
        sprintf(sqlbuf,"select f_name,f_path,f_MD5,f_size from user_file_info where u_id=%d and f_dir='%s' and f_id=%d;",rq->userid,rq->dir,rq->fileid);
        bool res=m_sql->SelectMysql(sqlbuf,4,lstRes);
        if(!res){
            cout<<"select fail:"<<sqlbuf<<endl;
            return;
        }
        //如果没有返回
        if(lstRes.size()==0)return ;

        string strName=lstRes.front(); lstRes.pop_front();
        string strPath=lstRes.front();lstRes.pop_front();
        string strMD5=lstRes.front();lstRes.pop_front();
        int size=stoi(lstRes.front());lstRes.pop_front();

        //有 写文件信息
        FileInfo *info=new FileInfo();
        info->absolutePath=strPath;
        info->dir=rq->dir;
        info->fid=rq->fileid;
        info->md5=strMD5;
        info->name=strName;
        info->size=size;
        info->type="file";

        info->fileFd=open(info->absolutePath.c_str(),O_RDONLY);
        if(info->fileFd<=0){
            cout<<"file open fail"<<endl;
            return;
        }
        m_mapTimestampToFileInfo.insert(user_time,info);
    }

    //现在有信息了
    //文件指针跳转　pos位置　同步pos
    lseek(info->fileFd,rq->pos,SEEK_SET);
    info->pos=rq->pos;

    //读文件快　发送文件块请求
    STRU_FILE_CONTENT_RQ contentRq;
    contentRq.fileid=rq->fileid;
    contentRq.timestamp=rq->timestamp;
    contentRq.userid=rq->userid;
    contentRq.len=read(info->fileFd,contentRq.content,_DEF_BUFFER);
    SendData(clientfd,(char*)&contentRq,sizeof(contentRq));

}

void CLogic::ContinueUploadRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    //拆包
    STRU_CONTINUE_UPLOAD_RQ* rq=(STRU_CONTINUE_UPLOAD_RQ*)szbuf;
    int64_t user_time=rq->userid*getNumber()+rq->timestamp;
    //需要看map中是否存在
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info)){
        //不存在　创建
        info=new FileInfo;
        info->dir=rq->dir;
       info->fid=rq->fileid;
        info->type="file";
        //查表　获取信息　给info赋值　然后打开文件　info 加到map中
        char  sqlbuf[1000]="";
        list<string>lstRes;
        sprintf(sqlbuf,"select f_name,f_path,f_size,f_MD5 from user_file_info where u_id=%d and f_dir='%s' and f_id=%d and f_state=0;",rq->userid,rq->dir,rq->fileid);
        bool res=m_sql->SelectMysql(sqlbuf,4,lstRes);
        if(!res){
            cout<<"select fail:"<<sqlbuf<<endl;
            return;
        }
        //如果没有返回
        if(lstRes.size()==0)return ;

        info->name=lstRes.front(); lstRes.pop_front();
        info->absolutePath=lstRes.front();lstRes.pop_front();
        info->size=stoi(lstRes.front());lstRes.pop_front();
        info->md5=lstRes.front();lstRes.pop_front();

        //给info赋值　然后打开文件　info加到map中
        info->fileFd=open(info->absolutePath.c_str(),O_WRONLY);
        if(info->fileFd<=0){
            cout<<"file open fail"<<errno<<endl;
            return;
        }
         m_mapTimestampToFileInfo.insert(user_time,info);
}
    //现在已经有这个信息了　lseek跳转并读取文件当前的位置（文件末尾）　更新　pos
    info->pos=lseek(info->fileFd,0,SEEK_END);
    //写回复　返回
    STRU_CONTINUE_UPLOAD_RS rs;
    rs.fileid=rq->fileid;
    rs.pos=info->pos;
    rs.timestamp=rq->timestamp;

    SendData(clientfd,(char*)&rs,sizeof(rs));

}
void CLogic::DeleteOneItem(int userid,int fileid,string dir){
    cout<<__func__<<endl;

    //删除文件需要u_id f_dir f_id
    //需要知道到底是什么类型　type name path
    //再次查询ｉｄ看能不能找到数据库记录，如果不能，删除本地文件
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"select f_type,f_name,f_path from user_file_info where u_id=%d and f_id=%d and f_dir='%s';",userid,fileid,dir.c_str());
    list<string>lst;
     bool res=m_sql->SelectMysql(sqlbuf,3,lst);
    if(!res){
        cout<<"selectMysql fail:"<<sqlbuf<<endl; return ;
    }
    if(lst.size()==0){
        return ;
    }

    string type=lst.front();lst.pop_front();
    string name=lst.front();lst.pop_front();
    string path=lst.front();lst.pop_front();
    if(type=="file"){
        DeleteFile(userid,fileid,dir,path);
    }else{
        DeleteFolder(userid,fileid,dir,name);
    }
}

void CLogic::DeleteFile(int userid,int fileid,string dir,string path){
    //删除用户文件对应的关系
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"delete from t_user_file where u_id=%d and f_id=%d and f_dir='%s';",userid,fileid,dir.c_str());
    bool res=m_sql->UpdataMysql(sqlbuf);
    if(!res){
        cout<<"delete fail:"<<sqlbuf<<endl;
    }
    //再次查询ｉｄ看能不能找到数据库记录，如果不能，删除本地文件
    sprintf(sqlbuf,"select f_id from t_file where f_id=%d;",fileid);
    list<string>lst;
    res=m_sql->SelectMysql(sqlbuf,1,lst);
    if(!res){
        cout<<"selectMysql fail:"<<sqlbuf<<endl;return;
    }
    if(lst.size()==0){
        unlink(path.c_str());//文件ｉｏ删除文件
    }
}

void CLogic::DeleteFolder(int userid,int fileid,string dir,string name){
    cout<<__func__<<endl;
    //删除用户文件对应的关系　u_id f_dir f_id
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"delete from t_user_file where u_id=%d and f_id=%d and f_dir='%s';",userid,fileid,dir.c_str());
    bool res=m_sql->UpdataMysql(sqlbuf);
    if(!res){
        cout<<"delete fail:"<<sqlbuf<<endl;
    }
    //拼接新路经
    std::string newdir=dir+name+"/";
    //查表　根据新路经查表　得到列表　f_type f_id name path
    sprintf(sqlbuf,"select f_type,f_id,f_name,f_path from user_file_info where u_id=%d and f_dir='%s';",userid,newdir.c_str());
    list<string>lst;
    res=m_sql->SelectMysql(sqlbuf,4,lst);
    if(!res){
        cout<<"selectMysql fail:"<<sqlbuf<<endl;return;
    }
    while(lst.size()!=0){
        //循环
        string type=lst.front();lst.pop_front();
        int fileid=stoi(lst.front());lst.pop_front();

         string name=lst.front();lst.pop_front();
         string path=lst.front();lst.pop_front();
         //如果是文件
         if(type=="file"){
             DeleteFile(userid,fileid,newdir,path);
         }else{
        //如果是文件夹
            DeleteFolder(userid,fileid,newdir,name);
         }
    }
}


void CLogic::GetShareByFile(int userid,int fileid,string dir,string name,string time){
    char sqlbuf[1000]="";
    sprintf(sqlbuf,"insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) values(%d,%d,'%s','%s','%s');",userid,fileid,dir.c_str(),name.c_str(),time.c_str());
    bool res =m_sql->UpdataMysql(sqlbuf);
    if(!res){
        cout<<"update fail:%s\n"<<sqlbuf<<endl;

    }
}
void CLogic::GetShareByFolder(int userid,int fileid,string dir,string name,string time,int fromuserid,string fromdir){
     //是文件夹
        //插入信息到用户文件关系表
        GetShareByFile(userid,fileid,dir,name,time);
         //拼接路径
        //获取人目录/->/06/
        string newdir=dir+name+"/";
        //分享人的目录/->/06/
        string newfromdir=fromdir+name+"/";
        //根据新路径在分享人那边查询 文件夹下的文件
        char sqlbuf[1000]="";
        sprintf(sqlbuf,"select f_id ,f_name,f_type from user_file_info where u_id=%d and f_dir='%s';",fromuserid,newfromdir.c_str());
        list<string>lst;
        bool res=m_sql->SelectMysql(sqlbuf,3,lst);
        if(!res){
            cout<<"select fail:"<<sqlbuf<<endl;return ;
        }
        //遍历列表
        while(lst.size()!=0){
            int fileid=stoi(lst.front());lst.pop_front();
            string name=lst.front();lst.pop_front();
            string type=lst.front();lst.pop_front();
            if(type=="file"){
            //是文件
                GetShareByFile(userid,fileid,newdir,name,time);
            }else{
            //是文件夹　－>递归
                GetShareByFolder(userid,fileid,newdir,name,time,fromuserid,newfromdir);
            }
        }
}
void CLogic::ShareItem(int userid, int fileid, string dir, string time, int link)
{

    char sqlbuf[1000]="";
    sprintf(sqlbuf,"update t_user_file set s_link='%d',s_linkTime='%s' where u_id=%d and f_id=%d and f_dir='%s';",link,time.c_str(),userid,fileid,dir.c_str());
     bool res=m_sql->UpdataMysql(sqlbuf);
     if(!res){
         cout<<"UpdataMysql fail"<<sqlbuf<<endl;
         return;
     }
}
