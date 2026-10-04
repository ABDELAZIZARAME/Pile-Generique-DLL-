#define PILE_EXPORTS
#include "pile.h"
#include <stdlib.h>
#include <string.h>

typedef struct Cell {
    void*          valeur;
    struct Cell*   suivant;
} Cell;

struct pile_t {
    Cell*   tete;
    size_t  elemSize;
    size_t  cnt;
};

PILE_API pile_t* creer(size_t elemSize)
{
    pile_t* p = malloc(sizeof *p);
    if (!p) return NULL;
    p->elemSize = elemSize;
    p->tete    = NULL;
    p->cnt     = 0;
    return p;
}

PILE_API void detruire(pile_t* p)
{
    Cell* cur = p->tete;
    while (cur) {
        Cell* tmp = cur;
        cur = cur->suivant;
        free(tmp->valeur);
        free(tmp);
    }
    free(p);
}

PILE_API int empiler(pile_t* p, const void* valeur)
{
    Cell* nouv = malloc(sizeof *nouv);
    if (!nouv) return 0;
    void* copie = malloc(p->elemSize);
    if (!copie) { free(nouv); return 0; }
    memcpy(copie, valeur, p->elemSize);
    nouv->valeur = copie;
    nouv->suivant = p->tete;
    p->tete = nouv;
    p->cnt++;
    return 1;
}

PILE_API int depiler(pile_t* p, void* valeur)
{
    if (!p->tete) return 0;
    Cell* tmp = p->tete;
    memcpy(valeur, tmp->valeur, p->elemSize);
    p->tete = tmp->suivant;
    free(tmp->valeur);
    free(tmp);
    p->cnt--;
    return 1;
}

PILE_API int sommet(const pile_t* p, void* valeur)
{
    if (!p->tete) return 0;
    memcpy(valeur, p->tete->valeur, p->elemSize);
    return 1;
}

PILE_API int est_vide(const pile_t* p) { return p->tete == NULL; }
PILE_API size_t taille(const pile_t* p) { return p->cnt; }

/* nom de l'implémentation pour le benchmark */
const char* impl_name = "liste";