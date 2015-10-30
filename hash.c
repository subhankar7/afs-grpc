#include<stdio.h>


unsigned long
hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int main(void) {

    char name[80];

    printf("%lu\n", hash("/abc/def/first_file"));


    snprintf(name, 80, "%lu", hash("/abc/def/first_file"));

    printf("%s\n", name);
    return 0;

}
