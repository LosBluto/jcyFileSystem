#include<stdio.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>
#include<conio.h>

#define SYS_NAME "jcywjwwx"
#define BLOCKSIZE 1024          //一个磁盘块大小
#define ALLBLOCKS 1024000       //所有磁盘块总大小
#define MAX_OPEN_FILES 10       //最多打开文件数
#define IBLOCKS 10              //给i节点分配的磁盘块数量

#define MAX_I 10
#define MAX_PATH_LENGTH 80
#define EXNAME_LENGTH 4


//一位的数据，位示图使用
typedef struct Bit{
    unsigned int data:1;
}Bit;

typedef struct FCB{
    unsigned char fileName[8];   //8B文件名
    unsigned char exName[EXNAME_LENGTH];
    unsigned short iNum;          //i节点编号
}fcb;

typedef struct INode{
    unsigned char attribute;		//文件属性字段1B
    unsigned short time;			//文件创建时间2B
    unsigned short date;			//文件创建日期2B
    unsigned short blockNum;			//块号2B
    long length;        //长度                4B
}iNode;

//打开文件的信息
typedef struct Opened{
    unsigned char fileName[8];   //文件名
    unsigned char exName[EXNAME_LENGTH];		//扩展名4B
    unsigned char attribute;		//文件属性字段1B
    unsigned short time;			//文件创建时间2B
    unsigned short date;			//文件创建日期2B
    unsigned short blockNum;			//块号2B
    unsigned short iNum;            //i节点编号
    long length;        //长度4B

    /* 以下为非fcb中信息 */
    unsigned char free;          //表示该数据结构是否被使用
    unsigned char *contents;//存放目录,只有一块的大小
    unsigned char path[MAX_PATH_LENGTH]; //该打开文件的父路径
    int fatherFd;                       //父目录描述符
    int itsPosition;                    //在父目录中的位置
    int ifFcbChange;                    //记录fcb内容是否被改变，方便之后写回
    int ifContentChange;                //记录目录是否被改变，方便写回目录
    int writePtr;                       //写指针位置
    int readPtr;                        //读指针位置

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
int curfd;                              //当前文件描述符

void start();               //启动p
void format();              //格式化
void my_exit();                //退出系统
void ls();                      //展示当前目录文件
int open(unsigned char *path,int tempFd);   //打开文件
void changeDir(unsigned char *path);       //cd操作
void create(unsigned char *name);                  //创建新的文件
void mkdir(unsigned char *name);                //创建文件夹
void close(int fd);                             //关闭指定文件描述符的文件
void removeFile(unsigned char *path);           //删除指定文件
void showOpenFiles();

void read();/*wx*/
void write();/*wx*/


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
void copyContent(unsigned char *contents,unsigned char *p,int num); //把磁盘中的目录拷贝到内存中,个数为num
unsigned short getTime(struct tm *nowTime);
unsigned short getDate(struct tm *nowTime);
unsigned char *subStr(unsigned char *path,unsigned char **part,char *sign);     //以sign标志截取path,返回截取后的指针
int changeToIndex(int fd);                             //把指定描述符文件更改为index存放
int removeByName(fcb *content, unsigned char *name);                  //删除指定目录下的文件
fcb *getContent(unsigned char *path,unsigned char** part);                  //获取指定文件的所在的content

char* cut_left(char *dest, char *src ,int n);/*wx*/
void changeOpenlistFlag(int fd, long len, int writePtr, int ifFcbchange);
long directToIndirect(unsigned short blockNum, int index, long length, unsigned char *text);
long addFromIndex(unsigned short blockNum, long index, long length, unsigned char *text);

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
    openFileList[i].iNum = 1;               //根目录使用第一个i节点

    openFileList[i].free = 1;
    openFileList[i].contents = (unsigned char*)malloc(BLOCKSIZE);
    strcpy(openFileList[i].path,"");
    openFileList[i].fatherFd = 0;
    openFileList[i].itsPosition = 1;            //根目录的位置是他自己的第一个
    openFileList[i].writePtr = 0;
    openFileList[i].readPtr = 0;

    copyContent(openFileList[i].contents,p,(int)(BLOCKSIZE)/sizeof(fcb));//把目录信息复制出来
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
    if(openFileList[curfd].attribute != 'c'){            //如果不是文件夹，直接返回
        printf("ls error: it is not a directory!!!\n");
        return;
    }
    fcb *p;             //便利指针
    iNode *ip;          //i节点的指针
    p = (fcb *)openFileList[curfd].contents;    //直接从打开文件中获取目录


    printf("%-10s\t%-10s\t%-10s\t%-10s\t%-10s\n","Name","Type","Length(B)","Date","Time");
    int i;
    for(i = 0; i<(int)BLOCKSIZE/sizeof(fcb);i++,p++){
        ip = getI(p->iNum);
        if(p->fileName[0] != '\0'){

            if(ip->attribute == 'c'){
                unsigned char fullName[15];
                strcpy(fullName,p->fileName);
                strcat(fullName,"/");
                printf("%-10s\t%-10s\t%-10s\t%d/%d/%d\t%02d:%02d:%02d\n",fullName,"<dir>","/",((ip->date)>>9)+1900,((ip->date)>>5)&0x000f,(ip->date)&0x001f,ip->time>>11,(ip->time>>5)&0x003f,ip->time*2&0x001f);
            }else{
                unsigned char fullName[MAX_PATH_LENGTH];
                strcpy(fullName,p->fileName);
                strcat(fullName,".");
                strcat(fullName,p->exName);
                printf("%-10s\t%-10s\t%-10d\t%d/%d/%d\t%02d:%02d:%02d\n",fullName, "<file>",ip->length,((ip->date)>>9)+1900,((ip->date)>>5)&0x000f,(ip->date)&0x001f,ip->time>>11,(ip->time>>5)&0x003f,ip->time*2&0x001f);
            }
        }
    }
}

int open(unsigned char *path,int tempFd){
    if(openFileList[curfd].attribute != 'c'){            //如果不是文件夹，直接返回
        printf("open error: it is not a directory!!!\n");
        return -1;
    }

    unsigned char *part;                    //记录/之前的路径
    unsigned char *splited;                 //记录/后的路径
    int i;
    int result;
    int temp = 0;                               //记录返回值

    if(path[0] == '\0'){                                    //递归结束标志，如果路径已经递归结束,返回
        return tempFd;
    }

    splited = subStr(path,&part,"/");

    if(strcmp("root",part)==0){
        result = open(splited,0);

    }else{                          //其余情况是在当前指定目录下面查找
        temp = openByName(tempFd,part);

        if(temp == -1 || temp == 0)  //获取打开文件的描述符,temp=0证明temp未被替代
            return -1;

        result = open(splited,temp);

        if(result == -1){                       //如果打开失败则需要释放之前打开的所有文件
            openFileList[temp].free = 0;
        }
    }


    return result;
}

void changeDir(unsigned char *path){
    if(!path){
        printf("cd error: path is not right!!!\n");
        return;
    }

    int result;
    //因为.和其余情况打开函数都已经考虑，cd只需要考虑..情况
    if(strcmp("..",path) == 0){      //如果cd到父目录,表示关闭当前目录，需要调用关闭文件
        int temp = curfd;
        curfd = openFileList[curfd].fatherFd;   //直接把当前描述符指向换成父文件描述符
        close(temp);                            //关闭之前的目录
    }else{
        result = open(path,curfd);
        if(result != -1)
            curfd = result;
    }
}

void create(unsigned char *name){
    /* 前期判断是否满足创建条件 */
    if(openFileList[curfd].attribute != 'c'){            //如果不是文件夹，直接返回
        printf("create error: it is not a directory!!!\n");
        return;
    }

    int iNum = getEmptyI();                                 //判断i节点表是否放满
    if(iNum == 0){
        printf("create error: file table is full!!!\n");
        return;
    }

    int blockNum = getEmptyBlock();                         //判断是否还有空块
    if(blockNum == 0){
        printf("create error: disc is full!!!\n");
        return;
    }

    int i;                                                  //判断当前目录是否满
    int temp;
    int result = 0;
    fcb *cp;
    cp = (fcb *)openFileList[curfd].contents;

    if((BLOCKSIZE-openFileList[curfd].length/sizeof(fcb))/sizeof(fcb) < 1){ //计算总大小和剩余大小的差值判断还能否放下
        printf("create error: the directory is full!!!\n");
        return;
    }

    /*判断处理文件名*/
    unsigned char *fileName;
    unsigned char *exName;
    fileName = strtok(name,".");
    exName = strtok(NULL,".");
    if(fileName != NULL && exName == NULL){   //输入的文件名没有拓展名，表示创建文件
        printf("create error: please input the exName!!!\n");
        return;
    }

    if(fileName == NULL || exName == NULL || strlen(exName)>EXNAME_LENGTH || strlen(fileName)>8){             //判断用户输入文件名是否正确
        printf("create error: fileName error!!!\n");
        return;
    }

    for(i=0; i<BLOCKSIZE/sizeof(fcb); i++){    //从整个块中查询,是否重名
        if(strcmp(fileName,cp[i].fileName)==0 && strcmp(exName,cp[i].exName)==0){       //排除同名
            printf("create error: fileName already exits!!!\n");
            return;
        }
    }

    for(i=0; i<BLOCKSIZE/sizeof(fcb); i++,cp++){
        if(cp->fileName[0] == '\0'){              //判断fcb为空的标志为fileName为\0
            break;                              //如果该位置不为空，则更换指针位置
        }
    }


    /* 正式创建文件,给fcb赋值 */
    iNode *ip;
    ip = getI(iNum);
    struct tm *nowTime = getNowTime();

    strcpy(cp->fileName,fileName);
    strcpy(cp->exName,exName);
    cp->iNum = iNum;
    ip->date = getDate(nowTime);
    ip->time = getTime(nowTime);
    occupyI(iNum);

    if(strcmp(exName,"di")==0){         //目录文件需要写入数据,并且放入.和..文件
        ip->attribute = 'c';
        ip->blockNum = blockNum;
        occupyBlock(blockNum);

        /* 给目录文件添加.和..的内容 */
        fcb *fp;
        fp = (fcb *)getBlock(blockNum);
        strcpy(fp->fileName,".");
        strcpy(fp->exName,"di");
        fp->iNum = iNum;

        fp++;
        strcpy(fp->fileName,"..");
        strcpy(fp->exName,"di");
        fp->iNum = openFileList[curfd].iNum;    //..的i节点号为父目录的i节点号

        ip->length = 2*sizeof(fcb);             //初始大小为两个fcb大小
    }else{                              //普通文件未写入数据
        ip->attribute = 'n';
        ip->blockNum = 0;
        ip->length = 0;
    }

    /* 创建成功之后 */
    openFileList[curfd].length += sizeof(fcb);
    openFileList[curfd].ifContentChange = 1;    //创建文件只会改变目录文件的content
    openFileList[curfd].ifFcbChange = 1;

}

void close(int fd){
    if(openFileList[fd].free == 0){                 //如果文件本就未被打开
        printf("close error: file do not open!!!\n");
        return;
    }

    if(openFileList[fd].attribute == 'c'){          //关闭当前文件所有子文件
        int i;
        unsigned char fullName[MAX_PATH_LENGTH];
        strcpy(fullName,openFileList[fd].path);
        strcat(fullName,"/");
        strcat(fullName,openFileList[fd].fileName);
        for(i=0;i<MAX_OPEN_FILES;i++){
            if(openFileList[i].free)
                if(strcmp(fullName,openFileList[i].path)==0)
                    close(i);
        }
    }

    if(openFileList[fd].ifFcbChange == 1){               //如果fcb被改变，写入父目录的content,并且把i节点内容写入虚拟磁盘
        int fatherFd = openFileList[fd].fatherFd;
        int position = openFileList[fd].itsPosition;
        fcb *p = (fcb *)openFileList[fatherFd].contents;       //寻找其fcb在目录中的位置
        p += (position-1);
        iNode *ip = (iNode *)getI(p->iNum);
        if(strcmp(p->fileName,".")!=0 && strcmp(p->fileName,"..")!=0)
            strcpy(p->fileName,openFileList[fd].fileName);
        strcpy(p->exName,openFileList[fd].exName);
        p->iNum = openFileList[fd].iNum;

        ip->attribute = openFileList[fd].attribute;
        ip->blockNum = openFileList[fd].blockNum;
        ip->date = openFileList[fd].date;
        ip->length = openFileList[fd].length;
        ip->time = openFileList[fd].time;

        openFileList[fatherFd].ifContentChange = 1;         //父目录的目录结构改变

        openFileList[fd].ifFcbChange = 0;
    }

    if(openFileList[fd].ifContentChange == 1){           //如果content被改变过，则写入虚拟磁盘，目录文件的数据块直接就是目录
        int blockNum = openFileList[fd].blockNum;
        unsigned char *p = getBlock(blockNum);
        copyContent(p,openFileList[fd].contents,(int)BLOCKSIZE/sizeof(fcb));

        openFileList[fd].ifContentChange=0;
    }

    curfd = openFileList[fd].fatherFd;              //指向父目录
    if(fd != 0)                                     //如果该文件不为根目录文件
        openFileList[fd].free = 0;                      //释放该描述符区域

    FILE *f;                                        //把信息写入文件中
    f = fopen(SYS_NAME,"w");
    fwrite(Disc,ALLBLOCKS,1,f);
    fclose(f);
}

void mkdir(unsigned char *name){                //创建目录本质上就是创建文件
    if(name == NULL){
        return;
    }
    if(openFileList[curfd].attribute != 'c'){            //如果不是文件夹，直接返回
        printf("mkdir error: it is not a directory!!!\n");
        return;
    }
    strcat(name,".di");
    create(name);
}

void removeFile(unsigned char *path){
    if(path == NULL)
        return;

    if(openFileList[curfd].attribute != 'c'){            //如果不是文件夹，直接返回
        printf("rm error: it is not a directory!!!\n");
        return;
    }

    int i;
    unsigned char fullPath[MAX_PATH_LENGTH];
    strcpy(fullPath,openFileList[curfd].path);
    strcat(fullPath,"/");
    strcat(fullPath,openFileList[curfd].fileName);
    for(i=0;i<MAX_OPEN_FILES;i++){                          //判断要删除的文件是否被打开
        if(openFileList[i].free){
            unsigned char fullName[15];


            strcpy(fullName,openFileList[i].fileName);
            if(openFileList[i].attribute != 'c'){                //不是目录文件
                strcat(fullName,".");
                strcat(fullName,openFileList[i].exName);
            }
            if(strcmp(fullName,path)==0 && strcmp(openFileList[i].path,fullPath)==0){
                printf("rm error: file is opened!!!\n");
                return;
            }
        }
    }


    fcb *content;
    content = (fcb *)openFileList[curfd].contents;

    removeByName(content,path);

    openFileList[curfd].length -= sizeof(fcb);
    openFileList[curfd].ifContentChange = 1;
    openFileList[curfd].ifFcbChange = 1;
}

void showOpenFiles(){
    int i;
    unsigned char fullName[MAX_PATH_LENGTH];
    for(i=0;i<MAX_OPEN_FILES;i++){
        if(openFileList[i].free){
            strcpy(fullName,openFileList[i].path);
            strcat(fullName,"/");
            strcat(fullName,openFileList[i].fileName);
            if(openFileList[i].attribute != 'c'){
                strcat(fullName,".");
                strcat(fullName,openFileList[i].exName);
            }

            printf("%s\n",fullName);
        }
    }
}

/*wx-----------------------------------------*/
void read(){
    if(openFileList[curfd].attribute == 'c'){
        printf("read error: can't read a directory!!!\n");
        return;
    }

    long file_len = openFileList[curfd].length;
    printf("file length:%ld\n",file_len);
    if(file_len == 0){
        return;
    }
    long index;
    printf("Please input read index(start with 1)：");
    scanf("%ld",&index);
    getchar();
    if(index > file_len || index < 1){
        printf("index error!\n");
        return;
    }

    unsigned short block_num = openFileList[curfd].blockNum;
    unsigned char *head = getBlock(block_num);

    if(file_len <= BLOCKSIZE){//读直接块
        int i;
        for(i = index - 1; i < file_len; i ++){
            printf("%c", head[i]);
        }
    }else{//读间接块
        unsigned short *p = (unsigned short *) head;
        unsigned short *max_index = p + file_len/BLOCKSIZE;
        p += index/BLOCKSIZE;/*移到index所在块*/
        int index_beyond = index % BLOCKSIZE;/*index所在数据块偏移量*/
        int last_beyond = file_len%BLOCKSIZE;
        unsigned char *q = getBlock(*p);

        int i;
        if(p == max_index){/*若index即为最后一块*/
           for(i = index_beyond-1; i < last_beyond; i++){
                    printf("%c", q[i]);
            }
            printf("\n");
            return;
        }else{
             for(i = index_beyond-1; i < BLOCKSIZE; i++){/*从index所在块的index处开始读*/
                    printf("%c", q[i]);
            }
        }
        p++;

        while(p < max_index){/*读中间块*/
            q = getBlock(*p);
            for(i = 0; i < BLOCKSIZE; i++){
                printf("%c", q[i]);
            }
            p++;
        }
        if(p == max_index){/*读最后一块*/
            q = getBlock(*p);
            for(i = 0; i < last_beyond; i++){
                    printf("%c", q[i]);
            }
        }
    }
    printf("\n");
}

void write(){
    if(openFileList[curfd].attribute == 'c'){
        printf("read error: can't write a directory!!!\n");
        return;
    }

    int type;
    long index;
    unsigned char text[BLOCKSIZE*(BLOCKSIZE/sizeof(short))];
    long old_len = openFileList[curfd].length;

    printf("Please input write type\n");
    printf("1. cut write\n");
    printf("2. add write\n");
    printf("3. cover write\n");
    printf("4. exist\n");
    type = getch();

    if(type == 51){
        printf("Please input write index：");
        scanf("%ld", &index);
        if(index > old_len){
            printf("index is too long!!!\n");
            return;
        }
        getchar();
    }else if(type == 52){
        return;
    }

    printf("Please input data:");
    gets(text);
    long new_len = strlen(text);

    //分新块
    if(old_len == 0){
        openFileList[curfd].blockNum = getEmptyBlock();
        if(!openFileList[curfd].blockNum){
            return;
        }
        occupyBlock(openFileList[curfd].blockNum);
    }

    unsigned short block_num = openFileList[curfd].blockNum;
    unsigned char *head = getBlock(block_num);

    switch(type){
        case 49: /*cut write*/
            if(new_len > BLOCKSIZE*(BLOCKSIZE/sizeof(short))){//新写的文件大小超过最大文件大小
                printf("write too long！\n");
                return;
            }
            /*第一大类 block_num为直接块*/
            if(new_len <= BLOCKSIZE  && old_len <= BLOCKSIZE){
                writeBlock(block_num, 0, new_len, text);

                changeOpenlistFlag(curfd, new_len, new_len%BLOCKSIZE, 1);
            }
            /*第二大类 block_num为间接块
            ？可以考虑暴力全释放直接块，重新分配 优：简单可合并 缺：重新分配需遍历磁盘位示图*/
            else if(new_len > BLOCKSIZE  && old_len > BLOCKSIZE){
                if(new_len < old_len){/*可能释放块*/
                    unsigned short *p = (unsigned short *) head;
                    unsigned short *max_index = p + old_len/BLOCKSIZE;
                    int beyond = new_len % BLOCKSIZE;

                    p = (unsigned short *) head;/*从头开始写*/
                    do{
                        writeBlock(*p, 0, BLOCKSIZE, text);
                        p++;
                    }while(strcmp(cut_left(text, text, BLOCKSIZE),"")!=0);

                    p++;
                    while(p <= max_index){/*释放块*/
                        releaseBlock(*p);
                        p++;
                    }

                    changeOpenlistFlag(curfd, new_len, new_len%BLOCKSIZE, 1);
                }else{/*可能新分配块*/

                    long writed_len = addFromIndex(block_num, 0, old_len, text);

                    changeOpenlistFlag(curfd, writed_len, writed_len%BLOCKSIZE, 1);
                }

            }
            /*第三大类 直接块<->间接块*/
            else if(old_len <= BLOCKSIZE < new_len||new_len <= BLOCKSIZE < old_len){
                if(old_len < new_len){/*block_num为直接块 一定新分配块*/

                    long writed_len = directToIndirect(block_num, 0, old_len, text);

                    changeOpenlistFlag(curfd, writed_len, writed_len%BLOCKSIZE, 1);
                }else if(new_len < old_len){/*block_num为间接块 一定释放块*/
                    unsigned short *p = (unsigned short *) head;
                    unsigned short *max_index = p + old_len/BLOCKSIZE;
                    while(p <= max_index){/*释放块*/
                        releaseBlock(*p);
                        p++;
                    }

                    writeBlock(block_num, 0, new_len, text);/*把之前的索引块变为当前的数据块*/

                    changeOpenlistFlag(curfd, new_len, new_len%BLOCKSIZE, 1);
                }

            }
            break;
        case 50:/*add write*/
            if(new_len + old_len > BLOCKSIZE*(BLOCKSIZE/sizeof(short))){//新写的文件大小超过最大文件大小
                printf("write too long！\n");
                return;
            }else if(new_len + old_len <= BLOCKSIZE){
                writeBlock(block_num, old_len, new_len, text);
                changeOpenlistFlag(curfd, new_len+old_len, (new_len+old_len)%BLOCKSIZE, 1);
            }else if(new_len + old_len > BLOCKSIZE){
                long writed_len;
                if(old_len <= BLOCKSIZE){/*直接块变间接块*/

                    writed_len = directToIndirect(block_num, old_len, old_len, text);

                }else if(old_len > BLOCKSIZE){/*间接块*/

                    writed_len = addFromIndex(block_num, old_len, old_len, text);

                }
                changeOpenlistFlag(curfd, writed_len, writed_len%BLOCKSIZE, 1);
            }

            break;
        case 51:/*cover write  */
                if(index-1 + new_len > BLOCKSIZE*(BLOCKSIZE/sizeof(short))){
                    printf("write too long！");
                    return;
                }else if(index-1 + new_len <= BLOCKSIZE && old_len <= BLOCKSIZE){/*直接块中cover*/
                    writeBlock(block_num, index-1, new_len, text);

                    if(index + new_len > old_len){
                        changeOpenlistFlag(curfd, new_len+index-1, new_len+index-1, 1);
                    }else{
                        changeOpenlistFlag(curfd, old_len, new_len+index-1, 1);
                    }
                }else if(index-1 + new_len > BLOCKSIZE && old_len <= BLOCKSIZE){/*直接块变间接块*/

                    long writed_len = directToIndirect(block_num, index-1, old_len, text);

                    changeOpenlistFlag(curfd, writed_len, writed_len%BLOCKSIZE, 1);
                }else{/*间接块中cover*/
                    long writed_len = addFromIndex(block_num, index-1, old_len, text);

                    if(index-1 + new_len > old_len){
                        changeOpenlistFlag(curfd, writed_len, writed_len%BLOCKSIZE, 1);
                    }else{/*文件长度未增加,不采用writed_len*/
                        changeOpenlistFlag(curfd, old_len, (new_len+index-1)%BLOCKSIZE, 1);
                    }
                }
            break;
            default:
                printf("write type error！\n");
    }

}
/*wx-------------------------------------------------------*/



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
    openFileList[opened].iNum = cp->iNum;

    openFileList[opened].free = 1;
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
        exName = (unsigned char*)malloc(EXNAME_LENGTH);
        strcpy(exName,"di");
    }

    fullName = (unsigned char*)malloc(sizeof(openFileList[fd].path)+sizeof(openFileList[fd].fileName)+sizeof(fileName));
    strcpy(fullName,openFileList[fd].path);
    strcat(fullName,"/");
    strcat(fullName,openFileList[fd].fileName);
    strcat(fullName,"/");
    strcat(fullName,fileName);   //获取绝对路

    //遍历比较打开文件是否已有该文件
    for(i = 0; i<MAX_OPEN_FILES; i++){
        if(openFileList[i].free == 0)   //未使用直接跳过
            continue;

        tempFullName = (unsigned char*)malloc(sizeof(openFileList[i].path)+sizeof(openFileList[i].fileName));
        strcpy(tempFullName,openFileList[i].path);
        strcat(tempFullName,"/");
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
    temp = (int)BLOCKSIZE/sizeof(fcb);    //目录的长度
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
    openFileList[tempFd].itsPosition = i+1;   //在父目录中的位置为i
    openFileList[tempFd].writePtr = 0;      //指针初值都为0
    openFileList[tempFd].readPtr = 0;
    if(ip->attribute == 'c'){               //如果为目录需要复制目录信息
        unsigned char *tempCp;
        tempCp = getBlock(ip->blockNum);
        openFileList[tempFd].contents = (unsigned char*)malloc(BLOCKSIZE);
        copyContent(openFileList[tempFd].contents,tempCp,(int)BLOCKSIZE/sizeof(fcb));
    }

    return tempFd;

}

//从p拷贝到contents
void copyContent(unsigned char *contents,unsigned char *p,int num){
    int i;
    fcb *cp;
    fcb *fp;
    cp = (fcb*)contents;
    fp = (fcb*)p;
    for(i=0; i<num; i++){
        cp[i] = fp[i];
    }
}

unsigned short getTime(struct tm *nowTime){
    return nowTime->tm_hour*2048 + nowTime->tm_min*32 + nowTime->tm_sec/2;
}

unsigned short getDate(struct tm *nowTime){
    return nowTime->tm_year*512 + (nowTime->tm_mon+1)*32 + nowTime->tm_mday;     //通过移位来保存日期
}

unsigned char *subStr(unsigned char *path,unsigned char **part,char* sign){
    int length = strlen(path);              //记录原路径长度
    int partLength;
    int i;
    unsigned char *temp;

    *part = strtok(path,sign);               //截取字符串
    temp = *part;
    partLength = strlen(*part);

    temp += partLength;                     //移动指针
    if(partLength >= length){               //防止越界
        temp[length-1] = '\0';
        return &temp[length-1];
    }

    for(i=partLength;i<length-1;i++,temp++){
        if(temp[0] == '\0' && temp[1] != '\0'){     //遇到后一个不为\0
            break;
        }
    }

    return &temp[1];
}

int changeToIndex(int fd){
    unsigned short *sp;
    unsigned short temp1 = getEmptyBlock();
    if(temp1 == 0)                   //磁盘块已满
        return -1;

    sp =(unsigned short*)getBlock(temp1);                        //获取盘块指针
    sp[0] = openFileList[fd].blockNum;                          //把原来的块号存入

    openFileList[fd].blockNum = temp1;                          //更改块号
    openFileList[fd].ifFcbChange = 1;
    return 0;
}

fcb *getContent(unsigned char *path,unsigned char **part){
    int i;
    iNode *ip;
    unsigned char *p;
    int tempFd = curfd;
    fcb *content;
    *part = strtok(path,"/");        //获取文件名
    if(strcmp(*part,"root"))         //如果从root开始
        tempFd = 0;                 //文件描述符置为0

    /* 开始循环查找目录 */
    content = (fcb *)openFileList[tempFd].contents;    //指向当前的目录
    while(*part){
        for(i=0;i<BLOCKSIZE/sizeof(fcb);i++,content++){               //查询目录是否存在
            if(content->fileName[0] != '\0'){               //当前目录不为空
                if(strcmp(content->fileName,*part)==0)       //目录中存在当前查找的
                    break;
            }
        }
        if(i >= BLOCKSIZE/sizeof(fcb)){                      //未找到文件
            printf("the file do not exist!!!\n");
            return NULL;
        }

        *part = strtok(NULL,"/");    //获取下一个
        if(*part)                    //如果下一个为空，那么表示已经找到其目录
            break;

        ip = getI(content->iNum);
        content = (fcb *)getBlock(ip->blockNum);
    }

    return content;
}

int removeByName(fcb *content,unsigned char *name){
    fcb *cp;
    int i;
    unsigned char *fileName;
    unsigned char *exName;
    unsigned char *tempName;

    fileName = strtok(name,".");
    exName = strtok(NULL,".");

    if(fileName != NULL && exName==NULL){   //这个情况为目录
        exName = (unsigned char*)malloc(EXNAME_LENGTH);
        strcpy(exName,"di");
    }else if(fileName == NULL && exName==NULL){
        printf("remove error: file do not exist!!!\n");
        return -1;
    }


    cp = content;

    for(i=0;i<(int)BLOCKSIZE/sizeof(fcb);i++,cp++){
        if(cp->fileName[0] != '\0'){
            if(strcmp(fileName,cp->fileName) == 0 &&            //如果名称相同则找到了
               strcmp(exName,cp->exName) == 0)
                break;
        }
    }

    if(i >= (int)BLOCKSIZE/sizeof(fcb)){
        printf("remove error: file do not exist!!!\n");
        return -1;
    }

    iNode *ip;
    ip = getI(cp->iNum);
    if(ip->attribute == 'c'){                //如果是目录文件，递归删除
        fcb *fp = (fcb *)getBlock(ip->blockNum);
        fcb *tempFp = fp;
        for(i=0;i<BLOCKSIZE/sizeof(fcb);i++,fp++){
            if(fp->fileName[0] != '\0'){        //存在的fcb
                if(strcmp(fp->fileName,".") == 0 || strcmp(fp->fileName,"..")==0)      //.和..跳过
                    continue;
                tempName = (unsigned char *)malloc(sizeof(fp->fileName)+sizeof(".")+sizeof(fp->exName)); //用一块新的空间来存放名称，方便之后的操作
                strcpy(tempName,fp->fileName);
                strcat(tempName,".");
                strcat(tempName,fp->exName);
                printf("%s\n",tempName);
                removeByName(tempFp,tempName);
            }
        }
    }else{                                  //非目录文件
        if(ip->length > BLOCKSIZE){          //如果为间接索引，先删除其余物理块
            unsigned short *sp = (unsigned short *)getBlock(ip->blockNum);  //获取索引表
            int length = ((int)(ip->length/BLOCKSIZE))+1;        //索引表中物理块数量
            for(i=0;i<length;i++,sp++){
                releaseBlock(*sp);
            }
        }
    }
    //释放当前文件
    cp->fileName[0] = '\0';         //fcb为空的标志为文件名是\0
    releaseBlock(ip->blockNum);     //释放物理块
    releaseI(cp->iNum);             //释放i节点

    return 0;
}

/*wx----------------------------*/
int writeBlock(unsigned short blockNum,int ptr,long length,char *text){
    unsigned char *p = getBlock(blockNum);
    if(ptr+length > BLOCKSIZE){
        printf("write error: write too long !!!");
        return -1;
    }
    p += ptr;
    int i;
    for(i = 0; i<length;i++){
        p[i] = text[i];
    }
    return 0;
}
/*从src中截掉前n位，返回剩余部分*/
char* cut_left(char *dest, char *src ,int n){
    char *p=dest;
    char *q=src;
    int len=strlen(src);
    if(n>len){
        n=len;
    }
    q += n;
    while(len--) *(p++)=*(q++);
    // *(p++)='\0';
    return dest;
}

void changeOpenlistFlag(int fd, long len, int writePtr, int ifFcbchange){
    openFileList[fd].length=len;
    openFileList[fd].writePtr=writePtr;
    openFileList[fd].ifFcbChange=ifFcbchange;
}

/*直接块变为间接块，从index开始追加text，返回当前文件长度（成功写完或者由于没有空磁盘而未写完）*/
long directToIndirect(unsigned short blockNum,int index, long length, unsigned char *text){
    long writed_len = length;/*当前文件长度为旧长度*/
    unsigned short cur_direct_block = openFileList[curfd].blockNum;
    unsigned short new_indirect_block = getEmptyBlock();
    if(!new_indirect_block){
        return writed_len;
    }
    occupyBlock(new_indirect_block);
    /*修改openlist的BlockNum*/
    openFileList[curfd].blockNum = new_indirect_block;/*新分配盘块作为间接块*/

    unsigned short *p = (unsigned short *)getBlock(new_indirect_block);
    *p = cur_direct_block;/*原直接盘块作为第一个数据块*/
    writeBlock(*p, index, BLOCKSIZE-index, text);/*将第一个数据块重新填(追加补)满*/
    cut_left(text, text, BLOCKSIZE-index);
    writed_len = BLOCKSIZE;/*当前长度为BLOCKSIZE*/
    p++;
    do{
        *p = getEmptyBlock();
        if(*p){
            occupyBlock(*p);
        }else{
            return writed_len;
        }
        if(strlen(text) > BLOCKSIZE){
            writeBlock(*p, 0, BLOCKSIZE, text);
            writed_len += BLOCKSIZE;
        }else{
            writeBlock(*p, 0, strlen(text), text);
            writed_len += strlen(text);
        }
        p++;
    }while(strcmp(cut_left(text, text, BLOCKSIZE),"")!=0);

    return writed_len;
}

/*在blockNum间接块所对应的数据块的index处覆盖text, length为原文件长度, 返回当前写指针之前的文件长度*/
long addFromIndex(unsigned short blockNum, long index, long length, unsigned char *text){
    long writed_len;
    if(index == length){/*add write*/
        writed_len = length;
    }else{/*cover write*/
        writed_len = index;/*调用该函数前已将index减1*/
    }
    unsigned char *head = getBlock(blockNum);
    unsigned short *p = (unsigned short *) head;
    unsigned short *max_index = p + length/BLOCKSIZE;
    p += index/BLOCKSIZE;/*移到index所在块*/
    int beyond = index % BLOCKSIZE;/*最后一块数据块*/

    if(strlen(text) <= BLOCKSIZE - beyond){
        writeBlock(*p, beyond, strlen(text), text);/*填部分或正好填满index块，text写完了*/
        writed_len += strlen(text);
    }else{/*填完index后 text未完*/
        writeBlock(*p, beyond, BLOCKSIZE-beyond, text);/*填满index块*/
        cut_left(text, text, BLOCKSIZE-beyond);
        writed_len += BLOCKSIZE-beyond;
        p++;
        do{
            if(p > max_index){/*超出旧数据块时新分配块*/
                *p = getEmptyBlock();
                if(*p){
                    occupyBlock(*p);
                }else{
                    return writed_len;
                }
            }
            if(strlen(text) > BLOCKSIZE){
                writeBlock(*p, 0, BLOCKSIZE, text);
                writed_len += BLOCKSIZE;
            }else{
                writeBlock(*p, 0, strlen(text), text);
                writed_len += strlen(text);
            }
            p++;
        }while(strcmp(cut_left(text, text, BLOCKSIZE),"")!=0);
    }
    return writed_len;
}
/*wx----------------------------*/

void begin(){
    start();

    char cmd[11][15] = {"exit","create","mkdir","open","cd","close","ls","rm","write","read","show"};
    char path[MAX_PATH_LENGTH];
    char input[MAX_PATH_LENGTH];
    char *tempInput;
    int i;
    int temp = 0;

    while(1){
        memset(input,0,MAX_PATH_LENGTH);
        strcpy(path,openFileList[curfd].path);
        strcat(path,"/");
        strcat(path,openFileList[curfd].fileName);
        if(openFileList[curfd].attribute != 'c'){           //如果不是目录文件，则显示后缀
            strcat(path,".");
            strcat(path,openFileList[curfd].exName);
        }
        printf("%s>:",path);
        gets(input);
        tempInput = strtok(input," ");          //空格切分输入

        for(i=0;i<11;i++){
            if(strcmp(cmd[i],input) == 0){
                break;
            }
        }

        switch(i){
        case 0:
            temp = 1;
            break;
        case 1:
            tempInput = strtok(NULL," ");       //获取第二个参数
            create(tempInput);
            break;
        case 2:
            tempInput = strtok(NULL," ");       //获取第二个参数
            mkdir(tempInput);
            break;
        case 3:
            tempInput = strtok(NULL," ");
            if(tempInput == NULL)
                break;
            int result = open(tempInput,curfd);
            if(result != -1)
                curfd = result;
            break;
        case 4:
            tempInput = strtok(NULL," ");
            changeDir(tempInput);
            break;
        case 5:
            tempInput = strtok(NULL," ");
            close(curfd);
            break;
        case 6:
            ls();
            break;
        case 7:
            tempInput = strtok(NULL," ");
            removeFile(tempInput);
            break;
        case 8:
            write();
            break;
        case 9:
            read();
            break;
        case 10:
            showOpenFiles();
            break;
        default:
            printf("please input right command!\n");
        }

        if(temp == 1){       //结束系统
            for(i=0;i<MAX_OPEN_FILES;i++){              //关闭所有打开文件
                if(openFileList[i].free == 1){
                    close(i);
                }
            }
            my_exit();
            break;
        }
    }
}

int main(){
    begin();
    return 0;
}
