#include "OperatorAuthorization.h"

// Platform-specific non-blocking keyboard input

#ifdef _WIN32
    // ============== WINDOWS ==============
    #include <conio.h>
    #include <windows.h>
    
    int OperatorAuthorization::readNonBlocking() {
        // Windows: _kbhit() checks if key is available, _getch() reads it
        return _kbhit() ? _getch() : -1;
    }

#else
    // ============== LINUX/macOS ==============
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    
    int OperatorAuthorization::readNonBlocking() {
        static bool flags_set = false;
        
        // First time: set O_NONBLOCK on stdin
        if (!flags_set) {
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            flags_set = true;
        }
        
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        
        if (n < 0 && errno == EAGAIN) {
            return -1;  // No data available
        }
        
        if (n <= 0) {
            return -1;
        }
        
        return (int)c;
    }

#endif
