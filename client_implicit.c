#include <stdio.h>
#include "pile.h"

int main(void)
{
    pile_t* p = creer(sizeof(int));
    if (!p) { puts("Échec création"); return 1; }
    int v = 42;
    empiler(p, &v);
    depiler(p, &v);
    printf("Valeur dépilée : %d\n", v);
    detruire(p);
    return 0;
}