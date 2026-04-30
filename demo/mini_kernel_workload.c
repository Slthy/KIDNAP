// mini_kernel_workload.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    unsigned char x;

    if (read(0, &x, 1) != 1) return 0;  // read one byte, put it on x

    if (x == 'A') {
        getpid();                       // proc_1
    } 
    else if (x == 'B') {
        int fd = open("/tmp/demo_file", O_CREAT | O_RDWR, 0644);    // proc_2
        if (fd >= 0) {
            write(fd, "hello", 5);      // proc_3
            close(fd);                  // proc_4
        }
    } 
    else if (x == 'C') {                // two char array -> two pointers, pipe
        pipe((int[2]){0});              // proc_5    
    } 
    else if (x == 'D') {
        mkdir("/tmp/demo_dir", 0755);   // proc_6
    }

    return 0;
}