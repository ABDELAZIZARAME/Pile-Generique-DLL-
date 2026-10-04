@echo off
setlocal

rem ====================== OPTIONS ======================
set CFLAGS=-O2 -Wall -Wextra
rem ----------------------------------------------------

rem ========== 1. Bibliothèques statiques (pour bench) ==========
echo Compilation des objets statiques...
gcc %CFLAGS% -DPILE_STATIC -c pile_tableau.c -o pile_tableau.o
gcc %CFLAGS% -DPILE_STATIC -c pile_liste.c   -o pile_liste.o
gcc %CFLAGS% -DPILE_STATIC -c bench.c        -o bench.o

rem bench avec tableau
gcc %CFLAGS% bench.o pile_tableau.o -o bench_tableau.exe
rem bench avec liste
gcc %CFLAGS% bench.o pile_liste.o   -o bench_liste.exe

rem ========== 2. Compile bench_dll.c (no special defines) ==========
echo Compilation de bench_dll.c...
gcc %CFLAGS% -c bench_dll.c -o bench_dll.o

rem ========== 3. Versions implicite de bench_dll (liaison avec import lib) ==========
echo Création des version implicite...
gcc %CFLAGS% bench_dll.o libpile_tableau.dll.a -o bench_dll_tableau_implicit.exe
gcc %CFLAGS% bench_dll.o libpile_liste.dll.a   -o bench_dll_liste_implicit.exe

rem ========== 4. Version explicite de bench_dll (chargement exécutif) ==========
echo Création de la version explicite (chargement exécutif)...
gcc %CFLAGS% bench_dll.o -o bench_dll.exe

rem ========== 5. Création des DLL ==========
echo Création des DLL...
gcc %CFLAGS% -DPILE_EXPORTS -shared pile_tableau.c -o pile_tableau.dll ^
    -Wl,--out-implib,libpile_tableau.dll.a
gcc %CFLAGS% -DPILE_EXPORTS -shared pile_liste.c -o pile_liste.dll ^
    -Wl,--out-implib,libpile_liste.dll.a

rem ========== 6. Clients (liaison explicite & implicite) ==========
echo Construction des clients...
gcc %CFLAGS% client_implicit.c libpile_tableau.dll.a -o client_implicit_tableau.exe
gcc %CFLAGS% client_implicit.c libpile_liste.dll.a   -o client_implicit_liste.exe
gcc %CFLAGS% client_explicit.c -o client_explicit.exe

rem ========== 7. Nettoyage des objets intermédiaires (optionnel) ==========
del *.o
echo.
echo Build terminé.
echo.
echo Vous pouvez maintenant lancer :
echo   bench_tableau.exe   > bench_tableau.csv
echo   bench_liste.exe    > bench_liste.csv
echo   bench_dll_tableau_implicit.exe   > bench_dll_tableau_implicit.csv
echo   bench_dll_liste_implicit.exe    > bench_dll_liste_implicit.csv
echo   bench_dll.exe tableau explicit   > bench_dll_tableau_explicit.csv
echo   bench_dll.exe liste explicit     > bench_dll_liste_explicit.csv
echo   client_implicit_tableau.exe
echo   client_implicit_liste.exe
echo   client_explicit.exe
endlocal