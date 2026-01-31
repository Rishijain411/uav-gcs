#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/select.h>
int main() {
    int fd = open("/dev/tty", O_RDONLY);  // NO O_NONBLOCK
    if (fd < 0) {
        std::cout << "Failed to open /dev/tty\n";
        return 1;
    }
    std::cout << "Ready! Type 'y' now:\n";
    
    fd_set readfds;
    struct timeval tv = {0, 0};
    
    for (int i = 0; i < 50; i++) {  // Try 50 times (5 seconds at 10Hz)
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        
        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0) {
            unsigned char c;
            read(fd, &c, 1);
            std::cout << "Got: '" << c << "' (ASCII " << (int)c << ")\n";
            close(fd);
            return 0;
        }
        usleep(100000);  // 100ms
    }
    
    std::cout << "Timeout - no input received\n";
    close(fd);
    return 0;
}
