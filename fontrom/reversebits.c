#include <stdio.h>
 
// Function to fix bits of num to match hardware on gtac-2 clone
unsigned int fixBits(unsigned char num)
{
    unsigned char fixed = 0;

    /* test each bit of original and set appropriate in fixed */
    if(num & 0x01) fixed |= 0x04;
    if(num & 0x02) fixed |= 0x80;
    if(num & 0x04) fixed |= 0x40;
    if(num & 0x08) fixed |= 0x20;
    if(num & 0x10) fixed |= 0x10;
    if(num & 0x20) fixed |= 0x08;
    if(num & 0x40) fixed |= 0x02;
    if(num & 0x80) fixed |= 0x01;

    return fixed;
}
 
// Driver code
int main()
{
  unsigned char buf[1];
  FILE * inputFileStream = stdin; 
  FILE * outFileStream = stdout;

  while (!feof(inputFileStream)) {
    fread(buf, 1, 1, inputFileStream);
    buf[0] = fixBits(buf[0]);
    if(!feof(inputFileStream))
      fwrite(buf, 1, 1, outFileStream);
  }
}

