#ifndef _MASS_H_
#define _MASS_H_

#define ARRAY_ENTRIES	220

// File mode flags (for mode in io_stat_t)
#define FIO_SO_IFMT		0x0038		// Format mask
#define FIO_SO_IFLNK		0x0008		// Symbolic link
#define FIO_SO_IFREG		0x0010		// Regular file
#define FIO_SO_IFDIR		0x0020		// Directory

// File mode checking macros
#define FIO_SO_ISLNK(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFLNK)
#define FIO_SO_ISREG(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFREG)
#define FIO_SO_ISDIR(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFDIR)

// status of loaded modules

/* USB Modules state
 * 0 = NOT LOADED
 * 1 = USBD.IRX LOADED
 * 2 = USBHDFSD.IRX LOADED
 */
extern u8 usb_mass_support;

// struct for mass_store_data
typedef struct {
   char name[120];
   char is_dir;   // 0 - not dir; 1 - dir;
} DATA_FILES;

int strtolower_to(char *dest, const char *src, int length);
char* strtolower(const char *src, int length);

int read_mass(const char* path, DATA_FILES* mass_tab, int max_results);
void clean_mass_store_data(DATA_FILES* mass_tab);

#endif
