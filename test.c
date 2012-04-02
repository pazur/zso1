#include <stdio.h>
#include <pthread.h>

#include "libtest.h"
#include "call_cnt.h"

#define TC 100

int run;

void * th(void * arg){ //2 x malloc, 2 x free, 2x f
    int i;
    while(!run){}
    for(i = 0; i < 10000;i++){
        h__f_x2();
    }
}

int main(){
    int res;
    int j;
    struct call_cnt * desc;
    void * retval;
    run = 0;
    res = intercept(&desc, "libtest.so.1");
    pthread_t threads[TC];
    for(j = 0; j < TC; j++){
        if(pthread_create(&threads[j], 0, &th, 0))
            perror("pthread_create");
    }
    run = 1;
    for(j = 0; j < TC; j++){
        pthread_join(threads[j], &retval);
    }
    stop_intercepting(desc);
    f__malloc_free();
    i();
    h__f_x2();
    print_stats_to_stream(stdout, desc);
    printf("intern: %d\n", get_num_intern_calls(desc));
    printf("extern: %d\n", get_num_extern_calls(desc));
    release_stats(desc);
    return 0;
}

