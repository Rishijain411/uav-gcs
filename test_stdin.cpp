#include <unistd.h>
#include <iostream>
int main() {
    std::cout << "isatty(0) = " << isatty(STDIN_FILENO) << "\n";
    return 0;
}
