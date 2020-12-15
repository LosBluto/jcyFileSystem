#include<stdio.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>

#define SYS_NAME "jcywjwwx"
#define BLOCKSIZE 1024          //һ�����̿��С
#define ALLBLOCKS 1024000       //���д��̿��ܴ�С
#define MAX_OPEN_FILES 10       //�����ļ���
#define IBLOCKS 10              //��i�ڵ����Ĵ��̿�����

#define MAX_I 10

//һλ�����ݣ�λʾͼʹ��
typedef struct Bit{
    unsigned int data:1;
}Bit;

typedef struct FCB{
    unsigned char fileName[8];   //8B�ļ���
    unsigned short iNum;          //i�ڵ���
}fcb;

typedef struct INode{
    unsigned char exName[3];		//��չ�� 3B
    unsigned char attribute;		//�ļ������ֶ�1B
    unsigned short time;			//�ļ�����ʱ��2B
    unsigned short date;			//�ļ���������2B
    unsigned short blockNum;			//���2B
    long length;        //����                4B
}iNode;

//���ļ�����Ϣ
typedef struct Opened{
    unsigned char fileName[8];   //�ļ���
    unsigned char exName[3];		//��չ��*/3B
    unsigned char attribute;		//�ļ������ֶ�1B
    unsigned short time;			//�ļ�����ʱ��2B
    unsigned short date;			//�ļ���������2B
    unsigned short blockNum;			//���2B
    long length;        //����4B

    /* ����Ϊ��pcb����Ϣ */
    unsigned char free;          //��ʾ�����ݽṹ�Ƿ�ʹ��
    unsigned char *contents;//���Ŀ¼,ֻ��һ��Ĵ�С


}Opened;

//������
typedef struct BLOCK0{
    unsigned short rootBlock;    //root���

    unsigned short iptr;                    //i���е�λ��
    unsigned short blockptr;                   //block���е�λ��
    Bit discTable[MAX_I][ALLBLOCKS/BLOCKSIZE/MAX_I];     //���̵�λʾͼ
    Bit iTable[MAX_I][BLOCKSIZE*IBLOCKS/sizeof(iNode)/MAX_I];                                 //i�ڵ��λʾͼ,����ó���Լ�ܴ�BLOCKSIZE*10/sizeof(iNode)��i�ڵ�
}block0;

unsigned char *Disc;                    //���̿ռ��׵�ַ
Opened openFileList[MAX_OPEN_FILES];    //�����ļ���
Opened *currentContent;                     //��ǰ�򿪵�Ŀ¼
int curfd;

void start();               //����
void format();              //��ʽ��
void my_exit();                //�˳�ϵͳ
void ls();

/* ����Ϊ���ߺ��� */
struct tm *getNowTime();
iNode *getI(unsigned short iNum);      //����i�ڵ�Ż�ȡi�ڵ��׵�ַ
unsigned char *getBlock(unsigned short blockNum);  //�����̿ںŻ�ȡ�̿�ָ��
block0 *getBlock0();                    //��ȡ������
unsigned short getEmptyI();             //��ȡһ����i�ڵ�ĺ���,δ�ҵ�����0
unsigned short getEmptyBlock();         //��ȡһ���տ�ĺ���,δ�ҵ�����0
int getFreeOpened();                    //��ȡ������
void occupyBlock(unsigned short blockNum);  //����̿鱻ռ��
void occupyI(unsigned short iNum);          //���i�ڵ㱻ռ��
void releaseBlock(unsigned short blockNum); //��Ǵ��̿鱻�ͷ� 0
void releaseI(unsigned short iNum);         //���i�ڵ㱻�ͷ� 0
int isEmptyI(unsigned short iNum);          //�ж�ָ��i�ڵ��Ƿ��ǿ�i�ڵ�
int isEmptyBlock(unsigned short blockNum);  //�ж�ָ��block�Ƿ��ǿ�block

/******************* ����Ϊ��Ҫϵͳ���� *******************/
//�����ļ�ϵͳ
void start(){
    /* ��ʼ��������� */
    Disc = (unsigned char *)malloc(ALLBLOCKS);          //��ʼ���������
    memset(Disc,0,ALLBLOCKS);                           //����ֵΪ\0

    FILE *f;
    f = fopen(SYS_NAME,"r");                            //���ļ�
    if(f){
        fread(Disc,ALLBLOCKS,1,f);                      //��Ϣ�������������
        fclose(f);                                      //��ȡ��ر��ļ�
        if(Disc[0] == '\0')
            format();
    }else{
        format();
    }

    /* ��ʼ���ڴ�����Ϣ�����Ŀ¼ */
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
    for(j = 0;j<BLOCKSIZE;j++){                 //��Ŀ¼��Ϣ���Ƴ���
        openFileList[i].contents[j] = p[j];
    }

    curfd = 0;

}

//��ʽ���ļ�ϵͳ
void format(){
    printf("starting format disc !!!\n");
    int i;

    struct tm *nowTime;
    FILE *f;

    unsigned char *p;       //ͷָ��
    unsigned char *ip;      //i�ڵ��ָ��
    block0 *b;      //������
    fcb *rootContent; //��Ŀ¼
    iNode *rootI;          //��Ŀ¼��i�ڵ�

    /* ��ʼ�������� */
    p = Disc;
    b = (block0 *)p;
    b->rootBlock = 12;      //��ʮ����
    b->iptr = 2;           //iptrָ��ֱ�ӵ��ڶ���
    b->blockptr = 13;      //block��ѯָ��ֱ�ӵ���13��
    for(i = 0;i<12;i++){
        b->discTable[0][i].data = 1; //ǰʮ�����ѱ�ʹ��,���Ϊ1
    }

    /* ��ʼ����Ŀ¼ */
    p += BLOCKSIZE*(b->rootBlock-1);      //��ָ��ŵ������ݿ���
    ip = Disc + BLOCKSIZE;  //�ڶ��鿪ʼ���i�ڵ�
    nowTime = getNowTime();

    rootI = (iNode *)ip;
    rootContent = (fcb *)p;
    b->iTable[0][0].data = 1;            //��0��i�ڵ�����Ϊʹ��

    //���.��..��Ϣ
    rootContent->iNum = 1;              //.��..��ָ��root��i�ڵ�
    strcpy(rootContent->fileName,".");

    rootContent++;
    rootContent->iNum = 1;
    strcpy(rootContent->fileName,"..");

    //��ʼ��i�ڵ�
    strcpy(rootI->exName,"con");
    rootI->attribute = 'c';
    rootI->blockNum = 12;
    rootI->date = nowTime->tm_year*512 + (nowTime->tm_mon+1)*32 + nowTime->tm_mday;     //ͨ����λ����������
    rootI->time = nowTime->tm_hour*2048 + nowTime->tm_min*32 + nowTime->tm_sec/2;         //ͨ����λ������ʱ��
    rootI->length = 2*sizeof(fcb);      //��ʼֻ��.��..����Ŀ¼


    f = fopen(SYS_NAME,"w");            //��Ϣд���ļ�
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
    fcb *p;             //����ָ��
    iNode *ip;          //i�ڵ��ָ��
    p = (fcb *)openFileList[curfd].contents;    //ֱ�ӴӴ��ļ��л�ȡĿ¼

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


/************************ ����Ϊ���ߺ��� *****************************/
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

    for(;b->blockptr < ALLBLOCKS/BLOCKSIZE ;b->blockptr++){     //�Դ��������
        if(isEmptyBlock(b->blockptr))
            return b->blockptr;
    }

    for(b->blockptr = 0;b->blockptr<temp;b->blockptr++){        //�ٴ�ͷ�������ϴε�λ��
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
