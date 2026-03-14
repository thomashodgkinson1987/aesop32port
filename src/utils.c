#include <stdio.h>

#include "utils.h"

// Tom: drop-in replacement for DOS ltoa()
char *ltoa(long val, char *buffer, int radix)
{
   if (radix == 10)
   {
      sprintf(buffer, "%ld", val);
   }
   else if (radix == 16)
   {
      sprintf(buffer, "%lx", val);
   }
   else if (radix == 8)
   {
      sprintf(buffer, "%lo", val);
   }
   else
   {
      // Fallback for weird bases (like base 2)
      sprintf(buffer, "%ld", val);
   }
   return buffer;
}

// Tom: drop-in replacement for DOS ultoa()
char *ultoa(unsigned long val, char *buffer, int radix)
{
   if (radix == 10)
   {
      sprintf(buffer, "%ld", val);
   }
   else if (radix == 16)
   {
      sprintf(buffer, "%lx", val);
   }
   else if (radix == 8)
   {
      sprintf(buffer, "%lo", val);
   }
   else
   {
      // Fallback for weird bases (like base 2)
      sprintf(buffer, "%ld", val);
   }
   return buffer;
}