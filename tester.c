#include <stdio.h>

enum x {
    AAA = 1,
    BBB = 2,
    // CCC = 3,
    DDD = 4,
};

#define DEFAULT ((enum x)-1)

int main(void) {
    enum x aaa = (enum x)__builtin_lookup_name("AAA", DEFAULT);
    enum x bbb = (enum x)__builtin_lookup_name("BBB", DEFAULT);
    enum x ccc = (enum x)__builtin_lookup_name("CCC", DEFAULT);

    if (aaa != DEFAULT) {
        printf("aaa exists: %d\n", aaa);
    } else {
        printf("aaa does not exist\n");
    }

    if (bbb != DEFAULT) {
        printf("bbb exists: %d\n", bbb);
    } else {
        printf("bbb does not exist\n");
    }

    if (ccc != DEFAULT) {
        printf("ccc exists: %d\n", ccc);
    } else {
        printf("ccc does not exist\n");
    }

    enum x ddd;
    if ((ddd = (enum x)__builtin_lookup_name("DDD", DEFAULT)) != DEFAULT) {
        printf("ddd exists: %d\n", ddd);
    } else {
        printf("ddd does not exist\n");
    }

    return 0;
}
