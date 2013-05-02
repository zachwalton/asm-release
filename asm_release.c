//
// asm_release.c
//
// Releases disks from ASM after removing them from a disk group by utilizing the
// oracleasm ABI.
//
// Usage: ./asm_release ASMXXXXXX 
// - where ASMXXXXXX is the ASM disk name.
//
// Constructed with help from the following article:
// http://www.scaleabilities.co.uk/2013/02/07/diagnosing-asmlib/
//
// License: GPLv2
// Author:  Zach Walton
// Date:    5/1/2013
//

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;

struct oracleasm_abi_info
{
/*00*/  __u32       ai_magic;   /* ASM_ABI_MAGIC */
        __u16       ai_version; /* ABI version */
        __u16       ai_type;    /* Type of operation */
        __u32       ai_size;    /* Size of passed struct */
        __u32       ai_status;  /* Did it succeed */
/*10*/        
};

struct oracleasm_close_disk_v2
{
/*00*/  struct oracleasm_abi_info   cd_abi;
/*10*/  __u64                       cd_handle;
/*18*/    
};

union MagicStr
{
    char  str[4];
    __u32 i;
};

int main(int argc, char * argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [asm device name].\n\ne.g. %s ASMXXXXXX\n",
                argv[0], 
                argv[0]);
        return 1;
    }

    int res, fd;
    char * prefix = "/dev/oracleasm/disks/";
    char * disk = argv[1];
    char path[80];
    strcpy(path, prefix);
    strcat(path, disk);
    struct stat *buf = malloc(sizeof(struct stat));
    // Fetch the inode number for the device that we want to free.  This is the
    // handle that we end up passing to the oracleasm ABI.
    res = stat((const char *)path, buf);
    if (res == -1) {
        fprintf(stderr, "Error opening file: \n%s\n%s\n", path, strerror(errno));
        return 1;
    }
    __u64 inode = buf->st_ino;

    free(buf);

    union MagicStr magicstr;
    strcpy(magicstr.str, "MSA\0"); // ASM magic string

    struct oracleasm_abi_info oabi =  {
                                        .ai_magic   = magicstr.i,

                                        // This actually just dereferences the first
                                        // element and drops the second, but since
                                        // the second element is null we don't care
                                        .ai_version = (__u16)*"\2\0",
                                        .ai_type    = (__u16)*"\6\0", // ASMOP_CLOSE_DISK
                                        .ai_size    = (__u32)sizeof(struct oracleasm_close_disk_v2),
                                        .ai_status  = (__u32)*"\0\0\0\0"
                                      };

    struct oracleasm_close_disk_v2 cdv2 = {
                                            .cd_abi    = oabi,
                                            .cd_handle = inode
                                          };

    // this needs to be replaced with code that determines the correct IID
    fd = open("/dev/oracleasm/iid/0000000000000009", O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file: %s", strerror(errno));
        return 1;
    }

    // This is the way that asmlib accepts operations from user space.
    res = read(fd, (char *) & cdv2, sizeof(struct oracleasm_close_disk_v2));
    errno = ~cdv2.cd_abi.ai_status - 1;
    fprintf(stderr, "Error issuing ASMOP: %s\n", strerror(errno));
}

