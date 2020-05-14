#include <unistd.h>
#include <stdio.h>

int main() {
    char buffer[256];

    int nRead = read(0, buffer, sizeof(buffer));
    write(1, buffer, nRead);
}