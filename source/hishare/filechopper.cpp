// This little utility lets you chop the last <n> bytes off
// of a file easily.  Default is to chop the last 64K off.
// Handy if you've got a big file that BeShare won't resume
// downloading on because the last bit got messed up during
// a disk crash.
//
// Compile thus:  gcc filechopper.cpp
//
// Usage:  filechopper <filename> [numbytestoremove=65536]
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#if __POWERPC__ /* atoll is missing from the PowerPC standard library. */
static long long atoll (const char *str) {
  long long result = 0;
  if (!str || !str[0])
    return 0;
  if (sscanf (str, "%Ld", &result) != 1)
    result = 0;
  return result;
}
#endif


int main(int argc, char ** argv)
{
   const long defaultChop = 65536;
   if ((argc != 3)&&(argc != 2))
   {
      printf("filechopper removes the last (n) bytes from the file you\n");
      printf("specify, which can be useful if your machine/net_server crashed\n");
      printf("during download and it won't resume now (because it has\n");
      printf("garbage bytes in it from the crash)\n");
      printf("usage:  filechopper <filename> [numbytestoremove=%li]\n", defaultChop);
      exit(0);
   }

   off_t count = (argc == 3) ? atoll(argv[2]) : defaultChop;
   FILE * fp = fopen(argv[1], "r");
   if (fp)
   {
      off_t fileLen;
      fseek(fp, 0, SEEK_END);
      // Try to get the 64 bit size of the file, ftell only gives us 32 bits, _ftell does 64 in BeOS but doesn't exist in Haiku, ftello is in Haiku.
#if __BEOS__
      fileLen = _ftell(fp);
#else // Haiku OS.
      fileLen = ftello(fp);
#endif
      fclose(fp);

      int fd = open(argv[1], O_RDWR|O_APPEND); 
      if (fd >= 0)
      {
         off_t newSize = fileLen - count;
         if (newSize >= 0)
         {
            ftruncate(fd, newSize);
            printf("Chopped the last %Li bytes off of [%s], new size is [%Li]\n", (long long int) count, argv[1], (long long int) newSize);
         }
         else printf("File is too short to chop that many bytes off!\n");
      }
      else printf("Error, file [%s] couldn't be opened???\n", argv[1]);
   }
   else printf("Error, file [%s] not found\n", argv[1]);

   return 0;
}
