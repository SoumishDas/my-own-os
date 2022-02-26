#ifndef STRINGS_H
#define STRINGS_H

void int_to_ascii(int n, char str[]);
void hex_to_ascii(int n, char str[]);
unsigned int ascii_to_int(char str[] );
void reverse(char s[]);
int strlen(char s[]);
void backspace(char s[]);
void append(char s[], char n);
int strcmp(char s1[], char s2[]);
int strsplitwords(char s1[],char *wordlist[]);
void strpartialcpy(char dest[],char src[], int nbytes);
void strcpy(char dest[], const char source[]);
char *boolstring( _Bool b );
#endif
