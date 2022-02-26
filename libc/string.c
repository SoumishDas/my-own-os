#include "string.h"
#include <stdint.h>
#include "mem.h"
/**
 * K&R implementation
 */
void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void hex_to_ascii(int n, char str[]) {
    append(str, '0');
    append(str, 'x');
    char zeros = 0;

    int32_t tmp;
    int i;
    for (i = 28; i > 0; i -= 4) {
        tmp = (n >> i) & 0xF;
        if (tmp == 0 && zeros == 0) continue;
        zeros = 1;
        if (tmp > 0xA) append(str, tmp - 0xA + 'a');
        else append(str, tmp + '0');
    }

    tmp = n & 0xF;
    if (tmp >= 0xA) append(str, tmp - 0xA + 'a');
    else append(str, tmp + '0');
}

/* K&R */
void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* K&R */
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);
    s[len-1] = '\0';
}

/* K&R 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2 */
int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

void strpartialcpy(char dest[],char src[], int nbytes)
{
    int i = 0;
    while (i<nbytes)
    {
        dest[i] = src[i];

        if (dest[i] == '\0')
        {
            break;
        }

        i++;
    }
    dest[nbytes]='\0';
}

void strcpy(char dest[], const char source[])
{
    int i = 0;
    while (1)
    {
        dest[i] = source[i];

        if (dest[i] == '\0')
        {
            break;
        }

        i++;
    } 
}

int strsplitwords(char s1[],char *wordlist[]){
    int i,tempIndex,lastSpaceIndex,wordlistIndex;
    i=0;
    tempIndex=0;
    lastSpaceIndex=-1;
    wordlistIndex=0;
    

    
    while (1) {
        
        if ((s1[i] == ' ' || s1[i] == '\0') && i!=0 && (i-lastSpaceIndex>1)){
            kprint("x");
            tempIndex = lastSpaceIndex+1;
            uint8_t *ptr= kmalloc(i-tempIndex);
            
            wordlist[wordlistIndex] = ptr;
            strpartialcpy(ptr,&s1[tempIndex],i-tempIndex);
            
            lastSpaceIndex = i;
            wordlistIndex++;     
        }
        i++;
        if(i>0){if(s1[i-1] == '\0'){break;}}
    }
    return wordlistIndex;
}

unsigned int ascii_to_int(char str[] )
{ 
    // Initialize a variable 
    int num = 0; 
    int n = strlen(str); 
  
    // Iterate till length of the string 
    for (int i = 0; i < n; i++){
  
        // Subtract 48 from the current digit 
        num = num * 10 + ((int)(str[i]) - 48); 
    }
    // Print the answer 
    n = num; 
} 

char *boolstring( _Bool b ) { return b ? "true" : "false"; }