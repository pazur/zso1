#include <stdlib.h>
#include <stdio.h>

void f__malloc_free(){
    void * x = malloc(42);
    free(x);
}

void g__calloc(){
    calloc(42,42);
}

void h__f_x2(){
    f__malloc_free();
    f__malloc_free();
}

void i(){
}

void j(){
    f__malloc_free();
    g__calloc();
    h__f_x2();
    i();
    j();
}
