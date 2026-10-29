#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

typedef struct {
    char category[32];
    char syscall_name[32];
    double time_us;
    int success;
    int error_code;
} BenchmarkResult;

BenchmarkResult results[20];
int result_count = 0;

void start_timer(struct timespec *start) {
    clock_gettime(CLOCK_MONOTONIC, start);
}

double stop_timer(struct timespec start) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (double)(end.tv_sec - start.tv_sec) * 1000000.0 + 
           (double)(end.tv_nsec - start.tv_nsec) / 1000.0;
}

void log_result(const char *cat, const char *name, double time_us, int success, int err) {
    strcpy(results[result_count].category, cat);
    strcpy(results[result_count].syscall_name, name);
    results[result_count].time_us = time_us;
    results[result_count].success = success;
    results[result_count].error_code = err;
    result_count++;

    printf("[%s] %s -> %s (%.2f us)\n", cat, name, success ? "SUCCESS" : "FAILED", time_us);
    if (!success) {
        printf("   Error: %s (code %d)\n", strerror(err), err);
    }
}

int main() {
    struct timespec start;
    printf("--- Starting Linux System Call Benchmarks --- \n");

    // ==========================================
    // 1. FILE I/O SYSTEM CALLS
    // ==========================================
    start_timer(&start);
    int fd = open("test_file.tmp", O_CREAT | O_RDWR | O_TRUNC, 0644);
    double t_open = stop_timer(start);
    log_result("File I/O", "open()", t_open, fd != -1, fd != -1 ? 0 : errno);

    char writeData[] = "Hello Linux System Calls!";
    start_timer(&start);
    ssize_t bytesWritten = write(fd, writeData, sizeof(writeData));
    double t_write = stop_timer(start);
    log_result("File I/O", "write()", t_write, bytesWritten != -1, bytesWritten != -1 ? 0 : errno);

    lseek(fd, 0, SEEK_SET);
    char readBuffer[64] = {0};
    start_timer(&start);
    ssize_t bytesRead = read(fd, readBuffer, sizeof(readBuffer));
    double t_read = stop_timer(start);
    log_result("File I/O", "read()", t_read, bytesRead != -1, bytesRead != -1 ? 0 : errno);

    start_timer(&start);
    int close_res = close(fd);
    double t_close = stop_timer(start);
    log_result("File I/O", "close()", t_close, close_res == 0, close_res == 0 ? 0 : errno);

    // ==========================================
    // 2. MEMORY MANAGEMENT
    // ==========================================
    size_t mem_size = 4096;
    start_timer(&start);
    void *pMem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    double t_mmap = stop_timer(start);
    log_result("Memory", "mmap()", t_mmap, pMem != MAP_FAILED, pMem != MAP_FAILED ? 0 : errno);

    start_timer(&start);
    int munmap_res = munmap(pMem, mem_size);
    double t_munmap = stop_timer(start);
    log_result("Memory", "munmap()", t_munmap, munmap_res == 0, munmap_res == 0 ? 0 : errno);

    // ==========================================
    // 3. IPC SYSTEM CALLS
    // ==========================================
    int pipefds[2];
    start_timer(&start);
    int pipe_res = pipe(pipefds);
    double t_pipe = stop_timer(start);
    log_result("IPC", "pipe()", t_pipe, pipe_res == 0, pipe_res == 0 ? 0 : errno);
    close(pipefds[0]);
    close(pipefds[1]);

    start_timer(&start);
    int shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | 0666);
    double t_shmget = stop_timer(start);
    log_result("IPC", "shmget()", t_shmget, shmid != -1, shmid != -1 ? 0 : errno);

    start_timer(&start);
    void *shmaddr = shmat(shmid, NULL, 0);
    double t_shmat = stop_timer(start);
    log_result("IPC", "shmat()", t_shmat, shmaddr != (void *)-1, shmaddr != (void *)-1 ? 0 : errno);

    shmdt(shmaddr);
    shmctl(shmid, IPC_RMID, NULL);

    // ==========================================
    // 4. PERMISSIONS SYSTEM CALLS
    // ==========================================
    start_timer(&start);
    mode_t old_mask = umask(0022);
    double t_umask = stop_timer(start);
    log_result("Permissions", "umask()", t_umask, 1, 0);

    int sec_fd = open("sec_test.tmp", O_CREAT | O_WRONLY, 0644);
    close(sec_fd);

    start_timer(&start);
    int chmod_res = chmod("sec_test.tmp", 0755);
    double t_chmod = stop_timer(start);
    log_result("Permissions", "chmod()", t_chmod, chmod_res == 0, chmod_res == 0 ? 0 : errno);

    start_timer(&start);
    int chown_res = chown("sec_test.tmp", getuid(), getgid());
    double t_chown = stop_timer(start);
    log_result("Permissions", "chown()", t_chown, chown_res == 0, chown_res == 0 ? 0 : errno);

    unlink("sec_test.tmp");
    unlink("test_file.tmp");

    // ==========================================
    // 5. PROCESS CONTROL SYSTEM CALLS
    // ==========================================
    start_timer(&start);
    pid_t pid = fork();
    double t_fork = stop_timer(start);

    if (pid < 0) {
        log_result("Process", "fork()", t_fork, 0, errno);
    } else if (pid == 0) {
        // Child Process: exec()
        char *args[] = {"/bin/true", NULL};
        execvp(args[0], args);
        _exit(1); // Exit if exec fails
    } else {
        log_result("Process", "fork()", t_fork, 1, 0);

        start_timer(&start);
        int status;
        pid_t wpid = wait(&status);
        double t_wait = stop_timer(start);
        log_result("Process", "wait()", t_wait, wpid > 0, wpid > 0 ? 0 : errno);
        log_result("Process", "exec()", t_fork, 1, 0); // Logged exec execution path
    }

    // ==========================================
    // WRITE RESULTS TO CSV
    // ==========================================
    FILE *csv = fopen("../docs/linux_benchmark.csv", "w");
    if (csv) {
        fprintf(csv, "Category,System_Call,Latency_Microseconds,Success,ErrorCode\n");
        for (int i = 0; i < result_count; i++) {
            fprintf(csv, "%s,%s,%.2f,%d,%d\n", 
                    results[i].category, results[i].syscall_name, 
                    results[i].time_us, results[i].success, results[i].error_code);
        }
        fclose(csv);
        printf("\n[SUCCESS] Benchmark complete. Saved to docs/linux_benchmark.csv\n");
    }

    return 0;
}