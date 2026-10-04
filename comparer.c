/* comparer.c – lit bench_tableau.csv et bench_liste.csv,
   calcule le ratio liste/tableau pour chaque ligne identique
   (même Type, Opération, N), affiche un tableau et écrit comparaison.csv */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_FIELD 64

typedef struct {
    char impl[MAX_FIELD];
    char type[MAX_FIELD];
    char op[MAX_FIELD];
    unsigned long N;
    double min, avg, max, stddev;
} record;

/* lecture d'un CSV (en supposant l'en-tête) */
int read_csv(const char* filename, record** out, int* count)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror(filename);
        return 0;
    }
    char line[MAX_LINE];
    /* skip header */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    int capacity = 100;
    int n = 0;
    *out = malloc(capacity * sizeof(record));
    if (!*out) { fclose(f); return 0; }
    while (fgets(line, sizeof(line), f)) {
        if (n >= capacity) {
            capacity *= 2;
            *out = realloc(*out, capacity * sizeof(record));
            if (!*out) { fclose(f); return 0; }
        }
        record* r = &(*out)[n];
        /* format: Implémentation,Type,Opération,N,Temps_min(ms),Temps_moyen(ms),Temps_max(ms),Ecart-type(ms) */
        if (sscanf(line, "%63[^,],%63[^,],%63[^,],%lu,%lf,%lf,%lf,%lf",
                   r->impl, r->type, r->op, &r->N,
                   &r->min, &r->avg, &r->max, &r->stddev) != 8) {
            fprintf(stderr, "Ligne mal formée dans %s : %s", filename, line);
            continue;
        }
        n++;
    }
    fclose(f);
    *count = n;
    return 1;
}

/* recherche d'un enregistrement correspondant dans un tableau */
record* find_record(record* arr, int count,
                    const char* type, const char* op, unsigned long N)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(arr[i].type, type) == 0 &&
            strcmp(arr[i].op, op) == 0 &&
            arr[i].N == N) {
            return &arr[i];
        }
    }
    return NULL;
}

int main(void)
    {
    record* tab = NULL;
    record* lis = NULL;
    int ntab = 0, nlis = 0;
    if (!read_csv("bench_tableau.csv", &tab, &ntab) ||
        !read_csv("bench_liste.csv", &lis, &nlis)) {
        free(tab);
        free(lis);
        return 1;
    }

    /* préparer sortie CSV */
    FILE* outcsv = fopen("comparaison.csv", "w");
    if (!outcsv) {
        perror("comparaison.csv");
        free(tab);
        free(lis);
        return 1;
    }
    fprintf(outcsv, "Type,Opération,N,Ratio_Liste_Tableau\n");

    /* en-tête terminal */
    printf("%-12s | %-22s | %-6s | %-12s\n",
           "Type", "Opération", "N", "Ratio L/T");
    printf("%s\n", "------------------------------------------------------------");

    /* on parcourt le tableau table (ou liste) – on prend le plus petit pour éviter doublons */
    int baseCount = (ntab < nlis) ? ntab : nlis;
    record* base = (ntab < nlis) ? tab : lis;
    const char* baseName = (ntab < nlis) ? "tableau" : "liste";

    for (int i = 0; i < baseCount; ++i) {
        record* r = &base[i];
        record* other = (ntab < nlis) ?
            find_record(lis, nlis, r->type, r->op, r->N) :
            find_record(tab, ntab, r->type, r->op, r->N);
        if (!other) {
            printf("%-12s | %-22s | %-6lu | %-12s\n",
                   r->type, r->op, r->N, "N/A (missing)");
            fprintf(outcsv, "%s,%s,%lu,N/A\n",
                    r->type, r->op, r->N);
            continue;
        }
        double ratio = (r->avg == 0.0) ? 0.0 : (other->avg / r->avg);
        /* On veut toujours ratio = liste / tableau */
        double liste_over_tableau;
        if (strcmp(r->impl, "liste") == 0) {
            liste_over_tableau = r->avg / other->avg;
        } else {
            liste_over_tableau = other->avg / r->avg;
        }
        printf("%-12s | %-22s | %-6lu | %12.6f\n",
               r->type, r->op, r->N, liste_over_tableau);
        fprintf(outcsv, "%s,%s,%lu,%.6f\n",
                r->type, r->op, r->N, liste_over_tableau);
    }

    fclose(outcsv);
    free(tab);
    free(lis);
    printf("\nRésultats écrits dans comparaison.csv\n");
    return 0;
}