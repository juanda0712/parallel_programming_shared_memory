#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdlib>

/**
 * Programa para generar un histograma de enteros en paralelo usando std::thread.
 * Permite tres variantes de sincronización:
 *  - private: cada hilo tiene su histograma local y luego se combinan.
 *  - mutex: todos los hilos actualizan el histograma global con mutex.
 *  - atomic: incrementos atómicos en el histograma global.
 *
 * Uso:
 *   ./program N R THREADS SEED VARIANT
 *   N       - cantidad de elementos a generar
 *   R       - rango de valores de los elementos [0, R-1]
 *   THREADS - número de hilos a utilizar
 *   SEED    - semilla para generación de números aleatorios
 *   VARIANT - 0 (private), 1 (mutex), 2 (atomic)
 */
int main(int argc, char** argv) {

    /** Validación de argumentos de entrada */
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " N R THREADS SEED VARIANT\n";
        return 1;
    }

    /** Lectura de parámetros */
    size_t N = std::stoull(argv[1]);       // número de elementos
    size_t R = std::stoull(argv[2]);       // rango de valores
    int nthreads = std::stoi(argv[3]);     // número de hilos
    unsigned seed = std::stoul(argv[4]);   // semilla de aleatoriedad
    int variant = std::stoi(argv[5]);      // variante de sincronización

    // --- Generar datos ---
    auto t_gen_start = std::chrono::high_resolution_clock::now();
    std::vector<int> data(N);
    for (size_t i = 0; i < N; i++)
        data[i] = rand_r(&seed) % R;  // generar números aleatorios
    auto t_gen_end = std::chrono::high_resolution_clock::now();
    double t_gen = std::chrono::duration<double>(t_gen_end - t_gen_start).count();

    // --- Inicializar histogramas ---
    std::vector<int> hist(R, 0);                        // histograma global
    std::vector<std::vector<int>> private_hists(nthreads, std::vector<int>(R,0)); // histogramas locales
    std::mutex mtx;                                     // mutex para sincronización
    std::atomic<int> atomic_hist;                       // variable atómica opcional (no usada explícitamente)

    // --- Contar ocurrencias ---
    auto t_count_start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < nthreads; t++) {
        threads.emplace_back([&, t]() {
            size_t chunk = N / nthreads;
            size_t start = t * chunk;
            size_t end = (t == nthreads-1) ? N : start + chunk;

            /** Variante private: cada hilo actualiza su histograma local */
            if (variant == 0) {
                for (size_t i = start; i < end; i++)
                    private_hists[t][data[i]]++;

            /** Variante mutex: se usa lock_guard para actualizar histograma global */
            } else if (variant == 1) {
                for (size_t i = start; i < end; i++) {
                    std::lock_guard<std::mutex> lock(mtx);
                    hist[data[i]]++;
                }

            /** Variante atomic: incrementos atómicos (comentado con pragma OpenMP) */
            } else {
                for (size_t i = start; i < end; i++) {
                    //#pragma GCC diagnostic ignored "-Watomic-alignment"
                    #pragma omp atomic
                    hist[data[i]]++;
                }
            }
        });
    }

    /** Esperar a que todos los hilos terminen */
    for (auto& th : threads) th.join();
    auto t_count_end = std::chrono::high_resolution_clock::now();
    double t_count = std::chrono::duration<double>(t_count_end - t_count_start).count();

    // --- Merge de histogramas privados ---
    double t_merge = 0.0;
    if (variant == 0) {
        auto t_merge_start = std::chrono::high_resolution_clock::now();
        for (int t = 0; t < nthreads; t++)
            for (size_t i = 0; i < R; i++)
                hist[i] += private_hists[t][i];
        auto t_merge_end = std::chrono::high_resolution_clock::now();
        t_merge = std::chrono::duration<double>(t_merge_end - t_merge_start).count();
    }

    /** Tiempo total de ejecución */
    double total = t_gen + t_count + t_merge;

    // --- Salida ---
    size_t total_counts = 0;
    for (size_t i = 0; i < R; i++) total_counts += hist[i];

    std::cout << "total counts=" << total_counts << "\n";
    std::cout << "t_gen=" << t_gen << " t_count=" << t_count
              << " t_merge=" << t_merge << " total=" << total << "\n";

    return 0;
}
