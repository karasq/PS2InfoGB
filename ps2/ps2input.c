//---------------------------------------------------------------------------
//File name:    ps2input.c
//---------------------------------------------------------------------------
#include <tamtypes.h>
#include <stdio.h>
#include <libpad.h>
#include "hw.h"
//---------------------------------------------------------------------------

#define waitPadReady WAIT_PAD_READY
#define scr_printf printf

extern char padBuf1[256] __attribute__((aligned(64))) __attribute__ ((section (".bss")));
extern char padBuf2[256] __attribute__((aligned(64))) __attribute__ ((section (".bss")));

extern unsigned long  dwKeyPad1;
extern unsigned long  dwKeyPad2;
//extern DWORD unsigned longdwKeySystem;

char actAlign[6];
 int actuators;

//---------------------------------------------------------------------------
int WAIT_PAD_READY(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1; 
    
    padStateInt2String(state, stateString);
    printf("0)Please wait, pad(%d,%d) is in state %s\n", port, slot, stateString);
        WaitForNextVRstart(1);   
            
    while( (state != PAD_STATE_DISCONN) && (state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) ) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
            printf("Please wait, pad(%d,%d) is in state %s\n", 
                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        printf("1)Pad OK!\n");
    }
    return 0;
}

//---------------------------------------------------------------------------
int initializePad(int port, int slot)
{

    int ret;
    int modes;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    scr_printf("The device has %d modes\n", modes);

    if (modes > 0) {
        scr_printf("( ");
        for (i = 0; i < modes; i++) {
            scr_printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
        scr_printf(")");
    }

    scr_printf("It is currently using mode %d\n", 
               padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller 
    // (it has no actuator engines)
    if (modes == 0) {
        scr_printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
        scr_printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        scr_printf("This is no Dual Shock controller??\n");
        return 1;
    }

    scr_printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
    scr_printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);        
    scr_printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    scr_printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        scr_printf("padSetActAlign: %d\n", 
                   padSetActAlign(port, slot, actAlign));
    }
    else {
        scr_printf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}

//---------------------------------------------------------------------------
int setup_pad(){

 padInit(0);
   
   // hard way to get psxpad working !
   
    printf("PortMax: %d\n", padGetPortMax());
    printf("SlotMax: %d\n", padGetSlotMax(0));  
   
   	if( padPortOpen(0, 0, padBuf1) == 0) {
        printf("padOpenPort 0 failed: \n");
        SleepThread();
    }
    WaitForNextVRstart(1);
    if(!initializePad(0, 0)) {
        printf("pad0 initalization failed!\n");
        SleepThread();
    }    
    WaitForNextVRstart(1);  
    if( padPortOpen(1, 0, padBuf2) == 0) {
        printf("padOpenPort 1 failed: \n");
        SleepThread();
    }    
    WaitForNextVRstart(1);
    if(!initializePad(1, 0)) {
        printf("pad1 initalization failed!\n");
        SleepThread();
    }    
    WaitForNextVRstart(1);

  return 0;  
}
//---------------------------------------------------------------------------
/*
#define GB_START	0x80
#define GB_SELECT	0x40
#define GB_B		0x20
#define GB_A		0x10
#define GB_UP		0x04
#define GB_DOWN		0x08
#define GB_LEFT		0x02
#define GB_RIGHT	0x01
*/
//---------------------------------------------------------------------------
#define INPUT_LEFT   0x02
#define INPUT_RIGHT  0x01
#define INPUT_UP	   0x04
#define INPUT_DOWN	0x08
#define INPUT_SELECT 0x40
#define INPUT_B2     0x20
#define INPUT_B1     0x10
#define INPUT_RUN    0x80

#define  TURBO_MASK (1 << 1) /* RA NB: 2 scans per transition */

//---------------------------------------------------------------------------
void ps2_update_input()
{
	static struct padButtonStatus pad1; // just in case
	static struct padButtonStatus pad2;
	static int pad1_connected = 0, pad2_connected = 0;
//RA NB: I'm redefining the turbo button stuff because the old stuff was crap.
//It was erroneously and incompletely implemented.
//I'm implementing turbo for all NES buttons, using eight variables below.
	static int p1_tB=0, p1_tA=0, p1_tSel=0, p1_tSta=0;
	static int p2_tB=0, p2_tA=0, p2_tSel=0, p2_tSta=0;
	int pad1_data = 0;
	int pad2_data = 0;
	int ret1;
	int ret2;
    
	ret1=padGetState(0, 0);
	while((ret1 != PAD_STATE_DISCONN)
		&& (ret1 != PAD_STATE_STABLE)
		&& (ret1 != PAD_STATE_FINDCTP1)) {
			if(ret1==PAD_STATE_DISCONN) {
				printf("Pad(%d, %d) is disconnected\n", 0, 0);
				break;
			}
			ret1=padGetState(0, 0);
	}

	if( ret1 != PAD_STATE_DISCONN )
		pad1_connected = padRead(0, 0, &pad1); //now working with psx pad !

	ret2=padGetState(1, 0);
	while((ret2 != PAD_STATE_DISCONN)
		&& (ret2 != PAD_STATE_STABLE)
		&& (ret2 != PAD_STATE_FINDCTP1)) {
		if(ret2==PAD_STATE_DISCONN) {
			printf("Pad(%d, %d) is disconnected\n", 1, 0);
			break;
		}
		ret2=padGetState(1, 0);
	}

	if( ret2 != PAD_STATE_DISCONN )
		pad2_connected = padRead(1, 0, &pad2); //now working with psx pad !

	//memset(&input, 0, sizeof(t_input));
	dwKeyPad1 = 0;
	dwKeyPad2 = 0;

	if ( pad1_connected ) {
		//	padRead(0, 0, &pad1); // port, slot, buttons
		pad1_data = 0xffff ^ pad1.btns;

		if(pad1_data & PAD_L1) p1_tB   += 1; else p1_tB   = 0; //Count Turbo B
		if(pad1_data & PAD_R1) p1_tA   += 1; else p1_tA   = 0; //Count Turbo A
		if(pad1_data & PAD_L2) p1_tSel += 1; else p1_tSel = 0; //Count Turbo Select
		if(pad1_data & PAD_R2) p1_tSta += 1; else p1_tSta = 0; //Count Turbo Start

		if(pad1_data & PAD_LEFT)				dwKeyPad1 |= INPUT_LEFT;
		if(pad1_data & PAD_RIGHT)				dwKeyPad1 |= INPUT_RIGHT;
		if(pad1_data & PAD_UP)					dwKeyPad1 |= INPUT_UP;
		if(pad1_data & PAD_DOWN)				dwKeyPad1 |= INPUT_DOWN;

		if((pad1_data&PAD_CROSS)   ||(TURBO_MASK&p1_tB))   dwKeyPad1 |= INPUT_B2;       
		if((pad1_data&PAD_CIRCLE)  ||(TURBO_MASK&p1_tA))   dwKeyPad1 |= INPUT_B1;
		if((pad1_data&PAD_SQUARE)  ||(TURBO_MASK&p1_tSel)) dwKeyPad1 |= INPUT_SELECT;
		if((pad1_data&PAD_TRIANGLE)||(TURBO_MASK&p1_tSta)) dwKeyPad1 |= INPUT_RUN;

		if((pad1.mode >> 4) == 0x07) {
			if(pad1.ljoy_h < 64)dwKeyPad1 |= INPUT_LEFT;
			else if(pad1.ljoy_h > 192) dwKeyPad1 |= INPUT_RIGHT;
			if(pad1.ljoy_v < 64) dwKeyPad1 |= INPUT_UP;
			else if(pad1.ljoy_v > 192) dwKeyPad1 |= INPUT_DOWN;
		}
	}

	if(pad2_connected) {
	//	padRead(1, 0, &pad2); // port, slot, buttons
		pad2_data = 0xffff ^ pad2.btns;

		if(pad2_data & PAD_L1) p2_tB   += 1; else p2_tB   = 0; //Count Turbo B
		if(pad2_data & PAD_R1) p2_tA   += 1; else p2_tA   = 0; //Count Turbo A
		if(pad2_data & PAD_L2) p2_tSel += 1; else p2_tSel = 0; //Count Turbo Select
		if(pad2_data & PAD_R2) p2_tSta += 1; else p2_tSta = 0; //Count Turbo Start

		if(pad2_data & PAD_LEFT)				dwKeyPad2 |= INPUT_LEFT;
		if(pad2_data & PAD_RIGHT)				dwKeyPad2 |= INPUT_RIGHT;
		if(pad2_data & PAD_UP)					dwKeyPad2 |= INPUT_UP;
		if(pad2_data & PAD_DOWN)				dwKeyPad2 |= INPUT_DOWN;

		if((pad2_data&PAD_CROSS)   ||(TURBO_MASK&p2_tB))   dwKeyPad2 |= INPUT_B2;       
		if((pad2_data&PAD_CIRCLE)  ||(TURBO_MASK&p2_tA))   dwKeyPad2 |= INPUT_B1;
		if((pad2_data&PAD_SQUARE)  ||(TURBO_MASK&p2_tSel)) dwKeyPad2 |= INPUT_SELECT;
		if((pad2_data&PAD_TRIANGLE)||(TURBO_MASK&p2_tSta)) dwKeyPad2 |= INPUT_RUN;

		if((pad2.mode >> 4) == 0x07) {
			if(pad2.ljoy_h < 64)       dwKeyPad2 |= INPUT_LEFT;
			else if(pad2.ljoy_h > 192) dwKeyPad2 |= INPUT_RIGHT;
			if(pad2.ljoy_v < 64)       dwKeyPad2 |= INPUT_UP;
			else if(pad2.ljoy_v > 192) dwKeyPad2 |= INPUT_DOWN;
		}
	}

	if(pad1_data & PAD_START) {
		while(1) {
			if(padGetState(0, 0) == PAD_STATE_STABLE) {
				padRead(0, 0, &pad1); // port, slot, buttons
				pad1_data = 0xffff ^ pad1.btns;
			}
			if(!(pad1_data & PAD_CROSS) && !(pad1_data & PAD_START))
				break;  // break when those 2 buttons are released
		}
        IngameMenu();
	}
}
//---------------------------------------------------------------------------
//End of file:  ps2input.c
//---------------------------------------------------------------------------
