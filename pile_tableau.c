#define PILE_EXPORTS
#include "pile.h"
#include <stdlib.h>
#include <string.h>

struct pile_t {
    void*   data;
    size_t  elemSize;
    size_t  capacity;
    size_t  top;
};

static int grow(pile_t* p)
{
    size_t newCap = p->capacity ? p->capacity * 2 : 1;
    void*   newData = realloc(p->data, newCap * p->elemSize);
    if (!newData) return 0;
    p->data   = newData;
    p->capacity = newCap;
    return 1;
}

PILE_API pile_t* creer(size_t elemSize)
{
    pile_t* p = malloc(sizeof *p);
    if (!p) return NULL;
    p->elemSize = elemSize;
    p->capacity = 0;
    p->top      = 0;
    p->data     = NULL;
    return p;
}

PILE_API void detruire(pile_t* p)
{
    free(p->data);
    free(p);
}

PILE_API int empiler(pile_t* p, const void* valeur)
{
    if (p->top == p->capacity && !grow(p)) return 0;
    memcpy((char*)p->data + p->top * p->elemSize, valeur, p->elemSize);
    p->top++;
    return 1;
}

PILE_API int depiler(pile_t* p, void* valeur)
{
    if (p->top == 0) return 0;
    p->top--;
    memcpy(valeur, (char*)p->data + p->top * p->elemSize, p->elemSize);
    return 1;
}

PILE_API int sommet(const pile_t* p, void* valeur)
{
    if (p->top == 0) return 0;
    memcpy(valeur, (char*)p->data + (p->top-1) * p->elemSize, p->elemSize);
    return 1;
}

PILE_API int est_vide(const pile_t* p) { return p->top == 0; }
PILE_API size_t taille(const pile_t* p) { return p->top; }

/* nom de l'implémentation pour le benchmark */
const char* impl_name = "tableau";