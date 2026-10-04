#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "pile.h"

typedef pile_t* (*pf_creer)(size_t);
typedef void    (*pf_detruire)(pile_t*);
typedef int     (*pf_empiler)(pile_t*,const void*);
typedef int     (*pf_depiler)(pile_t*,void*);

int main(void)
{
    HMODULE h = LoadLibrary("pile_tableau.dll");   // ou pile_liste.dll
    if (!h) { puts("LoadLibrary échoué"); return 1; }

    pf_creer     creer     = (pf_creer)    GetProcAddress(h, "creer");
    pf_detruire  detruire  = (pf_detruire) GetProcAddress(h, "detruire");
    pf_empiler   empiler   = (pf_empiler)  GetProcAddress(h, "empiler");
    pf_depiler   depiler   = (pf_depiler)  GetProcAddress(h, "depiler");
    if (!creer || !detruire || !empiler || !depiler) {
        puts("GetProcAddress échoué");
        FreeLibrary(h);
        return 1;
    }

    pile_t* p = creer(sizeof(int));
    int v = 99;
    empiler(p, &v);
    depiler(p, &v);
    printf("Valeur dépilée (explicite) : %d\n", v);
    detruire(p);
    FreeLibrary(h);
    return 0;
}