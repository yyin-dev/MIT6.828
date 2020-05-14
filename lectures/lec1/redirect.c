#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

int main() {
    int fd = creat("output.txt", S_IRUSR | S_IWUSR);
    char *str = "OS is SO fun!\n";
    dup2(fd, 1);
    close(fd);
    write(1, str, strlen(str));
}