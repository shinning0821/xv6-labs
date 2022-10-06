#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){
    if(argc != 1){
        printf("Sleep needs one argument!\n"); //检查参数数量是否正确
        exit(-1);
    }
    int p[2];
    int ret = pipe(p);
    if(ret < 0){
        printf("Pipe error!\n");
        ret++;
        exit(-1);
    }

    int pid = fork();
    if(pid<0) 
	{
		printf("Fork error\n");
		exit(-1);
	}

    else if(pid > 0){ 
        /* 父进程 */
        write(p[1],"ping",strlen("ping"));
        close(p[1]);
        wait(0);

        char buff[10];
        read(p[0], buff, sizeof(buff));
        close(p[0]);
        if(strcmp(buff,"pong")==0){
            printf("%d: received pong\n",getpid());
        }
        exit(0);
    }
    else if(pid == 0){
        /* 子进程 */
        char buff[10];
        read(p[0], buff, sizeof(buff));
        if(strcmp(buff,"ping")==0){
            printf("%d: received ping\n",getpid());
        }
        close(p[0]);
        
        write(p[1],"pong",strlen("pong"));
        close(p[1]);
    }
    
    exit(0); //确保进程退出
}