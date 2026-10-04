#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pile.h"

/* external name of the implementation (defined in pile_*.c) */
extern const char* impl_name;

/* Prevent optimization by accumulating popped values into a volatile */
volatile unsigned long long bench_checksum = 0;

/* Maximum element size we will handle (gros_t = 64 bytes) */
typedef union {
    int          i;
    double       d;
    char         gros[64];
} max_elem_t;

/* ------------------- chronomètre haute précision ------------------- */
static LARGE_INTEGER freq;
static void init_timer(void) { QueryPerformanceFrequency(&freq); }
static double now_seconds(void)
{
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
}

/* ------------------- types de test ------------------- */
typedef enum { TEST_PUSH, TEST_POP, TEST_PUSHPOP_ALT, TEST_PUSH_THEN_POP } test_type;

/* structure contenant les statistiques */
typedef struct {
    double min, max, avg, stddev;
} stats_t;

/* ajoute une mesure à l'ensemble (algorithme de mise à jour de moyenne/variance en ligne) */
static void add_sample(stats_t* s, double x, int* n, double* mean, double* M2)
{
    (*n)++;
    double delta = x - *mean;
    *mean += delta / (*n);
    double delta2 = x - *mean;
    *M2 += delta * delta2;
    if (*n == 1) { s->min = s->max = x; }
    else {
        if (x < s->min) s->min = x;
        if (x > s->max) s->max = x;
    }
}
static void finalize(stats_t* s, double mean, double M2, int n)
{
    s->avg = mean;
    s->stddev = (n > 1) ? sqrt(M2 / (n - 1)) : 0.0;
}

/* Initialization functions for each type */
static void init_int(void* ptr, size_t index)
{
    *(int*)ptr = (int)index;
}
static void init_double(void* ptr, size_t index)
{
    *(double*)ptr = (double)index;
}
typedef struct { char data[64]; } gros_t;
static void init_gros(void* ptr, size_t index)
{
    gros_t* g = (gros_t*)ptr;
    for (size_t k = 0; k < 64; ++k)
        g->data[k] = (char)((index + k) & 0xFF);
}

/* ------------------- scénarios de test ------------------- */
static void run_test(pile_t* (*creer_fn)(size_t),
                     void (*detruire_fn)(pile_t*),
                     int (*empiler_fn)(pile_t*, const void*),
                     int (*depiler_fn)(pile_t*, void*),
                     size_t elemSize,
                     size_t N,
                     test_type tt,
                     void (*init_val)(void*, size_t),
                     stats_t* out)
{
    int repeats = (N <= 1000000) ? 10 : 3; /* number of repetitions */
    int n = 0;
    double mean = 0.0, M2 = 0.0;
    /* buffer for a value of size elemSize, initialized to some pattern */
    max_elem_t value;
    memset(&value, 0xAB, sizeof(value)); /* arbitrary pattern */

    /* échauffement : une petite pile pour charger le code */
    pile_t* wk = creer_fn(elemSize);
    for (size_t i = 0; i < 100 && wk; ++i) {
        init_val(&value, i);
        empiler_fn(wk, &value);
        depiler_fn(wk, &value);
    }
    detruire_fn(wk);

    for (int r = 0; r < repeats; ++r) {
        pile_t* p = creer_fn(elemSize);
        if (!p) { fprintf(stderr, "Allocation échouée\n"); exit(1); }

        double t0 = now_seconds();

        if (tt == TEST_PUSH) {                         // N pushes
            for (size_t i = 0; i < N; ++i) {
                init_val(&value, i);
                empiler_fn(p, &value);
            }
        } else if (tt == TEST_POP) {                   // N pops (pile pré-remplie)
            /* First fill the pile */
            for (size_t i = 0; i < N; ++i) {
                init_val(&value, i);
                empiler_fn(p, &value);
            }
            /* Then pop N times */
            for (size_t i = 0; i < N; ++i) {
                depiler_fn(p, &value);
                bench_checksum += *(unsigned long long*)&value;
            }
        } else if (tt == TEST_PUSHPOP_ALT) {           // push/pop alterné
            for (size_t i = 0; i < N; ++i) {
                init_val(&value, i);
                empiler_fn(p, &value);
                depiler_fn(p, &value);
                bench_checksum += *(unsigned long long*)&value;
            }
        } else if (tt == TEST_PUSH_THEN_POP) {         // puis vide complètement
            for (size_t i = 0; i < N; ++i) {
                init_val(&value, i);
                empiler_fn(p, &value);
            }
            for (size_t i = 0; i < N; ++i) {
                depiler_fn(p, &value);
                bench_checksum += *(unsigned long long*)&value;
            }
        }

        double t1 = now_seconds();
        double elapsed = t1 - t0;
        add_sample(out, elapsed, &n, &mean, &M2);
        detruire_fn(p);
    }
    finalize(out, mean, M2, n);
}

/* Helper to get opposite implementation name */
static const char* opposite_impl(const char* name)
{
    return (strcmp(name, "tableau") == 0) ? "liste" : "tableau";
}

/* Read average from CSV file for given config; returns -1 if not found */
static double read_other_average(const char* other_impl,
                                 const char* type_name,
                                 const char* test_name,
                                 size_t N)
{
    char filename[260];
    sprintf(filename, "bench_%s.csv", other_impl);
    FILE* f = fopen(filename, "r");
    if (!f) return -1.0;
    char line[1024];
    /* skip header */
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        char impl[64], type[64], test[64];
        unsigned long n;
        double min, avg, max, stddev;
        if (sscanf(line, "%63[^,],%63[^,],%63[^,],%lu,%lf,%lf,%lf,%lf",
                   impl, type, test, &n, &min, &avg, &max, &stddev) == 10) {
            if (strcmp(impl, other_impl) == 0 &&
                strcmp(type, type_name) == 0 &&
                strcmp(test, test_name) == 0 &&
                n == (unsigned long)N) {
                fclose(f);
                return avg;
            }
        }
    }
    fclose(f);
    return -1.0;
}

/* ------------------- main ------------------- */
int main(void)
{
    init_timer();

    size_t Ns[]      = {1000, 10000};
    size_t nNs       = sizeof Ns / sizeof *Ns;
    const char* nomsN[] = {"1e3","1e4"};

    /* types d'elements with their sizes and init functions */
    struct {
        const char* name;
        size_t      size;
        void (*init)(void*, size_t);
    } types[] = {
        {"int",   sizeof(int),   init_int}
    };
    size_t nTypes = sizeof types / sizeof *types;

    test_type tests[] = {TEST_PUSH, TEST_POP};
    const char* testNames[] = {
        "N pushes",
        "N pops (après remplissage)"
    };
    size_t nTests = sizeof tests / sizeof *tests;

    /* Prepare CSV file names */
    char csv_name[260];
    sprintf(csv_name, "bench_%s.csv", impl_name);
    FILE* csv_csv = fopen(csv_name, "w");
    if (!csv_csv) {
        fprintf(stderr, "Impossible d'ouvrir %s pour écriture\n", csv_name);
        return 1;
    }
    fprintf(csv_csv, "Implémentation,Type,Opération,N,Temps_min(ms),Temps_moyen(ms),Temps_max(ms),Ecart-type(ms)\n");

    /* Print header for terminal table */
    printf("%-12s | %-8s | %-22s | %-6s | %-10s | %-10s | %-10s | %-12s | %-8s\n",
           "Implémentation", "Type", "Opération", "N", "Min (ms)", "Moyenne (ms)", "Max (ms)", "Écart-type (ms)", "Ratio L/T");
    printf("%s\n", "-------------------------------------------------------------------------------------------------------------------");

    /* Loop over implementations? Actually we only have one impl linked,
       but we will compute ratio by looking at the other CSV if exists. */
    for (size_t ii = 0; ii < nTests; ++ii) {
        for (size_t ty = 0; ty < nTypes; ++ty) {
            for (size_t nn = 0; nn < nNs; ++nn) {
                if (types[ty].size == 64 && Ns[nn] == 10000000) continue;
                stats_t s = {0};
                run_test(creer, detruire, empiler, depiler,
                         types[ty].size, Ns[nn], tests[ii], types[ty].init, &s);

                /* Convert seconds to milliseconds */
                double min_ms = s.min * 1000.0;
                double avg_ms = s.avg * 1000.0;
                double max_ms = s.max * 1000.0;
                double stddev_ms = s.stddev * 1000.0;

                /* Attempt to read opposite impl average for ratio */
                double other_avg_ms = -1.0;
                const char* other = opposite_impl(impl_name);
                other_avg_ms = read_other_average(other,
                                                  types[ty].name,
                                                  testNames[ii],
                                                  Ns[nn]);
                double ratio = (other_avg_ms >= 0.0) ? (other_avg_ms / avg_ms) : -1.0;

                /* Write to CSV */
                fprintf(csv_csv, "%s,%s,%s,%lu,%.6f,%.6f,%.6f,%.6f\n",
                        impl_name,
                        types[ty].name,
                        testNames[ii],
                        (unsigned long)Ns[nn],
                        min_ms, avg_ms, max_ms, stddev_ms);

                /* Print formatted row */
                printf("%-12s | %-8s | %-22s | %-6s | %10.6f | %10.6f | %10.6f | %12.6f | ",
                       impl_name,
                       types[ty].name,
                       testNames[ii],
                       nomsN[nn],
                       min_ms, avg_ms, max_ms, stddev_ms);
                if (ratio >= 0.0) {
                    printf("%8.3f\n", ratio);
                } else {
                    printf("%8s\n", "N/A");
                }
                printf("Test completed: %s %s %s N=%s\n", impl_name, types[ty].name, testNames[ii], nomsN[nn]);
            }
        }
    }

    fclose(csv_csv);
    printf("\nRésultats écrits dans %s\n", csv_name);
    return 0;
}