/*
 *  InfoGB - A portable GameBoy emulator
 *  Copyright (C) 2003  Jay's Factory <jays_factory@excite.co.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  based on gbe - gameboy emulator
 *  Copyright (C) 1999  Chuck Mason, Steven Fuller, Jeff Miller
 */

#include "system.h"

#ifdef WIN32
#    define PATH_SEPARATOR '\\'
#else
#    define PATH_SEPARATOR '/'
#endif

#include "rom.h"
#include "mem.h"
#include "data.h"
#include "RTC.h"

#include <tamtypes.h>
#include <string.h>
#include <malloc.h>
#include <fileio.h>

int color_gameboy = 0;
int super_gameboy = 0;
char tempname[128];

extern int  hostlist;
extern int DEVICE_FLAG;  // cdrom =1 mc =0 mass=2

unsigned char *cartridge_rom     = (unsigned char *)NULL;
unsigned char *cartridge_ram     = (unsigned char *)NULL;
         char *cartridge_fname   = (char *)NULL;

cartsize       cartridge_size    = SIZE_Unknown;
carttype       cartridge_type    = TYPE_Unknown;
mbc1type       cartridge_mbc1    = MBC1_Unused;
ramsize        cartridge_ramsize = (ramsize)SIZE_Unknown;

extern "C"{
#include "ps2/lib/testz/unzip/unzip.h"
//unzFile *fd2;
unz_file_info info;
//u8 buf[0x400000];
}

int check_zip(char *filename)
{
    char *extension = strrchr(filename, '.');
	if(!extension)
		return 0;

	if(!strcasecmp(extension, ".zip"))return 1;
	else if(!strcasecmp(extension, ".ZIP"))return 1;
	else return 0;
}
         
int load_rom(char *filename)
{
   //FILE *fp;
   unsigned long file_length;
   // char *ptr;
   // int i;
   
   /*
     if((fp = fopen(filename, "rb")) == NULL) 
          return 0; */
   int fd, fd_size;
   // int n;	
   int ret = 0;
   int ok = 0;
   char tmpnom[0x100];    
      
   if ( check_zip(filename) ) { 
      unzFile fd2 = unzOpen(filename);
      
      if ( fd2 == NULL ) return (1);
      
      // Go to first file in archive 
      ret = unzGoToFirstFile(fd2);
      
      if ( ret != UNZ_OK ) {
         unzClose(fd2);
         return (1);
      }
      
      // Get information on the file 
      ret = unzGetCurrentFileInfo(fd2, &info, tmpnom, 0x100, NULL, 0, NULL, 0);
      if ( ret != UNZ_OK ) {
         unzClose(fd2);
         return (1);
      }
      
      //Open the file for reading 
      ret = unzOpenCurrentFile(fd2);
      if ( ret != UNZ_OK ) {
         unzClose(fd2);
         return (1);
      }
      
      // Allocate file data buffer 
      fd_size = info.uncompressed_size;
      
      // Read (decompress) the file 
      // cartridge_rom = (unsigned char *)malloc(fd_size);
      cartridge_rom = (u8 *)memalign(64, fd_size);
      ret = unzReadCurrentFile(fd2,/*(char *)*/cartridge_rom, info.uncompressed_size);
      
      if ( ret != info.uncompressed_size ) {
         //free(buf2);
         unzCloseCurrentFile(fd2);
         unzClose(fd2);
         return (1);
      }
      //printf("zip decomp %d \n",(int)info.uncompressed_size);
      
      // Close the current file
      ret = unzCloseCurrentFile(fd2);
      if ( ret != UNZ_OK ) {
         unzClose(fd2);
         return (1);
      }
      // printf("zip close file\n");
      
      // Close the archive 
      ret = unzClose(fd2);
      if ( ret != UNZ_OK ) {
         return (1);
      }
      // printf("zip close archive\n");
      
      // Check for 512-byte header
      // ok = 0;
      // cartridge_rom = (unsigned char *)malloc(fd_size);
      // for(n = 0; n <fd_size;n++)cartridge_rom[n]=buf[n+ok];  
      printf("zip header / rom copy %d %d %s \n",fd_size,ok,tmpnom);
   } else {
      fd = fioOpen(filename, O_RDONLY);
      if ( fd <= 0 ) {
         //	display_error("Error opening file.",0);
         printf("%s not found.\n",filename);
         return 0;
      }
      
      file_length = fioLseek(fd,0,SEEK_END);
      fioLseek(fd,0,SEEK_SET);
      
      // fseek(fp, 0, SEEK_END);
      // file_length = ftell(fp);
      // rewind(fp);
      
      cartridge_rom = (unsigned char *)malloc(file_length);
      fioRead(fd, (char *)cartridge_rom, file_length);
      
      // fread((char *)cartridge_rom, file_length, 1, fp);
      // fclose(fp);
      fioClose(fd);
   }
   
   if ( hostlist ) {
      strcpy(tempname,filename);  
      
      fd = 0;
      while( (char) tempname[fd] != '.' ){
         fd++;
      }
      
      tempname[fd]='\0';
      printf("t:%s,\n",tempname);
      cartridge_fname = (char *)tempname;
   } else { 
      char stemp[120],stemp2[120];
      int u, u2;
      
      u = 0;
      u2 = 0;
      
      strcpy(tempname,filename);  
      
      while ( tempname[u] != '/' )
         u++;
         
      if ( DEVICE_FLAG==1 ) {
         u++;
         while ( tempname[u] != '/' )u++;
      }
      
      while (tempname[u] != '.') {	 
         stemp[u2]=tempname[u];
         u++;u2++;
      }
      
      stemp[u2] = '\0';
      
      sprintf(stemp2, "mc0:PS2GB%s", stemp);
      strcpy(tempname, stemp2);
      
      cartridge_fname = (char *)tempname;
   }
   // cartridge_fname[i+1]='\0';
   
   // fd++;
   // for(fd=0;fd<16;fd++)
   // strcpy(cartridge_fname,(char *)filename);
   
   printf("Cool:%s\n",cartridge_fname);
   printf("Cartridge Name: [");
   
   // fwrite((char *)&cartridge_rom[0x134], 0x0F, 1, stdout);
   
   for ( fd = 0; fd < 16; fd++ ) 
      printf("%c", cartridge_rom[0x134+fd]);
   printf("]\n");
   
   if ( (cartridge_rom[0x143] == 0x80) || (cartridge_rom[0x143] == 0xC0) )
      color_gameboy = 1;
   
   if ( cartridge_rom[0x146] == 0x03 )
      super_gameboy = 1;
   
   if ( force_system == REGULAR ) {
      //super_gameboy = 0;
      color_gameboy = 0;
   } else if ( force_system == COLOR ) {
      //super_gameboy = 0;
      color_gameboy = 1;
   }
   
   if ( color_gameboy )
      printf("Color Gameboy");
   else
      printf("Gameboy");
      
   if ( super_gameboy )
      printf("+Super Gameboy Support");
   
   printf(" Mode\n");  
   
   printf("Cart Type: ");
     switch(cartridge_rom[0x147]) {
          case 0x00:
               printf("ROM Only\n");
               cartridge_type = TYPE_ROM_Only;
               break;
          case 0x01:     /* MBC1 used */
               printf("ROM+MBC1\n");
               cartridge_type = TYPE_ROM_MBC1;
               cartridge_mbc1 = MBC1_16M_8K; /* Defaults .. */
               break;
          case 0x02:
               printf("ROM+MBC1+RAM\n");
               cartridge_type = TYPE_ROM_MBC1_RAM;
               cartridge_mbc1 = MBC1_16M_8K;
               break;
          case 0x03:
               printf("ROM+MBC1+RAM+BATTERY\n");
               cartridge_type = TYPE_ROM_MBC1_RAM_BATTERY;
               cartridge_mbc1 = MBC1_16M_8K;
               break;
          case 0x05:
               printf("ROM+MBC2\n");
               cartridge_type    = TYPE_ROM_MBC2;
               cartridge_ramsize = RAM_SIZE_2KB;  /* really 256 bytes */
               break;
          case 0x06:
               printf("ROM+MBC2+BATTERY\n");
               cartridge_type    = TYPE_ROM_MBC2_BATTERY;
               cartridge_ramsize = RAM_SIZE_2KB;
               break;
          // **** KarasQ: New cartridges support ****
          case 0x0F:
               printf("ROM+MBC3+TIMER+BATTERY\n");
               cartridge_type = TYPE_ROM_MBC3_TIMER_BATTERY;
               break;
          case 0x10:
               printf("ROM+MBC3+TIMER+RAM+BATTERY\n");
               cartridge_type = TYPE_ROM_MBC3_TIMER_RAM_BATTERY;
               break;
          //
          case 0x11:
               printf("ROM+MBC3\n");
               cartridge_type = TYPE_ROM_MBC3;
               break;
          case 0x12:
               printf("ROM+MBC3+RAM\n");
               cartridge_type = TYPE_ROM_MBC3_RAM;
               break;
          case 0x13:
               printf("ROM+MBC3+RAM+BATTERY\n");
               cartridge_type = TYPE_ROM_MBC3_RAM_BATTERY;
               break;
          case 0x19:
               printf("ROM+MBC5\n");
               cartridge_type = TYPE_ROM_MBC5;
               break;
          case 0x1A:
               printf("ROM+MBC5+RAM\n");
               cartridge_type = TYPE_ROM_MBC5_RAM;
               break;
          case 0x1B:
               printf("ROM+MBC5+RAM+BATTERY\n");
               cartridge_type = TYPE_ROM_MBC5_RAM_BATTERY;
               break;
          case 0x1C:
               printf("ROM+MBC5+RUMBLE\n");
               cartridge_type = TYPE_ROM_MBC5;
               break;
          case 0x1D:
               printf("ROM+MBC5+RAM+RUMBLE\n");
               cartridge_type = TYPE_ROM_MBC5_RAM;
               break;
          case 0x1E:
               printf("ROM+MBC5+RAM+BATTERY+RUMBLE\n");
               cartridge_type = TYPE_ROM_MBC5_RAM_BATTERY;
               break;
          // **** KarasQ: New cartridge support ****
          case 0xFE:
               printf("TYPE+ROM+HUDSON+HUC3\n");
               cartridge_type = TYPE_ROM_HUDSON_HUC3;
               break;
          //
          default:
               printf("Unknown!\n");
               cartridge_type = TYPE_Unknown;
               break;
     }
     
     if(cartridge_type == TYPE_Unknown) {
          printf("Error! Unknown cartridge type %d!\n", cartridge_rom[0x147]);
          return 0;
     }
     
     printf("Cart Size: ");
     switch(cartridge_rom[0x148]) {
          case 0x00:
               printf("32K\n");
               cartridge_size = SIZE_32k;
               break;
          case 0x01:
               printf("64K\n");
               cartridge_size = SIZE_64k;    /* This one's got 4 */
               break;
          case 0x02:
               printf("128K\n");
               cartridge_size = SIZE_128k;   /* This rom has 8 16 banks */
               break;
          case 0x03:
               printf("256K\n");
               cartridge_size = SIZE_256k;   /* This one's got 16 */
               break;
          case 0x04:
               printf("512K\n");
               cartridge_size = SIZE_512k;
               break;
          case 0x05:
               printf("1MB\n");
               cartridge_size = SIZE_1M;
               break;
          case 0x06:
               printf("2MB\n");
               cartridge_size = SIZE_2M;
               break;
          case 0x07:
               printf("4MB\n");
               cartridge_size = SIZE_4M;
               break;
          case 0x08:
               printf("8MB\n");
               cartridge_size = SIZE_8M;
               break;
          default:
               cartridge_size = SIZE_Unknown;
               printf("Unknown\n");
     }
     
     if(cartridge_size == SIZE_Unknown) {
          printf("Error! Unknown cartridge size: %02X!\n", cartridge_rom[0x148]);
          return -1;
     }

     if(cartridge_rom[0x149] != 0x00) {
          printf("Ram Size: ");
          switch(cartridge_rom[0x149]) {
               case RAM_SIZE_2KB:
                    printf("2 KB\n");
                    cartridge_ramsize = RAM_SIZE_2KB;
                    break;
               case RAM_SIZE_8KB:
                    printf("8 KB\n");
                    cartridge_ramsize = RAM_SIZE_8KB;
                    break;
               case RAM_SIZE_32KB:
                    printf("32 KB\n");
                    cartridge_ramsize = RAM_SIZE_32KB;
                    break;
               case RAM_SIZE_128KB:
                    printf("128 KB\n");
                    cartridge_ramsize =  RAM_SIZE_128KB;
                    break;
               default:
                    printf("Unknown (%d)\n", cartridge_rom[0x149]);
                    cartridge_ramsize = (ramsize)SIZE_Unknown;
                    break;
          }
          if(cartridge_ramsize == SIZE_Unknown) {
               printf("Error! Unknown ram size!\n");
               return -2;
          }
     }
   return 1;
}

void initialize_rom()
{
     install_rom();
     install_ram();
}

void free_rom()
{
     if(cartridge_rom)
          free(cartridge_rom);
          
     rom_save_ram();
     
     if(cartridge_ram)
          free(cartridge_ram);
          
     if(cartridge_fname)
          free(cartridge_fname);
                    
     cartridge_type = TYPE_Unknown;
     cartridge_size = SIZE_Unknown;
		 cartridge_ramsize = (ramsize)cartridge_size;
     color_gameboy  = 0;
     cartridge_rom  = (unsigned char *)NULL;
     cartridge_ram  = (unsigned char *)NULL;
     cartridge_fname= (char *)NULL;
     
}

void install_rom()
{
     int i;
     
     switch(cartridge_size) {
          case SIZE_32k:
               for(i = 0; i < 8; i++)
                    install_memory(i, cartridge_rom + (i * 0x1000));
               break;
          case SIZE_64k:
          case SIZE_128k:
          case SIZE_256k:          
          case SIZE_512k:          /* All these get bank 0 to $0-$3fff and 1 to $4000-$7fff */
          case SIZE_1M:
          case SIZE_2M:
          case SIZE_4M:
          case SIZE_8M:
               rom_select_bank(0, 0);
               rom_select_bank(1, 4);
               break;
          default:
               return;
     }
}

void install_ram()
{
     if(cartridge_ramsize == SIZE_Unknown)
          return;
     
     switch(cartridge_ramsize) {
          case RAM_SIZE_8KB:
               cartridge_ram = (unsigned char *)malloc(0x2000);
               memset(cartridge_ram, 0xFF, 0x2000);
               break;
          case RAM_SIZE_2KB:
               cartridge_ram = (unsigned char *)malloc(0x800);
               memset(cartridge_ram, 0xFF, 0x0800);
               break;
          case RAM_SIZE_32KB:
               cartridge_ram = (unsigned char *)malloc(0x2000 * 4);
               memset(cartridge_ram, 0xFF, 0x2000 * 4);
               break;
          case RAM_SIZE_128KB:
               cartridge_ram = (unsigned char *)malloc(0x2000 * 16);
               memset(cartridge_ram, 0xFF, 0x2000 * 16);
               break;
          default:
               return;
     }
     
     rom_load_ram();
     rom_select_ram_bank(0);
}

#include "cpu.h"

/*
MBC1: Bank ...
*/
void rom_select_bank(int bank, int wbank)
{
     int i, b;
     unsigned char *rptr;

     switch(cartridge_size) {
          case SIZE_32k:
               return;
          case SIZE_64k:      /* banks 0 - 3 */
               if(bank >= 4) {
                    printf("64k Invalid Bank: %02X\n", bank);
                    return;
               }
               break;
          case SIZE_128k:          /* banks 0 - 7 */
               if(bank >= 8) {
                    printf("test2\n");
                    return;
               }
               break;
          case SIZE_256k:          /* banks 0 - 15 */
               if(bank & 0xF0) {
                    printf("256k Invalid Bank: %d\n", bank);
                    bank &= 0x0F;
                    //return;
               }
               break;
          case SIZE_512k:          /* banks 0 - 31 */
               if(bank >= 32) {
                    printf("test4\n");
                    return;
               }
               break;
          case SIZE_1M:
               if(bank >= 64) {
                    printf("test5\n");
                    return;
               }
               break;
          case SIZE_2M:
               if(bank >= 128) {
                    bank &= 127;
                    //printf("test6\n");
                    //return;
               }
               break;
          case SIZE_4M:
               if (bank >= 256) {
                    printf("test8\n");
                    return;
               }
               break;
          case SIZE_8M:
               if (bank >= 512) {
                    printf("test9\n");
                    return;
               }
               break;
          default:
               return;
     }
     
     /* TODO: make sure only MBC5 (afaik) is only allowed to do this */
     if ((wbank == 4) && (bank == 0) && (cartridge_type < TYPE_ROM_MBC5)) {
          printf("Trying to set bank 1 to cartbank 0\n");
          bank = 1;
     }

     b = bank * 0x4000;
     rptr = &cartridge_rom[b];
     
     for(i = 0; i < 4; i++) {
          install_memory(i + wbank, rptr + (i * 0x1000));
     }
}

void rom_select_ram_bank(int bank)
{
     int b;
     switch(cartridge_ramsize) {
          case SIZE_Unknown:
               return;
          case RAM_SIZE_2KB:
               install_memory(0x0A, cartridge_ram);
               install_memory(0x0B, (char *)NULL);
               return;
          case RAM_SIZE_8KB:
               install_memory(0x0A, cartridge_ram);
               install_memory(0x0B, cartridge_ram + 0x1000);          
               return;
          default:
               break;
     }
     
     b = bank * 0x2000;
     
     install_memory(0x0A, cartridge_ram + b);
     install_memory(0x0B, cartridge_ram + b + 0x1000);
}

void rom_create_sav_path(char *path)
{
#ifndef AMIGA
     const char *GBEDIR = ".gbesav";
     char *var;
     
     if(!(var = getenv("HOME"))) {
          if(!(var = getenv("USER"))) {
               //sprintf(path, ".%c", PATH_SEPARATOR);
               return;
          } else {
               sprintf(path, "/home/%s/%s/", var, GBEDIR);
          }
     } else {
          sprintf(path, "%s/%s/", var, GBEDIR);
     }
     return;
#else
     strcpy(path, "");
#endif
}

void rom_save_ram()
{
     char filename[512];
     int fdo;

     /* Note all all these have 2 things in common:
          RAM and BATTERY ..
     */   
     switch(cartridge_type) {
          case TYPE_ROM_MBC1_RAM_BATTERY:
          case TYPE_ROM_MBC2_BATTERY:        /* MBC2 has ram in it */
          case TYPE_ROM_MBC3_RAM_BATTERY:
          case TYPE_ROM_MBC3_TIMER_BATTERY:     // KarasQ: new cartridge type 
          case TYPE_ROM_MBC3_TIMER_RAM_BATTERY: // KarasQ: new cartridge type       
          case TYPE_ROM_MBC5_RAM_BATTERY:
          case TYPE_ROM_HUDSON_HUC3:            // KarasQ: new cartridge type
               break;
          default:
               return;
     }
     
#if 1
     //rom_create_sav_path(filename);
     
     sprintf(filename,"%s",cartridge_fname);
     strcat(filename, ".sav");
     
   fdo = fioOpen(filename, O_WRONLY | O_CREAT | O_BINARY );
   if ( fdo < 0 ) {
      printf("error ouverture/ecriture %d",fdo);
          return;
   }
   
   switch ( cartridge_ramsize ) {
      case RAM_SIZE_2KB:
         //fwrite(cartridge_ram, 0x400, 2, fptr);
         fioWrite(fdo, cartridge_ram, 0x400*2);
         break;
      case RAM_SIZE_8KB:
         //fwrite(cartridge_ram, 0x400, 8, fptr);
         fioWrite(fdo, cartridge_ram, 0x400*8);
         break;
      case RAM_SIZE_32KB:
         //fwrite(cartridge_ram, 0x400, 32, fptr);
         fioWrite(fdo, cartridge_ram, 0x400*32);
         break;
      case RAM_SIZE_128KB:
         //fwrite(cartridge_ram, 0x400, 128, fptr);
         fioWrite(fdo, cartridge_ram, 0x400*128);
         break;
   }
     
     // Real Time Clock Data is saved if true
     if ( cartridge_type == TYPE_ROM_MBC3_TIMER_RAM_BATTERY ) {
         printf("RTC mode ON... ");
         //RTC.Control = RTC.LControl = 0;
         fioWrite(fdo, &RTC.Seconds, (10 * sizeof(int)) + sizeof(time_t));
     }

     printf("saveram successfully saved to %s\n", filename);
     
     // fflush(fptr);
     // fclose(fptr);
     
     fioClose(fdo);
#endif    
     return;
}

void rom_load_ram()
{
     char filename[512];
     int fdi;
     
     switch(cartridge_type) {
          case TYPE_ROM_MBC1_RAM_BATTERY:
          case TYPE_ROM_MBC2_BATTERY:
          case TYPE_ROM_MBC3_RAM_BATTERY:
          case TYPE_ROM_MBC3_TIMER_BATTERY:     // KarasQ: new cartridge type 
          case TYPE_ROM_MBC3_TIMER_RAM_BATTERY: // KarasQ: new cartridge type       
          case TYPE_ROM_MBC5_RAM_BATTERY:
          case TYPE_ROM_HUDSON_HUC3:            // KarasQ: new cartridge type
               break;
          default:
               return;
     }

#if 1
     //rom_create_sav_path(filename);
     
   strcpy(filename, cartridge_fname);
   strcat(filename, ".sav");
   
   fdi = fioOpen(filename, O_RDONLY | O_BINARY);
   if ( fdi <= 0 ) {    
      if ( cartridge_type == TYPE_ROM_MBC3_TIMER_RAM_BATTERY ||
           cartridge_type == TYPE_ROM_MBC3_TIMER_BATTERY ) {
            // Reset RTC for MBC3 if it's first run RTC
            RTC.LastTime = timer.getTime();
           
         //struct tm *lt;
         
         //lt = ps2time_localtime(&RTC.LastTime);
         //lt = localtime(&RTC.LastTime);
         
         /*RTC.Seconds = lt->tm_sec;
         RTC.Minutes = lt->tm_min;
         RTC.Hours = lt->tm_hour;
         RTC.Days = lt->tm_yday & 255;
         RTC.Control = (RTC.Control & 0xfe) | (lt->tm_yday > 255 ? 1: 0);*/
         
         RTC.Seconds = 0;
         RTC.Minutes = 0;
         RTC.Hours = 0;
         RTC.Days = 0 & 255;
         RTC.Control = (RTC.Control & 0xfe) | (0 > 255 ? 1: 0);
      }
      return;
   }
     
   switch ( cartridge_ramsize ) {
      case RAM_SIZE_2KB:
         // fread(cartridge_ram, 0x400, 2, fptr);
         fioRead(fdi, cartridge_ram, 2*0x400);
         break;
      case RAM_SIZE_8KB:
         //fread(cartridge_ram, 0x400, 8, fptr);
         fioRead(fdi, cartridge_ram, 8*0x400);
         break;
      case RAM_SIZE_32KB:
         //fread(cartridge_ram, 0x400, 32, fptr);
         fioRead(fdi, cartridge_ram, 32*0x400);
         break;
      case RAM_SIZE_128KB:
         //fread(cartridge_ram, 0x400, 128, fptr);
         fioRead(fdi, cartridge_ram, 128*0x400);
         break;
   }
     
     // Real Time Clock Data is load if true
     if ( cartridge_type == TYPE_ROM_MBC3_TIMER_RAM_BATTERY ||
          cartridge_type == TYPE_ROM_MBC3_TIMER_BATTERY ) {
         printf("RTC mode ON... ");
         fioRead(fdi, &RTC.Seconds, (10*sizeof(int)) + sizeof(int));
     }
     
     printf("saveram successfully read from %s\n", filename);
     
     fioClose(fdi);
#endif
     return;
}
