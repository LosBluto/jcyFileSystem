#include<stdio.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>

#define SYS_NAME "jcywjwwx"
#define BLOCKSIZE 1024          //一个磁盘块大小
#define ALLBLOCKS 1024000       //所有磁盘块总大小
#define MAX_OPEN_FILES 10       //最多打开文件数
#define IBLOCKS 10              //给i节点分配的磁盘块数量

#define MAX_I 10
#define MAX_PATH_LENGTH 80

//一位的数据，位示图使用
typedef struct Bit{
    unsigned int data:1;
}Bit;

typedef struct FCB{
    unsigned char fileName[8];   //8B文件名
    unsigned char exName[3];
    unsigned short iNum;          //i节点编号
}fcb;

typedef struct INode{
    unsigned char exName[3];		//扩展名 3B
    unsigned char attribute;		//文件属性字段1B
    unsigned short time;			//文件创建时间2B
    unsigned short date;			//文件创建日期2B
    unsigned short blockNum;			//块号2B
    long length;        //长度                4B
}iNode;

//打开文件的信息
typedef struct Opened{
    unsigned char fileName[8];   //文件名
    unsigned char exName[3];		//扩展名*/3B
    unsigned char attribute;		//文件属性字段1B
    unsigned short time;			//文件创建时间2B
    unsigned short date;			//文件创建日期2B
    unsigned short blockNum;			//块号2B
    long length;        //长度4B

    /* 以下为非pcb中信息 */
    unsigned char free;          //表示该数据结构是否被使用
    unsigned char *contents;//存放目录,只有一块的大小
    unsigned char path[MAX_PATH_LENGTH]; //该打开文件的父路径
    int fatherFd;

}Opened;

//引导块
typedef struct BLOCK0{
    unsigned short rootBlock;    //root块号

    unsigned short iptr;                    //i表中的位置
    unsigned short blockptr;                   //block表中的位置
    Bit discTable[MAX_I][ALLBLOCKS/BLOCKSIZE/MAX_I];     //磁盘的位示图
    Bit iTable[MAX_I][BLOCKSIZE*IBLOCKS/sizeof(iNode)/MAX_I];                                 //i节点的位示图,计算得出大约能存BLOCKSIZE*10/sizeof(iNode)个i节点
}block0;

unsigned char *Disc;                    //磁盘空间首地址
Opened openFileList[MAX_OPEN_FILES];    //最大打开文件数
int curfd;

void start();               //启动
void format();              //格式化
void my_exit();                //退出系统
void ls();                      //展示当前目录文件
int open(unsigned char *path,int tempFd);   //打开文件
void changeDir(unsigned char *path);       //cd操作

/* 以下为工具函数 */
struct tm *getNowTime();
iNode *getI(unsigned short iNum);      //根据i节点号获取i节点首地址
unsigned char *getBlock(unsigned short blockNum);  //根据盘口号获取盘口指针
block0 *getBlock0();                    //获取引导块
unsigned short getEmptyI();             //获取一个空i节点的号码,未找到返回0
unsigned short getEmptyBlock();         //获取一个空块的号码,未找到返回0
int getEmptyOpened();                    //获取空区域
void occupyBlock(unsigned short blockNum);  //标磁盘块被占用
void occupyI(unsigned short iNum);          //标记i节点被占用
void releaseBlock(unsigned short blockNum); //标记磁盘块被释放 0
void releaseI(unsigned short iNum);         //标记i节点被释放 0
int isEmptyI(unsigned short iNum);          //判断指定i节点是否是空i节点
int isEmptyBlock(unsigned short blockNum);  //判断指定block是否是空block
int writeBlock(unsigned short blockNum,int ptr,long length,char *text);
void copyToOpend(int openedFd,iNode *ip,fcb *cp);
char *splitPath(char *path);                //分割出path后面部分
int openByName(int fd,unsigned char *name); //通过文件名打开当前目录下的文件

/******************* 以下为主要系统函数 *******************/
//启动文件系统
void start(){
    /* 初始化虚拟磁盘 */
    Disc = (unsigned char *)malloc(ALLBLOCKS);          //初始化虚拟磁盘
    memset(Disc,0,ALLBLOCKS);                           //赋初值为\0

    FILE *f;
    f = fopen(SYS_NAME,"r");                            //打开文件
    if(f){
        fread(Disc,ALLBLOCKS,1,f);                      //信息读入虚拟磁盘中
        fclose(f);                                      //读取完关闭文件
        if(Disc[0] == '\0')
            format();
    }else{
        format();
    }

    /* 初始化内存中信息，如根目录 */
    iNode *rootI;
    unsigned char *p;

    rootI = (iNode *)(Disc+BLOCKSIZE);
    p = Disc+11*BLOCKSIZE;

    int i = 0;
    strcpy(openFileList[i].fileName,"root");
    strcpy(openFileList[i].exName,"di");
    openFileList[i].blockNum = rootI->blockNum;
    openFileList[i].attribute = rootI->attribute;
    openFileList[i].date = rootI->date;
    openFileList[i].time = rootI->time;
    openFileList[i].length = rootI->length;

    openFileList[i].free = 1;
    openFileList[i].contents = (unsigned char*)malloc(BLOCKSIZE);
    strcpy(openFileList[i].path,"/");
    openFileList[i].fatherFd = 0;
    int j;
    for(j = 0;j<BLOCKSIZE;j++){                 //把目录信息复制出来
        openFileList[i].contents[j] = p[j];
    }

    curfd = 0;

}

//格式化文件系统
void format(){
    printf("starting format disc !!!\n");
    int i;

    struct tm *nowTime;
    FILE *f;

    unsigned char *p;       //头指针
    unsigned char *ip;      //i节点的指针
    block0 *b;      //引导块
    fcb *rootContent; //根目录
    iNode *rootI;          //根目录的i节点

    /* 初始化引导块 */
    p = Disc;
    b = (block0 *)p;
    b->rootBlock = 12;      //第十二块
    b->iptr = 2;           //iptr指针直接到第二块
    b->blockptr = 13;      //block查询指针直接到第13块
    for(i = 0;i<12;i++){
        b->discTable[0][i].data = 1; //前十二块已被使用,标记为1
    }

    /* 初始化根目录 */
    p += BLOCKSIZE*(b->rootBlock-1);      //把指针放到根数据块上
    ip = Disc + BLOCKSIZE;  //第二块开始存放i节点
    nowTime = getNowTime();

    rootI = (iNode *)ip;
    rootContent = (fcb *)p;
    b->iTable[0][0].data = 1;            //第0个i节点设置为使用

    //存放.和..信息
    rootContent->iNum = 1;              //.和..都指向root的i节点
    strcpy(rootContent->fileName,".");
    strcpy(rootContent->exName,"di");


    rootContent++;
    rootContent->iNum = 1;
    strcpy(rootContent->fileName,"..");
    strcpy(rootContent->exName,"di");


    //初始化i节点
    rootI->attribute = 'c';
    rootI->blockNum = 12;
    rootI->date = nowTime->tm_year*512 + (nowTime->tm_mon+1)*32 + nowTime->tm_mday;     //通过移位来保存日期
    rootI->time = nowTime->tm_hour*2048 + nowTime->tm_min*32 + nowTime->tm_sec/2;         //通过移位来保存时间
    rootI->length = 2*sizeof(fcb);      //初始只有.与..两个目录


    f = fopen(SYS_NAME,"w");            //信息写入文件
    fwrite(Disc,ALLBLOCKS,1,f);
    fclose(f);
}

void my_exit(){
    FILE *f;
    f = fopen(SYS_NAME,"w");
    fwrite(Disc,ALLBLOCKS,1,f);
    fclose(f);
    free(Disc);
}

void ls(){
    fcb *p;             //便利指针
    iNode *ip;          //i节点的指针
    p = (fcb *)openFileList[curfd].contents;    //直接从打开文件中获取目录

    int i;
    for(i = 0; i<(int)openFileList[curfd].length/sizeof(fcb);i++){
        ip = getI(p->iNum);
        if(p->fileName[0] != '\0'){
            if(ip->attribute == 'c'){
                printf("%s\\\t\t<dir>\t\t%d/%d/%d\t%02d:%02d:%02d\n",p->fileName,((ip->date)>>9)+1900,((ip->date)>>5)&0x000f,(ip->date)&0x001f,ip->time>>11,(ip->time>>5)&0x003f,ip->time*2&0x001f);
            }else{
                printf("%s\\\t\t%dB\t\t%d/%d/%d\t%02d:%02d:%02d\n",p->fileName, ip->length,((ip->date)>>9)+1900,((ip->date)>>5)&0x000f,(ip->date)&0x001f,ip->time>>11,(ip->time>>5)&0x003f,ip->time*2&0x001f);
            }
        }
        p++;
    }
}

int open(unsigned char *path,int tempFd){
    unsigned char *part;                    //记录/之前的路径
    unsigned char *splited;                 //记录/后的路径
    int i;
    int result;
    int temp = 0;                               //记录返回值

    if(path[0] == '\0'){                                    //递归结束标志，如果路径已经递归结束,返回
        printf("success");
        curfd = tempFd;                     //文件最终打开成功，改变当前文件描述符的指向，指向为新打开文件
        return 0;
    }

    part = strtok(path,"/");

    if(strcmp("root",part)==0){
        splited = path+sizeof(part)+2;          //移动指针

        result = open(splited,0);
    }else{                          //其余情况是在当前指定目录下面查找
        splited = path+sizeof(part)+1;

        temp = openByName(tempFd,part);
        if(temp == -1 && temp != 0)  //获取打开文件的描述符,temp=0证明temp未被替代
            return -1;
        result = open(splited,tempFd);
    }

    if(result == -1){                       //如果打开失败则需要释放之前打开的所有文件
        openFileList[temp].free = 0;
    }

}

void changeDir(unsigned char *path){
    //因为.和其余情况打开函数都已经考虑，cd只需要考虑..情况
    if(strcmp("..",path) == 0)      //如果cd到父目录
        curfd = openFileList[curfd].fatherFd;   //直接把当前描述符指向换成父文件描述符
    else
        open(path,curfd);
}


/************************ 以下为工具函数 *****************************/
struct tm *getNowTime(){
    time_t *now;
    struct tm *nowTime;

    now = (time_t *)malloc(sizeof(time_t));
    time(now);
    nowTime = localtime(now);
    return nowTime;
}

unsigned short getEmptyI(){
    block0 *b = getBlock0();
    unsigned short temp = b->iptr;

    for(;b->iptr < (int)BLOCKSIZE*IBLOCKS/sizeof(iNode) ;b->iptr++){
        if(isEmptyI(b->iptr))
            return b->iptr;
    }

    for(b->iptr = 0;b->iptr < temp ;b->iptr++){
        if(isEmptyI(b->iptr))
            return b->iptr;
    }

    printf("Can't find empty i !!!\n");
    return 0;
}

unsigned short getEmptyBlock(){
    block0 *b = getBlock0();
    unsigned short temp = b->blockptr;

    for(;b->blockptr < ALLBLOCKS/BLOCKSIZE ;b->blockptr++){     //以此往后遍历
        if(isEmptyBlock(b->blockptr))
            return b->blockptr;
    }

    for(b->blockptr = 0;b->blockptr<temp;b->blockptr++){        //再从头遍历到上次的位置
        if(isEmptyBlock(b->blockptr))
            return b->blockptr;
    }

    printf("Can't find empty block !!!\n");
    return 0;
}

block0 *getBlock0(){
    block0 * temp;
    temp = (block0 *)Disc;
    return temp;
}

iNode *getI(unsigned short iNum){
    iNode *ip;
    ip = (iNode *)(Disc+BLOCKSIZE+(iNum-1)*sizeof(iNode));
    return ip;
}

int getEmptyOpened(){
    int i;
    for(i=0;i<MAX_OPEN_FILES;i++){
        if(openFileList[i].free == 0)
            return i;
    }
    printf("opened list is full!!!\n");
    return -1;
}

unsigned char *getBlock(unsigned short blockNum){
    unsigned char *temp;
    temp = Disc+(blockNum-1)*BLOCKSIZE;
}

void occupyBlock(unsigned short blockNum){
    int i = (int)(blockNum-1)/MAX_I;
    int j = (blockNum-1)%MAX_I;
    block0 *b = getBlock0();
    b->discTable[i][j].data = 1;
}

void occupyI(unsigned short iNum){
    int i = (int)(iNum-1)/MAX_I;
    int j = (iNum-1)%MAX_I;
    block0 *b = getBlock0();
    b->iTable[i][j].data = 1;
}

void releaseBlock(unsigned short blockNum){
    int i = (int)(blockNum-1)/MAX_I;
    int j = (blockNum-1)%MAX_I;
    block0 *b = getBlock0();
    b->discTable[i][j].data = 0;
}

void releaseI(unsigned short iNum){
    int i = (int)(iNum-1)/MAX_I;
    int j = (iNum-1)%MAX_I;
    block0 *b = getBlock0();
    b->iTable[i][j].data = 0;
}

int isEmptyI(unsigned short iNum){
    int i = (int)(iNum-1)/MAX_I;
    int j = (iNum-1)%MAX_I;
    block0 *b = getBlock0();
    if(b->iTable[i][j].data == 0)
        return 1;
    else
        return 0;
}

int isEmptyBlock(unsigned short blockNum){
    int i = (int)(blockNum-1)/MAX_I;
    int j = (blockNum-1)%MAX_I;
    block0 *b = getBlock0();
    if(b->discTable[i][j].data == 0)
        return 1;
    else
        return 0;
}

void copyToOpend(int opened,iNode *ip,fcb *cp){
    strcpy(openFileList[opened].fileName,cp->fileName);
    strcpy(openFileList[opened].exName,cp->exName);
    openFileList[opened].attribute = ip->attribute;
    openFileList[opened].blockNum = ip->blockNum;
    openFileList[opened].date = ip->date;
    openFileList[opened].time = ip->time;
    openFileList[opened].length = ip->length;

    openFileList[opened].free = 1;
}

int writeBlock(unsigned short blockNum,int ptr,long length,char *text){
    unsigned char *p = getBlock(blockNum);
    if(ptr+length > BLOCKSIZE){                //
        printf("write error: write too long !!!");
        return -1;
    }

    p += ptr;
    int i;
    for(i = 0; i<length;i++,p++){
        p[i] = text[i];
    }
    return 0;
}

int openByName(int fd,unsigned char *name){
    int i;              //遍历使用
    int tempFd;         //记录空描述符
    int temp;           //遍历使用
    fcb *cp;            //fcb的指针
    iNode *ip;          //i节点的指针
    unsigned char *fileName;    //文件名
    unsigned char *exName;               //文件拓展名
    unsigned char *fullName;             //文件绝对路径
    unsigned char *tempFullName;
    if(strcmp(name,".")==0)             //如果文件名为.则代表自己直接返回描述符
        return fd;

    fileName = strtok(name,".");        //获取文件名和拓展名
    if(fileName == NULL){               //如果文件名为空，证明输入的是..
        printf("path error!!!\n");
        return -1;
    }

    exName = strtok(NULL,".");
    if(exName == NULL){                  //如果拓展名截取为空，那么他的拓展名字段填为目录拓展名di
        exName = (unsigned char*)malloc(3);
        strcpy(exName,"di");
    }

    fullName = (unsigned char*)malloc(sizeof(openFileList[fd].path)+sizeof(openFileList[fd].fileName));
    strcpy(fullName,openFileList[fd].path);
    strcat(fullName,fileName);   //获取绝对路

    //遍历比较打开文件是否已有该文件
    for(i = 0; i<MAX_OPEN_FILES; i++){
        if(openFileList[i].free == 0)   //未使用直接跳过
            continue;

        tempFullName = (unsigned char*)malloc(sizeof(openFileList[i].path)+sizeof(openFileList[i].fileName));
        strcpy(tempFullName,openFileList[i].path);
        strcat(tempFullName,openFileList[i].fileName);   //获取绝对路径

        if(strcmp(fullName,tempFullName)==0 &&  //比较绝对路径
           strcmp(openFileList[i].exName,exName)==0)
            break;
    }
    if(i != MAX_OPEN_FILES){                    //如果没有遍历到最后，则是查询到了结果
        return i;                              //直接指向该打开文件
    }

    tempFd = getEmptyOpened();                  //寻找空打开位置
    if(tempFd == -1){
        printf("open error: open list is full !!!\n");
        return -1;
    }

    //从目录中查询该文件
    cp = (fcb *)openFileList[fd].contents;
    temp = (int)openFileList[fd].length/sizeof(fcb);    //目录的长度
    for(i = 0; i<temp;i++,cp++){
        if(cp->fileName[0] != '\0'){                //该目录存在
            if(strcmp(cp->fileName,fileName)==0 &&  //文件名和拓展名相同
               strcmp(cp->exName,exName) == 0)
                break;
        }
    }
    if(i == temp){                           //遍历到最后一位，则代表没有查询成功
        printf("open error: the file do not exit !!!\n");
        return -1;
    }

    //把文件信息复制到openlist中
    ip = getI(cp->iNum);
    copyToOpend(tempFd,ip,cp);             //复制到opened
    openFileList[tempFd].fatherFd = fd;
    strcpy(openFileList[tempFd].path,openFileList[fd].path);
    strcat(openFileList[tempFd].path,"/");
    strcat(openFileList[tempFd].path,openFileList[fd].fileName);

    return tempFd;

}


int main(){
    start();
    unsigned char part[80] = "/root/test/test/te";

    open(part,0);

    return 0;
}
