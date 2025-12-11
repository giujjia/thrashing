#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#else
#include <time.h>
#include <sched.h>
#endif

#define DEFAULT_ARRAY_SIZE (32 * 1024 / sizeof(int))
#define DEFAULT_ITERATIONS 1000000
#define DEFAULT_DISTANCE_32KB (32 * 1024)
#define DEFAULT_NUM_ARRAYS 2
#define DEFAULT_NUM_RUNS 5

#define CACHE_LINE_SIZE_BYTES 64
#define CACHE_SIZE_32KB (32 * 1024)
#define CACHE_ASSOCIATIVITY 8
#define CACHE_NUM_SETS (CACHE_SIZE_32KB / (CACHE_ASSOCIATIVITY * CACHE_LINE_SIZE_BYTES))

static size_t g_array_size = DEFAULT_ARRAY_SIZE;
static size_t g_iterations = DEFAULT_ITERATIONS;
static size_t g_distance_32kb = DEFAULT_DISTANCE_32KB;
static int g_num_arrays = DEFAULT_NUM_ARRAYS;
static int g_num_runs = DEFAULT_NUM_RUNS;
static inline uint64_t get_time_ns() {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static inline void* aligned_alloc_wrapper(size_t alignment, size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

static inline void aligned_free_wrapper(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#ifdef __linux__
void pin_to_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        fprintf(stderr, "Aviso: Nao foi possivel fixar CPU (pode continuar)\n");
    }
}
#else
void pin_to_cpu(int cpu) {
    (void)cpu;
}
#endif

static inline size_t cache_set_index(uintptr_t addr) {
    return ((addr / CACHE_LINE_SIZE_BYTES) % CACHE_NUM_SETS);
}

void print_cache_mapping(const char* label, void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    size_t set_idx = cache_set_index(addr);
    printf("%s: addr=%p, set_index=%zu\n", label, ptr, set_idx);
}


void test_thrashing() {
    printf("\n=== TESTE 1 THRASHING (Num arrays: %d, Stride: %zu bytes) ===\n", 
           g_num_arrays, g_distance_32kb);
    
    const int CACHE_LINE_SIZE = 64;
    const size_t SMALL_ARRAY_SIZE = (g_num_arrays > 2) ? 1024 : g_array_size;
    size_t total_size = ((g_num_arrays - 1) * g_distance_32kb) + (SMALL_ARRAY_SIZE * sizeof(int));
    
    char* block = (char*)aligned_alloc_wrapper(CACHE_LINE_SIZE, total_size);
    if (block == NULL) {
        printf("Erro ao alocar memoria\n");
        return;
    }
    
    int* arrays[g_num_arrays];
    for (int i = 0; i < g_num_arrays; i++) {
        arrays[i] = (int*)(block + (i * g_distance_32kb));
        memset(arrays[i], 0, SMALL_ARRAY_SIZE * sizeof(int));
    }
    
    printf("Mapeamento de cache (primeiros 3 arrays):\n");
    for (int i = 0; i < (g_num_arrays < 3 ? g_num_arrays : 3); i++) {
        print_cache_mapping("  Array", arrays[i]);
    }
    if (g_num_arrays > 3) {
        printf("  ... (total: %d arrays)\n", g_num_arrays);
    }
    
    printf("Cache: 32 KB, %d-way associativo, %d sets\n", 
           CACHE_ASSOCIATIVITY, CACHE_NUM_SETS);
    printf("Numero de arrays: %d (excede associatividade? %s)\n", 
           g_num_arrays, (g_num_arrays > CACHE_ASSOCIATIVITY) ? "SIM" : "NAO");
    
    for (int arr = 0; arr < g_num_arrays; arr++) {
        for (size_t i = 0; i < SMALL_ARRAY_SIZE; i++) {
            arrays[arr][i] = (int)(arr * 1000 + i);
        }
    }
    
    double times[g_num_runs];
    volatile int checksums[g_num_runs];
    
    for (int run = 0; run < g_num_runs; run++) {
        volatile int checksum = 0;
        uint64_t start = get_time_ns();
        int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
        
        for (int i = 0; i < iter_count; i++) {
            int idx = i % (int)SMALL_ARRAY_SIZE;
            for (int arr = 0; arr < g_num_arrays; arr++) {
                arrays[arr][idx] += 1;
                checksum += arrays[arr][idx];
            }
        }
        
        uint64_t end = get_time_ns();
        times[run] = (double)(end - start) / 1000000.0;
        checksums[run] = checksum;
    }
    
    double mean = 0.0, variance = 0.0;
    for (int i = 0; i < g_num_runs; i++) {
        mean += times[i];
    }
    mean /= g_num_runs;
    
    for (int i = 0; i < g_num_runs; i++) {
        double diff = times[i] - mean;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / g_num_runs);
    int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
    double total_accesses = (double)iter_count * g_num_arrays;
    double mean_per_access_ns = (mean * 1000000.0) / total_accesses;
    
    printf("Tempo (media de %d execucoes): %.3f ms (DP: %.3f ms)\n", 
           g_num_runs, mean, stddev);
    printf("Tempo medio por acesso: %.2f ns\n", mean_per_access_ns);
    printf("Checksum (ultima execucao): %d\n", checksums[g_num_runs - 1]);
    printf("Problema: Thrashing causado por %d arrays competindo por %d vias no mesmo conjunto\n",
           g_num_arrays, CACHE_ASSOCIATIVITY);
    
    aligned_free_wrapper(block);
}

void test_solution_padding() {
    printf("\n=== SOLUCAO 1 PADDING (Espacamento entre arrays) ===\n");
    
    const int CACHE_LINE_SIZE = 64;
    const int PADDING_BYTES = CACHE_LINE_SIZE * 2;
    size_t stride_with_padding = g_distance_32kb + PADDING_BYTES;
    const size_t SMALL_ARRAY_SIZE = (g_num_arrays > 2) ? 1024 : g_array_size;
    size_t total_size = ((g_num_arrays - 1) * stride_with_padding) + (SMALL_ARRAY_SIZE * sizeof(int));
    
    char* block = (char*)aligned_alloc_wrapper(CACHE_LINE_SIZE, total_size);
    if (block == NULL) { 
        printf("Erro alocacao padding\n"); 
        return; 
    }
    
    int* arrays[g_num_arrays];
    for (int i = 0; i < g_num_arrays; i++) {
        arrays[i] = (int*)(block + (i * stride_with_padding));
        memset(arrays[i], 0, SMALL_ARRAY_SIZE * sizeof(int));
    }
    
    printf("Stride modificado (com padding): %zu bytes\n", stride_with_padding);
    
    for (int arr = 0; arr < g_num_arrays; arr++) {
        for (size_t i = 0; i < SMALL_ARRAY_SIZE; i++) arrays[arr][i] = 1;
    }
    
    double times[g_num_runs];
    volatile int checksums[g_num_runs];
    
    for (int run = 0; run < g_num_runs; run++) {
        volatile int checksum = 0;
        uint64_t start = get_time_ns();
        
        int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
        
        for (int i = 0; i < iter_count; i++) {
            int idx = i % (int)SMALL_ARRAY_SIZE;
            for (int arr = 0; arr < g_num_arrays; arr++) {
                arrays[arr][idx] += 1;
                checksum += arrays[arr][idx];
            }
        }
        
        uint64_t end = get_time_ns();
        times[run] = (double)(end - start) / 1000000.0;
        checksums[run] = checksum;
    }
    
    double mean = 0.0, variance = 0.0;
    for(int i=0; i<g_num_runs; i++) mean += times[i];
    mean /= g_num_runs;
    for(int i=0; i<g_num_runs; i++) {
        double diff = times[i] - mean;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / g_num_runs);
    int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
    double total_accesses = (double)iter_count * g_num_arrays;
    double ns_per_access = (mean * 1000000.0) / total_accesses;
    
    printf("Tempo (media de %d execucoes): %.3f ms (DP: %.3f ms)\n", 
           g_num_runs, mean, stddev);
    printf("Tempo medio por acesso: %.2f ns\n", ns_per_access);
    printf("Checksum (ultima execucao): %d\n", checksums[g_num_runs - 1]);
    printf("Melhoria: Padding reduz conflitos de cache mapeando para sets diferentes\n");
    printf("Nota: Processa %d arrays, igual ao teste de thrashing.\n", g_num_arrays);
    
    aligned_free_wrapper(block);
}

void test_solution_array_fusion() {
    printf("\n=== SOLUCAO 2 ARRAY FUSION (Combinar arrays) ===\n");
    
    const size_t num_elements = (g_num_arrays > 2) ? 1024 : g_array_size;
    
    typedef struct {
        int val[32];
    } FusionGroup;
    
    if (g_num_arrays > 32) {
        printf("ERRO: g_num_arrays (%d) excede tamanho maximo da struct (32)\n", g_num_arrays);
        return;
    }
    
    FusionGroup *array = (FusionGroup*)malloc(num_elements * sizeof(FusionGroup));
    memset(array, 0, num_elements * sizeof(FusionGroup));
    
    for (size_t i = 0; i < num_elements; i++) {
        for (int arr = 0; arr < g_num_arrays; arr++) {
            array[i].val[arr] = (int)(arr * 1000 + i);
        }
    }
    
    double times[g_num_runs];
    volatile int checksums[g_num_runs];
    
    for (int run = 0; run < g_num_runs; run++) {
        volatile int checksum = 0;
        uint64_t start = get_time_ns();
        int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
        
        for (int i = 0; i < iter_count; i++) {
            int idx = i % (int)num_elements;
            for (int arr = 0; arr < g_num_arrays; arr++) {
                array[idx].val[arr] += 1;
                checksum += array[idx].val[arr];
            }
        }
        
        uint64_t end = get_time_ns();
        times[run] = (double)(end - start) / 1000000.0;
        checksums[run] = checksum;
    }
    
    double mean = 0.0, variance = 0.0;
    for (int i = 0; i < g_num_runs; i++) mean += times[i];
    mean /= g_num_runs;
    for (int i = 0; i < g_num_runs; i++) {
        double diff = times[i] - mean;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / g_num_runs);
    int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
    double total_accesses = (double)iter_count * g_num_arrays;
    double mean_per_access_ns = (mean * 1000000.0) / total_accesses;
    
    printf("Tempo (media de %d execucoes): %.3f ms (DP: %.3f ms)\n", 
           g_num_runs, mean, stddev);
    printf("Tempo medio por acesso: %.2f ns\n", mean_per_access_ns);
    printf("Checksum (ultima execucao): %d\n", checksums[g_num_runs - 1]);
    printf("Melhoria: Localidade espacial maxima - dados contiguos evitam thrashing\n");
    printf("Nota: Processa %d valores por iteracao (igual ao teste de thrashing)\n", g_num_arrays);
    
    free(array);
}

void test_solution_loop_blocking() {
    printf("\n=== SOLUCAO 3 LOOP BLOCKING/TILING ===\n");
    
    const int CACHE_LINE_SIZE = 64;
    const size_t SMALL_ARRAY_SIZE = (g_num_arrays > 2) ? 1024 : g_array_size;
    size_t total_size = ((g_num_arrays - 1) * g_distance_32kb) + (SMALL_ARRAY_SIZE * sizeof(int));
    
    char* block = (char*)aligned_alloc_wrapper(CACHE_LINE_SIZE, total_size);
    if (block == NULL) { 
        printf("Erro alocacao blocking\n"); 
        return; 
    }
    
    int* arrays[g_num_arrays];
    for (int i = 0; i < g_num_arrays; i++) {
        arrays[i] = (int*)(block + (i * g_distance_32kb));
        memset(arrays[i], 0, SMALL_ARRAY_SIZE * sizeof(int));
    }
    
    for(int k=0; k<g_num_arrays; k++) 
        for(size_t i=0; i<SMALL_ARRAY_SIZE; i++) arrays[k][i] = 1;

    const int BLOCK_SIZE = 64; 
    
    double times[g_num_runs];
    volatile int checksums[g_num_runs];
    
    for (int run = 0; run < g_num_runs; run++) {
        volatile int checksum = 0;
        uint64_t start = get_time_ns();
        int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
        
        for (int i_base = 0; i_base < iter_count; i_base += BLOCK_SIZE) {
            int i_limit = (i_base + BLOCK_SIZE < iter_count) ? i_base + BLOCK_SIZE : iter_count;
            
            for (int i = i_base; i < i_limit; i++) {
                int idx = i % (int)SMALL_ARRAY_SIZE;
                for (int arr = 0; arr < g_num_arrays; arr++) {
                    arrays[arr][idx] += 1;
                    checksum += arrays[arr][idx];
                }
            }
        }
        
        uint64_t end = get_time_ns();
        times[run] = (double)(end - start) / 1000000.0;
        checksums[run] = checksum;
    }
    
    double mean = 0.0, variance = 0.0;
    for(int i=0; i<g_num_runs; i++) mean += times[i];
    mean /= g_num_runs;
    for(int i=0; i<g_num_runs; i++) {
        double diff = times[i] - mean;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / g_num_runs);
    int iter_count = (int)(g_iterations / (g_num_arrays > 2 ? 4 : 1));
    double total_accesses = (double)iter_count * g_num_arrays;
    double ns_per_access = (mean * 1000000.0) / total_accesses;
    
    printf("Tempo (media de %d execucoes): %.3f ms (DP: %.3f ms)\n", 
           g_num_runs, mean, stddev);
    printf("Tempo medio por acesso: %.2f ns\n", ns_per_access);
    printf("Checksum (ultima execucao): %d\n", checksums[g_num_runs - 1]);
    printf("Melhoria: Loop blocking melhora localidade temporal processando blocos menores\n");
    printf("Nota: Processa %d arrays, igual ao teste de thrashing.\n", g_num_arrays);
    
    aligned_free_wrapper(block);
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        g_iterations = (size_t)atol(argv[1]);
    }
    if (argc > 2) {
        g_num_arrays = atoi(argv[2]);
    }
    if (argc > 3) {
        g_distance_32kb = (size_t)atol(argv[3]);
    }
    if (argc > 4) {
        g_num_runs = atoi(argv[4]);
    }
    
    pin_to_cpu(0);
    
    printf("========================================\n");
    printf("SIMULAÇÃO DE THRASHING EM CACHE\n");
    printf("========================================\n");
    printf("Configuracao:\n");
    printf("  iterations=%zu\n", g_iterations);
    printf("  num_arrays=%d\n", g_num_arrays);
    printf("  stride=%zu bytes (%.2f KB)\n", g_distance_32kb, g_distance_32kb / 1024.0);
    printf("  num_runs=%d (para calcular media/DP)\n", g_num_runs);
    printf("  array_size=%zu elementos (~%.2f KB)\n", g_array_size, (g_array_size * sizeof(int)) / 1024.0);
    printf("========================================\n");
    
    test_thrashing();
    test_solution_padding();
    test_solution_array_fusion();
    test_solution_loop_blocking();
    
    printf("\n========================================\n");
    printf("TESTES CONCLUIDOS\n");
    printf("========================================\n");
    printf("\nRESUMO:\n");
    printf("- Thrashing ocorre quando muitos arrays mapeiam para os mesmos sets de cache\n");
    printf("- Para caches associativos modernos (8-way), use num_arrays > 8 para forcar thrashing\n");
    printf("- Solucao 1 (Padding): Adiciona espaco entre arrays para evitar conflitos\n");
    printf("- Solucao 2 (Array Fusion): Combina arrays para melhorar localidade espacial\n");
    printf("- Solucao 3 (Loop Blocking): Processa em blocos menores para melhorar localidade temporal\n");
    printf("\nUso: %s [iterations] [num_arrays] [stride_bytes] [num_runs]\n", argv[0]);
    printf("Exemplo: %s 1000000 16 32768 5\n", argv[0]);
    
    return 0;
}