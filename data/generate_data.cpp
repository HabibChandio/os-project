#include <iostream>
#include <fstream>
#include <random>

int main() {
    std::ofstream outfile("sensor_data.bin", std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to create sensor_data.bin" << std::endl;
        return 1;
    }

    std::default_random_engine generator;
    std::normal_distribution<double> noise(0.0, 2.0); // True value ~ 25.0 with noise

    const int NUM_SAMPLES = 100000;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        double val = 25.0 + noise(generator);
        outfile.write(reinterpret_cast<const char*>(&val), sizeof(double));
    }

    outfile.close();
    std::cout << "[SUCCESS] Generated sensor_data.bin with " << NUM_SAMPLES << " samples." << std::endl;
    return 0;
}