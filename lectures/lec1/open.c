#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

int main() {
    int fd = creat("output.txt", S_IRUSR | S_IWUSR);
    char *str = "OS is fun!\n";
    write(fd, str, strlen(str));
}