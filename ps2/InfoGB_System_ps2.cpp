//---------------------------------------------------------------------------
//File name:    InfoGB_System_ps2.cpp
//---------------------------------------------------------------------------
#include <tamtypes.h>
#include <string.h>
#include <kernel.h>
#include <fileio.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <libpad.h>

#include "sbv_patches.h"
#include "lib/libcdvd/ee/cdvd_rpc.h"

extern "C"
{
#include "hw.h"
#include "gs.h"
#include "gfxpipe.h"
#include "sjpcm.h"
extern void textpixel(int ,int ,unsigned ,int ,int ,int , char *,...);
extern void textCpixel(int ,int ,int ,unsigned ,int ,int ,int ,char *,...);
extern void ps2_update_input();
extern int setup_pad();
extern int init_machine();
extern void display_intro(char* , int );
extern void display_error(char* , int );
extern void IngameMenu();
extern void guiFadeOut(int);
extern void guiFadeIn(int);
extern int loadromcfg();
extern char * menu_main();
extern void initromdata();
extern int initmc();
extern void ChooseMCdir(int );
extern void ProcessROMscan(char*);
extern void Save_CNF(char *);
extern void Load_CNF(char *);
}

#define SCREEN_W 320
#define SCREEN_H 256
#define NWIDTH SCREEN_W
#define NHEIGHT SCREEN_H
#define TXTMIN 0
#define TXTMAX 320

#define GS256 8
#define GS320 9
#define GS640 9
#define GS1024 10

#define NES_TEX		0x0A0000 + 0x0A0000 
#define NES_CLUT	0x128000 + 0x0A0000 
#define VRAM_MAX	0x3E8000

#include "../system.h"
#include "../joypad.h"
#include "../mem.h"
#include "../cpu.h"
#include "../rom.h"
#include "../vram.h"
#include "mass.h"

#define TextOutC2(x_start, x_end, y, string,  z) textCpixel((x_start)>>4,(x_end)>>4,((y)>>4)+4,GS_SET_RGBA(255,255,255,128),0,0,(z),(string))

int CNF_edited = false;
int dispx, dispy;
int STRETCH=1;   // 0= 160*144 1= 240*216 2= 320*288 )
int FILTER=1;   // 1 = filtering
int sound = 1;

char auto_ROM[1025] = "";       //Used to preselect a ROM pathname for auto booting
char auto_ROM_folder[1025] = "";  //Used to preselect a ROM folder for auto booting
int auto_ROM_flag = 0;          //0 disables auto boot of a preselect ROM

gameboy_proc_t *current_processor = NULL;
unsigned char gameboy_screen_mul  = 1;
force_type force_system           = NONE;
int infogb_ready                  = 0; 

/* for video */

#define APP_NAME     "InfoGB v0.5J"
//#define VBLANK_INT	 16.66667 
//#define VBLANK_INT	 21 

#define GFX_BITDEPTH 16
#define GFX_BYTEDEPTH 2			/* This should always be ( GFX_BITDEPTH / 8 ) */

unsigned short rgbtab[ 256 ];

static unsigned long infogb_window_width = 0;
static unsigned long infogb_window_height = 0;

/* local functions */
void infogb_open_audio();
void infogb_close_audio();

int hostlist = 0;

int choosedir = 1;
int DEVICE_FLAG = 0;  // mc = 0, cdrom = 1, usb_mass = 2

int mxdbg = 0, rempli = 0;

char padBuf1[256] __attribute__((aligned(64))) __attribute__ ((section (".bss")));
char padBuf2[256] __attribute__((aligned(64))) __attribute__ ((section (".bss")));

static char rom2[120] __attribute__((aligned(64)));

int whichdrawbuf = 0;
int endflag = 0;
int snd_sample = 0;
int snd_sampler = 48000;
int vwidth = 512, vheight = 256;

int center_x, num_roms;
int selection;
int frame_position;
int frame_selection;
int FREEMC = 0;

#define FALSE 0
#define TRUE 1

u16* gfx_buffer = NULL;  
u16 bitmap_data[256 * 256] __attribute__((aligned(16))) __attribute__ ((section (".bss"))); 

unsigned long  dwKeyPad1;
unsigned long  dwKeyPad2;

short signed int *sndbuf[2];
static int sample_pos = 0;

int infogb_init(char *display){return 2;};
int infogb_close(void){return 1;}
void infogb_close_audio(){}

//---------------------------------------------------------------------------
void testdir()
{
	num_roms = 0;
	initromdata();

	while (num_roms <= 1) {

debut:
		selection = 0;
		frame_selection = 0;
		frame_position = 0;
 
		choosedir = 1;  //dlanor: tell menu_main that we want a folder next
		if ( auto_ROM_flag ) {
			if(!strncmp(auto_ROM, "mc", 2))
				DEVICE_FLAG = 0; // mc
			else
				DEVICE_FLAG = 1; // cd
      }
       
		initromdata();
		ChooseMCdir(0);
		sprintf(rom2,"x");
	
		if(auto_ROM_flag)
			strcpy(rom2, auto_ROM_folder);
		else
			strcpy(rom2,menu_main());  //dlanor: Get rom2 = folder path, or device command

		if (strstr(rom2,"BROWSE MC:" ) != NULL) {
			display_error(" BROWSE MC0: ", 0);
			DEVICE_FLAG = 0;
			goto debut;  //dlanor: loop back to browse other device
		}
		else if(strstr(rom2,"BROWSE CD:" ) != NULL) {
			display_error(" BROWSE CDROM: ", 0);
			DEVICE_FLAG = 1;
			goto debut;  //dlanor: loop back to browse other device
		}
      else if(strstr(rom2,"BROWSE MASS:" ) != NULL) {
         display_error(" BROWSE MASS: ", 0);
         DEVICE_FLAG = 2;
         goto debut;  //dlanor: loop back to browse other device
      } 

		//Here rom2 points to name of a folder to scan for ROMs

		selection = 0;
		frame_selection = 0;
		frame_position = 0;   

		ProcessROMscan(rom2);

		if ( num_roms == 1 )
			display_error("No roms!", 0);
			
	}//fin while
	choosedir = 0;  //dlanor: tell menu_main that we want a ROM next
}

//---------------------------------------------------------------------------
/*===================================================================*/
/*                                                                   */
/*                main() : Application main                          */
/*                                                                   */
/*===================================================================*/

int main(int argc, char **argv)
{
   int i ;
   
	init_machine();  

	Load_CNF("mc0:PS2GB/INFOGB.CNF");

	guiFadeOut(4);    
	if(!auto_ROM_flag)
		display_intro("PS2 InfoGB 0.5J",0);   

	guiFadeIn(4); 
	guiFadeOut(4);  

	for(i=0;i<160*144;i++)bitmap_data[i]=0;
	for(i=0;i<256;i++)rgbtab[i]=0;

	gfx_buffer =(u16 *)&bitmap_data[0];

	infogb_init(NULL);

	infogb_window_width = 160;
	infogb_window_height = 144;

	sound=0;

	if(loadromcfg()>0)
		hostlist=1;
	else {
		u8 mbram[0x2000]; 
		int fd;
    
		fd = fioOpen("mc0:PS2GB/gb.brm",O_RDONLY); 
		if ( fd <= 0 ){
			//create ps2gb dir on mc0:
			if ( fioMkdir("mc0:PS2GB") < 0 ) {
				printf("Failed to create dir mc0:/PS2GB (folder exists?)");
				// KarasQ: fixed bug - froze "white screen" when folder PS2GB already exists
				// but file gb.brm dosen't in the same time
				/* SleepThread(); */ 
			}
			// create mbram 8ko! if doesnt exist ! first time
			memset(mbram, 0, 0x2000);
			fd = fioOpen("mc0:PS2GB/gb.brm",O_WRONLY | O_CREAT);
			if (fd < 0)
				printf("Error opening/creating BRM file %d",fd);
			else { 
				fioWrite(fd, mbram, 0x2000);
				fioClose(fd);
			}
		}
		else
			fioClose(fd);
   
		CDVD_Init();
		FREEMC = initmc();
		testdir();
	}//SleepThread();
	Load_CNF("mc0:PS2GB/INFOGB.CNF");
   
new_ROM_loop:
   dwKeyPad1 = 0;   
	dwKeyPad2 = 0;

	if(auto_ROM_flag){
		strcpy(rom2, auto_ROM);
		auto_ROM_flag = 0;
	}
	else
		strcpy(rom2, menu_main());  //dlanor: get rom2 = pathname of ROM
   
	if(!hostlist){
		// cdrom / mc / mass loading 
		if(strstr(rom2,"Return" ) != NULL){
			display_error(" Return ", 0);
			testdir();  //dlanor: this is NOT just a test, but a folder browser
			goto new_ROM_loop;
		}
		CDVD_Stop();
	}
   
   // KarasQ: display error and return to 
   // browser if ROM is not loaded successfully
   switch ( load_rom(rom2) ) {
      case  0:
         display_error("Unknown ROM type!", 0);
         goto new_ROM_loop;
      case -1:
         display_error("Unknown ROM size!", 0);
         goto new_ROM_loop;
      case -2:
         display_error("Unknown RAM size!", 0);
         goto new_ROM_loop;
   }
	
	initialize_memory();
	initialize_rom();    

	if(sound) {
		SjPCM_Clearbuff();
		SjPCM_Play();
	}
	gameboy_cpu_hardreset();
	gameboy_cpu_run();

	infogb_close_audio();
	infogb_close();

	if(sound) SjPCM_Pause();

	free_rom();
	free_memory();  

	goto new_ROM_loop;

	return(0);
}

//---------------------------------------------------------------------------
int infogb_poll_events()
{
 ps2_update_input(); 
 current_joypad =dwKeyPad1;
 if(endflag){    
   gbz80.running=0;
   endflag=0;
 }

 return 1;
}

//---------------------------------------------------------------------------
void infogb_set_color(int x, unsigned short c)
{
	rgbtab[ x & 0xff ] = c;
}

void infogb_plot_line(int y, int *index)
{
	unsigned int x;

	for (x = 0; x < infogb_window_width; x++ )
	{
      gfx_buffer[y * infogb_window_width  + x  ] =rgbtab[ index[x] & 0x00FF ] ;
	}
}


//---------------------------------------------------------------------------
void infogb_vram_blit()
{ 
 	gp_uploadTexture(&thegp, NES_TEX, 256, 0, 0, 0x02,&bitmap_data, 160, 144);
	gp_setTex(&thegp, NES_TEX, 256, 8, GS256, 0x02, 0, 0, 0,FILTER);
    
    if(STRETCH==2)gp_texrect(&thegp, 0<<4,16<<4, 0<<4, 0<<4, (320)<<4, (256-16)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));
    else if(STRETCH==1)gp_texrect(&thegp, 40<<4,20<<4, 0<<4, 0<<4, (240+40)<<4, (216+20)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));   
    else gp_texrect(&thegp, 80<<4,56<<4, 0<<4, 0<<4, (160+80)<<4, (144+56)<<4, 160<<4, 144<<4, 2, GS_SET_RGBA(255, 255, 255, 200));
     
	gp_hardflush(&thegp);

	WaitForNextVRstart(1);

	// Update drawing and display enviroments.
    GS_SetCrtFB(whichdrawbuf);
    whichdrawbuf ^= 1;
    GS_SetDrawFB(whichdrawbuf);    

    if(sound) SjPCM_Enqueue(sndbuf[0], sndbuf[1], snd_sampler,1 /*1*/);
   // if(sound){printf("mbson:%d ",mxdbg);mxdbg=0;}  
}

//---------------------------------------------------------------------------
void infogb_write_sample(short int l, short int r)
{
  sndbuf[0][sample_pos]=(l >> 0);
  sndbuf[1][sample_pos]=(r >> 0); 
  
  sample_pos++;
  //mxdbg++;
  if (sample_pos >= snd_sampler) {  	 
    //if(sound) SjPCM_Enqueue(sndbuf[0], sndbuf[1], snd_sampler, 0);
    // if(sound) SjPCM_Enqueue(sndbuf[0], sndbuf[1], Tsmp,1 /*1*/);   
 	sample_pos = 0;	 
  }  
}
//---------------------------------------------------------------------------
//End of file:  InfoGB_System_ps2.cpp
//---------------------------------------------------------------------------
