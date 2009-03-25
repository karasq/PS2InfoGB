// USB Mass Support
// Author: Krystian 'KarasQ' Karas
// Contact: k4rasq@gmail.com

#include <stdio.h>
#include "mass.h"

////////////// KarasQ: USB Mass functions ////////////// 
void clean_mass_store_data(DATA_FILES* mass_tab) {
   int i;
   for (i = 0; i < ARRAY_ENTRIES; i++) {
      strcpy(mass_tab[i].name, "\0");
      mass_tab[i].is_dir = 0;
   }
}

int read_mass(const char* path, DATA_FILES* mass_tab, int max_results)
{
   fio_dirent_t record;
   int current_result = 0, dirs = -1;

   if ( ( dirs = fioDopen(path) ) >= 0 ) {
      while ( fioDread(dirs, &record) > 0 ) {
         if ( FIO_SO_ISDIR(record.stat.mode) ) {
            // Skip entry if pseudo-folder "." or ".."
            if ( !strcmp(record.name, ".") || !strcmp(record.name, "..") ) {
               continue;
            }
            // Mark entry as a folder
            mass_tab[current_result].is_dir = 1;
         } else if ( !FIO_SO_ISREG(record.stat.mode) ) {
            // Skip entry which is neither a file nor a folder
            continue;
         }
         
         strcpy(mass_tab[current_result].name, record.name);
         current_result++;
         
         if ( current_result == max_results ) {
            break;
         }
      }
      
      // Close directory
      fioDclose(dirs);
   }
   return current_result;
}

////////////// KarasQ: c-string functions //////////////
int strtolower_to(char *dest, const char *src, int length) {
   int i = 0;
   if ( length > 0 ) {
      while ( *src && length > i ) {
         dest[i] = tolower(*src);
         src++; i++;
      }
      dest[i] = 0;
      return 1;
   } else {
      return 0;
   }
}

char* strtolower(const char *src, int length) {
   int i = 0;
   char* buffer = (char*) malloc(sizeof(char) * length + 1);
   if ( length > 0 ) {
      while ( *src && length > i ) {
         buffer[i] = tolower(*src);
         src++; i++;
      }
      buffer[i] = 0;
   } else {
      return 0;
   }
   return buffer;
}
/////////////////////////////////////////////////
