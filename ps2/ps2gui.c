//---------------------------------------------------------------------------
//File name:    ps2gui.c
//---------------------------------------------------------------------------

#include <tamtypes.h>
#include <libpad.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hw.h"
#include "gs.h"
#include "gfxpipe.h"
#include "sjpcm.h"

//
#include "../cpu.h"

//---------------------------------------------------------------------------

#define SCREEN_W 320
#define SCREEN_H 256
#define NWIDTH SCREEN_W
#define NHEIGHT SCREEN_H
//#define TXTMIN 0
//#define TXTMAX 320

#define GS256 8
#define GS320 9
#define GS640 9
#define GS1024 10

#define TextOutC2(x_start, x_end, y, string,  z) textCpixel((x_start)>>4,(x_end)>>4,((y)>>4)+4,GS_SET_RGBA(255,255,255,128),0,0,(z),(string))

#define NES_TEX		0x0A0000 + 0x0A0000
//extern unsigned short WorkFrame[ SCREEN_W * SCREEN_H ] __attribute__((aligned(16))) __attribute__ ((section (".bss")));

extern void textCpixel(int ,int ,int ,unsigned ,int ,int ,int ,char *,...);

extern int endflag,whichdrawbuf ;

extern int STRETCH;   // 0= original res (352x240 cut & 256 with black border)
extern int FILTER;   // 1 = filtering
extern int sound;
extern int dispx, dispy;
//extern  unsigned long dwKeySystem;
//extern int bThread;
extern int CNF_edited;

char boot_ELF[1025] = "mc0:/BOOT/BOOT.ELF";

extern char auto_ROM[1025];
extern char auto_ROM_folder[1025];
extern int auto_ROM_flag;

int ypos[25]; //Must never be less than 'menu_lines'

//---------------------------------------------------------------------------
// The following code is copyrighted by me, (c)2006 Ronald Andersson.
// I release it for fully free use by anyone, anywhere, for any purpose.
// But I still retain the copyright, so no one else can limit this release.
//---------------------------------------------------------------------------
// get_CNF_string analyzes a config file held in a single string, parsing it
// to find one config variable definition per call. The return value is a
// true/false flag, set true if a variable was found, but set false if not.
// This means it also returns false for an empty file.
//
// The config file is passed as the address of a string pointer, which will
// be set to point beyond the variable definition retrieved, but on failure
// it will remain at the point where failure was detected. Thus it will not
// pass beyond the terminating NUL of the string. The original string data
// will be partly slaughtered by the analysis, as new string terminators are
// inserted at end of each variable name, and also at the end of each value
// string. Those new strings are then passed back to the caller through the
// other arguments, these being pointers to string pointers of the calling
// procedure.
//
// So with a variable definition like this: "SomeVar = some bloody string",
// the results are the string pair: "SomeVar" and "some bloody string".
//
// A name must begin with a letter (here ascii > 0x40), so lines that begin
// with other non-whitespace characters will be considered comment lines by
// this function. (Simply ignored.) Whitespace is permitted in the variable
// definitions, and will be ignored if occurring before the value string,
// but once a value begins any whitespace used is considered a part of the
// value string. It will remain intact in the returned results.
//
// Note that the name part can only contain non-whitespace characters, but
// the value part can contain non-leading whitespace different from CR/LF.
// So a value starts with the first non-whitespace character after the '='
// and ends at the end of the line.
//
// Intended usage is to repeatedly call get_CNF_string to retrieve each of
// the variables in the config file, until the function returns false, which
// signals either the end of the file, or a syntax error. Analysis of the
// variables found, and usage of their values, is not dealt with at all.
//
// Such matters are left entirely up to the calling procedures, Which also
// means that caller may decide to allow comments terminating lines with a
// variable definition. That's just one of the many value analysis choices.
//---------------------------------------------------------------------------
int get_CNF_string(unsigned char **CNF_p_p, unsigned char **name_p_p,
                   unsigned char **value_p_p)
{
   unsigned char *np, *vp, *tp = *CNF_p_p;
   
start_line:
   
   while ( (*tp<=' ') && (*tp>'\0') ) tp += 1; //Skip leading whitespace, if any
   if ( *tp == '\0' ) return false;            //but exit at EOF
   np = tp;                                    //Current pos is potential name
   if ( *tp < 'A' ) {                          //but may be a comment line                                      //We must skip a comment line
      while ( (*tp!='\r') && (*tp!='\n') && (tp!='\0') ) tp+=1;  //Seek line end
      goto start_line;                     //Go back to try next line
   }

   while((*tp>='A')||((*tp>='0')&&(*tp<='9'))) tp+=1;  //Seek name end
   if(*tp=='\0')	return false;            //but exit at EOF
   *tp++ = '\0';                          //terminate name string (passing)
   while((*tp<=' ') && (*tp>'\0')) tp+=1; //Skip post-name whitespace, if any
   if(*tp!='=') return false;             //exit (syntax error) if '=' missing
   tp += 1;                               //skip '='
   while((*tp<=' ') && (*tp>'\0')) tp+=1; //Skip pre-value whitespace, if any
   if(*tp=='\0')	return false;            //but exit at EOF
   vp = tp;                               //Current pos is potential value

   while((*tp!='\r')&&(*tp!='\n')&&(tp!='\0')) tp+=1;  //Seek line end
   if(*tp!='\0') *tp++ = '\0';            //terminate value (passing if not EOF)
   while((*tp<=' ') && (*tp>'\0')) tp+=1;  //Skip following whitespace, if any

   *CNF_p_p = tp;                         //return new CNF file position
   *name_p_p = np;                        //return found variable name
   *value_p_p = vp;                       //return found variable value
   return true;                           //return control to caller
}	//Ends get_CNF_string

//---------------------------------------------------------------------------

void Load_CNF(char *CNF_path_p)
{
	int fd, var_cnt, disp_f = 0;
  size_t TST_size, CNF_size;
  char  *RAM_p, *CNF_p, *name, *value;

	fd = fioOpen(CNF_path_p,O_RDONLY); 
	if(fd < 0)	{printf("Load_CNF %s Open failed %d.\r\n", CNF_path_p, fd);	return;	}
	CNF_size = fioLseek(fd, 0, SEEK_END);
	fioLseek(fd, 0, SEEK_SET);
	CNF_p = (RAM_p = (char *)malloc(CNF_size+1));
	if	(CNF_p==NULL)	{	printf("Load_CNF failed malloc(%d).\r\n", CNF_size); return;	}
	TST_size = fioRead(fd, CNF_p, CNF_size);
	fioClose(fd);
	CNF_p[CNF_size] = '\0';

	printf("Load_CNF read %d bytes.\r\n",TST_size);

	for(var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
	{ // A variable was found, now we dispose of its value.
		printf("Found variable \"%s\" with value \"%s\"\r\n", name, value);
		if(!strcmp(name,"dispx"))	{	dispx = atoi(value);	disp_f = true;	}
		else if(!strcmp(name,"dispy")) {	dispy = atoi(value); disp_f = true;	}
		else if(!strcmp(name,"stretch")) STRETCH = atoi(value);
		else if(!strcmp(name,"filter")) FILTER = atoi(value);
		else if(!strcmp(name,"sound")) sound = atoi(value);
		else if(!strcmp(name,"boot_ELF")) strcpy(boot_ELF, value);
		else if(!strcmp(name,"auto_ROM")) strcpy(auto_ROM, value);
	}

	auto_ROM_flag = 0;
	if(auto_ROM[0]){
		name = strchr(auto_ROM, '/');
		value = strrchr(auto_ROM, '/');
		if(value!=NULL && value!=name){
			auto_ROM_flag = 1;
			strcpy(auto_ROM_folder,name+1);
			auto_ROM_folder[value-(name+1)] = '\0';
		}
	}

	if(strlen(CNF_p))  //Was there any unprocessed CNF remainder ?
		CNF_edited = false;  //false == current settings match CNF file
	else
		printf("Syntax error in CNF file at position %d.\r\n", (CNF_p-RAM_p));
	free(RAM_p);

  if(disp_f)		GS_SetDispMode(dispx,dispy,NWIDTH,NHEIGHT);
}	//Ends Load_CNF

//---------------------------------------------------------------------------

void Save_CNF(char *CNF_path_p)
{
	int fd, CNF_error;
  size_t CNF_size = 4096; //safe preliminary value
  char  *CNF_p;

	CNF_error = true;
	CNF_p = (char *)malloc(CNF_size);
	if	(CNF_p == NULL) return;

	sprintf(CNF_p,
		"# INFOGB.CNF == Configuration file for the emulator InfoGB\r\n"
		"# --------------------------------------------------------\r\n"
		"dispx    = %d\r\n"
		"dispy    = %d\r\n"
		"stretch  = %d\r\n"
		"filter   = %d\r\n"
		"sound    = %d\r\n"
		"boot_ELF = %s\r\n"
		"auto_ROM = %s\r\n"
		"# --------------------------------------------------------\r\n"
		"# End-Of-File for InfoGB.CNF\r\n"
		"%n", //NB: The %n specifier causes NO output, but only a measurement
		dispx,
		dispy,
		STRETCH,
		FILTER,
		sound,
		boot_ELF,
		auto_ROM,
		&CNF_size);

// Note that the final argument above measures accumulated string size,
// used for fioWrite below, so it's not one of the config variables.

	fd = fioOpen(CNF_path_p,O_CREAT|O_WRONLY|O_TRUNC); 
	if(fd < 0)	goto abort;
	if(CNF_size == fioWrite(fd, CNF_p, CNF_size))
		CNF_edited = false;
	fioClose(fd);
abort:
	free(CNF_p);
}	//Ends Save_CNF

//---------------------------------------------------------------------------

void display_intro(char* errmsg, int fatal) 
{
	static struct padButtonStatus lpad1;
	static int lpad1_data = 0;
	int cmpt = 0;
	
   while ( 1 ) {
      cmpt++; if( cmpt > 80) { cmpt = 0; }
	
      gp_frect(&thegp,0<<4, 0<<4, 320<<4, 256<<4, 1, GS_SET_RGBA(0, 0, 0, 255));
      gp_gouradrect(&thegp, (31+32-20)<<4, 70<<4,GS_SET_RGBA(0x00, 0x00, 0x40, 128), (20+32+255-31)<<4, (142+64)<<4,  GS_SET_RGBA(0x40,0x40, 0x80, 64), 3);
      
      textCpixel(0, 320, 80, GS_SET_RGBA(255, 255, 255, 255), 0, 0, 4, errmsg);
      textCpixel(0, 320, 80+18+8/*+18*/, GS_SET_RGBA(255, 255, 255, 255), 0,0,4, "InfoGB by 'dlanor' & 'KarasQ' rev.5b");
      textCpixel(0, 320, 80+36+18, GS_SET_RGBA(255, 255, 255, 255),0,0,4, "based on InfoGB by Jay'Factory");
      textCpixel(0, 320, 80+72, GS_SET_RGBA(255, 255, 255, 255),0,0,4, " USB MASS SUPPORT Version");
      
      if ( cmpt < 31) {
         textCpixel(0, (320), 224, GS_SET_RGBA(255, 255, 255, 255), 0, 0, 4," - Push Start - ");
      } else if ( cmpt < 64 ) {
         textCpixel(0,(320),224,GS_SET_RGBA(255, 255, 255,63 - cmpt),0,0,4," - Push Start - ");
      }
      
      textCpixel(0, 320, 190, GS_SET_RGBA(255, 255,255, 255), 0, 0, 4, " 7not6");

		gp_hardflush(&thegp);
		WaitForNextVRstart(1);
    	GS_SetCrtFB(whichdrawbuf);
      whichdrawbuf ^= 1;
      GS_SetDrawFB(whichdrawbuf);

      if ( !fatal ) {
         if ( padGetState(0, 0) == PAD_STATE_STABLE ) {
            padRead(0, 0, &lpad1); // port, slot, buttons
            lpad1_data = 0xffff ^ lpad1.btns;
         }

         if ( lpad1_data & PAD_START ) {
            break;
         }
      }
   }
}

//---------------------------------------------------------------------------

void display_error(char* errmsg, int fatal)
{
   static struct padButtonStatus lpad1;
   static int lpad1_data = 0;

   while ( 1 ) {
      gp_frect(&thegp,0<<4, 0<<4, 320<<4, 256<<4, 1, GS_SET_RGBA(0, 0, 0, 64));
      gp_frect(&thegp, (96)<<4, 92<<4, (320-96)<<4, 172<<4, 2, GS_SET_RGBA(98, 92, 210, 128));
      
   // gp_gouradrect(&thegp, (31+32-20)<<4, 70<<4,GS_SET_RGBA(0x00, 0x00, 0x40, 128), (20+32+255-31)<<4, (142+64)<<4,  GS_SET_RGBA(0x40,0x40, 0x80, 64), 3);
      gp_gouradrect(&thegp, (96)<<4, 92<<4, GS_SET_RGBA(0x00, 0x00, 0x40, 128), (320-96)<<4, 172<<4, GS_SET_RGBA(0x40,0x40, 0x80, 64), 3);

      TextOutC2(0<<4, 320<<4, 108<<4, errmsg, 5);
      
      if ( !fatal ) {
         TextOutC2(0<<4, 320<<4, 144<<4, " - Push Start - ", 5);
      }

      gp_hardflush(&thegp);
      WaitForNextVRstart(1);
      GS_SetCrtFB(whichdrawbuf);
      whichdrawbuf ^= 1;
      GS_SetDrawFB(whichdrawbuf);

      if ( !fatal ) {
         if( padGetState(0, 0) == PAD_STATE_STABLE ) {
            padRead(0, 0, &lpad1); // port, slot, buttons
            lpad1_data = 0xffff ^ lpad1.btns;
         }

         if ( lpad1_data & PAD_START ) { 
            break; 
         }
      }
   }
}

//---------------------------------------------------------------------------
void Reboot_PS2( char* apPath) {
   printf("Reboot_PS2 starting\r\n");
   RunLoaderElf(apPath, "");
}
//---------------------------------------------------------------------------

void IngameMenu()
{
   static struct padButtonStatus mpad1;
   static int mpad1_data = 0;
	
   int old_pad = 0;
   int new_pad;
	
   int selection = 0;
   int menu_lines = 11;
	
   int i, menu_top_y, line_height, menu_height, menu_bot_y;
   char tmps[1025];

   if ( sound ) { SjPCM_Pause(); }

   for (line_height = 18; line_height * menu_lines > 240; line_height -= 2);
      menu_top_y = (256 - line_height * menu_lines) / 2;
	  
   for (i=0; i<menu_lines; i++)
      ypos[i] = (menu_top_y+i*line_height)<<4;
	
   menu_height = menu_lines*line_height;
   menu_bot_y = menu_top_y+menu_height;

   while ( 1 ) {	//Start of main menu loop
      // All this probably isnt necessary.. eh..
		
      //	gp_uploadTexture(&thegp, PCE_TEX, 640, 0, 0, 0x02, &bmp, 640, 256);
      // gp_setTex(&thegp, PCE_TEX, 640, 640, 256, 0x02, 0, 0, 0,FILTER);
  	
      // gp_uploadTexture(&thegp, NES_TEX, 256, 0, 0, 0x02,&WorkFrame, 256, 240);
      //	gp_setTex(&thegp, NES_TEX, 256, 8, GS256, 0x02, 0, 0, 0,FILTER);
  
      if ( STRETCH == 2 )  {
         gp_texrect(&thegp, 0<<4,16<<4, 0<<4, 0<<4,
           (320)<<4, (256-16)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));
      } else if ( STRETCH == 1 ) {
         gp_texrect(&thegp, 40<<4,20<<4, 0<<4, 0<<4,
           (240+40)<<4, (216+20)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));   
		} else {
         gp_texrect(&thegp, 80<<4,56<<4, 0<<4, 0<<4,
           (160+80)<<4, (144+56)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));
      }

      // Shade emu display
      gp_frect(&thegp, 0, 0<<4, 320<<4, 256<<4, 2, GS_SET_RGBA(0, 0, 0, 64));
      
      gp_gouradrect(&thegp,
         (96-16)<<4, (menu_top_y+2)<<4, GS_SET_RGBA(0x00, 0x00, 0x40, 128),
			(320-96+16)<<4, (menu_bot_y)<<4, GS_SET_RGBA(0x40,0x40, 0x80, 64), 3+1);
			
		gp_linerect(&thegp,
			(95-16)<<4, (menu_top_y+1)<<4,
			(320-95+16)<<4, (menu_bot_y)<<4, 4+1, GS_SET_RGBA(255, 255, 255, 128));
		
      // Menu Text
      TextOutC2(0<<4,320<<4,ypos[0],"Continue Game",5+1);
      TextOutC2(0<<4,320<<4,ypos[1],"Save State to mc0:",5+1);
      TextOutC2(0<<4,320<<4,ypos[2],"Load State of mc0:",5+1);
      TextOutC2(0<<4,320<<4,ypos[3],"Reset Gameboy",5+1);
      TextOutC2(0<<4,320<<4,ypos[4],"Save  CNF",5+1);
      TextOutC2(0<<4,320<<4,ypos[5],"Load  CNF",5+1);
		
      // Sound On/Off
      if ( sound ) {
         TextOutC2(0<<4, 320<<4, ypos[6], "Sound: On", 5+1);
      } else {
         TextOutC2(0<<4, 320<<4, ypos[6]," Sound: Off", 5+1);
      }

      // Stretch
      if ( STRETCH == 1 ) {	
         TextOutC2(0<<4, 320<<4, ypos[7], "Stretch: X 1.5", 5+1);
      } else if ( STRETCH==2 ) {
         TextOutC2(0<<4, 320<<4, ypos[7], "Stretch: X 2", 5+1);
      } else {
         TextOutC2(0<<4, 320<<4, ypos[7]," Stretch: Off", 5+1);
      }
      
      // Filter On/Off
      if ( FILTER ) {	
			TextOutC2(0<<4, 320<<4, ypos[8], "Filter: On", 5+1);
      } else {
         TextOutC2(0<<4, 320<<4, ypos[8]," Filter: Off", 5+1);
      }
      
      if ( strlen(boot_ELF) == 0 ) {
         sprintf(tmps, "No system ELF set by CNF");
      } else if ( strlen(boot_ELF) < 22 ) {
         sprintf(tmps, "Run %s", boot_ELF);
      } else if ( strlen(boot_ELF) < 26 ) {
         strcpy(tmps, boot_ELF);
      } else {
         strcpy(tmps, "Run system ELF set by CNF");
      }
      
      TextOutC2(0<<4, 320<<4, ypos[9], tmps, 5+1);
      TextOutC2(0<<4, 320<<4, ypos[10], "Back to the main menu", 5+1);

      gp_frect(&thegp, (95-16)<<4, ypos[selection],
         (320-95+16)<<4, ypos[selection] + (16<<4), 6+1, GS_SET_RGBA(123, 255, 255, 40));

      gp_hardflush(&thegp);
      WaitForNextVRstart(1);
      GS_SetCrtFB(whichdrawbuf);
      whichdrawbuf ^= 1;
      GS_SetDrawFB(whichdrawbuf);

      if ( padGetState(0, 0) == PAD_STATE_STABLE ) {
         padRead(0, 0, &mpad1);
         mpad1_data = 0xffff ^ mpad1.btns;

         if ( (mpad1.mode >> 4) == 0x07 ) {
            if ( mpad1.ljoy_v < 64 ) { 
               mpad1_data |= PAD_UP; 
            } else if( mpad1.ljoy_v > 192 ) { 
               mpad1_data |= PAD_DOWN; 
            }
         }
      }
      new_pad = mpad1_data & ~old_pad;
      old_pad = mpad1_data;

      if ( mpad1_data & PAD_SELECT ) {
         if (    (mpad1_data & PAD_UP)   && dispy) dispy--;
         else if (mpad1_data & PAD_DOWN)           dispy++;
         else if((mpad1_data & PAD_LEFT) && dispx) dispx--;
         else if (mpad1_data & PAD_RIGHT)          dispx++;

         GS_SetDispMode(dispx,dispy,NWIDTH,NHEIGHT);
         continue;
		}
		
      if ( (mpad1_data & PAD_R2) || (mpad1_data & PAD_L2) )
         break;

      if ( (new_pad & PAD_UP) && (selection > -1) ) {
         selection--;
         if ( selection < 0 )
         selection = menu_lines-1;
      }
		
      if ( new_pad & PAD_DOWN ){
         selection++;
         if ( selection >= menu_lines )
            selection = 0;
      }

      if ( new_pad & PAD_CROSS ) {
         if ( selection == 0 ) {  // Continue Game
            break;
         } else if ( selection == 1 ) { // Save State to mc0:
         } else if ( selection == 2 ) { // Load State of mc0:
         } else if ( selection == 3 ) { // Gameboy Reset
            gameboy_cpu_hardreset();
            gameboy_cpu_run();
         } else if ( selection == 4 ) { // Save  CNF
            Save_CNF("mc0:PS2GB/INFOGB.CNF");
         } else if ( selection == 5 ) { // Load  CNF
            Load_CNF("mc0:PS2GB/INFOGB.CNF");
            auto_ROM_flag = 0;  //skip auto_ROM booting at reload of CNF
         } else if ( selection == 6 ) {	
            sound ^= 1;	
            CNF_edited |=1;
         } else if( selection == 7 ) {
            STRETCH++; if ( STRETCH > 2) { STRETCH = 0; }
         } else if(selection == 8) {
            FILTER  ^= 1;
            CNF_edited |=1;
         } else if ( selection == 9 ) { //"Exit to your system ELF"
            if ( strlen(boot_ELF) )
					Reboot_PS2(boot_ELF);
         } else if( selection == 10 ) { //"Back to the main menu"
            endflag = 1;               
            //bThread = 0;
            //dwKeySystem |= 1; 				 
            break;
         }
			
      }
   }	//Ends main menu loop

	//Debounce exiting keypress
   while ( 1 ) {
      if ( padGetState(0, 0) == PAD_STATE_STABLE ) {
         padRead(0, 0, &mpad1); // port, slot, buttons
         mpad1_data = 0xffff ^ mpad1.btns;
      }
      if ( !(mpad1_data & PAD_CROSS) && !(mpad1_data & PAD_R2) && !(mpad1_data & PAD_L2) ) {
         break;
      }
	}
	
	if ( sound ) { SjPCM_Play(); }
}

//---------------------------------------------------------------------------

void guiFadeOut(int pa) 
{
   int alpha = 0;
   while ( alpha < 0x80 )
   {
      gp_frect(&thegp,0<<4, 0<<4, 320<<4, 240<<4, 5, GS_SET_RGBA(255, 255, 255, alpha));
   // gp_frect(&thegp, 4,4,<<4, <<4, Z_SELECT, GS_SET_RGBA(123, 255, 255, 40));
      
      gp_hardflush(&thegp);
      WaitForNextVRstart(1);
      GS_SetCrtFB(whichdrawbuf);
      whichdrawbuf ^= 1;
      GS_SetDrawFB(whichdrawbuf);
      
      alpha +=pa;
	}
}

//---------------------------------------------------------------------------

void guiFadeIn(int pa)
{
   int alpha = 0x80;
   while ( alpha > 0 )
   {
      gp_frect(&thegp, 0<<4, 0<<4, 320<<4, 240<<4, 5, GS_SET_RGBA(255, 255, 255, alpha));
   // gp_frect(&thegp, 4,4,<<4, <<4, Z_SELECT, GS_SET_RGBA(123, 255, 255, 40));

      gp_hardflush(&thegp);
      WaitForNextVRstart(1);
      GS_SetCrtFB(whichdrawbuf);
      whichdrawbuf ^= 1;
      GS_SetDrawFB(whichdrawbuf);

      alpha -=pa;
   }
}

//---------------------------------------------------------------------------
//End of file:  ps2gui.c
//---------------------------------------------------------------------------
