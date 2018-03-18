#include<stdio.h>
#include<malloc.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>

#define BLOCKSIZE 1024
#define SIZE 1024000
#define END 65535
#define FREE 0
#define ROOT_BLOCKNUM 2
#define MAX_OPEN_FILE 10
#define MAX_TXT_SIZE	10000

typedef struct FCB
{
	char filename[8];   /*8B文件名*/
	char exname[3];		/*3B扩展名*/
	unsigned char attribute;		/*文件属性字段*/
	char retainbyte[10] ;			/*10B保留字*/
	unsigned short time;			/*文件创建时间*/
	unsigned short date;			/*文件创建日期*/
	unsigned short first;			/*首块号*/
	unsigned long length;		/*文件大小*/
}fcb;

/*
文件分配表 file allocation table
*/
typedef struct FAT
{
	unsigned short id;
}fat;

/*
属性对照表
位      7    6      5       4     3     2        1     0
属性  保留  保留  存档  子目录  卷标  系统文件  隐藏  只读
*/
typedef struct USEROPEN
{
	char filename[8];   /*8B文件名*/
	char exname[3];		/*3B扩展名*/
	unsigned char attribute;		/*文件属性字段*/
	char retainbyte[10] ;			/*10B保留字*/
	unsigned short time;			/*文件创建时间*/
	unsigned short date;			/*文件创建日期*/
	unsigned short first;			/*首块号*/
	unsigned long length;	/*文件大小*/

	char free; /*表示目录项是否为空 0空1分配*/
	int father;		/*父目录的文件描述符*/
	int dirno;		/*相应打开文件的目录项在父目录文件中的盘块号*/
	int diroff;	/*相应打开文件的目录项在父目录文件的dirno盘块中的目录项序号*/
	char dir[MAX_OPEN_FILE][80];	/*相应打开文件所在的目录*/
	int count;	/*读写指针在文件中的位置*/
	char fcbstate;		/*是否修改了文件的FCB内容，修改置1，否则为0*/
	char topenfile;	/*表示该用户打开的表项是否为空，若值为0，表示为空*/
}useropen;

typedef struct BLOCK0
{
	unsigned short fbnum;
	char information[200];
	unsigned short root;
	unsigned char *startblock;
}block0;

/*全局变量定义*/

unsigned char *myvhard;		/*指向虚拟磁盘的起始地址*/
useropen openfilelist[MAX_OPEN_FILE];	/*用户打开文件表数组*/
useropen *ptrcurdir;		/*指向用户打开文件表中的当前目录所在打开文件表项的位置*/
int curfd;
char currentdir[80];		/*记录当前目录的文件名*/
unsigned char *startp;		/*记录虚拟磁盘上数据区开始位置*/
char filename[]="c:\\myfilesys";   /*虚拟空间保存路径*/
unsigned char buffer[SIZE];

/*函数声明*/
void startsys();
void my_format();
void my_cd(char *dirname);
void my_mkdir(char *dirname);
void my_rmdir(char *dirname);
void my_ls();
void my_create(char *filename);
void my_rm(char *filename);
int my_open(char *filename);
int my_close(int fd);
int my_write(int fd);
int do_write(int fd,char *text,int len,char wstyle);
unsigned short findFree();
int my_read(int fd,int len);
int do_read(int fd,int len,char *text);
void my_exitsys();


/*函数设计*/

/*文件系统初始化*/
/*
原型声明:		void startsys()
功能描述：		文件系统初始化,初始化所建立的文件系统
输入：无
输出：无

函数功能实现算法描述：
	1）申请磁盘空间
	2）打开系统磁盘，若不存在，创建新的系统磁盘，并格式化
	3）初始化用户打开文件表，将表项0分配给根目录文件使用
		并填写根目录文件的相关信息
	4）将ptrcurdir指向该用户打开文件表项
	5）将当前目录设置为根目录
*/
void startsys()
{
	FILE *fp;
	int i;
	myvhard=(unsigned char *)malloc(SIZE);
	memset(myvhard,0,SIZE);
	fp=fopen(filename,"r");
	if(fp)
	{
		fread(buffer,SIZE,1,fp);
		fclose(fp);
		if(buffer[0]!=0xaa||buffer[1]!=0xaa)
		{
			printf("myfilesys is not exist,begin to creat the file...\n");
			my_format();
		}
		else
		{
			for(i=0;i<SIZE;i++)
					myvhard[i]=buffer[i];
		}
	}
	else
	{
		printf("myfilesys is not exist,begin to creat the file...\n");
		my_format();
	}


	strcpy(openfilelist[0].filename,"root");
	strcpy(openfilelist[0].exname,"di");
	openfilelist[0].attribute=0x2d;
	openfilelist[0].time=((fcb *)(myvhard+5*BLOCKSIZE))->time;
	openfilelist[0].date=((fcb *)(myvhard+5*BLOCKSIZE))->date;
	openfilelist[0].first=((fcb *)(myvhard+5*BLOCKSIZE))->first;
	openfilelist[0].length=((fcb *)(myvhard+5*BLOCKSIZE))->length;
	openfilelist[0].free=1;
	openfilelist[0].dirno=5;
	openfilelist[0].diroff=0;
	openfilelist[0].count=0;
	openfilelist[0].fcbstate=0;
	openfilelist[0].topenfile=0;
	openfilelist[0].father=0;

	memset(currentdir,0,sizeof(currentdir));
	strcpy(currentdir,"\\root\\");
	strcpy(openfilelist->dir[0],currentdir);
	startp=((block0 *)myvhard)->startblock;
	ptrcurdir=&openfilelist[0];
	curfd=0;
}



/*
原型声明:		void my_format()
功能描述：		对虚拟磁盘进行格式化，布局虚拟磁盘，建立根目录文件
输入：无
输出：无

函数功能实现算法描述：
	虚拟磁盘空间布局
		1块		2块	   2块	995块
		引导块	FAT1  FAT2 	数据区
	虚拟磁盘一共划分成1000个磁盘块
	每块1024个字节，磁盘空间布局如上
	将数据区的第一块（即虚拟磁盘的第6块）分配给根目录文件
*/
void my_format()
{
	FILE *fp;
	fat *fat1,*fat2;
	block0 *b0;
	time_t *now;
	struct tm *nowtime;
	unsigned char *p;
	fcb *root;
	int i;

	p=myvhard;
	b0=(block0 *)p;
	fat1=(fat *)(p+BLOCKSIZE);
	fat2=(fat*)(p+3*BLOCKSIZE);
	/*
	引导块
	*/
	b0->fbnum=0xaaaa;	/*文件系统魔数 10101010*/
	b0->root = 5;
	strcpy(b0->information,"My FileSystem Ver 1.0 \n Blocksize=1KB Whole size=1000KB Blocknum=1000 RootBlocknum=2\n");
	/*
	FAT1,FAT2
		前面五个磁盘块已分配，标记为END
	*/
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	fat1->id=6;
	fat2->id=6;
	fat1++;fat2++;
	fat1->id=END;
	fat2->id=END;
	fat1++;fat2++;
	/*
	将数据区的标记为空闲状态
	*/
	for(i=7;i<SIZE/BLOCKSIZE;i++)
	{
		(*fat1).id=FREE;
		(*fat2).id=FREE;
		fat1++;
		fat2++;
	}
	/*
	创建根目录文件root，将数据区的第一块分配给根目录区
	在给磁盘上创建两个特殊的目录项：".",".."，
	除了文件名之外，其它都相同
	*/
	p+=BLOCKSIZE*5;
	root=(fcb *)p;
	strcpy(root->filename,".");
	strcpy(root->exname,"di");
	root->attribute=40;
	now=(time_t *)malloc(sizeof(time_t));
	time(now);
	nowtime=localtime(now);
	root->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	root->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	root->first=5;
	root->length=2*sizeof(fcb);
	root++;

	strcpy(root->filename,"..");
	strcpy(root->exname,"di");
	root->attribute=40;
	time(now);
	nowtime=localtime(now);
	root->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	root->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	root->first=5;
	root->length=2*sizeof(fcb);
	root++;

	for(i=2;i<BLOCKSIZE*2/sizeof(fcb);i++,root++)
	{
		root->filename[0]='\0';
	}
	fp=fopen(filename,"w");
	b0->startblock=p+BLOCKSIZE*4;
	fwrite(myvhard,SIZE,1,fp);
	free(now);
	fclose(fp);

}


/**/
/*
原型声明:		void my_exitsys()
功能描述：		退出文件系统
输入：			无
输出：			无

函数功能实现算法描述：

*/
void my_exitsys()
{
	FILE *fp;
	fcb *rootfcb;
	char text[MAX_TXT_SIZE];
	while(curfd)
		curfd=my_close(curfd);
	if(openfilelist[curfd].fcbstate)
	{
		openfilelist[curfd].count=0;
		do_read(curfd,openfilelist[curfd].length,text);
		rootfcb=(char *)text;
		rootfcb->length=openfilelist[curfd].length;
		openfilelist[curfd].count=0;
		do_write(curfd,text,openfilelist[curfd].length,2);
	}
	fp=fopen(filename,"w");
	fwrite(myvhard,SIZE,1,fp);
	free(myvhard);
	fclose(fp);
}



/*
原型声明:		int do_read(int fd,int len,char *text)
功能描述：		实际读文件函数，读出指定文件中从指定指针开始的长度
				为len的内容到用户空间的text中
输入：
		fd		open（）函数的返回值，文件的描述符
		len		要求从文件中读出的字节数
		text	指向存放读出数据的用户区地址
输出：
	实际读出的字节数
函数功能实现算法描述：

*/
int do_read(int fd,int len,char *text)
{
	unsigned char *buf;
    unsigned short bknum;
	int off,i,lentmp;
	unsigned char *bkptr;
	char *txtmp,*p;
	fat *fat1,*fatptr;
    fat1=(fat *)(myvhard+BLOCKSIZE);
	lentmp = len;
	txtmp=text;
	/*
	申请1024B空间作为缓冲区buffer
	*/
	buf=(unsigned char *)malloc(1024);
	if(buf==NULL)
	{
		printf("malloc failed!\n");
		return -1;
	}

	off = openfilelist[fd].count;
	bknum = openfilelist[fd].first;
	fatptr = fat1+bknum;
	while(off >= BLOCKSIZE)
	{
		off=off-BLOCKSIZE;
		bknum=fatptr->id;
		fatptr=fat1+bknum;
		if(bknum == END)
		{
			printf("Error,the block is not exist.\n");
			return -1;
		}
	}

	bkptr=(unsigned char *)(myvhard+bknum*BLOCKSIZE);
	//strncpy(buf,bkptr,BLOCKSIZE);
	for(i=0;i<BLOCKSIZE;i++)
	{
		buf[i]=bkptr[i];
	}

	while(len > 0)
	{
		if(BLOCKSIZE-off > len)
		{
			//strncpy(txtmp,buf+off,len);
			//len=len-len;
			//openfilelist[fd].count+=len;
			//off+=len;
			for(p=buf+off;len>0;p++,txtmp++)
			{
				*txtmp=*p;
				len--;
				off++;
				openfilelist[fd].count++;
			}
		}
		else
		{
			//strncpy(txtmp,buf+off,BLOCKSIZE-off);
			//len=len-(BLOCKSIZE-off);
			//openfilelist[fd].count+=(BLOCKSIZE-off);
			for(p=buf+off;p<buf+BLOCKSIZE;p++,txtmp++)
			{
				*txtmp=*p;
				len--;
				openfilelist[fd].count++;
			}
			off=0;
			//txtmp+=(BLOCKSIZE-off);
			bknum =fatptr->id;
			fatptr = fat1+bknum;
			bkptr=(unsigned char *)(myvhard+bknum*BLOCKSIZE);
			//strncpy(buf,bkptr,BLOCKSIZE);
			for(i=0;i<BLOCKSIZE;i++)
			{
				buf[i]=bkptr[i];
			}
		}
	}
	free(buf);
	return lentmp-len;
}

/**/
/*
原型声明:		int my_read(int fd,int len)
功能描述：		读文件函数
输入：
		fd		打开文件的id
		len		要读出字符的个数
输出：			返回实际读的字符的个数

函数功能实现算法描述：

*/
int my_read(int fd,int len)
{
	char text[MAX_TXT_SIZE];
	if(fd > MAX_OPEN_FILE)
	{
		printf("The File is not exist!\n");
		return -1;
	}
	openfilelist[curfd].count=0;
	if(do_read(fd,len,text)>0)
	{
		text[len]='\0';
		printf("%s\n",text);
	}
	else
	{
		printf("Read Error!\n");
		return -1;
	}
	return len;
}


/**/
/*
原型声明:		int do_write(int fd,char *text,int len,char wstyle)
功能描述：		实际写文件函数
输入：
		fd		当前打开的文件的id
		text	指向要写入的内容的指针
		len		本次要写入字节数
		wstyle	写方式
输出：			实际写入的字节数

函数功能实现算法描述：

*/
int do_write(int fd,char *text,int len,char wstyle)
{
	unsigned char *buf;
	unsigned short bknum;
	int off,tmplen=0,tmplen2=0,i,rwptr;
	unsigned char *bkptr,*p;
	char *txtmp,flag=0;
	fat *fat1,*fatptr;
    fat1=(fat *)(myvhard+BLOCKSIZE);
	txtmp=text;
	/*
	申请临时1024B的buffer
	*/
	buf=(unsigned char *)malloc(BLOCKSIZE);
	if(buf==NULL)
	{
		printf("malloc failed!\n");
		return -1;
	}

	rwptr = off = openfilelist[fd].count;
	bknum = openfilelist[fd].first;
	fatptr=fat1+bknum;
	while(off >= BLOCKSIZE )
	{
		off=off-BLOCKSIZE;
		bknum=fatptr->id;
		if(bknum == END)
		{
			bknum=fatptr->id=findFree();
			if(bknum==END) return -1;
			fatptr=fat1+bknum;
			fatptr->id=END;
		}
		fatptr=fat1+bknum;
	}

	fatptr->id=END;
	bkptr=(unsigned char *)(myvhard+bknum*BLOCKSIZE);
	while(tmplen<len)
	{
			for(i=0;i<BLOCKSIZE;i++)
			{
				buf[i]=0;
			}
		//if(off)
		//{
			for(i=0;i<BLOCKSIZE;i++)
			{
					buf[i]=bkptr[i];
					tmplen2++;
					if(tmplen2==openfilelist[curfd].length)
						break;
			}
		//}
		//else
		//{
			//for(i=0;i<BLOCKSIZE;i++)
			//{
			//	buf[i]=0;
			//}
		//}

		for(p=buf+off;p<buf+BLOCKSIZE;p++)
		{
			*p=*txtmp;
			tmplen++;
			txtmp++;
			off++;
			if(tmplen==len)
				break;
			/*if((*p)==NULL)
			{
				break;
			}*/
		}

		for(i=0;i<BLOCKSIZE;i++)
		{
				bkptr[i]=buf[i];
		}
		openfilelist[fd].count=rwptr+tmplen;
		if(off>=BLOCKSIZE)
		{
			off=off-BLOCKSIZE;
			bknum=fatptr->id;
			if(bknum==END)
			{
				bknum=fatptr->id=findFree();
				if(bknum==END) return -1;
				fatptr=fat1+bknum;
				fatptr->id=END;
			}
			fatptr=fat1+bknum;
			bkptr=(unsigned char *)(myvhard+bknum*BLOCKSIZE);
		}
	}
	free(buf);
	if(openfilelist[fd].count>openfilelist[fd].length)
	{
		openfilelist[fd].length=openfilelist[fd].count;
	}
	openfilelist[fd].fcbstate=1;
	return tmplen;
}

/**/
/*
原型声明:		unsigned short findFree()
功能描述：		寻找下一个空闲盘块
输入：			无
输出：			返回空闲盘块的id

函数功能实现算法描述：

*/
unsigned short findFree()
{
	unsigned short i;
	fat *fat1,*fatptr;

	fat1=(fat *)(myvhard+BLOCKSIZE);
	for(i=6; i<END; i++)
	{
		fatptr=fat1+i;
		if(fatptr->id == FREE)
		{
			return i;
		}
	}
	printf("Error,Can't find free block!\n");
	return END;
}

/*
原型声明:		int findFreeO()
功能描述：		寻找空闲文件表项
输入：			无
输出：			返回空闲文件表项的id

函数功能实现算法描述：

*/
int findFreeO()
{
	int i;
	for(i=0;i<MAX_OPEN_FILE;i++)
	{
		if(openfilelist[i].free==0)
		{
			return i;
		}
	}
	printf("Error,open too many files!\n");
	return -1;
}
/**/
/*
原型声明:		int my_write(int fd)
功能描述：		写文件函数
输入：
		fd		打开文件的id
输出：			返回实际写的长度

函数功能实现算法描述：

*/
int my_write(int fd)
{
	int wstyle=0,wlen=0;
	fat *fat1,*fatptr;
	unsigned short bknum;
	unsigned char *bkptr;
	char text[MAX_TXT_SIZE];
	fat1=(fat *)(myvhard+BLOCKSIZE);
	if(fd>MAX_OPEN_FILE)
	{
		printf("The file is not exist!\n");
		return -1;
	}
	while(wstyle<1||wstyle>3)
	{
		printf("Please enter the number of write style:\n1.cut write\t2.cover write\t3.add write\n");
		scanf("%d",&wstyle);
		getchar();
		switch(wstyle)
		{
		case 1://截断写
			{
				bknum=openfilelist[fd].first;
				fatptr=fat1+bknum;
				while(fatptr->id!=END)
				{
					bknum=fatptr->id;
					fatptr->id=FREE;
					fatptr=fat1+bknum;
				}
				fatptr->id=FREE;
				bknum=openfilelist[fd].first;
				fatptr=fat1+bknum;
				fatptr->id=END;
				openfilelist[fd].length=0;
				openfilelist[fd].count=0;
				break;
			}
		case 2://覆盖写
			{
				openfilelist[fd].count=0;
				break;
			}
		case 3://追加写
			{
				bknum=openfilelist[fd].first;
				fatptr=fat1+bknum;
				openfilelist[fd].count=0;
				while(fatptr->id!=END)
				{
					bknum=fatptr->id;
					fatptr=fat1+bknum;
					openfilelist[fd].count+=BLOCKSIZE;
				}
				bkptr=(unsigned char *)(myvhard+bknum*BLOCKSIZE);
				while(*bkptr !=0)
				{
					bkptr++;
					openfilelist[fd].count++;
				}
				break;
			}
		default:
			break;
		}
	}
	printf("please input write data:\n");
	gets(text);
	if(do_write(fd,text,strlen(text),wstyle)>0)
	{
		wlen+=strlen(text);
	}
	else
	{
		return -1;
	}
	if(openfilelist[fd].count>openfilelist[fd].length)
	{
		openfilelist[fd].length=openfilelist[fd].count;
	}
	openfilelist[fd].fcbstate=1;
	return wlen;
}


/*
原型声明:		void my_mkdir(char *dirname)
功能描述：		创建子目录函数,在当前目录下创建名为dirname的目录
输入：
		dirname		指向新建目录的名字的指针
输出：无

函数功能实现算法描述：

*/
void my_mkdir(char *dirname)
{
	fcb *dirfcb,*pcbtmp;
	int rbn,i,fd;
	unsigned short bknum;
	char text[MAX_TXT_SIZE],*p;
	time_t *now;
	struct tm *nowtime;
	/*
	将当前的文件信息读到text中
	rbn 是实际读取的字节数
	*/
	openfilelist[curfd].count=0;
	rbn = do_read(curfd,openfilelist[curfd].length,text);
	dirfcb=(fcb *)text;
	/*
	检测是否有相同的目录名
	*/
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(dirname,dirfcb->filename)==0)
		{
			printf("Error,the dirname is already exist!\n");
			return -1;
		}
		dirfcb++;
	}

	/*
	分配一个空闲的打开文件表项
	*/
	dirfcb=(fcb *)text;
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(dirfcb->filename,"")==0)
			break;
		dirfcb++;
	}
	openfilelist[curfd].count=i*sizeof(fcb);
	fd=findFreeO();
	if(fd<0)
	{
		return -1;
	}

	/*
	寻找空闲盘块
	*/
	bknum = findFree();
	if(bknum == END )
	{
		return -1;
	}

	pcbtmp=(fcb *)malloc(sizeof(fcb));
	now=(time_t *)malloc(sizeof(time_t));

	//在当前目录下新建目录项
	pcbtmp->attribute=0x30;
	time(now);
	nowtime=localtime(now);
	pcbtmp->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	pcbtmp->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	strcpy(pcbtmp->filename,dirname);
	strcpy(pcbtmp->exname,"di");
	pcbtmp->first=bknum;
	pcbtmp->length=2*sizeof(fcb);

	openfilelist[fd].attribute=pcbtmp->attribute;
	openfilelist[fd].count=0;
	openfilelist[fd].date=pcbtmp->date;
	strcpy(openfilelist[fd].dir[fd],openfilelist[curfd].dir[curfd]);

	p=openfilelist[fd].dir[fd];
	while(*p!='\0')
		p++;
	strcpy(p,dirname);
	while(*p!='\0') p++;
	*p='\\';p++;
	*p='\0';

	openfilelist[fd].dirno=openfilelist[curfd].first;
	openfilelist[fd].diroff=i;
	strcpy(openfilelist[fd].exname,pcbtmp->exname);
	strcpy(openfilelist[fd].filename,pcbtmp->filename);
	openfilelist[fd].fcbstate=1;
	openfilelist[fd].first=pcbtmp->first;
	openfilelist[fd].length=pcbtmp->length;
	openfilelist[fd].free=1;
	openfilelist[fd].time=pcbtmp->time;
	openfilelist[fd].topenfile=1;

	do_write(curfd,(char *)pcbtmp,sizeof(fcb),2);

	pcbtmp->attribute=0x28;
	time(now);
	nowtime=localtime(now);
	pcbtmp->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	pcbtmp->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	strcpy(pcbtmp->filename,".");
	strcpy(pcbtmp->exname,"di");
	pcbtmp->first=bknum;
	pcbtmp->length=2*sizeof(fcb);

	do_write(fd,(char *)pcbtmp,sizeof(fcb),2);

	pcbtmp->attribute=0x28;
	time(now);
	nowtime=localtime(now);
	pcbtmp->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	pcbtmp->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	strcpy(pcbtmp->filename,"..");
	strcpy(pcbtmp->exname,"di");
	pcbtmp->first=openfilelist[curfd].first;
	pcbtmp->length=openfilelist[curfd].length;

	do_write(fd,(char *)pcbtmp,sizeof(fcb),2);

	openfilelist[curfd].count=0;
	do_read(curfd,openfilelist[curfd].length,text);

	pcbtmp=(fcb *)text;
	pcbtmp->length=openfilelist[curfd].length;
	my_close(fd);

	openfilelist[curfd].count=0;
	do_write(curfd,text,pcbtmp->length,2);
}

/**/
/*
原型声明:		void my_ls()
功能描述：		显示目录函数
输入：			无
输出：			无

函数功能实现算法描述：

*/
void my_ls()
{
	fcb *fcbptr;
	int i;
	char text[MAX_TXT_SIZE];
	unsigned short bknum;
	openfilelist[curfd].count=0;
	do_read(curfd,openfilelist[curfd].length,text);
	fcbptr=(fcb *)text;
	for(i=0;i<(int)(openfilelist[curfd].length/sizeof(fcb));i++)
	{
		if(fcbptr->filename[0]!='\0')
		{
			if(fcbptr->attribute&0x20)
			{
				printf("%s\\\t\t<DIR>\t\t%d/%d/%d\t%02d:%02d:%02d\n",fcbptr->filename,((fcbptr->date)>>9)+1980,((fcbptr->date)>>5)&0x000f,(fcbptr->date)&0x001f,fcbptr->time>>11,(fcbptr->time>>5)&0x003f,fcbptr->time&0x001f*2);
			}
			else
			{
				printf("%s.%s\t\t%dB\t\t%d/%d/%d\t%02d:%02d:%02d\t\n",fcbptr->filename,fcbptr->exname,fcbptr->length,((fcbptr->date)>>9)+1980,(fcbptr->date>>5)&0x000f,fcbptr->date&0x1f,fcbptr->time>>11,(fcbptr->time>>5)&0x3f,fcbptr->time&0x1f*2);
			}
		}
		fcbptr++;
	}
	openfilelist[curfd].count=0;
}

/**/
/*
原型声明:		void my_rmdir(char *dirname)
功能描述：		删除子目录函数
输入：
		dirname		指向新建目录的名字的指针
输出：无

函数功能实现算法描述：

*/
void my_rmdir(char *dirname)
{
	int rbn,fd;
	char text[MAX_TXT_SIZE];
	fcb *fcbptr,*fcbtmp,*fcbtmp2;
	unsigned short bknum;
	int i,j;
	fat *fat1,*fatptr;

	if(strcmp(dirname,".")==0 || strcmp(dirname,"..")==0)
	{
		printf("Error,can't remove this directory.\n");
		return -1;
	}
	fat1=(fat *)(myvhard+BLOCKSIZE);
	openfilelist[curfd].count=0;
	rbn=do_read(curfd,openfilelist[curfd].length,text);
	fcbptr=(fcb *)text;
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(dirname,fcbptr->filename)==0)
		{
			break;
		}
		fcbptr++;
	}
	if(i >= rbn/sizeof(fcb))
	{
		printf("Error,the directory is not exist.\n");
		return -1;
	}

	bknum=fcbptr->first;
	fcbtmp2=fcbtmp=(fcb *)(myvhard+bknum*BLOCKSIZE);
	for(j=0;j<fcbtmp->length/sizeof(fcb);j++)
	{
		if(strcmp(fcbtmp2->filename,".") && strcmp(fcbtmp2->filename,"..") && fcbtmp2->filename[0]!='\0')
		{
			printf("Error,the directory is not empty.\n");
			return -1;
		}
		fcbtmp2++;
	}

	while(bknum!=END)
	{
		fatptr=fat1+bknum;
		bknum=fatptr->id;
		fatptr->id=FREE;
	}

	strcpy(fcbptr->filename,"");
	strcpy(fcbptr->exname,"");
	fcbptr->first=END;
	openfilelist[curfd].count=0;
	do_write(curfd,text,openfilelist[curfd].length,2);

}


/*
原型声明:		int my_open(char *filename)
功能描述：		打开文件函数
输入：
		filename		指向要打开的文件的名字的指针
输出：			返回打开的文件的id

函数功能实现算法描述：

*/
int my_open(char *filename)
{
	int i,fd,rbn;
	char text[MAX_TXT_SIZE],*p,*fname,*exname;
	fcb *fcbptr;
	char exnamed=0;
	fname=strtok(filename,".");
	exname=strtok(NULL,".");
	if(!exname)
	{
		exname=(char *)malloc(3);
		strcpy(exname,"di");
		exnamed=1;
	}
	for(i=0;i<MAX_OPEN_FILE;i++)
	{
		if(strcmp(openfilelist[i].filename,filename)==0 &&strcmp(openfilelist[i].exname,exname)==0&& i!=curfd)
		{
			printf("Error,the file is already open.\n");
			return -1;
		}
	}
	openfilelist[curfd].count=0;
	rbn=do_read(curfd,openfilelist[curfd].length,text);
	fcbptr=(fcb *)text;

	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(filename,fcbptr->filename)==0 && strcmp(fcbptr->exname,exname)==0)
		{
				break;
		}
		fcbptr++;
	}
	if(i>=rbn/sizeof(fcb))
	{
		printf("Error,the file is not exist.\n");
		return curfd;
	}

	if(exnamed)
	{
		free(exname);
	}

	fd=findFreeO();
	if(fd==-1)
	{
		return -1;
	}

	strcpy(openfilelist[fd].filename,fcbptr->filename);
	strcpy(openfilelist[fd].exname,fcbptr->exname);
	openfilelist[fd].attribute=fcbptr->attribute;
	openfilelist[fd].count=0;
	openfilelist[fd].date=fcbptr->date;
	openfilelist[fd].first=fcbptr->first;
	openfilelist[fd].length=fcbptr->length;
	openfilelist[fd].time=fcbptr->time;
	openfilelist[fd].father=curfd;
	openfilelist[fd].dirno=openfilelist[curfd].first;
	openfilelist[fd].diroff=i;
	openfilelist[fd].fcbstate=0;
	openfilelist[fd].free=1;
	openfilelist[fd].topenfile=1;
	strcpy(openfilelist[fd].dir[fd],openfilelist[curfd].dir[curfd]);
	p=openfilelist[fd].dir[fd];
	while(*p!='\0')
		p++;
	strcpy(p,filename);
	while(*p!='\0') p++;
	if(openfilelist[fd].attribute&0x20)
	{
		*p='\\';p++;	*p='\0';

	}
	else
	{
		*p='.';p++;
		strcpy(p,openfilelist[fd].exname);
	}

	return fd;
}

/**/
/*
原型声明:		int my_close(int fd)
功能描述：		关闭文件函数
输入：
		fd		打开文件的id
输出：			返回fd的father的id

函数功能实现算法描述：

*/
int my_close(int fd)
{
	fcb *fafcb;
	char text[MAX_TXT_SIZE];
	int fa;
	/*
	检查fd的有效性
	*/
	if(fd > MAX_OPEN_FILE || fd<=0)
	{
		printf("Error,the file is not exist.\n");
		return -1;
	}

	fa=openfilelist[fd].father;
	if(openfilelist[fd].fcbstate)
	{
		fa=openfilelist[fd].father;
		openfilelist[fa].count=0;
		do_read(fa,openfilelist[fa].length,text);

		fafcb=(fcb *)(text+sizeof(fcb)*openfilelist[fd].diroff);
		fafcb->attribute=openfilelist[fd].attribute;
		fafcb->date=openfilelist[fd].date;
		fafcb->first=openfilelist[fd].first;
		fafcb->length=openfilelist[fd].length;
		fafcb->time=openfilelist[fd].time;
		strcpy(fafcb->filename,openfilelist[fd].filename);
		strcpy(fafcb->exname,openfilelist[fd].exname);

		openfilelist[fa].count=0;
		do_write(fa,text,openfilelist[fa].length,2);

	}
	openfilelist[fd].attribute=0;
	openfilelist[fd].count=0;
	openfilelist[fd].date=0;
	strcpy(openfilelist[fd].dir[fd],"");
	strcpy(openfilelist[fd].filename,"");
	strcpy(openfilelist[fd].exname,"");
	openfilelist[fd].length=0;
	openfilelist[fd].time=0;
	openfilelist[fd].free=0;
	openfilelist[fd].topenfile=0;

	return fa;
}

/**/
/*
原型声明:		void my_cd(char *dirname)
功能描述：		更改当前目录函数
输入：
	dirname		指向目录名的指针
输出：			无

函数功能实现算法描述：

*/
void my_cd(char *dirname)
{
	char *p,text[MAX_TXT_SIZE];
	int fd,i;
	p=strtok(dirname,"\\");
	if(strcmp(p,".")==0)
		return;
	if(strcmp(p,"..")==0)
	{
		fd=openfilelist[curfd].father;
		my_close(curfd);
		curfd=fd;
		ptrcurdir=&openfilelist[curfd];
		return;
	}
	if(strcmp(p,"root")==0)
	{
		for(i=1;i<MAX_OPEN_FILE;i++)
		{
			if(openfilelist[i].free)
				my_close(i);
		}
		ptrcurdir=&openfilelist[0];
		curfd=0;
		p=strtok(NULL,"\\");
	}

	while(p)
	{
		fd=my_open(p);
		if(fd>0)
		{
			ptrcurdir=&openfilelist[fd];
			curfd=fd;
		}
		else
			return -1;
		p=strtok(NULL,"\\");
	}
}

/**/
/*
原型声明:		void my_create(char *filename)
功能描述：		创建文件函数
输入：
		filename	指向文件名的指针
输出：			无

函数功能实现算法描述：

*/
void my_create(char *filename)
{
	char *fname,*exname,text[MAX_TXT_SIZE];
	int fd,rbn,i;
	fcb *filefcb,*fcbtmp;
	time_t *now;
	struct tm *nowtime;
	unsigned short bknum;
	fat *fat1,*fatptr;

	fat1=(fat *)(myvhard+BLOCKSIZE);
	fname=strtok(filename,".");
	exname=strtok(NULL,".");
	if(strcmp(fname,"")==0)
	{
		printf("Error,creating file must have a right name.\n");
		return -1;
	}
	if(!exname)
	{
		printf("Error,creating file must have a extern name.\n");
		return -1;
	}

	openfilelist[curfd].count=0;
	rbn=do_read(curfd,openfilelist[curfd].length,text);
	filefcb=(fcb *)text;
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(fname,filefcb->filename)==0 && strcmp(exname,filefcb->exname)==0)
		{
			printf("Error,the filename is already exist!\n");
			return -1;
		}
		filefcb++;
	}

	filefcb=(fcb *)text;
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(filefcb->filename,"")==0)
			break;
		filefcb++;
	}
	openfilelist[curfd].count=i*sizeof(fcb);

	bknum=findFree();
	if(bknum==END)
	{
		return -1;
	}
	fcbtmp=(fcb *)malloc(sizeof(fcb));
	now=(time_t *)malloc(sizeof(time_t));

	fcbtmp->attribute=0x00;
	time(now);
	nowtime=localtime(now);
	fcbtmp->time=nowtime->tm_hour*2048+nowtime->tm_min*32+nowtime->tm_sec/2;
	fcbtmp->date=(nowtime->tm_year-80)*512+(nowtime->tm_mon+1)*32+nowtime->tm_mday;
	strcpy(fcbtmp->filename,fname);
	strcpy(fcbtmp->exname,exname);
	fcbtmp->first=bknum;
	fcbtmp->length=0;

	do_write(curfd,(char *)fcbtmp,sizeof(fcb),2);
	free(fcbtmp);
	free(now);
	openfilelist[curfd].count=0;
	do_read(curfd,openfilelist[curfd].length,text);
	fcbtmp=(fcb *)text;
	fcbtmp->length=openfilelist[curfd].length;
	openfilelist[curfd].count=0;
	do_write(curfd,text,openfilelist[curfd].length,2);
	openfilelist[curfd].fcbstate=1;
	fatptr=(fat *)(fat1+bknum);
	fatptr->id=END;
}

/**/
/*
原型声明:		void my_rm(char *filename)
功能描述：		删除文件函数
输入：
		filename	指向文件名的指针
输出：			无

函数功能实现算法描述：

*/
void my_rm(char *filename)
{
	char *fname,*exname;
	char text[MAX_TXT_SIZE];
	fcb *fcbptr;
	int i,rbn;
	unsigned short bknum;
	fat *fat1,*fatptr;

	fat1=(fat *)(myvhard+BLOCKSIZE);
	fname=strtok(filename,".");
	exname=strtok(NULL,".");
	if(!fname || strcmp(fname,"")==0)
	{
		printf("Error,removing file must have a right name.\n");
		return -1;
	}
	if(!exname)
	{
		printf("Error,removing file must have a extern name.\n");
		return -1;
	}
	openfilelist[curfd].count=0;
	rbn=do_read(curfd,openfilelist[curfd].length,text);
	fcbptr=(fcb *)text;
	for(i=0;i<rbn/sizeof(fcb);i++)
	{
		if(strcmp(fname,fcbptr->filename)==0 && strcmp(exname,fcbptr->exname)==0)
		{
			break;
		}
		fcbptr++;
	}
	if(i>=rbn/sizeof(fcb))
	{
		printf("Error,the file is not exist.\n");
		return -1;
	}

	bknum=fcbptr->first;
	while(bknum!=END)
	{
		fatptr=fat1+bknum;
		bknum=fatptr->id;
		fatptr->id=FREE;
	}

	strcpy(fcbptr->filename,"");
	strcpy(fcbptr->exname,"");
	fcbptr->first=END;
	openfilelist[curfd].count=0;
	do_write(curfd,text,openfilelist[curfd].length,2);
}
/*主函数*/
void main()
{
	char cmd[15][10]={"mkdir","rmdir","ls","cd","create","rm","open","close","write","read","exit"};
	char s[30],*sp;
	int cmdn,i;
	startsys();
	printf("*********************File System V1.0*******************************\n\n");
	printf("命令名\t\t命令参数\t\t命令说明\n\n");
	printf("cd\t\t目录名(路径名)\t\t切换当前目录到指定目录\n");
	printf("mkdir\t\t目录名\t\t\t在当前目录创建新目录\n");
	printf("rmdir\t\t目录名\t\t\t在当前目录删除指定目录\n");
	printf("ls\t\t无\t\t\t显示当前目录下的目录和文件\n");
	printf("create\t\t文件名\t\t\t在当前目录下创建指定文件\n");
	printf("rm\t\t文件名\t\t\t在当前目录下删除指定文件\n");
	printf("open\t\t文件名\t\t\t在当前目录下打开指定文件\n");
	printf("write\t\t无\t\t\t在打开文件状态下，写该文件\n");
	printf("read\t\t无\t\t\t在打开文件状态下，读取该文件\n");
	printf("close\t\t无\t\t\t在打开文件状态下，读取该文件\n");
	printf("exit\t\t无\t\t\t退出系统\n\n");
	printf("*********************************************************************\n\n");
	while(1)
	{
		printf("%s>",openfilelist[curfd].dir[curfd]);
		gets(s);
		cmdn=-1;
		if(strcmp(s,""))
		{
			sp=strtok(s," ");
			for(i=0;i<15;i++)
			{
				if(strcmp(sp,cmd[i])==0)
				{
					cmdn=i;
					break;
				}
			}
			switch(cmdn)
			{
			case 0:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
					my_mkdir(sp);
				else
					printf("Please input the right command.\n");
				break;
			case 1:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
					my_rmdir(sp);
				else
					printf("Please input the right command.\n");
				break;
			case 2:
				if(openfilelist[curfd].attribute&0x20)
				{
					my_ls();
					putchar('\n');
				}
				else
					printf("Please input the right command.\n");
				break;
			case 3:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
					my_cd(sp);
				else
					printf("Please input the right command.\n");
				break;
			case 4:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
					my_create(sp);
				else
					printf("Please input the right command.\n");
				break;
			case 5:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
					my_rm(sp);
				else
					printf("Please input the right command.\n");
				break;
			case 6:
				sp=strtok(NULL," ");
				if(sp && openfilelist[curfd].attribute&0x20)
				{
					if(strchr(sp,'.'))
						curfd=my_open(sp);
					else
						printf("the openfile should have exname.\n");
				}
				else
                    printf("Please input the right command.\n");
				break;
			case 7:
				if(!(openfilelist[curfd].attribute&0x20))
					curfd=my_close(curfd);
				else
					printf("No files opened.\n");
				break;
			case 8:
				if(!(openfilelist[curfd].attribute&0x20))
					my_write(curfd);
				else
					printf("No files opened.\n");
				break;
			case 9:
				if(!(openfilelist[curfd].attribute&0x20))
					my_read(curfd,openfilelist[curfd].length);
				else
					printf("No files opened.\n");
				break;
			case 10:
				if(openfilelist[curfd].attribute&0x20)
				{
					my_exitsys();
					return ;
				}
				else
					printf("Please input the right command.\n");
				break;
			default:
				printf("Please input the right command.\n");
				break;
			}
		}
	}
}
