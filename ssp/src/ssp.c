#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_BUFF 256

/* Utility */
void error_check(int val, char * msg){
    if(val < 0){
        int err = errno;
        perror(msg);
        exit(err);
    }
}

/* SSP Data Structures */
typedef struct {
    pid_t pid;
    char *name;
    int status;
} SSP_Process;

SSP_Process * processes = NULL;
int processes_size = 100;
int ssp_id = -1;

void ssp_set_pid(int id, int pid){
    processes[id].pid = pid;
}

void ssp_set_stat(int id, int stat){
    processes[id].status = stat;
}

void ssp_set_name(int id, char * name){
    processes[id].name = name;
}

void ssp_proc_add(int id, int pid, char * name, int stat){
    ssp_set_pid(id, pid);
    ssp_set_name(id, name);
    ssp_set_stat(id, stat);
}

void ssp_proc_init(int id){
    ssp_set_pid(id, -1);
    ssp_set_name(id, NULL);
    ssp_set_stat(id, 0);
}

int ssp_expand(){
    processes_size *= 2;
    processes = (SSP_Process *)realloc(processes, processes_size * sizeof(SSP_Process));
    if (processes == NULL) {
        int err = errno;
        perror("realloc");
        exit(err);
    }
    for(int i = processes_size/2; i < processes_size; i++){
        ssp_proc_init(i);
    }
    return processes_size/2;
}

void ssp_proc_print(int id, int max_len){
    int pid = processes[id].pid;
    char * name = processes[id].name;
    int status = processes[id].status;
    printf("%7d %-*s %d\n", pid, max_len, name, status);
}

/* SSP Init Section */
void ssp_init() {
    processes_size = 100;
    processes = (SSP_Process *)malloc(processes_size * sizeof(SSP_Process));
    if (processes == NULL) {
        int err = errno;
        perror("malloc");
        exit(err);
    }
    for(int i = 0; i < processes_size; i++){
        ssp_proc_init(i);
    }

    prctl(PR_SET_CHILD_SUBREAPER, 1);
    // TODO: Add sigaction
}

/* SSP Create Section */
void set_fds(int fd0, int fd1, int fd2){
    error_check(dup2(fd0, 0), "dup2");
    error_check(dup2(fd1, 1), "dup2");
    error_check(dup2(fd2, 2), "dup2");
}

void close_fds(){
    char fd_path[MAX_BUFF];
    snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", getpid());
    int dir_fd = open(fd_path, O_RDONLY);
    DIR * dir = fdopendir(dir_fd);
    if(dir == NULL){
        int err = errno;
        perror("fd open dir");
        exit(err);
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_LNK) {
            int fd = atoi(entry->d_name);
            if (fd > 2 && fd != dir_fd) error_check(close(fd), "close");
        }
    }
    error_check(closedir(dir), "close fd dir");
}

int ssp_create(char *const *argv, int fd0, int fd1, int fd2) {
    int pid = fork();
    error_check(pid, "fork");

    if (pid == 0) {
        set_fds(fd0, fd1, fd2);
        close_fds();
        error_check(execvp(argv[0], argv), "execvp");
    } else {
        ssp_id++;
        if (ssp_id >= processes_size) ssp_id = ssp_expand();
        ssp_proc_add(ssp_id, pid, strdup(argv[0]), -1);
    }

    return ssp_id;
}

/* SSP Get Status Section */
void ssp_exit(int id, int status){
    if (WIFEXITED(status)) {
        // process exited normally
        ssp_set_stat(id, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        // process terminated due to a signal
        ssp_set_stat(id, WTERMSIG(status) + 128);
    }
}

void ssp_check_exit(int id){
    int status;
    int result = waitpid(processes[id].pid, &status, WNOHANG);
    if(result < 0) perror("check exit");
    if(result > 0) ssp_exit(id, status);
}

int ssp_get_status(int id) {
    ssp_check_exit(id);
    return processes[id].status;
}

/* SSP Send Signal Section */
void ssp_send_signal(int id, int signum) {
    if (ssp_id < 0 || id >= processes_size) {
        fprintf(stderr, "Invalid ssp_id\n");
        return;
    }

    pid_t pid = processes[ssp_id].pid;
    if (pid >= 0) {
        error_check(kill(pid, signum), "send_signal");
    }
}

/* SSP Wait Section */
void ssp_wait() {
    int status;
    for (int i = 0; i < processes_size; i++) {
        if (processes[i].pid != -1 &&
            waitpid(processes[i].pid, &status, 0) == processes[i].pid) {
            ssp_exit(i, status);
        }
    }

    for (int i = 0; i < processes_size; i++) {
        if (processes[i].pid != -1 &&
            (processes[i].status < 0 || processes[i].status > 255)) {
            fprintf(stderr, "Process %d has an invalid status: %d\n", processes[i].pid, processes[i].status);
            exit(1);
        }
    }
}

/* SSP Print Section */
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int longest_proc_name(){
    int max_len = strlen("CMD");
    for(int i = 0; i < processes_size; i++){
        if(processes[i].pid >= 0)
            max_len = max(max_len, strlen(processes[i].name));
    }
    return max_len;
}

void print_header(int max_len){
    printf("%7s %-*s %s\n", "PID", max_len, "CMD", "STATUS");
}

// TODO: Change to Signal Handler
void ssp_unknown(){
    int child_status;
    while(1){
        int child_pid = waitpid(-1, &child_status, WNOHANG);
        if(child_pid < 0){
            if(errno == ECHILD) return;
            else perror("wait child");
        }
        if(child_pid == 0)
            return;
        if(child_pid > 0){
            int found = 0;
            for(int i = 0; i < processes_size; i++){
                if (processes[i].pid == child_pid) {
                    found = 1;
                    break;
                }
            }
            if(!found){
                ssp_id++;
                if (ssp_id >= processes_size) ssp_id = ssp_expand();
                ssp_proc_add(ssp_id, child_pid, "<unknown>", 0);
                ssp_exit(ssp_id, child_status);
            }
            else{
                // TODO: Update the subprocess's status using ssp_exit()
            }
        }
    }
}

void ssp_print() {
    ssp_unknown();  // 非及时更新，只有在ssp_print被执行时才会更新subprocess，这时不需要while(1){}
                    // 因为在执行ssp_print时，所有的subprocess已经都运行完成，没有concurrency问题
    /* TODO: 想要及时更新则需要使用signal，相当于C#中的Event
     *  使用signal_handler，在ssp_init中register一个sigaction，名为SIGCHILD
     *  这个signal会在child结束时执行register的callback function，也即ssp_unknown
     *  同时，在ssp_unknown中，由于多个Child可能会同时Exit，所以会有Concurrency Issue
     *  这时需要给ssp_unknown添加 while(1){} 来确保所有的child都被记录下来
     *  */

    int max_len = longest_proc_name();
    print_header(max_len);

    for(int i = 0; i < processes_size; i++){
        if(processes[i].pid >= 0){
            ssp_check_exit(i);
            ssp_proc_print(i, max_len);
        }
    }
}
