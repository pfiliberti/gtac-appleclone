#include <stdio.h>
 
int main()
{
  unsigned char buf[1],disp[9];
  int x=0;
  int y=0;
  FILE * inputFileStream = stdin; 
  FILE * outFileStream = stdout;

  while (!feof(inputFileStream)) {
    fread(buf, 1, 1, inputFileStream);

    for(x=0; x< 7; x++) disp[x] = ' ';
    disp[7] = '\n';
    disp[8] = 0;

    if(buf[0] & 0x02) disp[0] = '#';
    if(buf[0] & 0x08) disp[1] = '#';
    if(buf[0] & 0x010) disp[2] = '#';
    if(buf[0] & 0x020) disp[3] = '#';
    if(buf[0] & 0x040) disp[4] = '#';
    if(buf[0] & 0x080) disp[5] = '#';
    if(buf[0] & 0x04) disp[6] = '#';

    if(!feof(inputFileStream))
    {
     fwrite(disp, 1, 8, outFileStream);
     if(y++ > 6)
     {
       y=0;
       disp[0] = '\n';
       disp[1] = '\n';
       disp[2] = 0x00;
       fwrite(disp, 1, 2, outFileStream);
     }
    }
  }
}

