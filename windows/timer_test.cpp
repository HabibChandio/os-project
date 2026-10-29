#include <windows.h>
#include <iostream>

int main() {
    LARGE_INTEGER frequency, start, end;

    // Get clock frequency (counts per second)
    QueryPerformanceFrequency(&frequency);

    // Start timer
    QueryPerformanceCounter(&start);

    // Simulated system operation
    Sleep(10); // Sleep for 10ms

    // End timer
    QueryPerformanceCounter(&end);

    double elapsedTime = (double)(end.QuadPart - start.QuadPart) * 1000000.0 / frequency.QuadPart;
    std::cout << "[SUCCESS] Win32 High-Resolution Timer Working!" << std::endl;
    std::cout << "Elapsed Time: " << elapsedTime << " microseconds" << std::endl;

    return 0;
}