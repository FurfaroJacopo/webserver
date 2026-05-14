#include <stdio.h>
#include <string.h>
#include <stdlib.h>
char* string_append(char* string1, char* string2) {
    int len1 = strlen(string1);
    int len2 = strlen(string2);
    int final_len = len1+len2+1;
    char* final_str = (char*)calloc(final_len, sizeof(char));
   strcpy(final_str,string1);
   strcat(final_str,string2);
    return final_str;
}
int main() {
   char* f = string_append("palle", " pelose");
   printf("%s\n", f);
}