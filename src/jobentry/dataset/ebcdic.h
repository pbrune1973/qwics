#ifndef _ebcdic_h_
#define _ebcdic_h_

extern unsigned char a2e[256];
extern unsigned char e2a[256];

void stre2a(char *str);
void stra2e(char *str);

#endif
