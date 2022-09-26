#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path) //参考ls中的fmtname代码
{
	static char buf[DIRSIZ+1];
 	char *p;
  	// Find first character after last slash.
  	for(p=path+strlen(path); p >= path && *p != '/'; p--)
    	;
  	p++;
  	// Return blank-padded name.
  	if(strlen(p) >= DIRSIZ)
    	return p;
  	memmove(buf, p, strlen(p));
	buf[strlen(p)] = 0;  //字符串结束符
  	return buf;
}

void find(char *path, char *name)
{
 	char buf[512], *p;  //声明的这些结构体、变量等也和ls一样
 	int fd;
 	struct dirent de;
 	struct stat st;

    if((fd = open(path, 0)) < 0){ //判断，也和ls一样
	    fprintf(2, "find open %s error\n", path);
	    exit(1);
	}
	if(fstat(fd, &st) < 0){
	    fprintf(2, "fstat %s error\n", path);
	    close(fd);
	    exit(1);
	}

	switch(st.type){
	    case T_FILE:  //如果是文件类型
	 	    if(!strcmp(fmtname(path), name))  //找到文件 
	     	    printf("%s\n", path); 
	  	        break;
	    case T_DIR:  //如果是目录类型且超出缓冲区
	 	    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
	        	printf("find: path too long\n");
	            break;
	   	}
	   	strcpy(buf, path); //将输入的目录字符串复制到buf中
	   	p = buf+strlen(buf);
	   	*p++ = '/';  //将`/`拼接在后面
	   	//读取 fd ，如果 read 返回字节数与 de 长度相等则循环
	   	while(read(fd, &de, sizeof(de)) == sizeof(de)){
	        if(de.inum == 0)
	            continue;
	        memmove(p, de.name, DIRSIZ);
	        p[DIRSIZ] = 0;
	        //不去递归处理.和..
	        if(!strcmp(de.name, ".") || !strcmp(de.name, ".."))
	            continue;
	        find(buf, name); //继续进入下一层目录递归处理
	   }
	   break;
	 }
	 close(fd);
}

int main(int argc, char *argv[])
{
  if(argc != 3){
    printf("Find needs two argument!\n"); //检查参数数量是否正确
    exit(1);
  }
    find(argv[1], argv[2]);
    exit(0);
}
