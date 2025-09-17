#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " N R THREADS SEED VARIANT\n";
        return 1;
    }

    size_t N = std::stoull(argv[1]);
    size_t R = std::stoull(argv[2]);
    int nthreads = std::stoi(argv[3]);
    unsigned seed = std::stoul(argv[4]);
    int variant = std::stoi(argv[5]);

    // --- Generar datos ---
    auto t_gen_start = std::chrono::high_resolution_clock::now();
    std::vector<int> data(N);
    for (size_t i = 0; i < N; i++)
        data[i] = rand_r(&seed) % R;
    auto t_gen_end = std::chrono::high_resolution_clock::now();
    double t_gen = std::chrono::duration<double>(t_gen_end - t_gen_start).count();

    // --- Inicializar histogramas ---
    std::vector<int> hist(R, 0);
    std::vector<std::vector<int>> private_hists(nthreads, std::vector<int>(R,0));
    std::mutex mtx;
    std::atomic<int> atomic_hist;

    // --- Contar ---
    auto t_count_start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < nthreads; t++) {
        threads.emplace_back([&, t]() {
            size_t chunk = N / nthreads;
            size_t start = t * chunk;
            size_t end = (t == nthreads-1) ? N : start + chunk;
            if (variant == 0) { // private
                for (size_t i = start; i < end; i++)
                    private_hists[t][data[i]]++;
            } else if (variant == 1) { // mutex
                for (size_t i = start; i < end; i++) {
                    std::lock_guard<std::mutex> lock(mtx);
                    hist[data[i]]++;
                }
            } else { // atomic
                for (size_t i = start; i < end; i++) {
                    //#pragma GCC diagnostic ignored "-Watomic-alignment"
                    #pragma omp atomic
                    hist[data[i]]++;
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    auto t_count_end = std::chrono::high_resolution_clock::now();
    double t_count = std::chrono::duration<double>(t_count_end - t_count_start).count();

    // --- Merge private ---
    double t_merge = 0.0;
    if (variant == 0) {
        auto t_merge_start = std::chrono::high_resolution_clock::now();
        for (int t = 0; t < nthreads; t++)
            for (size_t i = 0; i < R; i++)
                hist[i] += private_hists[t][i];
        auto t_merge_end = std::chrono::high_resolution_clock::now();
        t_merge = std::chrono::duration<double>(t_merge_end - t_merge_start).count();
    }

    double total = t_gen + t_count + t_merge;

    // --- Salida ---
    size_t total_counts = 0;
    for (size_t i = 0; i < R; i++) total_counts += hist[i];

    std::cout << "total counts=" << total_counts << "\n";
    std::cout << "t_gen=" << t_gen << " t_count=" << t_count
              << " t_merge=" << t_merge << " total=" << total << "\n";

    return 0;
}
