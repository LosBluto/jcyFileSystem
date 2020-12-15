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

//一位的数据，位示图使用
typedef struct Bit{
    unsigned int data:1;
}Bit;

typedef struct FCB{
    unsigned char fileName[8];   //8B文件名
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
Opened *currentContent;                     //当前打开的目录
int curfd;

void start();               //启动
void format();              //格式化
void my_exit();                //退出系统
void ls();

/* 以下为工具函数 */
struct tm *getNowTime();
iNode *getI(unsigned short iNum);      //根据i节点号获取i节点首地址
unsigned char *getBlock(unsigned short blockNum);  //根据盘口号获取盘口指针
block0 *getBlock0();                    //获取引导块
unsigned short getEmptyI();             //获取一个空i节点的号码,未找到返回0
unsigned short getEmptyBlock();         //获取一个空块的号码,未找到返回0
int getFreeOpened();                    //获取空区域
void occupyBlock(unsigned short blockNum);  //标磁盘块被占用
void occupyI(unsigned short iNum);          //标记i节点被占用
void releaseBlock(unsigned short blockNum); //标记磁盘块被释放 0
void releaseI(unsigned short iNum);         //标记i节点被释放 0
int isEmptyI(unsigned short iNum);          //判断指定i节点是否是空i节点
int isEmptyBlock(unsigned short blockNum);  //判断指定block是否是空block

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
    strcpy(openFileList[i].exName,rootI->exName);
    openFileList[i].blockNum = rootI->blockNum;
    openFileList[i].attribute = rootI->attribute;
    openFileList[i].date = rootI->date;
    openFileList[i].time = rootI->time;
    openFileList[i].length = rootI->length;

    openFileList[i].free = 1;
    openFileList[i].contents = (unsigned char*)malloc(BLOCKSIZE);
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

    rootContent++;
    rootContent->iNum = 1;
    strcpy(rootContent->fileName,"..");

    //初始化i节点
    strcpy(rootI->exName,"con");
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

int getFreeOpened(){
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

int main(){
    start();

    ls();

    return 0;
}
