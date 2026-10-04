#ifndef PILE_H
#define PILE_H

/* Macro controlling import/export */
#if defined PILE_EXPORTS
#  define PILE_API __declspec(dllexport)
#elif defined PILE_STATIC
#  define PILE_API
#else
#  define PILE_API __declspec(dllimport)
#endif

#include <stddef.h>

/* type opaque de pile */
typedef struct pile_t pile_t;

/* création / destruction */
PILE_API pile_t* creer  (size_t elemSize);
PILE_API void    detruire(pile_t* p);

/* opérations classiques */
PILE_API int     empiler (pile_t* p, const void* valeur);
PILE_API int     depiler (pile_t* p, void* valeur);
PILE_API int     sommet  (const pile_t* p, void* valeur);
PILE_API int     est_vide(const pile_t* p);
PILE_API size_t  taille  (const pile_t* p);

#endif /* PILE_H */