#include <windows.h>
#include <iostream>
#include <vector>
#include <string>

#define SHARED_MEM_NAME "Local\\KalmanSharedMem"
#define TOTAL_SAMPLES 100000

// Shared Memory Structure
struct SharedBuffer {
    double input[TOTAL_SAMPLES];
    double output[TOTAL_SAMPLES];
};

// 1D Kalman Filter computation on data slice
void run_kalman_filter(double* input, double* output, int start_idx, int end_idx) {
    double x_hat = 0.0; // State estimate
    double P = 1.0;     // Estimate error covariance
    double Q = 0.01;    // Process noise covariance
    double R = 2.0;     // Measurement noise covariance

    for (int i = start_idx; i < end_idx; ++i) {
        // Measurement Update
        double K = P / (P + R);
        x_hat = x_hat + K * (input[i] - x_hat);
        P = (1.0 - K) * P + Q;

        output[i] = x_hat;
    }
}

int main(int argc, char* argv[]) {
    // ==========================================
    // CHILD PROCESS EXECUTION MODE
    // ==========================================
    if (argc >= 4 && std::string(argv[1]) == "--child") {
        int start_idx = std::stoi(argv[2]);
        int end_idx = std::stoi(argv[3]);

        // Open Shared Memory created by parent
        HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
        if (hMap == NULL) {
            std::cerr << "[Child] OpenFileMapping failed: " << GetLastError() << std::endl;
            return 1;
        }

        SharedBuffer* shared_data = (SharedBuffer*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBuffer));
        if (shared_data == NULL) {
            std::cerr << "[Child] MapViewOfFile failed: " << GetLastError() << std::endl;
            CloseHandle(hMap);
            return 1;
        }

        // Perform parallel Kalman filtering computation
        run_kalman_filter(shared_data->input, shared_data->output, start_idx, end_idx);

        UnmapViewOfFile(shared_data);
        CloseHandle(hMap);
        return 0;
    }

    // ==========================================
    // PARENT PROCESS EXECUTION MODE
    // ==========================================
    std::cout << "--- Parallel Win32 Kalman Filter Started ---" << std::endl;

    LARGE_INTEGER freq, start_time, end_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_time);

    // 1. System Call File I/O: Read Sensor Input File
    HANDLE hInputFile = CreateFileA("../data/sensor_data.bin", GENERIC_READ, 0, NULL, 
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInputFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[Parent] CreateFile failed for input: " << GetLastError() << std::endl;
        return 1;
    }

    // 2. Memory Mapping System Call: Allocate Shared Memory Block
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 
                                     sizeof(SharedBuffer), SHARED_MEM_NAME);
    if (hMap == NULL) {
        std::cerr << "[Parent] CreateFileMapping failed: " << GetLastError() << std::endl;
        CloseHandle(hInputFile);
        return 1;
    }

    SharedBuffer* shared_data = (SharedBuffer*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBuffer));
    if (shared_data == NULL) {
        std::cerr << "[Parent] MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(hInputFile);
        CloseHandle(hMap);
        return 1;
    }

    DWORD bytesRead = 0;
    BOOL readSuccess = ReadFile(hInputFile, shared_data->input, sizeof(double) * TOTAL_SAMPLES, &bytesRead, NULL);
    CloseHandle(hInputFile);

    if (!readSuccess) {
        std::cerr << "[Parent] ReadFile failed: " << GetLastError() << std::endl;
        return 1;
    }

    // 3. Process Management: Spawn 2 Worker Child Processes
    int num_processes = 2;
    int chunk_size = TOTAL_SAMPLES / num_processes;
    std::vector<HANDLE> child_handles;

    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    for (int i = 0; i < num_processes; ++i) {
        int start_idx = i * chunk_size;
        int end_idx = (i == num_processes - 1) ? TOTAL_SAMPLES : (i + 1) * chunk_size;

        std::string cmd = std::string(exe_path) + " --child " + std::to_string(start_idx) + " " + std::to_string(end_idx);

        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        PROCESS_INFORMATION pi = { 0 };

        BOOL procCreated = CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        if (procCreated) {
            child_handles.push_back(pi.hProcess);
            CloseHandle(pi.hThread); // Close thread handle as it's unneeded
        } else {
            std::cerr << "[Parent] CreateProcess failed: " << GetLastError() << std::endl;
        }
    }

    // 4. Process Synchronization System Call: Wait for Children
    if (!child_handles.empty()) {
        WaitForMultipleObjects((DWORD)child_handles.size(), child_handles.data(), TRUE, INFINITE);
        for (HANDLE hProc : child_handles) {
            CloseHandle(hProc);
        }
    }

    // 5. System Call File I/O: Write Output Results
    HANDLE hOutputFile = CreateFileA("../data/kalman_output_win.bin", GENERIC_WRITE, 0, NULL, 
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutputFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(hOutputFile, shared_data->output, sizeof(double) * TOTAL_SAMPLES, &bytesWritten, NULL);
        CloseHandle(hOutputFile);
    }

    // Cleanup Memory Mapping
    UnmapViewOfFile(shared_data);
    CloseHandle(hMap);

    QueryPerformanceCounter(&end_time);
    double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / freq.QuadPart;

    std::cout << "[SUCCESS] Parallel Kalman Filtering Completed!" << std::endl;
    std::cout << "Execution Time: " << elapsed_ms << " ms" << std::endl;

    return 0;
}