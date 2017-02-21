/* 
 * This file implement some local pratical string manipulation function that 
 * Standard C library not support  
 */
#include<string.h>
#include<ctype.h>
int startwith(char *str, char *prefix)
{
    int len, prelen, i;

    len = strlen(str);
    prelen = strlen(prefix);
    if (len < prelen)
        return 0;
    for (i = 0; i < prelen; i++)
        if (str[i] != prefix[i])
            return 0;
    return 1;
}

void strtolower(char *str)
{
    int len, i;

    len = strlen(str);
    for(i = 0; i < len; i++)
        str[i] = tolower(str[i]);
}

char *igspace(char *str)
{
    int len, i;

    len = strlen(str);
    for (i = 0; i < len; i++)
        if (!isspace(str[i]))
            break;
    return str + i;
}

int isinteger(char *str)
{
    int len, i;

    len = strlen(str);
    for (i = 0; i < len; i++)
        if (!isdigit(str[i]))
            return 0;
    return 1;
}
