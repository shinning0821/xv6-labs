#include "kernel/types.h"
#include "user.h"
#include <stddef.h>

void primes(int *p){
    int current_number = 0;
    int next_number = 0;
    int fd[2];
    int ret = pipe(fd);
    if(ret < 0){
        printf("Pipe error!\n");
        exit(-1);
    }

    if (read(p[0], &current_number, sizeof(int))){  
        printf("prime %d\n", current_number);
        
        if(fork() > 0){
            /* 父进程 */
            close(fd[0]);
            while(read(p[0], &next_number, sizeof(int))){ // 一直到读取管道完毕为止
                if (next_number % current_number != 0 ){
                    // 写入管道
                    write(fd[1], &next_number, sizeof(int));
                }
            }
            close(fd[1]);
            close(p[0]);
            wait(0);
            exit(0);
        }
        else{
            /* 子进程 */
            close(fd[1]);
            close(p[0]);
            primes(fd);
            close(fd[0]);
            exit(0);
        }
    }
}
int main(int argc,char* argv[]){    //  用递归的方式实现
    if(argc != 1){
        printf("Sleep needs one argument!\n"); //检查参数数量是否正确
        exit(-1);
    }

    int pl[2];
    int ret = pipe(pl);
    if(ret < 0){
        printf("Pipe error!\n");
        exit(-1);
    }
    /* 父进程 */
    for (int i = 2; i < 36; i++){
    // 将其写入管道
        write(pl[1], &i, sizeof(int));
    }
    close(pl[1]);
    primes(pl);
    close(pl[0]);    
    exit(0);
}


