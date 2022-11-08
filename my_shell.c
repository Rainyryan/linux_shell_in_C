#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>

#define clear() printf("\033[H\033[J")
#define MAX_PIPED 20 // max number of piped commands
#define MAX_ARGS 40 // max number of arguments for a command
#define MAX_IN_LEN 500 // max input length

static int exec(char ***parsed, int fd_in, int fd_out);
void print(char ***parsed);

char **history;
int idx;

void pwd(){
    char cwd[200];
    getcwd(cwd, sizeof(cwd));
    printf("%s", cwd);
}

void initialize(){
    printf("\033[H\033[J"); // clear the terminal
    printf("Welcome! *(˃^_^˂)* \n");
}

int read_input(char *str){
    char *buffer;
    size_t insize = MAX_IN_LEN;
    printf("*(˃^_^˂)*>>>$");
    int len = getline(&buffer, &insize, stdin);
    buffer[len-1] = '\0';
	if(strlen(buffer) != 0) {
		strcpy(str, buffer);
		return 0;
	}else
		return 1;
}

void parse_spaces(char *in, char **out){
    *out = strtok(in, " ");
    while(*out){
        out++;
        *out = strtok(NULL, " ");
    }
}

void parse_pipes(char *in, char **out){
    *out = strtok(in, "|");
    while(*out){
        out++;
        *out = strtok(NULL, "|");
    }
}

void parse_input(char *in, char **piped, char ***parsed){
    parse_pipes(in, piped); // parsing
    while(*piped){
        parse_spaces(*piped, *parsed);
        piped++;
        parsed++;
    }
    *parsed = NULL; // this line took me 5 hours to debug and add
}

int mypid(char *parsed[]){
    char *cmd1 = *(parsed+1), *cmd2 = *(parsed+2);
    char *ps[] = {"ps", "-o", "pid=", NULL};
    char *tail[] = {"tail", "-2", NULL};
    char *head[] = {"head", "-1", NULL};
    char **args1, **args2;
    char path[50] = "/proc/";
    if(cmd1 == NULL || !strcmp(cmd1, "-i")){
        char **cmd[] = {ps, tail, head, NULL};
        exec(cmd, 0, 1);
    }else if(!strcmp(cmd1, "-p")){
        if(cmd2 == NULL){
            printf("please enter valid pid\n");
            return 1;
        }
        strcat(path, cmd2);
        strcat(path, "/stat");
        char *cat[] = {"cat", path, NULL};
        char *awk[] = {"awk", "{print$4}", NULL};
        char **cmd[] = {cat, awk, NULL};
        exec(cmd, 0, 1); 
    }else if(!strcmp(cmd1, "-c")){
        if(cmd2 == NULL){
            printf("please enter valid pid\n");
            return 1;
        }
        strcat(path, cmd2);
        strcat(path, "/task/");
        strcat(path, cmd2);
        strcat(path, "/children");
        char *cat[] = {"cat", path, NULL};
        char **cmd[] = {cat, NULL};
        exec(cmd, 0, 1);
        puts("");
    }
}

void show_history(){
    int a = history[idx] ? idx : 0;
    printf("history cmd:\n");
    for(int i = 0; i < 16 && history[a]; i++){
        printf("%2d: %s\n",i+1,  history[a++]);
        a %= 16;
    }
}

void replay_history(char *arg1){
    int n = atoi(arg1);
    int a = history[idx] ? (idx+n)%16 : n-1;
    char tmp[MAX_IN_LEN], input[MAX_IN_LEN], *piped_input[MAX_ARGS];
    char ***parsed_input;
    parsed_input = calloc(MAX_PIPED, sizeof(char**));
    for(int i = 0; i < MAX_PIPED; i++){
        parsed_input[i] = calloc(MAX_ARGS, sizeof(char*));
        for(int j = 0; j < MAX_ARGS; j++)
            parsed_input[i][j] = calloc(10, sizeof(char));
    }
    if(n > 0 && n <= 16 && history[(a+1)%16]){
        strcpy(input, history[a]);
        strcpy(tmp, input);
        strcpy(history[idx-1], tmp);
        parse_input(input, piped_input, parsed_input);
        exec(parsed_input, 0, 1);
        idx %= 16;
    }else{
        history[idx] = NULL;
        idx = idx ? idx-1 : 15;
    }
    free(parsed_input);
}

int exec_builtin(char *in, char *parsed[]){
    char *builtin_cmd[8] = {"help", "cd", "echo", "exit", "mypid", "record", "replay", NULL};
    char **ptr = builtin_cmd, *buffer, **tmp; // buffer for echo command
    buffer = calloc(MAX_IN_LEN, sizeof(char*));
    int i = 0, fd[2];
    pid_t pid, pid_1;
    while(*ptr){ // if there is only one command, check if it is builtin.
        if(!strcmp(*ptr, in)) break;
        ptr++;
        i++;
    }
    switch(i){
        case 0:
            printf("there's no help.\n");
            return 0;
        case 1:
            chdir(parsed[1]);
            return 0;
        case 2:
            tmp = parsed; 
            if(!strcmp(*(parsed+1), "-n")) tmp++;
            while(*++tmp){
                strcat(buffer, *tmp);
                strcat(buffer, " ");
            }
            if(!strcmp(*(parsed+1), "-n"))printf("%s", buffer);
            else printf("%s\n", buffer);
            return 0;
        case 3:
            printf("Bye bye.\n"), exit(0);
        case 4:
            mypid(parsed);
            return 0;
        case 5:
            show_history();
            return 0;
        case 6:
            replay_history(parsed[1]);
            return 0;
        default:
            return 1;
    }
}

static int exec(char ***parsed, int fd_in, int fd_out){
	int fd[2]; //fd[0] for read, fs[1] for write
	pid_t pid;
	int fd_reg = fd_in;
    if(!**parsed) return 1; // bugfix: blank space inputs would cause Segmentation Fault, since *cmd isn't NULL but **cmd is NULL
    if(!*(parsed+1)) //if there's only one command, check if it is builtin (for cd and exec fork() would not work)
        if(!exec_builtin((*parsed)[0], *parsed)) return 0;
	while(*parsed){
		if(pipe(fd) == -1)
            perror("pipe"), exit(1);
        else if((pid = fork()) == -1)
			perror("fork"), exit(1);
		else if(pid == 0){
			dup2(fd_reg, 0); // redirect all the input to fd_reg, relink fd0 to fdd (from stdin)
			if(*(parsed+1)) dup2(fd[1], 1); // if there's a following command, redirect all the output to write end of pipe (from stdout)
            else dup2(fd_out , 1);
            close(fd[0]);
            if(exec_builtin((*parsed)[0], *parsed)){ // if the command is not builtin (return 1), run execvp
                execvp((*parsed)[0], *parsed);
                perror("exec"), exit(1);
            }
			exit(0);
		}else{
			close(fd[1]);
			wait(NULL); 		// collect childs
			fd_reg = fd[0];
			parsed++;
		}
	}
    return 0;
}

void print(char ***in){ // this is for debug purposes
    char ***parsed = in, **cmd;
    while(*parsed){
        cmd = *parsed;
        while(*cmd)
            printf(">%s< ", *cmd), cmd++;
        printf("\n");
        parsed++;
    }
}

void parse_and_exec(char ***parsed){
    // parse for '<'
    int fd_in = 0, fd_out = 1;
    char **ptr_in = *parsed;
    while(*(ptr_in+1)){
        if(!strcmp(*ptr_in, "<")){
            if((fd_in = open(*(ptr_in+1), O_RDONLY | O_CREAT)) == -1)
                perror("open"), exit(1);
            while(*ptr_in){
                *ptr_in = *(ptr_in+2);
                ptr_in++;
            }
            break;
        }else
            ptr_in++;
    }
    // parse for '>'
    char ***ptr = parsed;
    while(*(ptr+1)) ptr++; // *ptr = last command 
    char **ptr_out = *ptr;
    while(*(ptr_out+1)){
        if(!strcmp(*ptr_out, ">")){
            if((fd_out = open(*(ptr_out+1), O_RDWR | O_CREAT | O_TRUNC)) == -1)
                perror("open"), exit(1);
            while(*ptr_out){
                *ptr_out = *(ptr_out+2);
                ptr_out++;
            }
            break;
        }else
            ptr_out++;
    }
    char **ptr1 = *ptr;
    while(*(ptr1+1)) ptr1++;
    if(!strcmp(*ptr1, "&")){
        *ptr1 = NULL;
        pid_t pid;
        if((pid = fork()) == -1)
            perror("fork"), exit(1);
        else if(pid == 0)
            exec(parsed, fd_in, fd_out), exit(0);
        else
            printf("[Pid]: %d\n", pid);
    }else
        exec(parsed, fd_in, fd_out);   
}

int main(){
    char input_str[MAX_IN_LEN], *piped_input[MAX_ARGS], ***parsed_input; // parsed_input = {[comm0, arg1, arg2...], [comm2, arg1, arg2...], [comm2, arg1, arg2...], [,,]...}
    initialize();
    history = malloc(16*sizeof(char*));
    idx = 0;

    while(1){
        *input_str = '\0'; 
        char **p = piped_input;
        while(*p) *p++ = '\0';
        parsed_input = calloc(MAX_PIPED, sizeof(char**));
        for(int i = 0; i < MAX_PIPED; i++){
            parsed_input[i] = calloc(MAX_ARGS, sizeof(char*));
            for(int j = 0; j < MAX_ARGS; j++)
                parsed_input[i][j] = calloc(10, sizeof(char));
        }
        pwd();
        if(read_input(input_str)) continue; // check for empty input
        history[idx] = malloc(MAX_IN_LEN*sizeof(char)); // record history
        strcpy(history[idx++], input_str);
        idx %= 16;
        parse_input(input_str, piped_input, parsed_input); // parse for '|' and " "
        parse_and_exec(parsed_input); // parse for '>' '<' '&' and execute
        free(parsed_input); //clear pointer
    }
}