#include "kernel/types.h"
#include "user.h"
#define MAXN 1024

int main(int argc, char *argv[])
{
  char buf[MAXN] = {"\0"};
  char* full_argv[MAXN];
  if(argc < 2){
    printf("too little args!");
    exit(1);
  }
  // 略去 xargs ，用来保存命令行参数
  for (int i = 1; i < argc; ++i) {
      full_argv[i-1] = argv[i];
  }
  
  int n;
	// 也就是从管道循环读取数据
  while((n = read(0, buf, MAXN)) > 0 ) {
        // 临时缓冲区存放追加的参数
		char temp[MAXN] = {"\0"};
      // xargs 命令的参数后面再追加参数
    full_argv[argc-1] = temp;
    // 内循环获取追加的参数并创建子进程执行命令
    for(int i = 0; i < strlen(buf); ++i) {
        // 读取单个输入行，当遇到换行符时，创建子线程
        if(buf[i] == '\n') {
            // 创建子线程执行命令
            if (fork() == 0) {
                exec(argv[1], full_argv);
            }
            // 等待子线程执行完毕
            wait(0);
        } else {
            // 否则，读取管道的输出作为输入
            temp[i] = buf[i];
        }
        }
    }
  exit(0);
}
