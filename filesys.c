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
	char filename[8];   /*8B�ļ���*/
	char exname[3];		/*3B��չ��*/
	unsigned char attribute;		/*�ļ������ֶ�*/
	char retainbyte[10] ;			/*10B������*/
	unsigned short time;			/*�ļ�����ʱ��*/
	unsigned short date;			/*�ļ���������*/
	unsigned short first;			/*�׿��*/
	unsigned long length;		/*�ļ���С*/
}fcb;

/*
�ļ������ file allocation table
*/
typedef struct FAT
{
	unsigned short id;
}fat;

/*
���Զ��ձ�
λ      7    6      5       4     3     2        1     0
����  ����  ����  �浵  ��Ŀ¼  ���  ϵͳ�ļ�  ����  ֻ��
*/
typedef struct USEROPEN
{
	char filename[8];   /*8B�ļ���*/
	char exname[3];		/*3B��չ��*/
	unsigned char attribute;		/*�ļ������ֶ�*/
	char retainbyte[10] ;			/*10B������*/
	unsigned short time;			/*�ļ�����ʱ��*/
	unsigned short date;			/*�ļ���������*/
	unsigned short first;			/*�׿��*/
	unsigned long length;	/*�ļ���С*/

	char free; /*��ʾĿ¼���Ƿ�Ϊ�� 0��1����*/
	int father;		/*��Ŀ¼���ļ�������*/
	int dirno;		/*��Ӧ���ļ���Ŀ¼���ڸ�Ŀ¼�ļ��е��̿��*/
	int diroff;	/*��Ӧ���ļ���Ŀ¼���ڸ�Ŀ¼�ļ���dirno�̿��е�Ŀ¼�����*/
	char dir[MAX_OPEN_FILE][80];	/*��Ӧ���ļ����ڵ�Ŀ¼*/
	int count;	/*��дָ�����ļ��е�λ��*/
	char fcbstate;		/*�Ƿ��޸����ļ���FCB���ݣ��޸���1������Ϊ0*/
	char topenfile;	/*��ʾ���û��򿪵ı����Ƿ�Ϊ�գ���ֵΪ0����ʾΪ��*/
}useropen;

typedef struct BLOCK0
{
	unsigned short fbnum;
	char information[200];
	unsigned short root;
	unsigned char *startblock;
}block0;

/*ȫ�ֱ�������*/

unsigned char *myvhard;		/*ָ��������̵���ʼ��ַ*/
useropen openfilelist[MAX_OPEN_FILE];	/*�û����ļ�������*/
useropen *ptrcurdir;		/*ָ���û����ļ����еĵ�ǰĿ¼���ڴ��ļ������λ��*/
int curfd;
char currentdir[80];		/*��¼��ǰĿ¼���ļ���*/
unsigned char *startp;		/*��¼�����������������ʼλ��*/
char filename[]="c:\\myfilesys";   /*����ռ䱣��·��*/
unsigned char buffer[SIZE];

/*��������*/
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


/*�������*/

/*�ļ�ϵͳ��ʼ��*/
/*
ԭ������:		void startsys()
����������		�ļ�ϵͳ��ʼ��,��ʼ�����������ļ�ϵͳ
���룺��
�������

��������ʵ���㷨������
	1��������̿ռ�
	2����ϵͳ���̣��������ڣ������µ�ϵͳ���̣�����ʽ��
	3����ʼ���û����ļ���������0�������Ŀ¼�ļ�ʹ��
		����д��Ŀ¼�ļ��������Ϣ
	4����ptrcurdirָ����û����ļ�����
	5������ǰĿ¼����Ϊ��Ŀ¼
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
ԭ������:		void my_format()
����������		��������̽��и�ʽ��������������̣�������Ŀ¼�ļ�
���룺��
�������

��������ʵ���㷨������
	������̿ռ䲼��
		1��		2��	   2��	995��
		������	FAT1  FAT2 	������
	�������һ�����ֳ�1000�����̿�
	ÿ��1024���ֽڣ����̿ռ䲼������
	���������ĵ�һ�飨��������̵ĵ�6�飩�������Ŀ¼�ļ�
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
	������
	*/
	b0->fbnum=0xaaaa;	/*�ļ�ϵͳħ�� 10101010*/
	b0->root = 5;
	strcpy(b0->information,"My FileSystem Ver 1.0 \n Blocksize=1KB Whole size=1000KB Blocknum=1000 RootBlocknum=2\n");
	/*
	FAT1,FAT2
		ǰ��������̿��ѷ��䣬���ΪEND
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
	���������ı��Ϊ����״̬
	*/
	for(i=7;i<SIZE/BLOCKSIZE;i++)
	{
		(*fat1).id=FREE;
		(*fat2).id=FREE;
		fat1++;
		fat2++;
	}
	/*
	������Ŀ¼�ļ�root�����������ĵ�һ��������Ŀ¼��
	�ڸ������ϴ������������Ŀ¼�".",".."��
	�����ļ���֮�⣬��������ͬ
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
ԭ������:		void my_exitsys()
����������		�˳��ļ�ϵͳ
���룺			��
�����			��

��������ʵ���㷨������

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
ԭ������:		int do_read(int fd,int len,char *text)
����������		ʵ�ʶ��ļ�����������ָ���ļ��д�ָ��ָ�뿪ʼ�ĳ���
				Ϊlen�����ݵ��û��ռ��text��
���룺
		fd		open���������ķ���ֵ���ļ���������
		len		Ҫ����ļ��ж������ֽ���
		text	ָ���Ŷ������ݵ��û�����ַ
�����
	ʵ�ʶ������ֽ���
��������ʵ���㷨������

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
	����1024B�ռ���Ϊ������buffer
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
ԭ������:		int my_read(int fd,int len)
����������		���ļ�����
���룺
		fd		���ļ���id
		len		Ҫ�����ַ��ĸ���
�����			����ʵ�ʶ����ַ��ĸ���

��������ʵ���㷨������

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
ԭ������:		int do_write(int fd,char *text,int len,char wstyle)
����������		ʵ��д�ļ�����
���룺
		fd		��ǰ�򿪵��ļ���id
		text	ָ��Ҫд������ݵ�ָ��
		len		����Ҫд���ֽ���
		wstyle	д��ʽ
�����			ʵ��д����ֽ���

��������ʵ���㷨������

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
	������ʱ1024B��buffer
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
ԭ������:		unsigned short findFree()
����������		Ѱ����һ�������̿�
���룺			��
�����			���ؿ����̿��id

��������ʵ���㷨������

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
ԭ������:		int findFreeO()
����������		Ѱ�ҿ����ļ�����
���룺			��
�����			���ؿ����ļ������id

��������ʵ���㷨������

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
ԭ������:		int my_write(int fd)
����������		д�ļ�����
���룺
		fd		���ļ���id
�����			����ʵ��д�ĳ���

��������ʵ���㷨������

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
		case 1://�ض�д
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
		case 2://����д
			{
				openfilelist[fd].count=0;
				break;
			}
		case 3://׷��д
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
ԭ������:		void my_mkdir(char *dirname)
����������		������Ŀ¼����,�ڵ�ǰĿ¼�´�����Ϊdirname��Ŀ¼
���룺
		dirname		ָ���½�Ŀ¼�����ֵ�ָ��
�������

��������ʵ���㷨������

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
	����ǰ���ļ���Ϣ����text��
	rbn ��ʵ�ʶ�ȡ���ֽ���
	*/
	openfilelist[curfd].count=0;
	rbn = do_read(curfd,openfilelist[curfd].length,text);
	dirfcb=(fcb *)text;
	/*
	����Ƿ�����ͬ��Ŀ¼��
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
	����һ�����еĴ��ļ�����
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
	Ѱ�ҿ����̿�
	*/
	bknum = findFree();
	if(bknum == END )
	{
		return -1;
	}

	pcbtmp=(fcb *)malloc(sizeof(fcb));
	now=(time_t *)malloc(sizeof(time_t));

	//�ڵ�ǰĿ¼���½�Ŀ¼��
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
ԭ������:		void my_ls()
����������		��ʾĿ¼����
���룺			��
�����			��

��������ʵ���㷨������

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
ԭ������:		void my_rmdir(char *dirname)
����������		ɾ����Ŀ¼����
���룺
		dirname		ָ���½�Ŀ¼�����ֵ�ָ��
�������

��������ʵ���㷨������

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
ԭ������:		int my_open(char *filename)
����������		���ļ�����
���룺
		filename		ָ��Ҫ�򿪵��ļ������ֵ�ָ��
�����			���ش򿪵��ļ���id

��������ʵ���㷨������

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
ԭ������:		int my_close(int fd)
����������		�ر��ļ�����
���룺
		fd		���ļ���id
�����			����fd��father��id

��������ʵ���㷨������

*/
int my_close(int fd)
{
	fcb *fafcb;
	char text[MAX_TXT_SIZE];
	int fa;
	/*
	���fd����Ч��
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
ԭ������:		void my_cd(char *dirname)
����������		���ĵ�ǰĿ¼����
���룺
	dirname		ָ��Ŀ¼����ָ��
�����			��

��������ʵ���㷨������

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
ԭ������:		void my_create(char *filename)
����������		�����ļ�����
���룺
		filename	ָ���ļ�����ָ��
�����			��

��������ʵ���㷨������

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
ԭ������:		void my_rm(char *filename)
����������		ɾ���ļ�����
���룺
		filename	ָ���ļ�����ָ��
�����			��

��������ʵ���㷨������

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
/*������*/
void main()
{
	char cmd[15][10]={"mkdir","rmdir","ls","cd","create","rm","open","close","write","read","exit"};
	char s[30],*sp;
	int cmdn,i;
	startsys();
	printf("*********************File System V1.0*******************************\n\n");
	printf("������\t\t�������\t\t����˵��\n\n");
	printf("cd\t\tĿ¼��(·����)\t\t�л���ǰĿ¼��ָ��Ŀ¼\n");
	printf("mkdir\t\tĿ¼��\t\t\t�ڵ�ǰĿ¼������Ŀ¼\n");
	printf("rmdir\t\tĿ¼��\t\t\t�ڵ�ǰĿ¼ɾ��ָ��Ŀ¼\n");
	printf("ls\t\t��\t\t\t��ʾ��ǰĿ¼�µ�Ŀ¼���ļ�\n");
	printf("create\t\t�ļ���\t\t\t�ڵ�ǰĿ¼�´���ָ���ļ�\n");
	printf("rm\t\t�ļ���\t\t\t�ڵ�ǰĿ¼��ɾ��ָ���ļ�\n");
	printf("open\t\t�ļ���\t\t\t�ڵ�ǰĿ¼�´�ָ���ļ�\n");
	printf("write\t\t��\t\t\t�ڴ��ļ�״̬�£�д���ļ�\n");
	printf("read\t\t��\t\t\t�ڴ��ļ�״̬�£���ȡ���ļ�\n");
	printf("close\t\t��\t\t\t�ڴ��ļ�״̬�£���ȡ���ļ�\n");
	printf("exit\t\t��\t\t\t�˳�ϵͳ\n\n");
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
