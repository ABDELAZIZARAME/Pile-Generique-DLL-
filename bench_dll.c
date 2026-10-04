/* bench_dll.c – benchmark of the generic stack, usable with:
 *   • static link   (link against pile_*.o)
 *   • implicit DLL  (link against pile_*.dll.a)
 *   • explicit DLL  (LoadLibrary + GetProcAddress)
 *
 * Usage:
 *   bench_dll.exe <impl> <mode>
 *   <impl> : tableau or liste
 *   <mode> : static | implicit | explicit
 *
 * The program prints a readable table to the console and writes
 *   bench_<impl>_<mode>.csv  (same format as bench_*.csv)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pile.h"          /* interface – dllexport/dllimport handled by defines */

/* ------------------------------------------------------------------ */
/*  volontairement identique à bench.c : chronomètre, statistiques, … */
/* ------------------------------------------------------------------ */
static LARGE_INTEGER freq;
static void init_timer(void) { QueryPerformanceFrequency(&freq); }
static double now_seconds(void)
{
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

/* global to prevent optimisation */
volatile unsigned long long bench_checksum = 0;

typedef enum { TEST_PUSH, TEST_POP, TEST_PUSHPOP_ALT, TEST_PUSH_THEN_POP } test_type;
typedef struct { double min, max, avg, stddev; } stats_t;
static void add_sample(stats_t* s, double x, int* n, double* mean, double* M2)
{
    (*n)++; double d = x - *mean; *mean += d/(*n);
    double d2 = x - *mean; *M2 += d*d2;
    if (*n==1) { s->min=s->max=x; }
    else { if (x<s->min) s->min=x; if (x>s->max) s->max=x; }
}
static void finalize(stats_t* s, double mean, double M2, int n)
{
    s->avg = mean;
    s->stddev = (n>1) ? sqrt(M2/(n-1)) : 0.0;
}

/* value buffer – big enough for the largest element (64 bytes) */
typedef union { int i; double d; char gros[64]; } max_elem_t;
static void init_max(max_elem_t* v, size_t index)
{
    memset(v, 0xAB, sizeof(*v));
    /* optional: put index in first byte so values differ */
    v->gros[0] = (char)(index & 0xFF);
}

/* ------------------------------------------------------------------ */
/*  test runner – receives function pointers (set according to mode)   */
/* ------------------------------------------------------------------ */
typedef pile_t* (*pf_creer_func)(size_t);
typedef void    (*pf_detruire_func)(pile_t*);
typedef int     (*pf_empiler_func)(pile_t*,const void*);
typedef int     (*pf_depiler_func)(pile_t*,void*);

static void run_test(pf_creer_func creer, pf_detruire_func detruire,
                     pf_empiler_func empiler, pf_depiler_func depiler,
                     size_t elemSize, size_t N, test_type tt,
                     stats_t* out)
{
    int repeats = (N <= 1000000) ? 10 : 3;
    int n=0; double mean=0.0, M2=0.0;
    max_elem_t value;

    /* warm‑up */
    pile_t* wk = creer(elemSize);
    for (size_t i=0;i<100 && wk; ++i) {
        init_max(&value, i); empiler(wk,&value); depiler(wk,&value);
    }
    detruire(wk);

    for (int r=0; r<repeats; ++r) {
        pile_t* p = creer(elemSize);
        if (!p) { fprintf(stderr,"Allocation échouée\n"); exit(1); }

        double t0 = now_seconds();

        if (tt==TEST_PUSH) {
            for (size_t i=0;i<N;++i) {
                init_max(&value, i); empiler(p,&value);
            }
        } else if (tt==TEST_POP) {
            for (size_t i=0;i<N;++i) { init_max(&value,i); empiler(p,&value); }
            for (size_t i=0;i<N;++i) {
                depiler(p,&value); /* checksum to avoid optimisation */
                bench_checksum += *(unsigned long long*)&value;
            }
        } else if (tt==TEST_PUSHPOP_ALT) {
            for (size_t i=0;i<N;++i) {
                init_max(&value,i); empiler(p,&value);
                depiler(p,&value);
                bench_checksum += *(unsigned long long*)&value;
            }
        } else if (tt==TEST_PUSH_THEN_POP) {
            for (size_t i=0;i<N;++i) { init_max(&value,i); empiler(p,&value); }
            for (size_t i=0;i<N;++i) {
                depiler(p,&value);
                bench_checksum += *(unsigned long long*)&value;
            }
        }

        double t1 = now_seconds();
        add_sample(out, t1-t0, &n, &mean, &M2);
        detruire(p);
    }
    finalize(out, mean, M2, n);
}

/* ------------------------------------------------------------------ */
/*  main – decides which set of function pointers to use                */
/* ------------------------------------------------------------------ */
int main(int argc, char* argv[])
{
    if (argc!=3) {
        fprintf(stderr,
            "Usage: %s <impl> <mode>\n"
            "  <impl> : tableau | liste\n"
            "  <mode> : static | implicit | explicit\n", argv[0]);
        return 1;
    }
    const char* impl_name = argv[1];
    const char* mode_str  = argv[2];
    init_timer();

    /* ---------- choose function pointers ---------- */
    pf_creer_func fn_creer = NULL;
    pf_detruire_func fn_detruire = NULL;
    pf_empiler_func fn_empiler = NULL;
    pf_depiler_func fn_depiler = NULL;

    if (strcmp(mode_str,"static")==0) {
        /* link against the .o files – just use the symbols */
#ifndef EXPLICIT_LINK
        fn_creer   = creer;
        fn_detruire= detruire;
        fn_empiler = empiler;
        fn_depiler = depiler;
#endif
    }
    else if (strcmp(mode_str,"implicit")==0) {
        /* import via DLL – the symbols are already declared dllimport
           in pile.h when neither PILE_STATIC nor PILE_EXPORTS is defined */
#ifndef EXPLICIT_LINK
        fn_creer   = creer;
        fn_detruire= detruire;
        fn_empiler = empiler;
        fn_depiler = depiler;
#endif
    }
    else if (strcmp(mode_str,"explicit")==0) {
        /* Load the appropriate DLL and resolve the four functions */
        char dllname[260];
        sprintf(dllname, "pile_%s.dll", impl_name);
        HMODULE h = LoadLibrary(dllname);
        if (!h) {
            fprintf(stderr,"LoadLibrary %s failed\n", dllname);
            return 1;
        }

        fn_creer = (pf_creer_func)GetProcAddress(h, "creer");
        fn_detruire = (pf_detruire_func)GetProcAddress(h, "detruire");
        fn_empiler = (pf_empiler_func)GetProcAddress(h, "empiler");
        fn_depiler = (pf_depiler_func)GetProcAddress(h, "depiler");
        if (!fn_creer || !fn_detruire || !fn_empiler || !fn_depiler) {
            fprintf(stderr,"GetProcAddress failed for %s\n", dllname);
            FreeLibrary(h);
            return 1;
        }
        /* we keep the module loaded for the whole run */
        /* (freeing at the end is optional – we do it just before exit) */
    }
    else {
        fprintf(stderr,"Unknown mode: %s\n", mode_str);
        return 1;
    }

    /* ---------- benchmark parameters ---------- */
    size_t Ns[] = {1000,10000};
    size_t nNs  = sizeof Ns/sizeof *Ns;
    const char* nomsN[] = {"1e3","1e4"};
    struct { const char* name; size_t size; } types[] = {
        {"int",   sizeof(int)}
    };
    size_t nTypes = sizeof types/sizeof *types;
    test_type tests[] = {TEST_PUSH, TEST_POP};
    const char* testNames[] = {
        "N pushes",
        "N pops (après remplissage)"
    };
    size_t nTests = sizeof tests/sizeof *tests;

    /* ---------- prepare CSV output ---------- */
    char csvname[260];
    sprintf(csvname, "bench_%s_%s.csv", impl_name, mode_str);
    FILE* fcsv = fopen(csvname, "w");
    if (!fcsv) { perror(csvname); return 1; }
    fprintf(fcsv,
        "Implémentation,Type,Opération,N,Temps_min(ms),Temps_moyen(ms),Temps_max(ms),Ecart-type(ms)\n");

    /* ---------- terminal header ---------- */
    printf("%-12s | %-8s | %-22s | %-6s | %-10s | %-10s | %-10s | %-12s\n",
           "Implémentation","Type","Opération","N",
           "Min(ms)","Moyenne(ms)","Max(ms)","Écart-type(ms)");
    printf("%s\n",
        "-----------------------------------------------------------------------------------------------");

    /* ---------- run all configurations ---------- */
    for (size_t ii=0; ii<nTests; ++ii) {
        for (size_t ty=0; ty<nTypes; ++ty) {
            for (size_t nn=0; nn<nNs; ++nn) {
                if (types[ty].size == 64 && Ns[nn] == 10000000) continue;
                stats_t s = {0};
                run_test(fn_creer, fn_detruire, fn_empiler, fn_depiler,
                         types[ty].size, Ns[nn], tests[ii], &s);

                double min_ms = s.min*1000.0;
                double avg_ms = s.avg*1000.0;
                double max_ms = s.max*1000.0;
                double std_ms = s.stddev*1000.0;

                /* CSV line */
                fprintf(fcsv,
                    "%s,%s,%s,%lu,%.6f,%.6f,%.6f,%.6f\n",
                    impl_name,
                    types[ty].name,
                    testNames[ii],
                    (unsigned long)Ns[nn],
                    min_ms, avg_ms, max_ms, std_ms);

                /* terminal line */
                printf("%-12s | %-8s | %-22s | %-6s | %10.6f | %10.6f | %10.6f | %12.6f\n",
                       impl_name,
                       types[ty].name,
                       testNames[ii],
                       nomsN[nn],
                       min_ms, avg_ms, max_ms, std_ms);
                printf("Test completed: %s %s %s N=%s\n", impl_name, types[ty].name, testNames[ii], nomsN[nn]);
            }
        }
    }

    fclose(fcsv);
    if (strcmp(mode_str,"explicit")==0) {
        /* free the DLL loaded for explicit mode */
        char dllname[260];
        sprintf(dllname, "pile_%s.dll", impl_name);
        HMODULE h = GetModuleHandle(dllname);
        if (h) FreeLibrary(h);
    }
    printf("\nRésultats écrits dans %s\n", csvname);
    return 0;
}