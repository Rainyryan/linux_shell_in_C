#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

int main(){
    char *ps[] = {"ps", "-o", "pid=", NULL};
    char *tail[] = {"tail", "-1", NULL};
    char **args1 = ps, **args2 = tail;
    pid_t pid, pid_1;
    int fd[2];
    if((pid = fork()) == -1)
        perror("fork"),exit(1);
    else if(pid == 0){
        if(pipe(fd) == -1)
            perror("pipe"), exit(1);
        else if((pid_1 = fork()) == -1)
            perror("fork"), exit(1);
        else if(pid_1 == 0){ // child-child
            close(fd[0]);
            dup2(fd[1], 1);
            close(fd[1]);
            execvp(*args1, args1);
            perror("exec"), exit(1);
        }else{ // child
            wait(NULL);
            close(fd[1]);
            dup2(fd[0], 0);
            close(fd[0]);
            execvp(*args2, args2);
            perror("exec"), exit(1);
        }
    }else{ // parent
        wait(NULL);
    }
}