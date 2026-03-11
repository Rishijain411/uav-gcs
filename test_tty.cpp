#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
int main() {
    int fd = open("/dev/tty", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cout << "/dev/tty open failed: " << strerror(errno) << "\n";
        return 1;
    }
    std::cout << "Opened /dev/tty successfully\n";
    std::cout << "Type 'y' now, I will try to read it...\n";
    
    unsigned char c;
    ssize_t n = read(fd, &c, 1);
    std::cout << "Read result: " << n << " bytes\n";
    if (n > 0) std::cout << "Character read: '" << c << "' (ASCII " << (int)c << ")\n";
    
    close(fd);
    return 0;
}
