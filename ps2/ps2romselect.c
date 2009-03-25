//---------------------------------------------------------------------------
//File name:    ps2romselect.c
//---------------------------------------------------------------------------
#include <tamtypes.h>
#include <string.h>
#include <kernel.h>
#include <fileio.h>
#include <sifrpc.h>
#include <stdlib.h>
#include <stdio.h>
#include <libpad.h>
#include <libmc.h>
#include "lib/libcdvd/common/cdvd.h"

#include "hw.h"
#include "gs.h"
#include "gfxpipe.h"

/////////////////////////
#include "mass.h"
/////////////////////////

//---------------------------------------------------------------------------
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

#define TextOutC2(x_start, x_end, y, string,  z) textCpixel((x_start)>>4,(x_end)>>4,((y)>>4)+4,GS_SET_RGBA(255,255,255,128),0,0,(z),(string))

#define Z_BOX1		3
#define Z_BOX2		4
#define Z_LIST		5
#define Z_SELECT	6
#define Z_SCROLLBG	7
#define Z_SCROLL	8
#define Z_SCROLL_M	9

// ARRAY_ENTRIES - defined in mass.h

extern int hostlist,center_x,num_roms;
extern int selection;
extern int frame_position;
extern int frame_selection;
//char romname[100][120];
//typedef u8 byte;

struct ROMdata {
	char name[40]; // These values should be more than enough
	char filename[120];
};

// Store for names of files placed on mass device
DATA_FILES mass_store_data[ARRAY_ENTRIES];

struct ROMdata *romdata;

const char cdpath[120]="/";
const char mcpath[120]="/*";
char usbpath[120];

static  mcTable mcDir[ARRAY_ENTRIES] __attribute__((aligned(64)));
static int mc_Type, mc_Free, mc_Format;

extern int FILTER,DEVICE_FLAG,choosedir;

struct TocEntry myTocEntries[ARRAY_ENTRIES] __attribute__((aligned(64)));

char scroll_text[] = "                                                            PS2-InfoGB 0.5J , this one is only for test/debug purpose ,   Just another port of InfoGB 0.5J  -   Thanks to: Sjeep   , Ebola  , G , Evilo ,  - Vzzrzzn - Pukko - Gustavo Scotti - Oobles  - The people of PS2DEV/SCENE ,               and anybody that I've missed out ...                 ?????";

extern void textCpixel(int ,int ,int ,unsigned ,int ,int ,int ,char *,...);

extern int whichdrawbuf ;
extern int dispx, dispy;	

//---------------------------------------------------------------------------
int loadromcfg()
{
	int finok=0,i,j,fd_size,nb=0,fd;
	char  *buf;

	fd = fioOpen("host:romlist.txt", O_RDONLY);

	if(fd <= 0) {
		printf("host:romlist.txt not found.\n");
		return -1;
	}   

	fd_size = fioLseek(fd,0,SEEK_END);
	fioLseek(fd,0,SEEK_SET);
	//printf("%d \n",fd_size);
	buf=malloc(sizeof(char)*fd_size);
	fioRead(fd, buf, fd_size);
	nb=0;j=0;

	romdata = malloc(sizeof(struct ROMdata) * ARRAY_ENTRIES);
	memset((u8 *)romdata,0,sizeof(struct ROMdata) * ARRAY_ENTRIES);

	for (i=0; i<(fd_size); i++){
		if(buf[i]==0){printf("buf fin\n");break;}
	    
		if(buf[i]!=';' && finok==0){
			romdata[nb].filename[j]=buf[i];
			j++;   
		}
		if(buf[i]==';'){
			finok=1;
			romdata[nb].filename[j]='\0';
		}     	

		if(buf[i]=='\n'){j=0;nb++;finok=0;}
		if(nb==ARRAY_ENTRIES)break;
	}

	for (i=0; i<nb; i++){
		// strcpy
		printf("%s | ",romdata[i].filename);
	}

	printf("\nnb roms = %d\n",nb);

	num_roms=nb;
	fioClose(fd);
	choosedir=0;
	return 1;
}
//---------------------------------------------------------------------------
char * menu_main()  //dlanor: This is the browser for Folder/ROM
{
	int i,dec=0;
	int y;
	int pad_cmd;
	static int scroll_delay = 0;
   
	while(1) {
		if((pad_cmd=menu_update_input())!=0) { // X or Triangle was pressed
			//gp_gouradrect(&thegp, (96-16)<<4, 80<<4, GS_SET_RGBA(0x00, 0x00, 0x40, 64),
			//	(320-96+16)<<4, 154<<4,  GS_SET_RGBA(0x40,0x40, 0x80, 10), Z_BOX1);
			gp_hardflush(&thegp);
			WaitForNextVRstart(1);
			GS_SetCrtFB(whichdrawbuf);
			whichdrawbuf ^= 1;
			GS_SetDrawFB(whichdrawbuf);
			break;
		}

		// Draw Scroll Box
		gp_gouradrect(&thegp, (31+32)<<4, 70<<4,GS_SET_RGBA(0x00, 0x00, 0x40, 64),
			(32+255-31)<<4, (2+142+64)<<4,  GS_SET_RGBA(0x40,0x40, 0x80, 10), Z_BOX1);
		// Draw text in scroll box
		y = 67<<4;
		for(i=frame_position;i<(frame_position+8);i++) {
			if((67+(frame_selection*18))<<4==y){
				if(i<num_roms){
					if(hostlist)
						textCpixel(0,320,(y>>4)+4,GS_SET_RGBA(255, 255, 255, 255),
							0,0,Z_SCROLL/*Z_LIST*/,(char *)&romdata[i].filename);	
					else
						textCpixel(0,320,(y>>4)+4,GS_SET_RGBA(255, 255, 255, 255),
							0,0,Z_SCROLL/*Z_LIST*/,(char *)&romdata[i].name);	
				}
			}else{
				if(i<num_roms){
					if(hostlist)
						textCpixel(0,320,(y>>4)+4,GS_SET_RGBA(200, 200, 200, 128),
							0,0,Z_LIST,(char *)&romdata[i].filename);	
					else
						textCpixel(0,320,(y>>4)+4,GS_SET_RGBA(200, 200, 200, 128),
							0,0,Z_LIST,(char *)&romdata[i].name);	
				}   
			}
			y += 18<<4;
		}

		gp_frect(&thegp,
			(32+31+0)<<4, (69+(frame_selection*18))<<4,
			(255-31+32)<<4, (-3+76+7+(frame_selection*18))<<4,
			Z_SELECT, GS_SET_RGBA(32, 81, 124, 50));

		// Draw scroller
		for(i=0;i<sizeof(scroll_text)+1;i++)
			if((i*6-dec)<320 && (i*6-dec)>0)
				printch(i*6-dec,216+4,GS_SET_RGBA((i*6-dec)%256, i%256, dec%256, 255),
					scroll_text[i],0,0,Z_SCROLL);	

		if(--scroll_delay < 0) {
			dec++;
			if(dec>(6*sizeof(scroll_text)+1))
				dec=0;
			scroll_delay = 0;
		}    	

		gp_frect(&thegp,0<<4,0<<4,9<<4,240<<4,Z_SCROLL_M,GS_SET_RGBA(0, 0, 0, 128));

		gp_hardflush(&thegp);

		WaitForNextVRstart(1);

		// Update drawing and display enviroments.
		GS_SetCrtFB(whichdrawbuf);
		whichdrawbuf ^= 1;
		GS_SetDrawFB(whichdrawbuf);

	}//ends while
	if(pad_cmd == PAD_TRIANGLE) selection = 0;

	if(hostlist)return (char *)&romdata[selection].filename;

	if(choosedir==1)return (char *)&romdata[selection].name;
	else return (char *)&romdata[selection].filename;  
}
//---------------------------------------------------------------------------
int menu_update_input()
{
	static struct padButtonStatus lpad1; // just in case
	static int pad1_connected = 0;
	static int padcountdown = 0;
	static int pad_held_down = 0;
	int ret1;
	int lpad1_data = 0;

	ret1=padGetState(0, 0);
	while((ret1 != PAD_STATE_STABLE) && (ret1 != PAD_STATE_FINDCTP1)) {
		if(ret1==PAD_STATE_DISCONN) {
			printf("Pad(%d, %d) is disconnected\n", 0, 0);
		}
		ret1=padGetState(0, 0);
	}//ends while

	pad1_connected=padRead(0, 0, &lpad1); //now working with psx pad !
	if(pad1_connected) {
		//	padRead(0, 0, &pad1); // port, slot, buttons
		lpad1_data = 0xffff ^ lpad1.btns;

		if((lpad1.mode >> 4) == 0x07) {
			if(lpad1.ljoy_v < 64) lpad1_data |= PAD_UP;
			else if(lpad1.ljoy_v > 192) lpad1_data |= PAD_DOWN;
			if(lpad1.ljoy_h < 64) lpad1_data |= PAD_LEFT;
			else if(lpad1.ljoy_h > 192) lpad1_data |= PAD_RIGHT;
		}
	}

	if(lpad1_data & PAD_CROSS){        
		while(1) {
			if(padGetState(0, 0) == PAD_STATE_STABLE) {
				padRead(0, 0, &lpad1); // port, slot, buttons
				lpad1_data = 0xffff ^ lpad1.btns;
			}
			if(!(lpad1_data & PAD_CROSS)) break;
		}//ends while
		return PAD_CROSS;
	}

	if(lpad1_data & PAD_TRIANGLE){        
		while(1) {
			if(padGetState(0, 0) == PAD_STATE_STABLE) {
				padRead(0, 0, &lpad1); // port, slot, buttons
				lpad1_data = 0xffff ^ lpad1.btns;
			}
			if(!(lpad1_data & PAD_TRIANGLE)) break;
		}//ends while
		return PAD_TRIANGLE;
	}

	if(padcountdown) padcountdown--;

	if((lpad1_data & PAD_DOWN) && (padcountdown==0) && (selection!=(num_roms-1))) {
		selection++;

		//if the pad has been held down for a certain amount of time, give padcountdown
		//a lower value, in effect making the scrolling of the text faster
		if(pad_held_down++<4) padcountdown=10;
		else padcountdown=2;

		//move the display frame if necessary
		if(selection>(frame_position+7)) frame_position++;

		frame_selection = selection - frame_position;
		return 0;

	}else if((lpad1_data & PAD_UP) && (padcountdown==0) && (selection>0)) {
		selection--;

		if(pad_held_down++<4) padcountdown=10;
		else padcountdown = 2;

		if(selection<frame_position) frame_position--;

		frame_selection = selection - frame_position;
		return 0;
	}else if((lpad1_data & PAD_LEFT) && (padcountdown==0)) {
		if((frame_position -= 8) < 0)
			frame_position = 0;
		if((selection -= 8) < 0)
			selection = 0;

		if(pad_held_down++<4) padcountdown=10;
		else padcountdown = 2;

		frame_selection = selection - frame_position;
		return 0;
	}else if((lpad1_data & PAD_RIGHT) && (padcountdown==0)) {
		if((num_roms > 8) && ((frame_position += 8) > (num_roms-8)))
			frame_position = num_roms-8;
		selection += 8;
		if(selection > (num_roms-1))
			selection = num_roms-1;

		if(pad_held_down++<4) padcountdown=10;
		else padcountdown = 2;

		frame_selection = selection - frame_position;
		return 0;
	}

	//if up or down are NOT being pressed, reset the pad_held_down flag
	if(!(lpad1_data & (PAD_UP | PAD_DOWN | PAD_RIGHT | PAD_LEFT))) pad_held_down = 0;

	/*
     
	//check controller status
	if((padGetState(0, 0)) == PAD_STATE_STABLE) {
		if(pad1_connected == 0) {
			WaitForNextVRstart(1);
		}
		pad1_connected = 1;
	} else pad1_connected = 0;
	*/

	return 0;
}
//---------------------------------------------------------------------------
void initromdata()
{
	int i; 

	for(i=0; i < ARRAY_ENTRIES; i++){	 
		strcpy(romdata[num_roms].name,"");	
		strcpy(romdata[num_roms].filename,"");	
	}
}
//---------------------------------------------------------------------------
int initmc()
{
	int ret,numMC,CPtype;

	numMC=0;
	CPtype=0;

	// 128 roms / directory

	romdata = malloc(sizeof(struct ROMdata) * ARRAY_ENTRIES);
	memset((u8 *)romdata,0,sizeof(struct ROMdata) * ARRAY_ENTRIES);

	if(mcInit(MC_TYPE_MC) < 0) {
		printf("Failed to initialise memcard server!\n");
		SleepThread();
	}

	// Since this is the first call, -1 should be returned.
	mcGetInfo(0, 0, &mc_Type, &mc_Free, &mc_Format); 
	mcSync(0, NULL, &ret);
	printf("mcGetInfo returned %d\n",ret);
	printf("Type: %d Free: %d Format: %d\n\n", mc_Type, mc_Free, mc_Format);

	// Assuming that the same memory card is connected, this should return 0
	mcGetInfo(numMC,0,&mc_Type,&mc_Free,&mc_Format);
	mcSync(0, NULL, &ret);
	printf("mcGetInfo returned %d\n",ret);
	printf("Type: %d Free: %d Format: %d\n\n", mc_Type, mc_Free, mc_Format);

	return (int)(mc_Free*1000);
}

//---------------------------------------------------------------------------
void ChooseMCdir(int num)
{   
	int i, ret2;

	if ( DEVICE_FLAG == 1 ) { // Getting dirs from cd
		num_roms = 0;

		strcpy(romdata[num_roms].name,"BROWSE MC:");	
		strcpy(romdata[num_roms].filename,"BROWSE MC:");	
		num_roms++;	
		
		strcpy(romdata[num_roms].name,"BROWSE MASS:");
		strcpy(romdata[num_roms].filename,"BROWSE MASS:");
      num_roms++;	

		while(CDVD_DiskReady(CdBlock)==CdNotReady);
		ret2 = CDVD_getdir(cdpath, NULL, CDVD_GET_DIRS_ONLY, myTocEntries, ARRAY_ENTRIES, NULL);
		printf("Retrieved %d directory entries\n\n",ret2);
		for (i = 0;i<ret2;i++){
			printf("Dir name: %s\tLBA = %d\tSize = %d\n",
				myTocEntries[i].filename,
				myTocEntries[i].fileLBA,
				myTocEntries[i].fileSize); 
			sprintf(romdata[num_roms].filename,"cdfs:/%s",myTocEntries[i].filename);
			strcpy(romdata[num_roms].name,myTocEntries[i].filename);
			//printf("%s - %s \n",romdata[num_roms].name, mcDir[i].name);			    
			num_roms++;	 
		}
	} else if ( DEVICE_FLAG == 0 ) { // Getting dirs from mc
		mcGetDir(num, 0, mcpath, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);

		num_roms = 0;

		strcpy(romdata[num_roms].name,"BROWSE CD:");//romdata[num_roms].filename	
		strcpy(romdata[num_roms].filename,"BROWSE CD:");//romdata[num_roms].filename	

		num_roms++;	
		
		strcpy(romdata[num_roms].name,"BROWSE MASS:");
		strcpy(romdata[num_roms].filename,"BROWSE MASS:");
		
      num_roms++;	

		for(i=0; i < ret2; i++){
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR){
				sprintf(romdata[num_roms].filename,"mc0:%s",mcDir[i].name);
				strcpy(romdata[num_roms].name,mcDir[i].name);
				//printf("%s - %s \n",romdata[num_roms].name, mcDir[i].name);			    
				num_roms++;	
			}
		}
	} else if ( DEVICE_FLAG == 2 ) { // KarasQ: Getting dirs from mass
		num_roms = 0;
		
		strcpy(romdata[num_roms].name,"BROWSE MC:");	
		strcpy(romdata[num_roms].filename,"BROWSE MC:");
		num_roms++;	
		
		strcpy(romdata[num_roms].name,"BROWSE CD:");
		strcpy(romdata[num_roms].filename,"BROWSE CD:");
      num_roms++;	
      	
      if ( usb_mass_support == 2 ) {
         clean_mass_store_data(mass_store_data);
         ret2 = read_mass("mass:", mass_store_data, ARRAY_ENTRIES - 1);
         
         if ( ret2 > 0 ) {
            for (i = 0; i < ret2; i++) {
               if ( mass_store_data[i].is_dir ) {
                  sprintf(romdata[num_roms].filename, "mass:%s", mass_store_data[i].name);
                  strncpy(romdata[num_roms].name, mass_store_data[i].name, 35);
			         
			         num_roms++;
               }
            }
         }
      } else {
         display_error("USB IRX Modules not loaded!", 0);
      }
   }
}



//---------------------------------------------------------------------------
void ProcessROMscan(char rom3[120])
{
	int numMC,CPtype;
	int i,ret2;
	static char tname[120];

	numMC = 0;
	CPtype = 0;

	num_roms = 0;
	strcpy(romdata[num_roms].name,"Return");
	strcpy(romdata[num_roms].filename,"Return");
	num_roms++;	

	if ( DEVICE_FLAG == 0 ) {       
		//Process ROMs from MC

		strcpy(tname,rom3);
		strcat(tname,"/*.GB"); 

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);

		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);			    
				num_roms++;	     	 		     
			}
		}

		strcpy(tname,rom3);
		strcat(tname,"/*.gb"); 

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);

		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);			    
				num_roms++;	     	 		     
			}
		}

		strcpy(tname,rom3);
		strcat(tname,"/*.GBC"); 

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);

		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);			    
				num_roms++;	     	 		     
			}
		}	 

		strcpy(tname,rom3);
		strcat(tname,"/*.gbc"); 

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);
    		
		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);
				num_roms++;	     	 		     
			}
		}
 	
		strcpy(tname,rom3);
		strcat(tname,"/*.ZIP"); 

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);

		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);			    
				num_roms++;	     	 		     
			}
		} 

		strcpy(tname,rom3);
		strcat(tname,"/*.zip");

		mcGetDir(numMC, 0, tname, 0, ARRAY_ENTRIES - 10, mcDir);
		mcSync(0, NULL, &ret2);
		printf("\nmcGetDir returned %d\n\nListing of %s directory on memory card:\n\n",
			ret2,tname);

		for(i=0; i < ret2; i++){  
			if(mcDir[i].attrFile & MC_ATTR_SUBDIR);
			//	printf("[DIR] %d %s\n",i,mcDir[i].name);
			else{
				sprintf(romdata[num_roms].filename,"mc0:%s/%s",rom3,mcDir[i].name);
				strncpy(romdata[num_roms].name,mcDir[i].name,20);
				printf("%s - %d bytes %s\n", mcDir[i].name,
					mcDir[i].fileSizeByte,romdata[num_roms].filename);			    
				num_roms++;	     	 		     
			}
		} 
	} else if ( DEVICE_FLAG == 1 ) {  //If processing CD
		//Process ROMs from CDVD

		strcpy(tname,rom3);
		strcat(tname,"/"); 

		while(CDVD_DiskReady(CdBlock)==CdNotReady);
		ret2 = CDVD_getdir(tname,".GB" ,
			CDVD_GET_FILES_ONLY, myTocEntries, ARRAY_ENTRIES-1, tname);
		printf("Retrieved %d .GB entries\n\n",ret2);

		for (i = 0; i < ret2; i++) {
			printf("Dir name: %s\tLBA = %d\tSize = %d\n",
				myTocEntries[i].filename,
				myTocEntries[i].fileLBA,
				myTocEntries[i].fileSize); 
			sprintf(romdata[num_roms].filename,"cdfs:/%s/%s",rom3,myTocEntries[i].filename);
			strncpy(romdata[num_roms].name,myTocEntries[i].filename,20);
			num_roms++;	 
		}

		while(CDVD_DiskReady(CdBlock)==CdNotReady);
		ret2 = CDVD_getdir(tname,".GBC" ,
			CDVD_GET_FILES_ONLY, myTocEntries, ARRAY_ENTRIES, tname);
		printf("Retrieved %d .GBC entries\n\n",ret2);

		for (i = 0;i<ret2;i++) {
			printf("Dir name: %s\tLBA = %d\tSize = %d\n",
				myTocEntries[i].filename,
				myTocEntries[i].fileLBA,
				myTocEntries[i].fileSize); 

			sprintf(romdata[num_roms].filename,"cdfs:/%s/%s",rom3,myTocEntries[i].filename);
			strncpy(romdata[num_roms].name,myTocEntries[i].filename,20);
			num_roms++;	 
		}

		while(CDVD_DiskReady(CdBlock)==CdNotReady);
		ret2 = CDVD_getdir(tname,".ZIP" ,
			CDVD_GET_FILES_ONLY, myTocEntries, ARRAY_ENTRIES, tname);
		printf("Retrieved %d .ZIP entries\n\n",ret2);

		for (i = 0;i<ret2;i++) {
			printf("Dir name: %s\tLBA = %d\tSize = %d\n",
				myTocEntries[i].filename,
				myTocEntries[i].fileLBA,
				myTocEntries[i].fileSize); 

			sprintf(romdata[num_roms].filename,"cdfs:/%s/%s",rom3,myTocEntries[i].filename);
			strncpy(romdata[num_roms].name,myTocEntries[i].filename,20);
			num_roms++;	 
		}
	} else if ( DEVICE_FLAG == 2 ) {
      // KarasQ: Process ROMs from USB_MASS
      if ( usb_mass_support == 2 ) {
         clean_mass_store_data(mass_store_data);
         
         sprintf(usbpath, "mass:%s/", rom3);
         ret2 = read_mass(usbpath, mass_store_data, ARRAY_ENTRIES - 1);
         
         if ( ret2 > 0 ) {
            for (i = 0; i < ret2; i++) {
               strtolower_to(tname, mass_store_data[i].name, 35);
               
               if ( !mass_store_data[i].is_dir && ( 
                  strstr(tname, ".zip") || strstr(tname, ".gbc") || strstr(tname, ".gb") )
               ) {
                  sprintf(romdata[num_roms].filename, "mass:%s/%s", rom3, mass_store_data[i].name);
			         strncpy(romdata[num_roms].name, mass_store_data[i].name, 35);
			         
			         num_roms++;
               }
            }
         }
      } else {
         display_error("USB IRX Modules not loaded!", 0);
      }
   }
}
//---------------------------------------------------------------------------
//End of file:  ps2romselect.c
//---------------------------------------------------------------------------
