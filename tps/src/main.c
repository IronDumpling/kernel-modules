#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_BUFF 4096

int isNumeric(char *str){
    while(*str){
        if(!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

char * GetFirstLine(char *file, ssize_t bytes_read){
    for(int i = 0; i < bytes_read; i++){
        if(file[i] == '\n'){
            file[i] = '\0';
            break;
        }
    }
    return file;
}

char * GetProcName(char * line){
    line = strstr(line, "Name:");
    line += strlen("Name:");
    while(isspace(*line) || *line == '\t')
        line++;
    return line;
}

void PrintProcName(char * pid, char * name){
    int space_len = 5 - strlen(pid);

    char space[space_len];
    for(int i = 0; i < space_len; i++){
        space[i] = ' ';
    }
    space[space_len] = '\0';

    printf("%s%s %s\n", space, pid, name);
}

int main() {
    struct dirent *entry;
    char pid_path[MAX_BUFF], file[MAX_BUFF];
    int status_file;

    DIR *proc_dir;
    proc_dir = opendir("/proc");
    if(proc_dir == NULL){
        perror("Unable to open '/proc' directory.\n");
        return 1;
    }

    printf("  PID CMD\n");

    while((entry = readdir(proc_dir))){
        snprintf(pid_path, sizeof(pid_path), "/proc/%s", entry->d_name);

        // check if directory is valid
        struct stat path_stat;
        if(stat(pid_path, &path_stat) == -1 ||
                !S_ISDIR(path_stat.st_mode) ||
                !isNumeric(entry->d_name))
            continue;

        // open the status file
        status_file = open(strcat(pid_path, "/status"), O_RDONLY);
        if(status_file == -1){
            perror("Unable to open status file.\n");
            continue;
        }

        // read the status file
        ssize_t bytes_read = read(status_file, file, sizeof(file));
        if(bytes_read == -1){
            perror("Unable to read status file.\n");
            close(status_file);
            continue;
        }

        char * line = GetFirstLine(file, bytes_read);
        if(close(status_file) == -1){
            perror("Unable to open status file.\n");
            continue;
        }

        // output process
        if(line[0] != '\0'){
            char *proc_name = GetProcName(line);
            PrintProcName(entry->d_name, proc_name);
        }
    }

    if(closedir(proc_dir) == -1){
        perror("Unable to open status file.\n");
        return -1;
    }

    return 0;
}
