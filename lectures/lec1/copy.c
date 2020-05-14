#include <stdio.h>

int main() {
    int BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];

   fgets(buffer, sizeof(buffer), stdin);
   fputs(buffer, stdout);
}
