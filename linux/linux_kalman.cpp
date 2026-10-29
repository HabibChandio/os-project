#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define TOTAL_SAMPLES 100000

struct SharedBuffer {
    double input[TOTAL_SAMPLES];
    double output[TOTAL_SAMPLES];
};

// 1D Kalman Filter algorithm
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

int main() {
    std::cout << "--- Parallel Linux (POSIX) Kalman Filter Started ---" << std::endl;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // 1. System Call File I/O: Open Sensor Input File
    int input_fd = open("../data/sensor_data.bin", O_RDONLY);
    if (input_fd < 0) {
        std::cerr << "[Parent] open() failed for sensor_data.bin" << std::endl;
        return 1;
    }

    // 2. Memory System Calls: Allocate Shared Memory Block (shmget & shmat)
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedBuffer), IPC_CREAT | 0666);
    if (shmid < 0) {
        std::cerr << "[Parent] shmget() failed" << std::endl;
        close(input_fd);
        return 1;
    }

    SharedBuffer* shared_data = (SharedBuffer*)shmat(shmid, NULL, 0);
    if (shared_data == (void*)-1) {
        std::cerr << "[Parent] shmat() failed" << std::endl;
        close(input_fd);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // Read binary data into mapped memory
    ssize_t bytes_read = read(input_fd, shared_data->input, sizeof(double) * TOTAL_SAMPLES);
    close(input_fd);

    if (bytes_read < 0) {
        std::cerr << "[Parent] read() failed" << std::endl;
        return 1;
    }

    // 3. Process Control: Spawn Child Processes via fork()
    int num_processes = 2;
    int chunk_size = TOTAL_SAMPLES / num_processes;

    for (int i = 0; i < num_processes; ++i) {
        int start_idx = i * chunk_size;
        int end_idx = (i == num_processes - 1) ? TOTAL_SAMPLES : (i + 1) * chunk_size;

        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[Parent] fork() failed" << std::endl;
            return 1;
        } else if (pid == 0) {
            // CHILD PROCESS EXECUTION
            run_kalman_filter(shared_data->input, shared_data->output, start_idx, end_idx);
            
            // Detach shared memory in child before exiting
            shmdt(shared_data);
            _exit(0);
        }
    }

    // 4. Process Control: Wait for all Child Processes via wait()
    for (int i = 0; i < num_processes; ++i) {
        int status;
        wait(&status);
    }

    // 5. System Call File I/O: Write Output Results to File
    int output_fd = open("../data/kalman_output_linux.bin", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (output_fd >= 0) {
        write(output_fd, shared_data->output, sizeof(double) * TOTAL_SAMPLES);
        close(output_fd);
    }

    // Clean up POSIX Shared Memory
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_ms = (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                        (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    std::cout << "[SUCCESS] Parallel POSIX Kalman Filtering Completed!" << std::endl;
    std::cout << "Execution Time: " << elapsed_ms << " ms" << std::endl;

    return 0;
}