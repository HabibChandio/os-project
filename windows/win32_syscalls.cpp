#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <iostream>
#include <fstream>
#include <vector>

// Helper structure to log benchmark results
struct BenchmarkResult {
    std::string category;
    std::string syscall_name;
    double time_us;
    bool success;
    DWORD error_code;
};

std::vector<BenchmarkResult> results;
LARGE_INTEGER timer_freq;

void start_timer(LARGE_INTEGER &start) {
    QueryPerformanceCounter(&start);
}

double stop_timer(LARGE_INTEGER start) {
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - start.QuadPart) * 1000000.0 / timer_freq.QuadPart;
}

void log_result(const std::string &cat, const std::string &name, double time_us, bool success, DWORD err) {
    results.push_back({cat, name, time_us, success, err});
    std::cout << "[" << cat << "] " << name << " -> " 
              << (success ? "SUCCESS" : "FAILED") 
              << " (" << time_us << " us)" << std::endl;
    if (!success) {
        std::cerr << "   Error Code: " << err << std::endl;
    }
}

int main() {
    QueryPerformanceFrequency(&timer_freq);
    LARGE_INTEGER start;

    std::cout << "--- Starting Win32 System Call Benchmarks ---" << std::endl;

    // ==========================================
    // 1. FILE I/O SYSTEM CALLS
    // ==========================================
    // CreateFile
    start_timer(start);
    HANDLE hFile = CreateFileA("test_file.tmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    double t_create = stop_timer(start);
    bool s_create = (hFile != INVALID_HANDLE_VALUE);
    log_result("File I/O", "CreateFile()", t_create, s_create, s_create ? 0 : GetLastError());

    // WriteFile
    char writeData[] = "Hello System Calls!";
    DWORD bytesWritten = 0;
    start_timer(start);
    BOOL s_write = WriteFile(hFile, writeData, sizeof(writeData), &bytesWritten, NULL);
    double t_write = stop_timer(start);
    log_result("File I/O", "WriteFile()", t_write, s_write, s_write ? 0 : GetLastError());

    // ReadFile
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    char readBuffer[64] = {0};
    DWORD bytesRead = 0;
    start_timer(start);
    BOOL s_read = ReadFile(hFile, readBuffer, sizeof(readBuffer), &bytesRead, NULL);
    double t_read = stop_timer(start);
    log_result("File I/O", "ReadFile()", t_read, s_read, s_read ? 0 : GetLastError());

    // CloseHandle
    start_timer(start);
    BOOL s_close = CloseHandle(hFile);
    double t_close = stop_timer(start);
    log_result("File I/O", "CloseHandle()", t_close, s_close, s_close ? 0 : GetLastError());

    // ==========================================
    // 2. MEMORY MANAGEMENT
    // ==========================================
    SIZE_T memSize = 4096; // 4 KB page
    start_timer(start);
    LPVOID pMem = VirtualAlloc(NULL, memSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    double t_valloc = stop_timer(start);
    log_result("Memory", "VirtualAlloc()", t_valloc, pMem != NULL, pMem != NULL ? 0 : GetLastError());

    start_timer(start);
    BOOL s_vfree = VirtualFree(pMem, 0, MEM_RELEASE);
    double t_vfree = stop_timer(start);
    log_result("Memory", "VirtualFree()", t_vfree, s_vfree, s_vfree ? 0 : GetLastError());

    // ==========================================
    // 3. IPC SYSTEM CALLS
    // ==========================================
    // Anonymous Pipe
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    start_timer(start);
    BOOL s_pipe = CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0);
    double t_pipe = stop_timer(start);
    log_result("IPC", "CreatePipe()", t_pipe, s_pipe, s_pipe ? 0 : GetLastError());
    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);

    // Shared Memory Mapping
    start_timer(start);
    HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "LocalSharedMem");
    double t_cfmap = stop_timer(start);
    log_result("IPC", "CreateFileMapping()", t_cfmap, hMapFile != NULL, hMapFile != NULL ? 0 : GetLastError());

    start_timer(start);
    LPVOID pMapView = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 1024);
    double t_mvmap = stop_timer(start);
    log_result("IPC", "MapViewOfFile()", t_mvmap, pMapView != NULL, pMapView != NULL ? 0 : GetLastError());

    UnmapViewOfFile(pMapView);
    CloseHandle(hMapFile);

    // ==========================================
    // 4. PROCESS CONTROL
    // ==========================================
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi = { 0 };
    char cmd[] = "cmd.exe /c echo Hello Child Process";

    start_timer(start);
    BOOL s_proc = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    double t_proc = stop_timer(start);
    log_result("Process", "CreateProcess()", t_proc, s_proc, s_proc ? 0 : GetLastError());

    if (s_proc) {
        start_timer(start);
        DWORD waitRes = WaitForSingleObject(pi.hProcess, INFINITE);
        double t_wait = stop_timer(start);
        log_result("Process", "WaitForSingleObject()", t_wait, waitRes == WAIT_OBJECT_0, GetLastError());

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // ==========================================
    // 5. PERMISSIONS
    // ==========================================
    // SetFileSecurity
    HANDLE hSecFile = CreateFileA("sec_test.tmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CloseHandle(hSecFile);

    PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);

    start_timer(start);
    BOOL s_sec = SetFileSecurityA("sec_test.tmp", DACL_SECURITY_INFORMATION, pSD);
    double t_sec = stop_timer(start);
    log_result("Permissions", "SetFileSecurity()", t_sec, s_sec, s_sec ? 0 : GetLastError());

    LocalFree(pSD);
    DeleteFileA("sec_test.tmp");
    DeleteFileA("test_file.tmp");

    // ==========================================
    // WRITE RESULTS TO CSV
    // ==========================================
    std::ofstream csv("../docs/win32_benchmark.csv");
    csv << "Category,System_Call,Latency_Microseconds,Success,ErrorCode\n";
    for (const auto &r : results) {
        csv << r.category << "," << r.syscall_name << "," << r.time_us << "," 
            << (r.success ? "1" : "0") << "," << r.error_code << "\n";
    }
    csv.close();

    std::cout << "\n[SUCCESS] Benchmark complete. Saved to docs/win32_benchmark.csv" << std::endl;
    return 0;
}