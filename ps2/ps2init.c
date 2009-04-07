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

#include "hw.h"
#include "gs.h"
#include "gfxpipe.h"
#include "sjpcm.h"

#include "sbv_patches.h"
#include <libmc.h>

#define SCREEN_W 320
#define SCREEN_H 256
#define NWIDTH SCREEN_W
#define NHEIGHT SCREEN_H

/* = external IRX modules = */
extern u8 *libcdvd_irx;		
extern int size_libcdvd_irx;

extern u8 *sjpcm_irx;		
extern int size_sjpcm_irx;

// ==== KarasQ: USB mass support =====
extern u8 *usbd_irx;
extern int size_usbd_irx;

extern u8 *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

/* USB Modules state
 * 0 = NOT LOADED
 * 1 = USBD.IRX LOADED
 * 2 = USBHDFSD.IRX LOADED
 */
 
u8 usb_mass_support = 0;

/* ==================================== */

extern int dispx, dispy;
extern int sound ;
extern int snd_sample;
extern int snd_sampler;
//extern  unsigned char* gfx_buffer ;
extern unsigned long  dwKeyPad1;
extern unsigned long  dwKeyPad2;
//extern unsigned long  dwKeySystem;
//extern unsigned short NesPalette[ 64 ];
extern short signed int *sndbuf[2];
#define FALSE 0
#define TRUE 1

extern int bThread;

void delay(int count) {
   int i, ret;
   for (i = 0; i < count; i++) {
      ret = 0x01000000;
      while ( ret-- ) {
         asm("nop\nnop\nnop\nnop");
      }
   }
}

void LoadModules(void)
{
   int ret;
   
   ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
   if (ret < 0) {
      printf("Failed to load module: SIO2MAN");
   }

   ret = SifLoadModule("rom0:MCMAN", 0, NULL);
	if (ret < 0) {
      printf("Failed to load module: MCMAN");
   }
	
   ret = SifLoadModule("rom0:MCSERV", 0, NULL);
   if (ret < 0) {
      printf("Failed to load module: MCSERV");
   }

   ret = SifLoadModule("rom0:PADMAN", 0, NULL);
	if (ret < 0) {
      printf("Failed to load module: PADMAN");
   } 
   
   ret = SifLoadModule("rom0:LIBSD", 0, NULL);
   if (ret < 0) {
      printf("Failed to load module: LIBSD");
   }
   
   SifExecModuleBuffer(&sjpcm_irx, size_sjpcm_irx, 0, NULL, &ret);
	if (ret < 0) {
      display_error("Failed to load module: SJPCM.IRX :(", 1);    
	}
   
   /*
   ret = SifLoadModule("host:sjpcm.irx", 0, NULL);
	if (ret < 0) {
      printf("Failed to load module: SJPCM.IRX");
   } 
   */
   
   SifExecModuleBuffer(&libcdvd_irx, size_libcdvd_irx,0, NULL, &ret);
   if (ret < 0) {
      display_error("Failed to load module: LIBCDVD.IRX :(", 1);    
   }
   
   // Load USB modules
   SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, &ret);
   if (ret < 0) {
      display_error("Failed to load module: USBD.IRX :(", 1);
   } else {
      usb_mass_support = 1;
   }
   
   SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);
   if (ret < 0) {
      display_error("Failed to load module: USBHDFSD.IRX :(", 1);
   } else {
      usb_mass_support = 2;
   }
}

int gsinit()
{
   DmaReset();
	
   if ( pal_ntsc() == 3 ) {
      GS_InitGraph(GS_PAL, GS_NONINTERLACE);
      dispx = 75; //40; //65
      dispy = 40;
      GS_SetDispMode(dispx,dispy, NWIDTH, NHEIGHT);
      snd_sampler = 48000 / 50;
   } else {
      GS_InitGraph(GS_NTSC, GS_NONINTERLACE);
      dispx = 65;
      dispy = 17;
      GS_SetDispMode(dispx,dispy,NWIDTH,NHEIGHT);
      snd_sampler = 48000 / 60;
   }
   
   GS_SetEnv(NWIDTH, NHEIGHT, 0, 0x50000, GS_PSMCT32, 0xA0000, GS_PSMZ16S);
   install_VRstart_handler();
	createGfxPipe(&thegp, /*(void *)0xF00000,*/ 0x50000);
	gp_hardflush(&thegp);
	
   return 0;
}


int init_machine(void)
{
   int /* r,g,b, */ i;
   
   // KarasQ: PS2 init
   fioExit();
	SifExitIopHeap();
	SifLoadFileExit();
	SifExitRpc();
		
   SifIopReset("rom0:UDNL rom0:EELOADCNF", 0);
   while ( SifIopSync() );
   //..
   
   SifInitRpc(0);
   
   sbv_patch_enable_lmb();          // not sure we really need to do this again
   sbv_patch_disable_prefix_check();// here, but will it do any harm?
	
   LoadModules();
   delay(5);
   
   gsinit();
   
   sndbuf[0] = malloc(sizeof(short signed int )*snd_sampler);
   sndbuf[1] = malloc(sizeof(short signed int )*snd_sampler);
  
   for ( i=0; i < snd_sampler; i++) 
      sndbuf[0][i]=sndbuf[1][i]=0;
      
   setup_pad();    
    
	if ( sound ) {
      if( SjPCM_Init(1) < 0 )
         printf("SjPCM Bind failed!!");
   }
   
   /* Initialize a pad state */
   // dwKeyPad1 = 0;
   // dwKeyPad2   = 0;
   // dwKeySystem = 0;

   /* Initialize thread state */
   // bThread = FALSE;
   /*
   for ( i=0; i < 64; i++) {
      // bgr -< rgb
      r =  (  NesPalette[ i ] & 0x7c00 )>>10;
      g =  (  NesPalette[ i ] & 0x03e0 )>>5;
      b =  (  NesPalette[ i ] & 0x001f )<<0;      
      NesPalette[ i ] = ((b<<10)|(g<<5)|(r<<0));
   }
   */
   
   //gfx_buffer=malloc(sizeof(unsigned char)*140*160);
   return (1);
}

