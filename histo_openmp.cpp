#include <iostream>
#include <vector>
#include <cstdlib>
#include <omp.h>

/**
 * Programa para generar un histograma de enteros en paralelo usando OpenMP.
 * Permite tres variantes de sincronización:
 *  - private: cada hilo tiene su histograma local y luego se combinan.
 *  - mutex (critical): todos los hilos actualizan el histograma global con sección crítica.
 *  - atomic: incrementos atómicos en el histograma global.
 *
 * Uso:
 *   ./program N R VARIANT
 *   N       - cantidad de elementos a generar
 *   R       - rango de valores de los elementos [0, R-1]
 *   VARIANT - 0 (private), 1 (mutex), 2 (atomic)
 */
int main(int argc, char** argv) {

    /** Validación de argumentos de entrada */
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " N R VARIANT\n";
        return 1;
    }

    /** Lectura de parámetros */
    size_t N = std::stoull(argv[1]);     // número de elementos
    size_t R = std::stoull(argv[2]);     // rango de valores
    int variant = std::stoi(argv[3]);    // variante de sincronización

    // --- Generar datos ---
    /** Vector de datos aleatorios */
    double t_gen_start = omp_get_wtime();
    std::vector<int> data(N);
    unsigned seed = 12345; // semilla fija para reproducibilidad
    #pragma omp parallel for
    for (size_t i = 0; i < N; i++) {
        data[i] = rand_r(&seed) % R;
    }
    double t_gen_end = omp_get_wtime();
    double t_gen = t_gen_end - t_gen_start;

    // --- Inicializar histogramas ---
    std::vector<int> hist(R, 0); // histograma global
    int nthreads = omp_get_max_threads();
    std::vector<std::vector<int>> private_hists(nthreads, std::vector<int>(R,0)); // histogramas privados

    // --- Contar ocurrencias ---
    double t_count_start = omp_get_wtime();
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();

        /** Variante private: cada hilo actualiza su histograma local */
        if (variant == 0) {
            #pragma omp for
            for (size_t i = 0; i < N; i++)
                private_hists[tid][data[i]]++;

        /** Variante mutex: uso de sección crítica para actualizar histograma global */
        } else if (variant == 1) {
            #pragma omp for
            for (size_t i = 0; i < N; i++) {
                #pragma omp critical
                hist[data[i]]++;
            }

        /** Variante atomic: incrementos atómicos en histograma global */
        } else {
            #pragma omp for
            for (size_t i = 0; i < N; i++) {
                #pragma omp atomic
                hist[data[i]]++;
            }
        }
    }
    double t_count_end = omp_get_wtime();
    double t_count = t_count_end - t_count_start;

    // --- Merge de histogramas privados ---
    double t_merge = 0.0;
    if (variant == 0) {
        double t_merge_start = omp_get_wtime();
        for (int t = 0; t < nthreads; t++) {
            for (size_t i = 0; i < R; i++)
                hist[i] += private_hists[t][i];
        }
        double t_merge_end = omp_get_wtime();
        t_merge = t_merge_end - t_merge_start;
    }

    /** Tiempo total */
    double total = t_gen + t_count + t_merge;

    // --- Salida ---
    size_t total_counts = 0;
    for (size_t i = 0; i < R; i++) total_counts += hist[i];

    std::cout << "t_gen=" << t_gen << " t_count=" << t_count
              << " t_merge=" << t_merge << " total=" << total << "\n";
    std::cout << "total counts=" << total_counts << "\n";

    return 0;
}
