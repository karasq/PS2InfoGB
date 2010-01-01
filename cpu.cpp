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

#include <fileio.h>
#include "PS2InfoGB.h"

#include "system.h"
#include "rom.h"
#include "mem.h"
#include "cpu.h"
#include "vram.h"
#include "regs.h"
#include "data.h"
#include "joypad.h"
#include "sound.h"
#include "globals.h"
#include "RTC.h"

#define rgbtab g_RGBTab

// Macros
#define memory_read_pc_word() (gameboy_memory[gbz80.PC.uw >> 12][gbz80.PC.uw & 0x0FFF] | \
      (gameboy_memory[(gbz80.PC.uw+1) >> 12][(gbz80.PC.uw+1) & 0x0FFF] << 8))
#define memory_read_pc_byte() (gameboy_memory[gbz80.PC.uw >> 12][gbz80.PC.uw & 0x0FFF])

// Interrupt type stuff
#define VBL	0x01
#define LCD	0x02
#define TIM	0x04
#define SIO	0x08
#define JOY	0x10 

static int current_opcode;
jmp_buf EmuJmp;

gameboy_proc_t gbz80;

int HaltActive;

int ScrCycle;
int SioCycle;
int DivCycle;
int TimCycle;
int JoyProbe;
int TimLoad;

int VblIntDelay;
int LcdIntDelay;

int DrawDelay;

int vram_mode;

int OldJoy;

int CPUSpeed = 1;

extern int HDMADst;
extern int HDMASrc;
extern int HDMACnt;
extern char vidram[0x4000];
extern char intram[0x8000];
extern int vram_mode;
extern int HuC3_RAMFlag;
extern int HuC3_RAMValue;
extern int HuC3_Reg[2];

extern unsigned char mbc1_ram_bank_enable;
extern unsigned char mbc1_address_line;
extern unsigned short int mbc5_rom_select;
extern unsigned char *gameboy_memory[16];
extern unsigned short rgbtab[256];

typedef struct _SaveGame {
   int HaltActive;
   int ScrCycle;
   int SioCycle;
   int DivCycle;
   int TimCycle;
   int JoyProbe;
   int TimLoad;
   int VblIntDelay;
   int LcdIntDelay;
   int DrawDelay;
   int CPUSpeed;
   int vram_mode;
   int color_gameboy;
   int HDMADst;
   int HDMASrc;
   int HDMACnt;
   char himem[0x100];
   char vidram[0x4000];
   char intram[0x8000];
   unsigned char oam[0xA0];
   unsigned short rgbtab[0x100];
   unsigned short int bkg_pal[8][4];
   unsigned short int obj_pal[8][4];
   unsigned short int mbc5_rom_select;
   unsigned char mbc1_ram_bank_enable;
   unsigned char mbc1_address_line;
   int HuC3_RAMFlag;
   int HuC3_RAMValue;
   int HuC3_Reg[2];
   gameboy_proc_t cpu;
   RTC_Data RTC;
   char dummy[0x200]; // for future sound data
} SaveGame;

bool gameboy_save_state() {
   SaveGame Save;
   
   int f = fioOpen(g_PS2Browser.getSavePath(cartridge_fname, ".srm"), 
               O_WRONLY | O_CREAT | O_BINARY );
   
   if ( f < 0 ) {
      return false;
   }
   
   memcpy(&Save.cpu, &gbz80, sizeof(gameboy_proc_t));
   memcpy(&Save.RTC, &RTC, sizeof(RTC_Data));
   memcpy(Save.himem, &hiram[0xF00], sizeof(unsigned char)*0x100); // registers only
   memcpy(Save.vidram, vidram, sizeof(char)*0x4000);
   memcpy(Save.intram, intram, sizeof(char)*0x8000);
   memcpy(Save.oam, sprite_oam, sizeof(unsigned char)*0xA0);
   memcpy(Save.rgbtab, rgbtab, sizeof(unsigned short)*0x100);
   memcpy(Save.bkg_pal, bkg_palettes, sizeof(unsigned short int)*0x20);
   memcpy(Save.obj_pal, obj_palettes, sizeof(unsigned short int)*0x20);
   
   if ( cartridge_type == TYPE_ROM_HUDSON_HUC3 ) {
      memcpy(Save.HuC3_Reg, HuC3_Reg, sizeof(int)*2);
      Save.HuC3_RAMFlag = HuC3_RAMFlag;
      Save.HuC3_RAMValue = HuC3_RAMValue;
   }
   
   Save.HaltActive = HaltActive;
   Save.ScrCycle = ScrCycle;
   Save.SioCycle = SioCycle;
   Save.DivCycle = DivCycle;
   Save.TimCycle = TimCycle;
   Save.JoyProbe = JoyProbe;
   Save.TimLoad = TimLoad;
   Save.VblIntDelay = VblIntDelay;
   Save.LcdIntDelay = LcdIntDelay;
   Save.DrawDelay = DrawDelay;
   
   Save.color_gameboy = color_gameboy;
   Save.CPUSpeed = CPUSpeed;
   Save.HDMADst = HDMADst;
   Save.HDMASrc = HDMASrc;
   Save.HDMACnt = HDMACnt;
   Save.vram_mode = vram_mode;
   
   Save.mbc1_ram_bank_enable = mbc1_ram_bank_enable;
   Save.mbc1_address_line = mbc1_address_line;
   Save.mbc5_rom_select = mbc5_rom_select;
   
   fioWrite(f, &Save, sizeof(SaveGame));
   fioClose(f);
   
   return true;
}

bool gameboy_read_state() {
   SaveGame Save;
   
   int f = fioOpen(g_PS2Browser.getSavePath(cartridge_fname, ".srm"),
               O_RDONLY | O_BINARY);
   
   if ( f < 0 ) {
      return false;
   }
   
   fioRead(f, &Save, sizeof(SaveGame));
   fioClose(f);
    
   memcpy(&gbz80, &Save.cpu, sizeof(gameboy_proc_t));
   memcpy(&RTC, &Save.RTC, sizeof(RTC_Data));
   memcpy(&hiram[0xF00], Save.himem, sizeof(unsigned char)*0x100); // registers only
   memcpy(sprite_oam, Save.oam, sizeof(unsigned char)*0xA0);
   memcpy(vidram, Save.vidram, sizeof(char)*0x4000);
   memcpy(intram, Save.intram, sizeof(char)*0x8000);
   memcpy(rgbtab, Save.rgbtab, sizeof(unsigned short)*0x100);
   memcpy(bkg_palettes, Save.bkg_pal, sizeof(unsigned short int)*0x20);
   memcpy(obj_palettes, Save.obj_pal, sizeof(unsigned short int)*0x20);
   
   if ( cartridge_type == TYPE_ROM_HUDSON_HUC3 ) {
      memcpy(HuC3_Reg, Save.HuC3_Reg, sizeof(int)*2);
      HuC3_RAMFlag = Save.HuC3_RAMFlag;
      HuC3_RAMValue = Save.HuC3_RAMValue;
   } 
   
   HaltActive = Save.HaltActive;
   ScrCycle = Save.ScrCycle;
   SioCycle = Save.SioCycle;
   DivCycle = Save.DivCycle; 
   TimCycle = Save.TimCycle ;
   JoyProbe = Save.JoyProbe;
   TimLoad = Save.TimLoad;
   VblIntDelay = Save.VblIntDelay;
   LcdIntDelay = Save.LcdIntDelay;
   DrawDelay = Save.DrawDelay;
   
   color_gameboy = Save.color_gameboy;
   vram_mode = Save.vram_mode;
   CPUSpeed = Save.CPUSpeed;
   HDMADst = Save.HDMADst;
   HDMASrc = Save.HDMASrc;
   HDMACnt = Save.HDMACnt;
   
   mbc1_ram_bank_enable = Save.mbc1_ram_bank_enable;
   mbc1_address_line = Save.mbc1_address_line;
   mbc5_rom_select = Save.mbc5_rom_select;
   
   current_joypad = 0;
   
   return true;
}

void gameboy_cpu_hardreset()
{	
	if (color_gameboy)
		gbz80.AF.b.h = 0x11;
	else
		gbz80.AF.b.h = 0x01;
	
	gbz80.AF.b.l = 0xB0;
	gbz80.BC.uw  = 0x0013;
	gbz80.DE.uw  = 0x00D8;
	gbz80.HL.uw  = 0x014D;
	gbz80.SP.uw  = 0xFFFE;
	gbz80.PC.uw  = 0x0100;
	gbz80.IFF    = 0;

	gbz80.machine_cycles = 0;
	
	memory_write_byte(0xFF05, 0x00);
	memory_write_byte(0xFF06, 0x00);
	memory_write_byte(0xFF07, 0x00);
	memory_write_byte(0xFF26, 0x80);
	memory_write_byte(0xFF10, 0x80);
	memory_write_byte(0xFF11, 0xBF);
	memory_write_byte(0xFF12, 0xF3);
	memory_write_byte(0xFF14, 0xBF);
	memory_write_byte(0xFF16, 0x3F);
	memory_write_byte(0xFF17, 0x00);
	memory_write_byte(0xFF19, 0xBF);
	memory_write_byte(0xFF1A, 0x7A);
	memory_write_byte(0xFF1B, 0xFF);
	memory_write_byte(0xFF1C, 0x9F);
	memory_write_byte(0xFF1E, 0xBF);
	memory_write_byte(0xFF20, 0xFF);
	memory_write_byte(0xFF21, 0x00);
	memory_write_byte(0xFF22, 0x00);
	memory_write_byte(0xFF23, 0xBF);
	memory_write_byte(0xFF24, 0x77);
	memory_write_byte(0xFF25, 0xF3);
	memory_write_byte(0xFF40, 0x91);
	memory_write_byte(0xFF41, 0x80);
	memory_write_byte(0xFF42, 0x00);
	memory_write_byte(0xFF43, 0x00);
	memory_write_byte(0xFF45, 0x00);
	memory_write_byte(0xFF47, 0xFC);
	memory_write_byte(0xFF48, 0xFF);
	memory_write_byte(0xFF49, 0xFF);
	memory_write_byte(0xFF4A, 0x00);
	memory_write_byte(0xFF4B, 0x00);
	memory_write_byte(0xFFFF, 0x00);
}

void gameboy_cpu_run()
{	
	gbz80.running = 1;
	
	setjmp(EmuJmp);
	
	if ( super_gameboy ) {
      g_CycleTarget = g_CycleTargetSGB;
   } else {
      g_CycleTarget = g_CycleTargetGB;
   }
   
	while ( gbz80.running ) {
		gameboy_update();	
	}
}

static inline void gameboy_stack_write_word(unsigned short val)
{
	gbz80.SP.uw -= 2;
	memory_write_word(gbz80.SP.uw, val);
}

static inline unsigned short int gameboy_stack_read_word()
{
	unsigned short int ret;
	
	ret = memory_read_word(gbz80.SP.uw);
	
	gbz80.SP.uw += 2;
	
	return ret;
}

static inline void gameboy_update_stuff(int cycles);

inline void gameboy_update()
{
   //int CyclesUpdate = 0;
         
   //while ( g_CycleTarget > CyclesUpdate )
   //{
      if (gbz80.running == 0) 
         return;
         
      gbz80.machine_cycles = 0;
      
      if (!HaltActive)  {
         current_opcode = memory_read_pc_byte();
         gbz80.PC.uw++;
         
         gameboy_cpu_execute_opcode(current_opcode);
      } else {
         //MORE_CYCLES(8);
         gbz80.machine_cycles += 8;
      }
      
      //CyclesUpdate += gbz80.machine_cycles;
      gameboy_update_stuff(gbz80.machine_cycles);
   //}
}

static inline void gameboy_update_stuff(int cycles) 
{
   // Sound
   if (SoundEnabled)  {
      //ProcessSound(cycles);

	  int c = 0;
	  SoundCycle -= cycles;
	  while (SoundCycle <= 0) {
		 SoundCycle += SoundLoader*CPUSpeed;
		 c += SoundLoader;
	  }
	  if (c)
		 ProcessSound(c);
   }
   
   if (SIOCONT & 0x80) {
      if (SIOCONT & 0x01) {
         int NewSet = 0;
         
         SioCycle += cycles;
         
         if (color_gameboy) {
            if (SIOCONT & 0x02) {
               if (KEY1REG & 0x80) {
                  if (SioCycle > 15)  /* 64 ? */
                     NewSet |= SIO;
               } else {
                  if (SioCycle > 127)
                     NewSet |= SIO;
					}
            } else {
               if (KEY1REG & 0x80) {
                  if (SioCycle > 511)  /* 2048 ? */
                     NewSet |= SIO;
               } else {
                  if (SioCycle > 4095)
                     NewSet |= SIO;
               }
            }
         } else {
            if (SioCycle > 4095) {
               NewSet |= SIO;
            }
			}
         
         if (NewSet & SIO) {
            if (SIOCONT & 0x01)
               IFLAGS |= 0x08;
            
            SioCycle = 0;
            SIOCONT &= 0x7F;
         }	
      }
   }
   
   DivCycle -= cycles;
   while (DivCycle <= 0) {
      DIVIDER++;
      DivCycle += 256*CPUSpeed;
   }
   
   if (TIMCONT & 0x04) {		
      TimCycle -= cycles;
      
      while (TimCycle <= 0) {
         TIMECNT++;
         if (TIMECNT == 0) {
            TIMECNT = TIMEMOD;
            IFLAGS |= 0x04;
         }
         TimCycle += TimLoad;
      }
   }	
   
   if (LCDCONT & 0x80) {
      extern int window_current_line;	
      ScrCycle -= cycles;
      
      while (ScrCycle <= 0) 
      {
         switch(vram_mode) 
         {
            case 0: /* end of hblank */ 
               CURLINE++;
               if (LCDSTAT & 0x40) { 
                  if (CURLINE == CMPLINE) {
                     if (!(IFLAGS & 0x02)) {
                        IFLAGS |= 0x02;
                        LcdIntDelay = 32*CPUSpeed+ScrCycle;
                     }
                  }
               }
               if (CURLINE == 0x90) {
                  if (LCDSTAT & 0x10) {
                     if (!(IFLAGS & 0x02)) {
                        IFLAGS |= 0x02;
                        LcdIntDelay = 32*CPUSpeed+ScrCycle;
                     }
                  }
                  
                  vram_mode = 1;
                  infogb_vram_blit();
                  
                  window_current_line = 0;
                  vram_sysupdate();
                  
                  if (OldJoy != current_joypad) {
                     OldJoy = current_joypad;
                     IFLAGS |= 0x10;
                  }
                  
                  if (!(IFLAGS & 0x01)) {
                     IFLAGS |= 0x01;
                     VblIntDelay = 16*CPUSpeed+ScrCycle;
                  }
                  ScrCycle += 451*CPUSpeed;
               } else {
                  if (LCDSTAT & 0x20) {
                     if (!(IFLAGS & 0x02)) {
                        IFLAGS |= 0x02;
                        LcdIntDelay = 0*CPUSpeed+ScrCycle;
                     }
                  }
                  vram_mode = 2;
                  ScrCycle += 80*CPUSpeed;
               }
               break;
            case 1:
               CURLINE++;	
               if (CURLINE > 153) {
                  CURLINE -= 154;
                  vram_mode = 2;
                  if (LCDSTAT & 0x20) {
                     if (!(IFLAGS & 0x02)) {
                        IFLAGS |= 0x02;
                        LcdIntDelay = 0*CPUSpeed+ScrCycle;
                     }
                  }
               
                  if (LCDSTAT & 0x40) { 
                     if (CURLINE == CMPLINE) {
                        if (!(IFLAGS & 0x02)) {
                           IFLAGS |= 0x02;
                           LcdIntDelay = 32*CPUSpeed+ScrCycle;
                        }
                     }
                  }
                  ScrCycle += 80*CPUSpeed;
               } else {
                  vram_mode = 1;
                  if (LCDSTAT & 0x40) { 
                     if (CURLINE == CMPLINE) {
                        if (!(IFLAGS & 0x02)) {
                           IFLAGS |= 0x02;
                           LcdIntDelay = 32*CPUSpeed+ScrCycle;
                        }
                     }
                  }
                  ScrCycle += 451*CPUSpeed;
               }
               break;
            case 2:
               vram_mode = 3;
            // DrawDelay = 152*CPUSpeed;
               vram_plot_screen();
               ScrCycle += 172*CPUSpeed;
               break;
            case 3:
               vram_mode = 0;
               if (LCDSTAT & 0x08) {
                  if (!(IFLAGS & 0x02)) {
                     IFLAGS |= 0x02;
                     LcdIntDelay = 32*CPUSpeed+ScrCycle;
                  }
               }
               if (HDMADst) {
                  if (HDMACnt) {
                     int i, a, b;
                     a = HDMASrc >> 12;
                     b = HDMADst >> 12;
                     
                     for (i = 0; i < 16; i++) {
                        gameboy_memory[b][HDMADst & 0x0FFF] = gameboy_memory[a][HDMASrc & 0x0FFF];
                        HDMASrc++;
                        HDMADst++;
                     }
                     HDMACnt--;
                     
                     if (HDMACnt == 0)
                        HDMADst = 0;
                     
                     //MORE_CYCLES((204*CPUSpeed));
                     gbz80.machine_cycles += (204*CPUSpeed);
                  }
               } 
               ScrCycle += 204*CPUSpeed;
               break;
         } // end switch
      } // end while
   
   } else {
      vram_mode = 0;
      CURLINE = 0;
      ScrCycle = 0;
      LCDSTAT &= ~0x04;
      JoyProbe += cycles;
      
      if (JoyProbe > (70221*CPUSpeed)) {
         JoyProbe -= 70221*CPUSpeed;
         vram_sysupdate();
         
         if (OldJoy != current_joypad) {
            OldJoy = current_joypad;
            IFLAGS |= 0x10;
         }
      }
   }

	if (gbz80.IFF) {
		int Int = IFLAGS & IENABLE;
		if (Int & 0x01) {
			VblIntDelay -= cycles;
			if (VblIntDelay <= 0) {
				VblIntDelay = 0;
				IFLAGS &= ~0x01;
				gbz80.IFF = 0;
				gameboy_stack_write_word(gbz80.PC.uw);
				gbz80.PC.uw = 0x40;
			}
		} else if (Int & 0x02) {
			//LcdIntDelay -= cycles;
			//if (LcdIntDelay <= 0) {
			//	LcdIntDelay = 0;
				IFLAGS &= ~0x02;
				gbz80.IFF = 0;
				gameboy_stack_write_word(gbz80.PC.uw);
				gbz80.PC.uw = 0x48;
			//}
		} else if (Int & 0x04) {
			IFLAGS &= ~0x04;
			gbz80.IFF = 0;
			gameboy_stack_write_word(gbz80.PC.uw);
			gbz80.PC.uw = 0x50;
		} else if (Int & 0x08) {
			IFLAGS &= ~0x08;
			gbz80.IFF = 0;
			gameboy_stack_write_word(gbz80.PC.uw);
			gbz80.PC.uw = 0x58;
		} else if (Int & 0x10) {
			IFLAGS &= ~0x10;
			gbz80.IFF = 0;
			gameboy_stack_write_word(gbz80.PC.uw);
			gbz80.PC.uw = 0x60;
		} 
	} else if (HaltActive) {
		HaltActive = 0;
	}
}

// Opcodes implementations
// ADC
#define INST_ADC(before, adding) {\
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 1 : 0;\
	int result = before + adding + carry;\
\
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);\
\
	if(result & 0xFF00)\
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);\
\
	if(result & 0xFF)\
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);\
	else\
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);\
\
	if (((adding & 0x0F) + (before & 0x0F) + carry) > 0x0F)\
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);\
\
	before = result & 0xFF;\
}

inline void inst_adc_a_a() { INST_ADC(gbz80.AF.b.h, gbz80.AF.b.h) }
inline void inst_adc_a_b() { INST_ADC(gbz80.AF.b.h, gbz80.BC.b.h) }
inline void inst_adc_a_c() { INST_ADC(gbz80.AF.b.h, gbz80.BC.b.l) }
inline void inst_adc_a_d() { INST_ADC(gbz80.AF.b.h, gbz80.DE.b.h) }
inline void inst_adc_a_e() { INST_ADC(gbz80.AF.b.h, gbz80.DE.b.l) }
inline void inst_adc_a_h() { INST_ADC(gbz80.AF.b.h, gbz80.HL.b.h) }
inline void inst_adc_a_l() { INST_ADC(gbz80.AF.b.h, gbz80.HL.b.l) }

inline void inst_adc_a_n() {
	int value = memory_read_pc_byte();
	INST_ADC(gbz80.AF.b.h, value)
	gbz80.PC.uw++;
}
   
inline void inst_adc_a_hl_indirect() { 
	int value = memory_read_byte(gbz80.HL.uw);
	INST_ADC(gbz80.AF.b.h, value)
}

// ADD 8-bit
#define INST_ADD(before, adding) {\
	int result = before + adding;\
\
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);\
\
	if (result & 0xFF00)\
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);\
\
	if (result & 0x00FF)\
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);\
	else\
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);\
\
	if (((adding & 0x0F) + (before & 0x0F)) > 0x0F)\
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);\
\
	before = result & 0xFF;\
}

inline void inst_add_a_a()
{
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	if (gbz80.AF.b.h & 0x08)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.AF.b.h += gbz80.AF.b.h;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
}

inline void inst_add_a_b() { INST_ADD(gbz80.AF.b.h, gbz80.BC.b.h) }
inline void inst_add_a_c() { INST_ADD(gbz80.AF.b.h, gbz80.BC.b.l) }
inline void inst_add_a_d() { INST_ADD(gbz80.AF.b.h, gbz80.DE.b.h) }
inline void inst_add_a_e() { INST_ADD(gbz80.AF.b.h, gbz80.DE.b.l) }
inline void inst_add_a_h() { INST_ADD(gbz80.AF.b.h, gbz80.HL.b.h) }
inline void inst_add_a_l() { INST_ADD(gbz80.AF.b.h, gbz80.HL.b.l) }

inline void inst_add_a_n() {
	int value = memory_read_pc_byte();
	INST_ADD(gbz80.AF.b.h, value);
	gbz80.PC.uw++;
}

inline void inst_add_a_hl_indirect() {
	int value = memory_read_byte(gbz80.HL.uw);
	INST_ADD(gbz80.AF.b.h, value)
}

// ADD 16-bit
#define INST_ADD_HL(value) {\
	unsigned long result = gbz80.HL.uw + value;\
\
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);\
\
	if(result & 0xFFFF0000)\
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);\
\
	if (((gbz80.HL.uw & 0xFFF) + (value & 0xFFF)) > 0xFFF)\
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);\
	else\
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);\
\
	gbz80.HL.uw = (unsigned short)(result & 0xFFFF);\
}

inline void inst_add_hl_bc() { INST_ADD_HL(gbz80.BC.uw) }
inline void inst_add_hl_de() { INST_ADD_HL(gbz80.DE.uw) }
inline void inst_add_hl_hl() { INST_ADD_HL(gbz80.HL.uw) }
inline void inst_add_hl_sp() { INST_ADD_HL(gbz80.SP.uw) }

///////////////////////////////////////////////////////
inline void inst_add_sp_n() {
	signed char value = memory_read_pc_byte();
	int result = gbz80.SP.uw + value;
	
	gbz80.PC.uw++;
	FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO | FLAGS_NEGATIVE);
	
	if (result & 0xFFFF0000)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	gbz80.SP.uw = result & 0xFFFF;
	
	/* TODO: i think */
	if (((gbz80.SP.uw & 0xFFF) + (value & 0xFFF)) > 0xFFF)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
}
////////////////////////////////////////////////////////

// AND A
#define INST_AND(reg) {\
	gbz80.AF.b.h &= reg;\
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE);\
	FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);\
\
	if (gbz80.AF.b.h)\
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);\
	else\
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);\
}

inline void inst_and_a() {
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE);
	FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
}

inline void inst_and_b() { INST_AND(gbz80.BC.b.h) }
inline void inst_and_c() { INST_AND(gbz80.BC.b.l) }
inline void inst_and_d() { INST_AND(gbz80.DE.b.h) }
inline void inst_and_e() { INST_AND(gbz80.DE.b.l) }
inline void inst_and_h() { INST_AND(gbz80.HL.b.h) }
inline void inst_and_l() { INST_AND(gbz80.HL.b.l) }
inline void inst_and_hl_indirect() { INST_AND(memory_read_byte(gbz80.HL.uw)) }

inline void inst_and_n() {
	INST_AND(memory_read_pc_byte());
	gbz80.PC.uw++;
}

inline void inst_call_c_nn()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gameboy_stack_write_word(gbz80.PC.uw + 2);
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(24);
	} else {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	}
}

inline void inst_call_nc_nn()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	} else {
		gameboy_stack_write_word(gbz80.PC.uw + 2);
		gbz80.PC.uw = memory_read_pc_word();;
		MORE_CYCLES(24);
	}
}

inline void inst_call_nn()
{
	gameboy_stack_write_word(gbz80.PC.uw + 2);
	gbz80.PC.uw = memory_read_word(gbz80.PC.uw);
	MORE_CYCLES(24);
}

inline void inst_call_nz_nn()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	} else {
		gameboy_stack_write_word(gbz80.PC.uw + 2);
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(24);
	}
}

inline void inst_call_z_nn()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		gameboy_stack_write_word(gbz80.PC.uw + 2);
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(24);
	} else {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	}
}

// CB inst
// CB Bit
#define INST_CB_BIT(reg, bit) {\
	if (reg & (1 << bit))\
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);\
	else\
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);\
\
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);\
	FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);\
}

// CB Bit 0
inline void inst_cb_bit_0_a() { INST_CB_BIT(gbz80.AF.b.h, 0) }
inline void inst_cb_bit_0_b() { INST_CB_BIT(gbz80.BC.b.h, 0) }
inline void inst_cb_bit_0_c() { INST_CB_BIT(gbz80.BC.b.l, 0) }
inline void inst_cb_bit_0_d() { INST_CB_BIT(gbz80.DE.b.h, 0) }
inline void inst_cb_bit_0_e() { INST_CB_BIT(gbz80.DE.b.l, 0) }
inline void inst_cb_bit_0_h() { INST_CB_BIT(gbz80.HL.b.h, 0) }
inline void inst_cb_bit_0_l() { INST_CB_BIT(gbz80.HL.b.l, 0) }
inline void inst_cb_bit_0_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 0) }

// CB Bit 1
inline void inst_cb_bit_1_a() { INST_CB_BIT(gbz80.AF.b.h, 1) }
inline void inst_cb_bit_1_b() { INST_CB_BIT(gbz80.BC.b.h, 1) }
inline void inst_cb_bit_1_c() { INST_CB_BIT(gbz80.BC.b.l, 1) }
inline void inst_cb_bit_1_d() { INST_CB_BIT(gbz80.DE.b.h, 1) }
inline void inst_cb_bit_1_e() { INST_CB_BIT(gbz80.DE.b.l, 1) }
inline void inst_cb_bit_1_h() { INST_CB_BIT(gbz80.HL.b.h, 1) }
inline void inst_cb_bit_1_l() { INST_CB_BIT(gbz80.HL.b.l, 1) }
inline void inst_cb_bit_1_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 1) }

// CB Bit 2
inline void inst_cb_bit_2_a() { INST_CB_BIT(gbz80.AF.b.h, 2) }
inline void inst_cb_bit_2_b() { INST_CB_BIT(gbz80.BC.b.h, 2) }
inline void inst_cb_bit_2_c() { INST_CB_BIT(gbz80.BC.b.l, 2) }
inline void inst_cb_bit_2_d() { INST_CB_BIT(gbz80.DE.b.h, 2) }
inline void inst_cb_bit_2_e() { INST_CB_BIT(gbz80.DE.b.l, 2) }
inline void inst_cb_bit_2_h() { INST_CB_BIT(gbz80.HL.b.h, 2) }
inline void inst_cb_bit_2_l() { INST_CB_BIT(gbz80.HL.b.l, 2) }
inline void inst_cb_bit_2_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 2) }

// CB Bit 3
inline void inst_cb_bit_3_a() { INST_CB_BIT(gbz80.AF.b.h, 3) }
inline void inst_cb_bit_3_b() { INST_CB_BIT(gbz80.BC.b.h, 3) }
inline void inst_cb_bit_3_c() { INST_CB_BIT(gbz80.BC.b.l, 3) }
inline void inst_cb_bit_3_d() { INST_CB_BIT(gbz80.DE.b.h, 3) }
inline void inst_cb_bit_3_e() { INST_CB_BIT(gbz80.DE.b.l, 3) }
inline void inst_cb_bit_3_h() { INST_CB_BIT(gbz80.HL.b.h, 3) }
inline void inst_cb_bit_3_l() { INST_CB_BIT(gbz80.HL.b.l, 3) }
inline void inst_cb_bit_3_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 3) }

// CB Bit 4
inline void inst_cb_bit_4_a() { INST_CB_BIT(gbz80.AF.b.h, 4) }
inline void inst_cb_bit_4_b() { INST_CB_BIT(gbz80.BC.b.h, 4) }
inline void inst_cb_bit_4_c() { INST_CB_BIT(gbz80.BC.b.l, 4) }
inline void inst_cb_bit_4_d() { INST_CB_BIT(gbz80.DE.b.h, 4) }
inline void inst_cb_bit_4_e() { INST_CB_BIT(gbz80.DE.b.l, 4) }
inline void inst_cb_bit_4_h() { INST_CB_BIT(gbz80.HL.b.h, 4) }
inline void inst_cb_bit_4_l() { INST_CB_BIT(gbz80.HL.b.l, 4) }
inline void inst_cb_bit_4_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 4) }

// CB Bit 5
inline void inst_cb_bit_5_a() { INST_CB_BIT(gbz80.AF.b.h, 5) }
inline void inst_cb_bit_5_b() { INST_CB_BIT(gbz80.BC.b.h, 5) }
inline void inst_cb_bit_5_c() { INST_CB_BIT(gbz80.BC.b.l, 5) }
inline void inst_cb_bit_5_d() { INST_CB_BIT(gbz80.DE.b.h, 5) }
inline void inst_cb_bit_5_e() { INST_CB_BIT(gbz80.DE.b.l, 5) }
inline void inst_cb_bit_5_h() { INST_CB_BIT(gbz80.HL.b.h, 5) }
inline void inst_cb_bit_5_l() { INST_CB_BIT(gbz80.HL.b.l, 5) }
inline void inst_cb_bit_5_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 5) }

// CB Bit 6
inline void inst_cb_bit_6_a() { INST_CB_BIT(gbz80.AF.b.h, 6) }
inline void inst_cb_bit_6_b() { INST_CB_BIT(gbz80.BC.b.h, 6) }
inline void inst_cb_bit_6_c() { INST_CB_BIT(gbz80.BC.b.l, 6) }
inline void inst_cb_bit_6_d() { INST_CB_BIT(gbz80.DE.b.h, 6) }
inline void inst_cb_bit_6_e() { INST_CB_BIT(gbz80.DE.b.l, 6) }
inline void inst_cb_bit_6_h() { INST_CB_BIT(gbz80.HL.b.h, 6) }
inline void inst_cb_bit_6_l() { INST_CB_BIT(gbz80.HL.b.l, 6) }
inline void inst_cb_bit_6_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 6) }

// CB Bit 7
inline void inst_cb_bit_7_a() { INST_CB_BIT(gbz80.AF.b.h, 7) }
inline void inst_cb_bit_7_b() { INST_CB_BIT(gbz80.BC.b.h, 7) }
inline void inst_cb_bit_7_c() { INST_CB_BIT(gbz80.BC.b.l, 7) }
inline void inst_cb_bit_7_d() { INST_CB_BIT(gbz80.DE.b.h, 7) }
inline void inst_cb_bit_7_e() { INST_CB_BIT(gbz80.DE.b.l, 7) }
inline void inst_cb_bit_7_h() { INST_CB_BIT(gbz80.HL.b.h, 7) }
inline void inst_cb_bit_7_l() { INST_CB_BIT(gbz80.HL.b.l, 7) }
inline void inst_cb_bit_7_hl_indirect() { INST_CB_BIT(memory_read_byte(gbz80.HL.uw), 7) }

// CB RESET
#define INST_CB_BIT_RESET(reg, bit) {\
   reg &= ~(1 << bit);\
}

// Bit 0
inline void inst_cb_res_0_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 0) }
inline void inst_cb_res_0_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 0) }
inline void inst_cb_res_0_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 0) }
inline void inst_cb_res_0_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 0) }
inline void inst_cb_res_0_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 0) }
inline void inst_cb_res_0_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 0) }
inline void inst_cb_res_0_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 0) }

inline void inst_cb_res_0_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 0)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 1
inline void inst_cb_res_1_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 1) }
inline void inst_cb_res_1_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 1) }
inline void inst_cb_res_1_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 1) }
inline void inst_cb_res_1_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 1) }
inline void inst_cb_res_1_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 1) }
inline void inst_cb_res_1_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 1) }
inline void inst_cb_res_1_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 1) }

inline void inst_cb_res_1_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 1)
	memory_write_byte(gbz80.HL.uw, what);
}
// Bit 2
inline void inst_cb_res_2_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 2) }
inline void inst_cb_res_2_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 2) }
inline void inst_cb_res_2_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 2) }
inline void inst_cb_res_2_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 2) }
inline void inst_cb_res_2_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 2) }
inline void inst_cb_res_2_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 2) }
inline void inst_cb_res_2_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 2) }

inline void inst_cb_res_2_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 2)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 3
inline void inst_cb_res_3_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 3) }
inline void inst_cb_res_3_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 3) }
inline void inst_cb_res_3_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 3) }
inline void inst_cb_res_3_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 3) }
inline void inst_cb_res_3_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 3) }
inline void inst_cb_res_3_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 3) }
inline void inst_cb_res_3_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 3) }

inline void inst_cb_res_3_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 3)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 4
inline void inst_cb_res_4_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 4) }
inline void inst_cb_res_4_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 4) }
inline void inst_cb_res_4_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 4) }
inline void inst_cb_res_4_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 4) }
inline void inst_cb_res_4_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 4) }
inline void inst_cb_res_4_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 4) }
inline void inst_cb_res_4_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 4) }

inline void inst_cb_res_4_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 4)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 5
inline void inst_cb_res_5_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 5) }
inline void inst_cb_res_5_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 5) }
inline void inst_cb_res_5_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 5) }
inline void inst_cb_res_5_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 5) }
inline void inst_cb_res_5_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 5) }
inline void inst_cb_res_5_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 5) }
inline void inst_cb_res_5_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 5) }

inline void inst_cb_res_5_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 5)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 6
inline void inst_cb_res_6_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 6) }
inline void inst_cb_res_6_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 6) }
inline void inst_cb_res_6_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 6) }
inline void inst_cb_res_6_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 6) }
inline void inst_cb_res_6_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 6) }
inline void inst_cb_res_6_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 6) }
inline void inst_cb_res_6_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 6) }

inline void inst_cb_res_6_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 6)
	memory_write_byte(gbz80.HL.uw, what);
}

// Bit 7
inline void inst_cb_res_7_a() { INST_CB_BIT_RESET(gbz80.AF.b.h, 7) }
inline void inst_cb_res_7_b() { INST_CB_BIT_RESET(gbz80.BC.b.h, 7) }
inline void inst_cb_res_7_c() { INST_CB_BIT_RESET(gbz80.BC.b.l, 7) }
inline void inst_cb_res_7_d() { INST_CB_BIT_RESET(gbz80.DE.b.h, 7) }
inline void inst_cb_res_7_e() { INST_CB_BIT_RESET(gbz80.DE.b.l, 7) }
inline void inst_cb_res_7_h() { INST_CB_BIT_RESET(gbz80.HL.b.h, 7) }
inline void inst_cb_res_7_l() { INST_CB_BIT_RESET(gbz80.HL.b.l, 7) }

inline void inst_cb_res_7_hl_indirect() {
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	INST_CB_BIT_RESET(what, 7)
	memory_write_byte(gbz80.HL.uw, what);
}

//

inline void inst_cb_rl_a()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;

	if (gbz80.AF.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.AF.b.h <<= 1;
	gbz80.AF.b.h += carry;

	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_b()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;

	if (gbz80.BC.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.h <<= 1;
	gbz80.BC.b.h += carry;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_c()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;

	if (gbz80.BC.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.l <<= 1;
	gbz80.BC.b.l += carry;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_d()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;

	if (gbz80.DE.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);

	gbz80.DE.b.h <<= 1;
	gbz80.DE.b.h += carry;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_e()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;

	if (gbz80.DE.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);

	gbz80.DE.b.l <<= 1;
	gbz80.DE.b.l += carry;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_h()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;
	
	if (gbz80.HL.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.h <<= 1;
	gbz80.HL.b.h += carry;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rl_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;
	
	if (value & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	value <<= 1;
	value += carry;
	
	if (value)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	memory_write_byte(gbz80.HL.uw, value);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(16);
}

inline void inst_cb_rl_l()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;
	
	if (gbz80.HL.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.l <<= 1;
	gbz80.HL.b.l += carry;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_a()
{
	int carry = (gbz80.AF.b.h & 0x80) >> 7;
	
	if (gbz80.AF.b.h & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.AF.b.h <<= 1;
	gbz80.AF.b.h += carry;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8); 
}

inline void inst_cb_rlc_b()
{
	int carry = (gbz80.BC.b.h & 0x80) >> 7;
	
	if (gbz80.BC.b.h & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.h <<= 1;
	gbz80.BC.b.h += carry;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_c()
{
	int carry = (gbz80.BC.b.l & 0x80) >> 7;
	
	if (gbz80.BC.b.l & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.l <<= 1;
	gbz80.BC.b.l += carry;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_d()
{
	int carry = (gbz80.DE.b.h & 0x80) >> 7;
	
	if (gbz80.DE.b.h & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.h <<= 1;
	gbz80.DE.b.h += carry;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_e()
{
	int carry = (gbz80.DE.b.l & 0x80) >> 7;
	
	if (gbz80.DE.b.l & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.l <<= 1;
	gbz80.DE.b.l += carry;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_h()
{
	int carry = (gbz80.HL.b.h & 0x80) >> 7;
	
	if (gbz80.HL.b.h & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.h <<= 1;
	gbz80.HL.b.h += carry;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rlc_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	int carry = (value & 0x80) >> 7;
	
	if (value & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	value <<= 1;
	value += carry;
	
	if (value)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	memory_write_byte(gbz80.HL.uw, value);
	
	MORE_CYCLES(16);
}

inline void inst_cb_rlc_l()
{
	int carry = (gbz80.HL.b.l & 0x80) >> 7;
	
	if (gbz80.HL.b.l & 0x80) 
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.l <<= 1;
	gbz80.HL.b.l += carry;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_rr_a()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	if (gbz80.AF.b.h & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.AF.b.h >>= 1;	
	gbz80.AF.b.h |= carry;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_b()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	if (gbz80.BC.b.h & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.BC.b.h >>= 1;
	gbz80.BC.b.h |= carry;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_c()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;

	if (gbz80.BC.b.l & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.BC.b.l >>= 1;
	gbz80.BC.b.l |= carry;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_d()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	
	if (gbz80.DE.b.h & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.DE.b.h >>= 1;
	gbz80.DE.b.h |= carry;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_e()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	if (gbz80.DE.b.l & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.DE.b.l >>= 1;
	gbz80.DE.b.l |= carry;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_h()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	if (gbz80.HL.b.h & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.HL.b.h >>= 1;
	gbz80.HL.b.h |= carry;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rr_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	if (value & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	value >>= 1;	
	value |= carry;
		
	if (value) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);	
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	memory_write_byte(gbz80.HL.uw, value);
	
	MORE_CYCLES(16);
}

inline void inst_cb_rr_l()
{
	int carry = FLAGS_ISCARRY(gameboy_proc) ? 0x80 : 0;
	
	
	if (gbz80.HL.b.l & 1)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);	
	
	gbz80.HL.b.l >>= 1;
	gbz80.HL.b.l |= carry;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
			
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_a()
{
	int carry = gbz80.AF.b.h & 0x01;
	
	gbz80.AF.b.h >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.AF.b.h |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_b()
{
	int carry = gbz80.BC.b.h & 0x01;
	
	gbz80.BC.b.h >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.BC.b.h |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_c()
{
	int carry = gbz80.BC.b.l & 0x01;
	
	gbz80.BC.b.l >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.BC.b.l |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_d()
{
	int carry = gbz80.DE.b.h & 0x01;
	
	gbz80.DE.b.h >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.DE.b.h |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_e()
{
	int carry = gbz80.DE.b.l & 0x01;
	
	gbz80.DE.b.l >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.DE.b.l |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_h()
{
	int carry = gbz80.HL.b.h & 0x01;
	
	gbz80.HL.b.h >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.HL.b.h |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_rrc_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	int carry = value & 0x01;
	
	value >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		value |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (value)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	memory_write_byte(gbz80.HL.uw, value);
	
	MORE_CYCLES(16);
}

inline void inst_cb_rrc_l()
{
	int carry = gbz80.HL.b.l & 0x01;
	
	gbz80.HL.b.l >>= 1;
	
	if (carry) {
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		gbz80.HL.b.l |= 0x80;
	} else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_a()
{
	gbz80.AF.b.h |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_b()
{
	gbz80.BC.b.h |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_c()
{
	gbz80.BC.b.l |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_d()
{
	gbz80.DE.b.h |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_e()
{
	gbz80.DE.b.l |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_h()
{
	gbz80.HL.b.h |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_0_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x01);
	MORE_CYCLES(12);
}

inline void inst_cb_set_0_l()
{
	gbz80.HL.b.l |= 0x01;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_a()
{
	gbz80.AF.b.h |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_b()
{
	gbz80.BC.b.h |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_c()
{
	gbz80.BC.b.l |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_d()
{
	gbz80.DE.b.h |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_e()
{
	gbz80.DE.b.l |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_h()
{
	gbz80.HL.b.h |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_1_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x02);
	MORE_CYCLES(12);
}

inline void inst_cb_set_1_l()
{
	gbz80.HL.b.l |= 0x02;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_a()
{
	gbz80.AF.b.h |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_b()
{
	gbz80.BC.b.h |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_c()
{
	gbz80.BC.b.l |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_d()
{
	gbz80.DE.b.h |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_e()
{
	gbz80.DE.b.l |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_h()
{
	gbz80.HL.b.h |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_2_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x04);
	MORE_CYCLES(12);
}

inline void inst_cb_set_2_l()
{
	gbz80.HL.b.l |= 0x04;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_a()
{
	gbz80.AF.b.h |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_b()
{
	gbz80.BC.b.h |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_c()
{
	gbz80.BC.b.l |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_d()
{
	gbz80.DE.b.h |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_e()
{
	gbz80.DE.b.l |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_h()
{
	gbz80.HL.b.h |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_3_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x08);
	MORE_CYCLES(12);
}

inline void inst_cb_set_3_l()
{
	gbz80.HL.b.l |= 0x08;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_a()
{
	gbz80.AF.b.h |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_b()
{
	gbz80.BC.b.h |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_c()
{
	gbz80.BC.b.l |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_d()
{
	gbz80.DE.b.h |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_e()
{
	gbz80.DE.b.l |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_h()
{
	gbz80.HL.b.h |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_4_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x10);
	MORE_CYCLES(12);
}

inline void inst_cb_set_4_l()
{
	gbz80.HL.b.l |= 0x10;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_a()
{
	gbz80.AF.b.h |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_b()
{
	gbz80.BC.b.h |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_c()
{
	gbz80.BC.b.l |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_d()
{
	gbz80.DE.b.h |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_e()
{
	gbz80.DE.b.l |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_h()
{
	gbz80.HL.b.h |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_5_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x20);
	MORE_CYCLES(12);
}

inline void inst_cb_set_5_l()
{
	gbz80.HL.b.l |= 0x20;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_a()
{
	gbz80.AF.b.h |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_b()
{
	gbz80.BC.b.h |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_c()
{
	gbz80.BC.b.l |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_d()
{
	gbz80.DE.b.h |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_e()
{
	gbz80.DE.b.l |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_h()
{
	gbz80.HL.b.h |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_6_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x40);
	MORE_CYCLES(12);
}

inline void inst_cb_set_6_l()
{
	gbz80.HL.b.l |= 0x40;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_a()
{
	gbz80.AF.b.h |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_b()
{
	gbz80.BC.b.h |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_c()
{
	gbz80.BC.b.l |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_d()
{
	gbz80.DE.b.h |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_e()
{
	gbz80.DE.b.l |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_h()
{
	gbz80.HL.b.h |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_set_7_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, what | 0x80);
	MORE_CYCLES(12);
}

inline void inst_cb_set_7_l()
{
	gbz80.HL.b.l |= 0x80;
	MORE_CYCLES(8);
}

inline void inst_cb_sla_a()
{
	if (gbz80.AF.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.AF.b.h <<= 1;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_b()
{
	if (gbz80.BC.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.h <<= 1;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_c()
{
	if (gbz80.BC.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.l <<= 1;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_d()
{
	if (gbz80.DE.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.h <<= 1;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_e()
{
	if (gbz80.DE.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.l <<= 1;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_h()
{
	if (gbz80.HL.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.h <<= 1;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sla_hl_indirect()
{
	unsigned char data = memory_read_byte(gbz80.HL.uw);
	
	if (data & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	data <<= 1;

	memory_write_byte(gbz80.HL.uw, data);	/* fix */
		
	if (data)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(16);
}

inline void inst_cb_sla_l()
{
	if (gbz80.HL.b.l & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.l <<= 1;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_a()
{
	if (gbz80.AF.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.AF.b.h = (gbz80.AF.b.h & 0x80) | (gbz80.AF.b.h >> 1);
	
	if (gbz80.AF.b.h) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_b()
{
	if (gbz80.BC.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.h = (gbz80.BC.b.h & 0x80) | (gbz80.BC.b.h >> 1);
	
	if (gbz80.BC.b.h) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_c()
{
	if (gbz80.BC.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.l = (gbz80.BC.b.l & 0x80) | (gbz80.BC.b.l >> 1);
	
	if (gbz80.BC.b.l) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_d()
{
	if (gbz80.DE.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.h = (gbz80.DE.b.h & 0x80) | (gbz80.DE.b.h >> 1);
	
	if (gbz80.DE.b.h) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_e()
{
	if (gbz80.DE.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.l = (gbz80.DE.b.l & 0x80) | (gbz80.DE.b.l >> 1);
	
	if (gbz80.DE.b.l) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_h()
{
	if (gbz80.HL.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.h = (gbz80.HL.b.h & 0x80) | (gbz80.HL.b.h >> 1);
	
	if (gbz80.HL.b.h) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_sra_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	
	if (value & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	value = (value & 0x80) | (value >> 1);
	
	if (value) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	memory_write_byte(gbz80.HL.uw, value);
	
	MORE_CYCLES(16);
}

inline void inst_cb_sra_l()
{
	if (gbz80.HL.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.l = (gbz80.HL.b.l & 0x80) | (gbz80.HL.b.l >> 1);
	
	if (gbz80.HL.b.l) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_a()
{
	if (gbz80.AF.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.AF.b.h >>= 1;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_b()
{
	if (gbz80.BC.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.h >>= 1;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_c()
{
	if (gbz80.BC.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.BC.b.l >>= 1;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_d()
{
	if (gbz80.DE.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.h >>= 1;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_e()
{
	if (gbz80.DE.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.DE.b.l >>= 1;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_h()
{
	if (gbz80.HL.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.h >>= 1;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_srl_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	
	if (value & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	value >>= 1;
	
	if (value)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	memory_write_byte(gbz80.HL.uw, value);
	
	MORE_CYCLES(16);
}

inline void inst_cb_srl_l()
{
	if (gbz80.HL.b.l & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	gbz80.HL.b.l >>= 1;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_a()
{
	unsigned char what = gbz80.AF.b.h;
	gbz80.AF.b.h = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_b()
{
	unsigned char what = gbz80.BC.b.h;
	gbz80.BC.b.h = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_c()
{
	unsigned char what = gbz80.BC.b.l;
	gbz80.BC.b.l = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_d()
{
	unsigned char what = gbz80.DE.b.h;
	gbz80.DE.b.h = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_e()
{
	unsigned char what = gbz80.DE.b.l;
	gbz80.DE.b.l = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_h()
{
	unsigned char what = gbz80.HL.b.h;
	gbz80.HL.b.h = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_cb_swap_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	memory_write_byte(gbz80.HL.uw, ((what & 0xF) << 4) | ((what & 0xF0) >> 4));
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(16);
}

inline void inst_cb_swap_l()
{
	unsigned char what = gbz80.HL.b.l;
	gbz80.HL.b.l = ((what & 0xF) << 4) | ((what & 0xF0) >> 4);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY | FLAGS_CARRY);
	
	MORE_CYCLES(8);
}

inline void inst_ccf()
{
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	if (FLAGS_ISCARRY(gameboy_proc))
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	MORE_CYCLES(4);
}

inline void inst_cp_a()
{
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE | FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_cp_b()
{
	unsigned char value = gbz80.BC.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_c()
{
	unsigned char value = gbz80.BC.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_d()
{
	unsigned char value = gbz80.DE.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_e()
{
	unsigned char value = gbz80.DE.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_h()
{
	unsigned char value = gbz80.HL.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_hl_indirect()
{
	unsigned char value = memory_read_byte(gbz80.HL.uw);
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(8);
}

inline void inst_cp_l()
{
	unsigned char value = gbz80.HL.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(4);
}

inline void inst_cp_n()
{
	unsigned char value = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if (gbz80.AF.b.h == value)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if (value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	MORE_CYCLES(8);
}

inline void inst_cpl()
{
	gbz80.AF.b.h = ~gbz80.AF.b.h;
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

#include "daa.cpp"

inline void inst_daa()
{
	int x = daa_table[((((gbz80.AF.b.l & 0xF0) << 4) | gbz80.AF.b.h) << 1) + 0];
	int y = daa_table[((((gbz80.AF.b.l & 0xF0) << 4) | gbz80.AF.b.h) << 1) + 1];
	gbz80.AF.b.h = x;
	gbz80.AF.b.l = y;
	MORE_CYCLES(4);	
}

inline void inst_dec_a()
{
	if (gbz80.AF.b.h & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.AF.b.h--;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_b()
{
	if (gbz80.BC.b.h & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.BC.b.h--;
	
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_bc()
{
	gbz80.BC.uw--;
	MORE_CYCLES(8);
}

inline void inst_dec_c()
{
	if (gbz80.BC.b.l & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.BC.b.l--;
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_d()
{
	if (gbz80.DE.b.h & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.DE.b.h--;
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_de()
{
	gbz80.DE.uw--;
	MORE_CYCLES(8);
}

inline void inst_dec_e()
{
	if (gbz80.DE.b.l & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.DE.b.l--;
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_h()
{
	if (gbz80.HL.b.h & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.HL.b.h--;
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_hl()
{
	gbz80.HL.uw--;
	MORE_CYCLES(8);
}

inline void inst_dec_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	if (what & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	what--;
	memory_write_byte(gbz80.HL.uw, what);
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	MORE_CYCLES(12);
}

inline void inst_dec_l()
{
	if (gbz80.HL.b.l & 0x0F) 
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	gbz80.HL.b.l--;
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_dec_sp()
{
	gbz80.SP.uw--;
	MORE_CYCLES(8);
}

inline void inst_di()
{
	gbz80.IFF = 0;
	MORE_CYCLES(4);
}

inline void inst_ei()
{
	gbz80.IFF = 1;
	MORE_CYCLES(4);
}

inline void inst_halt()
{
	MORE_CYCLES(4);
	
	if (gbz80.IFF) {
		HaltActive = 1;
	}
}

inline void inst_inc_a()
{
/*	
	if ((gbz80.AF.b.h & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/		
	gbz80.AF.b.h++;
	
	if (gbz80.AF.b.h & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
		
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_b()
{
/*
	if ((gbz80.BC.b.h & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/		
	gbz80.BC.b.h++;
	
	if (gbz80.BC.b.h & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
		
	if (gbz80.BC.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_bc()
{
	gbz80.BC.uw++;
	MORE_CYCLES(8);
}

inline void inst_inc_c()
{
/*
	if ((gbz80.BC.b.l & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/
		
	gbz80.BC.b.l++;
	
	if (gbz80.BC.b.l & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (gbz80.BC.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_d()
{
/*
	if ((gbz80.DE.b.h & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/
		
	gbz80.DE.b.h++;
	
	if (gbz80.DE.b.h & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (gbz80.DE.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_de()
{
	gbz80.DE.uw++;
	MORE_CYCLES(8);
}

inline void inst_inc_e()
{
/*
	if ((gbz80.DE.b.l & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/
		
	gbz80.DE.b.l++;
	
	if (gbz80.DE.b.l & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (gbz80.DE.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_h()
{
/*
	if ((gbz80.HL.b.h & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/		
	gbz80.HL.b.h++;
	
	if (gbz80.HL.b.h & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (gbz80.HL.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_hl()
{
	gbz80.HL.uw++;
	MORE_CYCLES(8);
}

inline void inst_inc_hl_indirect()
{
	unsigned char what = memory_read_byte(gbz80.HL.uw);
	
/*
	if ((what & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/
		
	what++;
	memory_write_byte(gbz80.HL.uw, what);
	
	if (what & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (what)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(12);
}

inline void inst_inc_l()
{
/*
	if ((gbz80.HL.b.l & 0x0F) == 0x0F)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
*/
		
	gbz80.HL.b.l++;
	
	if (gbz80.HL.b.l & 0x0F)
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	
	if (gbz80.HL.b.l)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE);
	
	MORE_CYCLES(4);
}

inline void inst_inc_sp()
{
	gbz80.SP.uw++;
	MORE_CYCLES(8);
}

inline void inst_jp_c_nn()
{
	if(FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(16);
	} else {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	}
}

inline void inst_jp_hl()
{
	gbz80.PC.uw = gbz80.HL.uw;
	MORE_CYCLES(4);
}

inline void inst_jp_nc_nn()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	} else {
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(16);
	}
}

inline void inst_jp_nn() /* C3 */
{
	gbz80.PC.uw = memory_read_pc_word();
	MORE_CYCLES(16);
}

inline void inst_jp_nz_nn()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	} else {
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(16);
	}
}

inline void inst_jp_z_nn()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		gbz80.PC.uw = memory_read_pc_word();
		MORE_CYCLES(16);
	} else {
		gbz80.PC.uw += 2;
		MORE_CYCLES(12);
	}
}

inline void inst_jr_c_n()
{
	signed char offset = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.PC.uw += offset;
		MORE_CYCLES(12);
	} else {
		MORE_CYCLES(8);
	}
}

inline void inst_jr_n()
{
	signed char offset = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	gbz80.PC.uw += offset;
	MORE_CYCLES(12);
}

inline void inst_jr_nc_n()
{
	signed char offset = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	if(FLAGS_ISCARRY(gameboy_proc)) {
		MORE_CYCLES(8);
	} else {
		gbz80.PC.uw += offset;
		MORE_CYCLES(12);
	}
}

inline void inst_jr_nz_n()
{
	signed char offset = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	if (FLAGS_ISZERO(gameboy_proc)) {
		MORE_CYCLES(8);
	} else {
		gbz80.PC.uw += offset;
		MORE_CYCLES(12);
	}
}

inline void inst_jr_z_n()
{
	signed char offset = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	if (FLAGS_ISZERO(gameboy_proc)) {
		gbz80.PC.uw += offset;
		MORE_CYCLES(12);
	} else {
		MORE_CYCLES(8);
	}
}

#define INST_LD(x, y) \
{ \
	x = y; \
}

inline void inst_ld_a_bc_indirect()
{
	gbz80.AF.b.h = memory_read_byte(gbz80.BC.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_a_c_indirect()
{
	gbz80.AF.b.h = memory_read_hibyte(gbz80.BC.b.l);
	MORE_CYCLES(8);
}

inline void inst_ld_a_de_indirect()
{
	gbz80.AF.b.h = memory_read_byte(gbz80.DE.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_a_ff_n_indirect()
{
	gbz80.AF.b.h = memory_read_hibyte(memory_read_pc_byte());
	gbz80.PC.uw++;
	MORE_CYCLES(12);
}

inline void inst_ld_a_hld_indirect()
{
	gbz80.AF.b.h = memory_read_byte(gbz80.HL.uw);
	gbz80.HL.uw--;
	MORE_CYCLES(8);
}

inline void inst_ld_a_hli_indirect()
{
	gbz80.AF.b.h = memory_read_byte(gbz80.HL.uw);
	gbz80.HL.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_a_n()
{
	gbz80.AF.b.h = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_a_nn_indirect()
{
	gbz80.AF.b.h = memory_read_byte(memory_read_pc_word());
	gbz80.PC.uw += 2;
	MORE_CYCLES(16);
}

inline void inst_ld_b_a()
{
	gbz80.BC.b.h = gbz80.AF.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_b_c()
{
	gbz80.BC.b.h = gbz80.BC.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_b_d()
{
	gbz80.BC.b.h = gbz80.DE.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_b_e()
{
	gbz80.BC.b.h = gbz80.DE.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_b_h()
{
	gbz80.BC.b.h = gbz80.HL.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_b_hl_indirect()
{
	gbz80.BC.b.h = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_b_l()
{
	gbz80.BC.b.h = gbz80.HL.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_b_n()
{
	gbz80.BC.b.h = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_bc_indirect_a()
{
	memory_write_byte(gbz80.BC.uw, gbz80.AF.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_bc_nn()
{
	gbz80.BC.uw = memory_read_pc_word();
	gbz80.PC.uw += 2;
	
	MORE_CYCLES(12);
}

inline void inst_ld_c_a()
{
	gbz80.BC.b.l = gbz80.AF.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_c_b()
{
	gbz80.BC.b.l = gbz80.BC.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_c_d()
{
	gbz80.BC.b.l = gbz80.DE.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_c_e()
{
	gbz80.BC.b.l = gbz80.DE.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_c_h()
{
	gbz80.BC.b.l = gbz80.HL.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_c_hl_indirect()
{
	gbz80.BC.b.l = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_c_indirect_a()
{
	memory_write_hibyte(gbz80.BC.b.l, gbz80.AF.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_c_l()
{
	gbz80.BC.b.l = gbz80.HL.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_c_n()
{
	gbz80.BC.b.l = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_d_a()
{
	gbz80.DE.b.h = gbz80.AF.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_d_b()
{
	gbz80.DE.b.h = gbz80.BC.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_d_c()
{
	gbz80.DE.b.h = gbz80.BC.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_d_e()
{
	gbz80.DE.b.h = gbz80.DE.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_d_h()
{
	gbz80.DE.b.h = gbz80.HL.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_d_hl_indirect()
{
	gbz80.DE.b.h = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_d_l()
{
	gbz80.DE.b.h = gbz80.HL.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_d_n()
{
	gbz80.DE.b.h = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_de_indirect_a()
{
	memory_write_byte(gbz80.DE.uw, gbz80.AF.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_de_nn()
{
	gbz80.DE.uw = memory_read_pc_word();
	gbz80.PC.uw += 2;
	
	MORE_CYCLES(12);
}

inline void inst_ld_e_a()
{
	gbz80.DE.b.l = gbz80.AF.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_e_b()
{
	gbz80.DE.b.l = gbz80.BC.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_e_c()
{
	gbz80.DE.b.l = gbz80.BC.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_e_d()
{
	gbz80.DE.b.l = gbz80.DE.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_e_h()
{
	gbz80.DE.b.l = gbz80.HL.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_e_hl_indirect()
{
	gbz80.DE.b.l = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_e_l()
{
	gbz80.DE.b.l = gbz80.HL.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_e_n()
{
	gbz80.DE.b.l = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_ff_n_indirect_a()
{
	memory_write_hibyte(memory_read_pc_byte(), gbz80.AF.b.h);
	gbz80.PC.uw++;
	MORE_CYCLES(12);
}

inline void inst_ld_h_a()
{
	gbz80.HL.b.h = gbz80.AF.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_h_b()
{
	gbz80.HL.b.h = gbz80.BC.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_h_c()
{
	gbz80.HL.b.h = gbz80.BC.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_h_d()
{
	gbz80.HL.b.h = gbz80.DE.b.h;
	MORE_CYCLES(4);
}

inline void inst_ld_h_e()
{
	gbz80.HL.b.h = gbz80.DE.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_h_hl_indirect()
{
	gbz80.HL.b.h = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_h_l()
{
	gbz80.HL.b.h = gbz80.HL.b.l;
	MORE_CYCLES(4);
}

inline void inst_ld_h_n()
{
	gbz80.HL.b.h = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_a()
{
	memory_write_byte(gbz80.HL.uw, gbz80.AF.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_b()
{
	memory_write_byte(gbz80.HL.uw, gbz80.BC.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_c()
{
	memory_write_byte(gbz80.HL.uw, gbz80.BC.b.l);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_d()
{
	memory_write_byte(gbz80.HL.uw, gbz80.DE.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_e()
{
	memory_write_byte(gbz80.HL.uw, gbz80.DE.b.l);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_h()
{
	memory_write_byte(gbz80.HL.uw, gbz80.HL.b.h);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_l()
{
	memory_write_byte(gbz80.HL.uw, gbz80.HL.b.l);
	MORE_CYCLES(8);
}

inline void inst_ld_hl_indirect_n()
{
	memory_write_byte(gbz80.HL.uw, memory_read_pc_byte());
	gbz80.PC.uw++;
	MORE_CYCLES(12);
}

inline void inst_ld_hl_nn()
{
	gbz80.HL.uw = memory_read_pc_word();
	gbz80.PC.uw += 2;
	
	MORE_CYCLES(12);
}

inline void inst_ld_hl_sp_n()
{
	signed char offset = memory_read_pc_byte();
	int result = gbz80.SP.uw + offset;
	gbz80.PC.uw++;
	
	if(result & 0xFFFF0000)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	
	/* TODO, I think so.. */
	if (((gbz80.SP.uw & 0xFFF) + (offset & 0xFFF)) > 0xFFF)
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO | FLAGS_NEGATIVE);
	
	gbz80.HL.uw  = result & 0xFFFF;
	
	MORE_CYCLES(12);
}

inline void inst_ld_hld_indirect_a()
{
	memory_write_byte(gbz80.HL.uw, gbz80.AF.b.h);
	gbz80.HL.uw--;
	MORE_CYCLES(8);
}

inline void inst_ld_hli_indirect_a()
{
	memory_write_byte(gbz80.HL.uw, gbz80.AF.b.h);
	gbz80.HL.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_l_a()
{
	gbz80.HL.b.l = gbz80.AF.b.h;
	MORE_CYCLES(8);
}

inline void inst_ld_l_b()
{
	gbz80.HL.b.l = gbz80.BC.b.h;
	MORE_CYCLES(8);
}

inline void inst_ld_l_c()
{
	gbz80.HL.b.l = gbz80.BC.b.l;
	MORE_CYCLES(8);
}

inline void inst_ld_l_d()
{
	gbz80.HL.b.l = gbz80.DE.b.h;
	MORE_CYCLES(8);
}

inline void inst_ld_l_e()
{
	gbz80.HL.b.l = gbz80.DE.b.l;
	MORE_CYCLES(8);
}

inline void inst_ld_l_h()
{
	gbz80.HL.b.l = gbz80.HL.b.h;
	MORE_CYCLES(8);
}

inline void inst_ld_l_hl_indirect()
{
	gbz80.HL.b.l = memory_read_byte(gbz80.HL.uw);
	MORE_CYCLES(8);
}

inline void inst_ld_l_n()
{
	gbz80.HL.b.l = memory_read_pc_byte();
	gbz80.PC.uw++;
	MORE_CYCLES(8);
}

inline void inst_ld_nn_indirect_a()
{
	memory_write_byte(memory_read_pc_word(), gbz80.AF.b.h);
	gbz80.PC.uw += 2;
	MORE_CYCLES(16);
}

inline void inst_ld_nn_indirect_sp()
{
	memory_write_word(memory_read_pc_word(), gbz80.SP.uw);
	gbz80.PC.uw += 2;
	MORE_CYCLES(20);
}

inline void inst_ld_sp_hl()
{
	gbz80.SP.uw = gbz80.HL.uw;
	MORE_CYCLES(8);
}

inline void inst_ld_sp_nn()
{
	gbz80.SP.uw = memory_read_pc_word();
	gbz80.PC.uw += 2;
	MORE_CYCLES(12);
}

inline void inst_or_a()
{
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_b()
{
	gbz80.AF.b.h |= gbz80.BC.b.h;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_c()
{
	gbz80.AF.b.h |= gbz80.BC.b.l;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_d()
{
	gbz80.AF.b.h |= gbz80.DE.b.h;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_e()
{
	gbz80.AF.b.h |= gbz80.DE.b.l;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_h()
{
	gbz80.AF.b.h |= gbz80.HL.b.h;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_hl_indirect()
{
	gbz80.AF.b.h |= memory_read_byte(gbz80.HL.uw);
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_or_l()
{
	gbz80.AF.b.h |= gbz80.HL.b.l;
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_or_n()
{
	gbz80.AF.b.h |= memory_read_pc_byte();
	gbz80.PC.uw++;
	
	if (gbz80.AF.b.h)
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(8);
}

inline void inst_pop_af()
{
	gbz80.AF.uw = gameboy_stack_read_word();
	MORE_CYCLES(12);
}

inline void inst_pop_bc()
{
	gbz80.BC.uw = gameboy_stack_read_word();
	MORE_CYCLES(12);
}

inline void inst_pop_de()
{
	gbz80.DE.uw = gameboy_stack_read_word();
	MORE_CYCLES(12);
}

inline void inst_pop_hl()
{
	gbz80.HL.uw = gameboy_stack_read_word();
	MORE_CYCLES(12);
}

inline void inst_push_af()
{
	gameboy_stack_write_word(gbz80.AF.uw);
	MORE_CYCLES(16);
}

inline void inst_push_bc()
{
	gameboy_stack_write_word(gbz80.BC.uw);
	MORE_CYCLES(16);
}

inline void inst_push_de()
{
	gameboy_stack_write_word(gbz80.DE.uw);
	MORE_CYCLES(16);
}

inline void inst_push_hl()
{
	gameboy_stack_write_word(gbz80.HL.uw);
	MORE_CYCLES(16);
}

inline void inst_ret()
{
	gbz80.PC.uw = gameboy_stack_read_word();
	MORE_CYCLES(16);
}

inline void inst_ret_c()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.PC.uw = gameboy_stack_read_word();
		MORE_CYCLES(20);
	} else {
		MORE_CYCLES(8);
	}
}

inline void inst_ret_nc()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		MORE_CYCLES(8);
	} else {
		gbz80.PC.uw = gameboy_stack_read_word();
		MORE_CYCLES(20);
	}
}

inline void inst_ret_nz()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		MORE_CYCLES(8);
	} else {
		gbz80.PC.uw = gameboy_stack_read_word();
		MORE_CYCLES(20);
	}
}

inline void inst_ret_z()
{
	if (FLAGS_ISZERO(gameboy_proc)) {
		gbz80.PC.uw = gameboy_stack_read_word();
		MORE_CYCLES(20);
	} else {
		MORE_CYCLES(8);
	}
}

inline void inst_reti()
{
	gbz80.PC.uw = gameboy_stack_read_word();
	gbz80.IFF = 1;
	MORE_CYCLES(16);
}

inline void inst_rla()
{
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;
	if (gbz80.AF.b.h & 0x80)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	gbz80.AF.b.h <<= 1;
	gbz80.AF.b.h += carry;
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_ZERO | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_rlca()
{
	int carry = (gbz80.AF.b.h & 0x80) >> 7;
	if (carry)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	gbz80.AF.b.h <<= 1;
	gbz80.AF.b.h += carry;
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_ZERO | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_rra()
{
	int carry = (FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0) << 7;
	if (gbz80.AF.b.h & 0x01)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	gbz80.AF.b.h >>= 1;
	gbz80.AF.b.h += carry;
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_ZERO | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_rrca()
{
	int carry = gbz80.AF.b.h & 0x01;
	if (carry)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	gbz80.AF.b.h >>= 1;
	if (carry)
		gbz80.AF.b.h |= 0x80;
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_ZERO | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

inline void inst_rst_00h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x00;
	MORE_CYCLES(16);
}

inline void inst_rst_08h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x08;
	MORE_CYCLES(16);
}

inline void inst_rst_10h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x10;
	MORE_CYCLES(16);
}

inline void inst_rst_18h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x18;
	MORE_CYCLES(16);
}

inline void inst_rst_20h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x20;
	MORE_CYCLES(16);
}

inline void inst_rst_28h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x28;
	MORE_CYCLES(16);
}

inline void inst_rst_30h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x30;
	MORE_CYCLES(16);
}

inline void inst_rst_38h()
{
	gameboy_stack_write_word(gbz80.PC.uw);
	gbz80.PC.uw = 0x38;
	MORE_CYCLES(16);
}

inline void inst_sbc_a()
{
	if (FLAGS_ISCARRY(gameboy_proc)) {
		gbz80.AF.b.h = 0xFF;
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	} else {
		gbz80.AF.b.h = 0;
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	}
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
}

#define INST_SBC(x) { \
	int num = x;					\
	int carry = FLAGS_ISSET(gameboy_proc, FLAGS_CARRY) ? 1 : 0;	\
	int value = num + carry;					\
									\
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);			\
									\
	if(value > gbz80.AF.b.h)				\
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);			\
	else								\
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);			\
									\
	if((value & 0xFF) == gbz80.AF.b.h)			\
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);			\
	else								\
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);			\
									\
	if (((num & 0x0F) + carry) > (gbz80.AF.b.h & 0x0F))	\
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);		\
	else								\
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);		\
									\
	gbz80.AF.b.h -= value;					\
}

inline void inst_scf()
{
	FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	FLAGS_CLEAR(gameboy_proc, FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	
	MORE_CYCLES(4);
}

inline void inst_stop()
{
	if (KEY1REG & 0x01) {
		KEY1REG ^= 0x81;
	} else {
		unsigned long oc = current_joypad;
		while(oc != current_joypad)
			vram_sysupdate();
	}
	
	if (KEY1REG & 0x80)
		CPUSpeed = 2;
	else
		CPUSpeed = 1;
		
	gbz80.PC.uw++; 
	MORE_CYCLES(4);
}

inline void inst_sub_a()
{
	gbz80.AF.b.h = 0;
	FLAGS_SET(gameboy_proc, FLAGS_ZERO | FLAGS_NEGATIVE);
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
	FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
	
	MORE_CYCLES(8);
}

inline void inst_sub_b()
{
	int value = gbz80.BC.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_c()
{
	int value = gbz80.BC.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_d()
{
	int value = gbz80.DE.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_e()
{
	int value = gbz80.DE.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_h()
{
	int value = gbz80.HL.b.h;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_hl_indirect()
{
	int value = memory_read_byte(gbz80.HL.uw);
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(8);
}

inline void inst_sub_l()
{
	int value = gbz80.HL.b.l;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(4);
}

inline void inst_sub_n()
{
	int value = memory_read_pc_byte();
	gbz80.PC.uw++;
	
	FLAGS_SET(gameboy_proc, FLAGS_NEGATIVE);
	
	if(value > gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_CARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY);
		
	if(value == gbz80.AF.b.h)
		FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO);
	
	if ((value & 0x0F) > (gbz80.AF.b.h & 0x0F))
		FLAGS_SET(gameboy_proc, FLAGS_HALFCARRY);
	else
		FLAGS_CLEAR(gameboy_proc, FLAGS_HALFCARRY);
		
	gbz80.AF.b.h -= value;
	
	MORE_CYCLES(8);
}

inline void inst_unknown()
{
	gbz80.running = 0;
	gbz80.PC.uw  -= 1;
	printf("Unknown opcode:\n");
	printf("[%04X]: AF:%04X BC:%04X DE:%04X HL:%04X SP:%04X (%02X/%02X) %02X %02X)\n", 
	gbz80.PC.uw, gbz80.AF.uw, gbz80.BC.uw,
	gbz80.DE.uw, gbz80.HL.uw, gbz80.SP.uw,
	memory_read_pc_byte(), memory_read_byte(gbz80.PC.uw), memory_read_byte(gbz80.PC.uw+1), memory_read_byte(gbz80.PC.uw+2));
}

inline void inst_xor_a() /* AF */
{
	gbz80.AF.b.h = 0;
	FLAGS_SET(gameboy_proc, FLAGS_ZERO);
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY);
	MORE_CYCLES(4);
}

#define INST_XOR(x) \
{ \
	gbz80.AF.b.h ^= x; \
	\
	if (gbz80.AF.b.h) \
		FLAGS_CLEAR(gameboy_proc, FLAGS_ZERO); \
	else \
		FLAGS_SET(gameboy_proc, FLAGS_ZERO); \
	\
	FLAGS_CLEAR(gameboy_proc, FLAGS_CARRY | FLAGS_NEGATIVE | FLAGS_HALFCARRY); \
}

inline void inst_cb()
{
	int cbinst;
	
	cbinst = memory_read_pc_byte();

	gbz80.machine_cycles = g_gbCyclesCBTable[cbinst];
	gbz80.PC.uw++;
	
	switch ( (unsigned char)cbinst ) {
		case 0x00: inst_cb_rlc_b(); break; /* RLC B */ 
		case 0x01: inst_cb_rlc_c(); break;
		case 0x02: inst_cb_rlc_d(); break;
		case 0x03: inst_cb_rlc_e(); break;
		case 0x04: inst_cb_rlc_h(); break;
		case 0x05: inst_cb_rlc_l(); break;
		case 0x06: inst_cb_rlc_hl_indirect(); break;
		case 0x07: inst_cb_rlc_a(); break;
      case 0x08: inst_cb_rrc_b(); break; /* RRC B */ 
		case 0x09: inst_cb_rrc_c(); break;
		case 0x0A: inst_cb_rrc_d(); break;
		case 0x0B: inst_cb_rrc_e(); break;
		case 0x0C: inst_cb_rrc_h(); break;
		case 0x0D: inst_cb_rrc_l(); break;
		case 0x0E: inst_cb_rrc_hl_indirect(); break;
		case 0x0F: inst_cb_rrc_a(); break;
		case 0x10: inst_cb_rl_b(); break; /* RL B */ 
		case 0x11: inst_cb_rl_c(); break;
		case 0x12: inst_cb_rl_d(); break;
		case 0x13: inst_cb_rl_e(); break;
		case 0x14: inst_cb_rl_h(); break;
		case 0x15: inst_cb_rl_l(); break;
		case 0x16: inst_cb_rl_hl_indirect(); break;
		case 0x17: inst_cb_rl_a(); break;
		case 0x18: inst_cb_rr_b(); break; /* RR B */ 
		case 0x19: inst_cb_rr_c(); break;
		case 0x1A: inst_cb_rr_d(); break;
		case 0x1B: inst_cb_rr_e(); break;
		case 0x1C: inst_cb_rr_h(); break;
		case 0x1D: inst_cb_rr_l(); break;
		case 0x1E: inst_cb_rr_hl_indirect(); break;
		case 0x1F: inst_cb_rr_a(); break;
		case 0x20: inst_cb_sla_b(); break; /* SLA B */ 
		case 0x21: inst_cb_sla_c(); break;
		case 0x22: inst_cb_sla_d(); break;
		case 0x23: inst_cb_sla_e(); break;
		case 0x24: inst_cb_sla_h(); break;
		case 0x25: inst_cb_sla_l(); break;
		case 0x26: inst_cb_sla_hl_indirect(); break;
		case 0x27: inst_cb_sla_a(); break;
		case 0x28: inst_cb_sra_b(); break; /* SRA B */ 
		case 0x29: inst_cb_sra_c(); break;
		case 0x2A: inst_cb_sra_d(); break;
		case 0x2B: inst_cb_sra_e(); break;
		case 0x2C: inst_cb_sra_h(); break;
		case 0x2D: inst_cb_sra_l(); break;
		case 0x2E: inst_cb_sra_hl_indirect(); break;
		case 0x2F: inst_cb_sra_a(); break;
      case 0x30: inst_cb_swap_b(); break; /* SWAP B */ 
		case 0x31: inst_cb_swap_c(); break;
		case 0x32: inst_cb_swap_d(); break;
		case 0x33: inst_cb_swap_e(); break;
		case 0x34: inst_cb_swap_h(); break;
		case 0x35: inst_cb_swap_l(); break;
		case 0x36: inst_cb_swap_hl_indirect(); break;
		case 0x37: inst_cb_swap_a(); break;
      case 0x38: inst_cb_srl_b(); break; /* SRL B */ 
		case 0x39: inst_cb_srl_c(); break;
		case 0x3A: inst_cb_srl_d(); break;
		case 0x3B: inst_cb_srl_e(); break;
		case 0x3C: inst_cb_srl_h(); break;
		case 0x3D: inst_cb_srl_l(); break;
		case 0x3E: inst_cb_srl_hl_indirect(); break;
		case 0x3F: inst_cb_srl_a(); break;
		case 0x40: inst_cb_bit_0_b(); break; /* BIT 0, B */ 
		case 0x41: inst_cb_bit_0_c(); break;
		case 0x42: inst_cb_bit_0_d(); break;
		case 0x43: inst_cb_bit_0_e(); break;
		case 0x44: inst_cb_bit_0_h(); break;
		case 0x45: inst_cb_bit_0_l(); break;
		case 0x46: inst_cb_bit_0_hl_indirect(); break;
		case 0x47: inst_cb_bit_0_a(); break;
		case 0x48: inst_cb_bit_1_b(); break; /* BIT 1, B */ 
		case 0x49: inst_cb_bit_1_c();	break;
		case 0x4A: inst_cb_bit_1_d(); break;
		case 0x4B: inst_cb_bit_1_e(); break;
		case 0x4C: inst_cb_bit_1_h(); break;
		case 0x4D: inst_cb_bit_1_l(); break;
		case 0x4E: inst_cb_bit_1_hl_indirect(); break;
		case 0x4F: inst_cb_bit_1_a(); break;
		case 0x50: inst_cb_bit_2_b(); break; /* BIT 2, B */
		case 0x51: inst_cb_bit_2_c(); break;
		case 0x52: inst_cb_bit_2_d(); break;
		case 0x53: inst_cb_bit_2_e(); break;
		case 0x54: inst_cb_bit_2_h(); break;
		case 0x55: inst_cb_bit_2_l(); break;
		case 0x56: inst_cb_bit_2_hl_indirect(); break;
		case 0x57: inst_cb_bit_2_a(); break;
		case 0x58: inst_cb_bit_3_b(); break; /* BIT 3, B */
		case 0x59: inst_cb_bit_3_c(); break;
		case 0x5A: inst_cb_bit_3_d(); break;
		case 0x5B: inst_cb_bit_3_e(); break;
		case 0x5C: inst_cb_bit_3_h(); break;
		case 0x5D: inst_cb_bit_3_l(); break;
		case 0x5E: inst_cb_bit_3_hl_indirect(); break;
		case 0x5F: inst_cb_bit_3_a(); break;
		case 0x60: inst_cb_bit_4_b(); break; /* BIT 4, B */
		case 0x61: inst_cb_bit_4_c(); break;
		case 0x62: inst_cb_bit_4_d(); break;
		case 0x63: inst_cb_bit_4_e(); break;
		case 0x64: inst_cb_bit_4_h(); break;
		case 0x65: inst_cb_bit_4_l(); break;
		case 0x66: inst_cb_bit_4_hl_indirect(); break;
		case 0x67: inst_cb_bit_4_a(); break;
      case 0x68: inst_cb_bit_5_b(); break; /* BIT 5, B */
		case 0x69: inst_cb_bit_5_c(); break;
		case 0x6A: inst_cb_bit_5_d(); break;
		case 0x6B: inst_cb_bit_5_e(); break;
		case 0x6C: inst_cb_bit_5_h(); break;
		case 0x6D: inst_cb_bit_5_l(); break;
		case 0x6E: inst_cb_bit_5_hl_indirect(); break;
		case 0x6F: inst_cb_bit_5_a(); break;			
		case 0x70: inst_cb_bit_6_b(); break; /* BIT 6, B */
		case 0x71: inst_cb_bit_6_c(); break;
		case 0x72: inst_cb_bit_6_d(); break;
		case 0x73: inst_cb_bit_6_e(); break;
		case 0x74: inst_cb_bit_6_h(); break;
		case 0x75: inst_cb_bit_6_l(); break;
		case 0x76: inst_cb_bit_6_hl_indirect(); break;
		case 0x77: inst_cb_bit_6_a(); break;
      case 0x78: inst_cb_bit_7_b(); break; /* BIT 7, B */
		case 0x79: inst_cb_bit_7_c(); break;
		case 0x7A: inst_cb_bit_7_d(); break;
		case 0x7B: inst_cb_bit_7_e(); break;
		case 0x7C: inst_cb_bit_7_h(); break;
		case 0x7D: inst_cb_bit_7_l(); break;
		case 0x7E: inst_cb_bit_7_hl_indirect(); break;
		case 0x7F: inst_cb_bit_7_a(); break;
		case 0x80: inst_cb_res_0_b(); break; /* RES 0, B */
		case 0x81: inst_cb_res_0_c(); break;
		case 0x82: inst_cb_res_0_d(); break;
		case 0x83: inst_cb_res_0_e(); break;
		case 0x84: inst_cb_res_0_h(); break;
		case 0x85: inst_cb_res_0_l(); break;
		case 0x86: inst_cb_res_0_hl_indirect(); break;
		case 0x87: inst_cb_res_0_a(); break;
		case 0x88: inst_cb_res_1_b(); break; /* RES 1, B */
		case 0x89: inst_cb_res_1_c(); break;
		case 0x8A: inst_cb_res_1_d(); break;
		case 0x8B: inst_cb_res_1_e(); break;
		case 0x8C: inst_cb_res_1_h(); break;
		case 0x8D: inst_cb_res_1_l(); break;
		case 0x8E: inst_cb_res_1_hl_indirect(); break;
		case 0x8F: inst_cb_res_1_a(); break;
		case 0x90: inst_cb_res_2_b(); break; /* RES 2, B */
		case 0x91: inst_cb_res_2_c(); break;
		case 0x92: inst_cb_res_2_d(); break;
		case 0x93: inst_cb_res_2_e(); break;
		case 0x94: inst_cb_res_2_h(); break;
		case 0x95: inst_cb_res_2_l(); break;
		case 0x96: inst_cb_res_2_hl_indirect(); break;
		case 0x97: inst_cb_res_2_a(); break; 
		case 0x98: inst_cb_res_3_b(); break; /* RES 3, B */
		case 0x99: inst_cb_res_3_c(); break;
		case 0x9A: inst_cb_res_3_d(); break;
		case 0x9B: inst_cb_res_3_e(); break;
		case 0x9C: inst_cb_res_3_h(); break;
		case 0x9D: inst_cb_res_3_l(); break;
		case 0x9E: inst_cb_res_3_hl_indirect(); break;
		case 0x9F: inst_cb_res_3_a(); break;	
		case 0xA0: inst_cb_res_4_b(); break; /* RES 4, B */ 
		case 0xA1: inst_cb_res_4_c(); break;
		case 0xA2: inst_cb_res_4_d(); break;
		case 0xA3: inst_cb_res_4_e(); break;
		case 0xA4: inst_cb_res_4_h(); break;
		case 0xA5: inst_cb_res_4_l(); break;
		case 0xA6: inst_cb_res_4_hl_indirect(); break;
		case 0xA7: inst_cb_res_4_a(); break; 
		case 0xA8: inst_cb_res_5_b(); break; /* RES 5, B */ 
		case 0xA9: inst_cb_res_5_c(); break;
		case 0xAA: inst_cb_res_5_d(); break;
		case 0xAB: inst_cb_res_5_e(); break;
		case 0xAC: inst_cb_res_5_h(); break;
		case 0xAD: inst_cb_res_5_l(); break;
		case 0xAE: inst_cb_res_5_hl_indirect(); break;
		case 0xAF: inst_cb_res_5_a(); break; 
		case 0xB0: inst_cb_res_6_b(); break; /* RES 6, B */ 
		case 0xB1: inst_cb_res_6_c(); break;
		case 0xB2: inst_cb_res_6_d(); break;
		case 0xB3: inst_cb_res_6_e(); break;
		case 0xB4: inst_cb_res_6_h(); break;
		case 0xB5: inst_cb_res_6_l(); break;
		case 0xB6: inst_cb_res_6_hl_indirect(); break;
		case 0xB7: inst_cb_res_6_a(); break;
		case 0xB8: inst_cb_res_7_b(); break; /* RES 7, B */ 
		case 0xB9: inst_cb_res_7_c(); break;
		case 0xBA: inst_cb_res_7_d(); break;
		case 0xBB: inst_cb_res_7_e(); break;
		case 0xBC: inst_cb_res_7_h();	break;
		case 0xBD: inst_cb_res_7_l(); break;
		case 0xBE: inst_cb_res_7_hl_indirect(); break;
		case 0xBF: inst_cb_res_7_a(); break;
		case 0xC0: inst_cb_set_0_b(); break; /* SET 0, B */ 
		case 0xC1: inst_cb_set_0_c(); break;
		case 0xC2: inst_cb_set_0_d(); break;
		case 0xC3: inst_cb_set_0_e(); break;
		case 0xC4: inst_cb_set_0_h(); break;
		case 0xC5: inst_cb_set_0_l(); break;
		case 0xC6: inst_cb_set_0_hl_indirect(); break;
		case 0xC7: inst_cb_set_0_a(); break;
		case 0xC8: inst_cb_set_1_b(); break; /* SET 1, B */ 
		case 0xC9: inst_cb_set_1_c(); break;
		case 0xCA: inst_cb_set_1_d(); break;
		case 0xCB: inst_cb_set_1_e(); break;
		case 0xCC: inst_cb_set_1_h(); break;
		case 0xCD: inst_cb_set_1_l(); break;
		case 0xCE: inst_cb_set_1_hl_indirect(); break;
		case 0xCF: inst_cb_set_1_a(); break; 
		case 0xD0: inst_cb_set_2_b(); break; /* SET 2, B */ 
		case 0xD1: inst_cb_set_2_c(); break;
		case 0xD2: inst_cb_set_2_d(); break;
		case 0xD3: inst_cb_set_2_e(); break;
		case 0xD4: inst_cb_set_2_h(); break;
		case 0xD5: inst_cb_set_2_l(); break;
		case 0xD6: inst_cb_set_2_hl_indirect(); break;
		case 0xD7: inst_cb_set_2_a(); break;
		case 0xD8: inst_cb_set_3_b(); break; /* SET 3, B */ 
		case 0xD9: inst_cb_set_3_c(); break;
		case 0xDA: inst_cb_set_3_d(); break;
		case 0xDB: inst_cb_set_3_e(); break;
		case 0xDC: inst_cb_set_3_h(); break;
		case 0xDD: inst_cb_set_3_l(); break;
		case 0xDE: inst_cb_set_3_hl_indirect(); break;
		case 0xDF: inst_cb_set_3_a(); break;
		case 0xE0: inst_cb_set_4_b(); break;  /* SET 4, B */ 
		case 0xE1: inst_cb_set_4_c(); break;
		case 0xE2: inst_cb_set_4_d(); break;
		case 0xE3: inst_cb_set_4_e(); break;
		case 0xE4: inst_cb_set_4_h(); break;
		case 0xE5: inst_cb_set_4_l(); break;
		case 0xE6: inst_cb_set_4_hl_indirect(); break;
		case 0xE7: inst_cb_set_4_a(); break; 
		case 0xE8: inst_cb_set_5_b(); break; /* SET 5, B */ 
		case 0xE9: inst_cb_set_5_c(); break;
		case 0xEA: inst_cb_set_5_d(); break;
		case 0xEB: inst_cb_set_5_e(); break;
		case 0xEC: inst_cb_set_5_h(); break;
		case 0xED: inst_cb_set_5_l(); break;
		case 0xEE: inst_cb_set_5_hl_indirect(); break;
		case 0xEF: inst_cb_set_5_a(); break;
		case 0xF0: inst_cb_set_6_b(); break; /* SET 6, B */ 
		case 0xF1: inst_cb_set_6_c(); break;
		case 0xF2: inst_cb_set_6_d(); break;
		case 0xF3: inst_cb_set_6_e(); break;
		case 0xF4: inst_cb_set_6_h(); break;
		case 0xF5: inst_cb_set_6_l(); break;
		case 0xF6: inst_cb_set_6_hl_indirect(); break;
		case 0xF7: inst_cb_set_6_a(); break;
		case 0xF8: inst_cb_set_7_b(); break; /* SET 7, B */ 
		case 0xF9: inst_cb_set_7_c(); break;
		case 0xFA: inst_cb_set_7_d(); break;
		case 0xFB: inst_cb_set_7_e(); break;
		case 0xFC: inst_cb_set_7_h(); break;
		case 0xFD: inst_cb_set_7_l(); break;
		case 0xFE: inst_cb_set_7_hl_indirect(); break;
		case 0xFF: inst_cb_set_7_a(); break;
	}
}

void gameboy_cpu_execute_opcode(unsigned char OpCode)
{
   gbz80.machine_cycles = g_gbCyclesTable[OpCode];
   
   // Jump table
   switch ( OpCode )
   {
      case 0x00: MORE_CYCLES(4); break;               // NOP
      case 0x01: inst_ld_bc_nn(); break;              // LD BC, $XXXX
      case 0x02: inst_ld_bc_indirect_a(); break;      // LD [BC], A
      case 0x03: inst_inc_bc(); break;                // INC BC
      case 0x04: inst_inc_b(); break;                 // INC B
      case 0x05: inst_dec_b(); break;                 // DEC B
      case 0x06: inst_ld_b_n(); break;                // LD B, $XX
      case 0x07: inst_rlca(); break;                  // RLCA
      case 0x08: inst_ld_nn_indirect_sp(); break;     // LD [$XXXX], SP
      case 0x09: inst_add_hl_bc(); break;             // ADD HL, BC
      case 0x0A: inst_ld_a_bc_indirect(); break;      // LD A, [BC]
      case 0x0B: inst_dec_bc(); break;                // DEC BC
      case 0x0C: inst_inc_c(); break;                 // INC C
      case 0x0D: inst_dec_c(); break;                 // DEC C
      case 0x0E: inst_ld_c_n(); break;                // LD C, $XX
		case 0x0F: inst_rrca(); break;                  // RRCA
		case 0x10: inst_stop(); break;                  // STOP
      case 0x11: inst_ld_de_nn(); break;              // LD DE, $XXXX
      case 0x12: inst_ld_de_indirect_a(); break;      // LD [DE], A
      case 0x13: inst_inc_de(); break;                // INC DE
      case 0x14: inst_inc_d(); break;                 // INC D
      case 0x15: inst_dec_d(); break;                 // DEC D
      case 0x16: inst_ld_d_n(); break;                // LD D, $XX
		case 0x17: inst_rla(); break;                   // RLA
      case 0x18: inst_jr_n(); break;                  // JR $XX
      case 0x19: inst_add_hl_de(); break;             // ADD HL, DE
      case 0x1A: inst_ld_a_de_indirect(); break;      // LD A, [DE]
      case 0x1B: inst_dec_de(); break;                // DEC DE
      case 0x1C: inst_inc_e(); break;                 // INC E
      case 0x1D: inst_dec_e(); break;                 // DEC E
      case 0x1E: inst_ld_e_n(); break;                // LD E, $XX
      case 0x1F: inst_rra(); break;                   // RRA
      case 0x20: inst_jr_nz_n(); break;               // JR NZ, $XX
      case 0x21: inst_ld_hl_nn(); break;              // LD HL, $XXXX
      case 0x22: inst_ld_hli_indirect_a(); break;     // LD [HL+], A
      case 0x23: inst_inc_hl(); break;                // INC HL
      case 0x24: inst_inc_h(); break;                 // INC H
      case 0x25: inst_dec_h(); break;                 // DEC H
      case 0x26: inst_ld_h_n(); break;                // LD H, $XX
      case 0x27: inst_daa(); break;                   // DAA
      case 0x28: inst_jr_z_n(); break;                // JR Z, $XX
      case 0x29: inst_add_hl_hl(); break;             // ADD HL, HL
      case 0x2A: inst_ld_a_hli_indirect(); break;     // LD A, [HL+]
      case 0x2B: inst_dec_hl(); break;                // DEC HL
      case 0x2C: inst_inc_l(); break;                 // INC L
      case 0x2D: inst_dec_l(); break;                 // DEC L
      case 0x2E: inst_ld_l_n(); break;                // LD L, $XX
		case 0x2F: inst_cpl(); break;                   // CPL
      case 0x30: inst_jr_nc_n(); break;               // JR NC, $XX
      case 0x31: inst_ld_sp_nn(); break;              // LD SP, $XXXX
      case 0x32: inst_ld_hld_indirect_a(); break;     // LD [HL-], A
      case 0x33: inst_inc_sp(); break;                // INC SP
      case 0x34: inst_inc_hl_indirect(); break;       // INC [HL]
      case 0x35: inst_dec_hl_indirect(); break;       // DEC [HL] 
      case 0x36: inst_ld_hl_indirect_n(); break;      // LD [HL], $XX 
		case 0x37: inst_scf(); break;                   // SCF
      case 0x38: inst_jr_c_n(); break;                // JR C, $XX
      case 0x39: inst_add_hl_sp(); break;             // ADD HL, SP
      case 0x3A: inst_ld_a_hld_indirect(); break;     // LD A, [HL-]
		case 0x3B: inst_dec_sp(); break;                // DEC SP
      case 0x3C: inst_inc_a(); break;                 // INC A
      case 0x3D: inst_dec_a(); break;                 // DEC A 
      case 0x3E: inst_ld_a_n(); break;                // LD A, $XX 
		case 0x3F: inst_ccf(); break;                   // CCF
		case 0x40: MORE_CYCLES(4); break;               // LD B, B
      case 0x41: inst_ld_b_c(); break;                // LD B, C
      case 0x42: inst_ld_b_d(); break;                // LD B, D
      case 0x43: inst_ld_b_e(); break; 
      case 0x44: inst_ld_b_h(); break;
      case 0x45: inst_ld_b_l(); break;
      case 0x46: inst_ld_b_hl_indirect(); break;
      case 0x47: inst_ld_b_a(); break;
      case 0x48: inst_ld_c_b(); break;
      case 0x49: MORE_CYCLES(4); break; /* LD C, C */ 
      case 0x4A: inst_ld_c_d(); break; 
      case 0x4B: inst_ld_c_e(); break;
      case 0x4C: inst_ld_c_h(); break;
      case 0x4D: inst_ld_c_l(); break;
      case 0x4E: inst_ld_c_hl_indirect(); break;
      case 0x4F: inst_ld_c_a(); break;
      case 0x50: inst_ld_d_b(); break; /* LD D, B */ 
      case 0x51: inst_ld_d_c(); break; /* LD D, C */ 
      case 0x52: MORE_CYCLES(4); break; /* LD D, D */ 
      case 0x53: inst_ld_d_e(); break;
      case 0x54: inst_ld_d_h(); break;
      case 0x55: inst_ld_d_l(); break;
      case 0x56: inst_ld_d_hl_indirect(); break;
      case 0x57: inst_ld_d_a(); break;
      case 0x58: inst_ld_e_b(); break; /* LD E, B */ 
      case 0x59: inst_ld_e_c(); break;
      case 0x5A: inst_ld_e_d(); break;		
      case 0x5B: MORE_CYCLES(4); break; /* LD E, E */ 
      case 0x5C: inst_ld_e_h(); break;
      case 0x5D: inst_ld_e_l(); break;
      case 0x5E: inst_ld_e_hl_indirect(); break;
      case 0x5F: inst_ld_e_a(); break;
      case 0x60: inst_ld_h_b(); break; /* LD H, B */ 
      case 0x61: inst_ld_h_c(); break; /* LD H, C */ 
      case 0x62: inst_ld_h_d(); break; /* LD H, D */ 
      case 0x63: inst_ld_h_e(); break; 
      case 0x64: MORE_CYCLES(4); break; /* LD H, H */
      case 0x65: inst_ld_h_l(); break;
      case 0x66: inst_ld_h_hl_indirect(); break;
      case 0x67: inst_ld_h_a(); break;
      case 0x68: inst_ld_l_b(); break; /* LD L, B */ 
      case 0x69: inst_ld_l_c(); break; /* LD L, C */ 
      case 0x6A: inst_ld_l_d(); break;
      case 0x6B: inst_ld_l_e(); break;
      case 0x6C: inst_ld_l_h(); break;		
      case 0x6D: MORE_CYCLES(4); break;  /* LD L, L */ 
      case 0x6E: inst_ld_l_hl_indirect(); break;
      case 0x6F: inst_ld_l_a(); break;
      case 0x70: inst_ld_hl_indirect_b(); break; /* LD [HL], B */ 
      case 0x71: inst_ld_hl_indirect_c(); break; /* LD [HL], C */ 
      case 0x72: inst_ld_hl_indirect_d(); break; /* LD [HL], D */ 
      case 0x73: inst_ld_hl_indirect_e(); break; /* LD [HL], E */ 
      case 0x74: inst_ld_hl_indirect_h(); break; /* LD [HL], H */ 
      case 0x75: inst_ld_hl_indirect_l(); break; /* LD [HL], L */ 
      case 0x76: inst_halt(); break; /* HALT */ 
      case 0x77: inst_ld_hl_indirect_a(); break; /* LD [HL], A */ 
		case 0x78: INST_LD(gbz80.AF.b.h, gbz80.BC.b.h); MORE_CYCLES(4); break; /* LD A, B */ 
      case 0x79: INST_LD(gbz80.AF.b.h, gbz80.BC.b.l); MORE_CYCLES(4); break; /* LD A, C */ 
      case 0x7A: INST_LD(gbz80.AF.b.h, gbz80.DE.b.h); MORE_CYCLES(4); break; /* LD A, D */ 
      case 0x7B: INST_LD(gbz80.AF.b.h, gbz80.DE.b.l); MORE_CYCLES(4); break; /* LD A, E */ 
      case 0x7C: INST_LD(gbz80.AF.b.h, gbz80.HL.b.h); MORE_CYCLES(4); break; /* LD A, H */ 
      case 0x7D: INST_LD(gbz80.AF.b.h, gbz80.HL.b.l); MORE_CYCLES(4); break; /* LD A, L */ 
      case 0x7E: INST_LD(gbz80.AF.b.h, memory_read_byte(gbz80.HL.uw)); MORE_CYCLES(8); break; /* LD A, [HL] */ 
      case 0x7F: MORE_CYCLES(4); break; /* LD A, A */ 
      case 0x80: inst_add_a_b(); break; /* ADD A, B */ 
      case 0x81: inst_add_a_c(); break; //* ADD A, C */ 
      case 0x82: inst_add_a_d(); break; /* ADD A, D */ 
      case 0x83: inst_add_a_e(); break; /* ADD A, E */ 
      case 0x84: inst_add_a_h(); break; /* ADD A, H */ 
      case 0x85: inst_add_a_l(); break; /* ADD A, L */ 
      case 0x86: inst_add_a_hl_indirect(); break; /* ADD A, [HL] */ 
      case 0x87: inst_add_a_a(); break; /* ADD A, A */ 
      case 0x88: inst_adc_a_b(); break;  /* ADC A, B */ 
      case 0x89: inst_adc_a_c(); break;  /* ADC A, C */ 
      case 0x8A: inst_adc_a_d(); break;/* ADC A, D */ 
      case 0x8B: inst_adc_a_e(); break; /* ADC A, E */ 
      case 0x8C: inst_adc_a_h(); break; /* ADC A, H */ 
      case 0x8D: inst_adc_a_l(); break; /* ADC A, L */ 
      case 0x8E: inst_adc_a_hl_indirect(); break;  /* ADC A, [HL] */ 
      case 0x8F: inst_adc_a_a(); break; /* ADC A, A */ 
      case 0x90: inst_sub_b(); break; /* SUB A, B */ 
      case 0x91: inst_sub_c(); break;
      case 0x92: inst_sub_d(); break;
      case 0x93: inst_sub_e(); break;
      case 0x94: inst_sub_h(); break;
      case 0x95: inst_sub_l(); break;
      case 0x96: inst_sub_hl_indirect(); break;
      case 0x97: inst_sub_a(); break; /* SUB A, A */ 
      case 0x98: INST_SBC(gbz80.BC.b.h); MORE_CYCLES(4); break;  /* SBC A, B */ 
      case 0x99: INST_SBC(gbz80.BC.b.l); MORE_CYCLES(4); break; /* SBC A, C */  
      case 0x9A: INST_SBC(gbz80.DE.b.h); MORE_CYCLES(4); break; /* SBC A, D */ 
      case 0x9B: INST_SBC(gbz80.DE.b.l); MORE_CYCLES(4); break; /* SBC A, E */ 
      case 0x9C: INST_SBC(gbz80.HL.b.h); MORE_CYCLES(4); break; /* SBC A, H */ 
      case 0x9D: INST_SBC(gbz80.HL.b.l); MORE_CYCLES(4); break;  /* SBC A, L */ 
      case 0x9E: INST_SBC(memory_read_byte(gbz80.HL.uw)); MORE_CYCLES(8); break; /* SBC A, [HL] */ 
      case 0x9F: inst_sbc_a(); break; /* SBC A, A */ 
      case 0xA0: inst_and_b(); break; /* AND B */ 
      case 0xA1: inst_and_c(); break; /* AND C */ 
      case 0xA2: inst_and_d(); break;
      case 0xA3: inst_and_e(); break;
      case 0xA4: inst_and_h(); break;
      case 0xA5: inst_and_l(); break;
      case 0xA6: inst_and_hl_indirect(); break;
      case 0xA7: inst_and_a(); break;
      case 0xA8: INST_XOR(gbz80.BC.b.h); MORE_CYCLES(4); break; /* XOR B */ 
      case 0xA9: INST_XOR(gbz80.BC.b.l); MORE_CYCLES(4); break; /* XOR C */ 
      case 0xAA: INST_XOR(gbz80.DE.b.h); MORE_CYCLES(4); break; /* XOR D */ 
      case 0xAB: INST_XOR(gbz80.DE.b.l); MORE_CYCLES(4); break; /* XOR E */ 
      case 0xAC: INST_XOR(gbz80.HL.b.h); MORE_CYCLES(4); break; /* XOR H */ 
      case 0xAD: INST_XOR(gbz80.HL.b.l); MORE_CYCLES(4); break; /* XOR L */ 
      case 0xAE: INST_XOR(memory_read_byte(gbz80.HL.uw)); MORE_CYCLES(8); break; /* XOR [HL] */ 
      case 0xAF: inst_xor_a(); break; /* XOR A */ 
      case 0xB0: inst_or_b(); break; /* OR B */ 
      case 0xB1: inst_or_c(); break; /* OR C */ 
      case 0xB2: inst_or_d(); break;  /* OR D */ 
      case 0xB3: inst_or_e(); break; /* OR E */ 
      case 0xB4: inst_or_h(); break; /* OR H */ 
      case 0xB5: inst_or_l(); break; /* OR L */ 
      case 0xB6: inst_or_hl_indirect(); break;
      case 0xB7: inst_or_a(); break;
		case 0xB8: inst_cp_b(); break; /* CP B */ 
      case 0xB9: inst_cp_c(); break; /* CP C */ 
      case 0xBA: inst_cp_d(); break; /* CP D */ 
      case 0xBB: inst_cp_e(); break; /* CP E */ 
      case 0xBC: inst_cp_h(); break; /* CP H */ 
      case 0xBD: inst_cp_l(); break; /* CP L */ 
      case 0xBE: inst_cp_hl_indirect(); break; /* CP [HL] */ 
      case 0xBF: inst_cp_a(); break; /* CP A */  
      case 0xC0: inst_ret_nz();break;/* RET NZ */ 
      case 0xC1: inst_pop_bc(); break; /* POP BC */ 
      case 0xC2: inst_jp_nz_nn(); break; /* JP NZ, $XXXX */ 
      case 0xC3: inst_jp_nn(); break; /* JP $XXXX */ 
      case 0xC4: inst_call_nz_nn(); break; /* CALL NZ, $XXXX */ 
      case 0xC5: inst_push_bc(); break; /* PUSH BC */ 
      case 0xC6: inst_add_a_n(); break; /* ADD A, $XX */ 
		case 0xC7: inst_rst_00h(); break; /* RST $00 */
      case 0xC8: inst_ret_z(); break; /* RET Z */ 
      case 0xC9: inst_ret(); break; /* RET */ 
      case 0xCA: inst_jp_z_nn(); break; /* JP Z, $XXXX */ 
      case 0xCB: inst_cb(); break; /* CB .. */ 
      case 0xCC: inst_call_z_nn(); break; /* CALL Z, $XXXX */
      case 0xCD: inst_call_nn(); break; /* CALL $XXXX */ 
      case 0xCE: inst_adc_a_n(); break; /* ADC A, $XX */ 
		case 0xCF: inst_rst_08h(); break; /* RST $08 */
      case 0xD0: inst_ret_nc(); break; /* RET NC */ 
      case 0xD1: inst_pop_de(); break; /* POP DE */ 
      case 0xD2: inst_jp_nc_nn(); break; /* JP NC, $XXXX */ 
   // case 0xD3: /* UNKNOWN */
      case 0xD4: inst_call_nc_nn(); break; /* CALL NC, $XXXX */ 
      case 0xD5: inst_push_de(); break; /* PUSH DE */ 
      case 0xD6: inst_sub_n(); break; /* SUB $XX */ 
      case 0xD7: inst_rst_10h(); break; /* RST $10 */ 
      case 0xD8: inst_ret_c(); break; /* RET C */ 
      case 0xD9: inst_reti(); break; /* RETI */ 
      case 0xDA: inst_jp_c_nn(); break; /* JP C, $XXXX */ 
   // case 0xDB: /* UNKNOWN */
      case 0xDC: inst_call_c_nn(); break; /* CALL C, $XXXX */ 
   // case 0xDD: /* UNKNOWN */
      case 0xDE: INST_SBC(memory_read_pc_byte()); MORE_CYCLES(8); gbz80.PC.uw++; break; /* SBC A, $XX */ 
      case 0xDF: inst_rst_18h(); break; /* RST $18 */ 
      case 0xE0: inst_ld_ff_n_indirect_a(); break; /* LD [$FFXX], A */ 
      case 0xE1: inst_pop_hl(); break; /* POP HL */ 
      case 0xE2: inst_ld_c_indirect_a(); break; /* LD [C], A */ 
   // case 0xE3: /* UNKNOWN */
	// case 0xE4: /* UNKNOWN */
      case 0xE5: inst_push_hl(); break; /* PUSH HL */ 
      case 0xE6: inst_and_n(); break; /* AND $XX */ 
      case 0xE7: inst_rst_20h(); break; /* RST $20 */ 
      case 0xE8: inst_add_sp_n(); break; /* ADD SP, $XX */
      case 0xE9: inst_jp_hl(); break; /* JP HL */ 
      case 0xEA: inst_ld_nn_indirect_a(); break; /* LD [$XXXX], A */ 
   // case 0xEB: /* UNKNOWN */
	// case 0xEC:
	// case 0xED:
      case 0xEE: INST_XOR(memory_read_pc_byte()); MORE_CYCLES(8); gbz80.PC.uw++; break; /* XOR $XX */
      case 0xEF: inst_rst_28h(); break; /* RST $28 */ 
      case 0xF0: inst_ld_a_ff_n_indirect(); break; /* LD A, [$FFXX] */ 
      case 0xF1: inst_pop_af(); break; /* POP AF */ 
      case 0xF2: inst_ld_a_c_indirect(); break; /* LD A, [C] */
      case 0xF3: inst_di(); break; /*  DI */ 
   // case 0xF4: /* UNKNOWN */
      case 0xF5: inst_push_af(); break; /* PUSH AF */ 
      case 0xF6: inst_or_n(); break; /* OR $XX */ 
      case 0xF7: inst_rst_30h(); break; /* RST $30 */ 
      case 0xF8: inst_ld_hl_sp_n(); break; /* LD HL, SP+$XX */ 
		case 0xF9: inst_ld_sp_hl(); break; /* LD SP, HL */	
      case 0xFA: inst_ld_a_nn_indirect(); break; /* LD A, [$XXXX] */ 
      case 0xFB: inst_ei(); break; /* EI */ 
   // case 0xFC: /* UNKNOWN */
	// case 0xFD:
      case 0xFE: inst_cp_n(); break; /* CP $XX */ 
      case 0xFF: inst_rst_38h(); break; /* RST $38 */ 
      
      default: inst_unknown(); return;
   }
}
