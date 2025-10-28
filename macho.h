/******************************************************************************
 * @file            macho.h
 *****************************************************************************/
#ifndef     _MACHO_H
#define     _MACHO_H

struct macho_header_64 {

    unsigned char magic[4];
    unsigned char cpu_type[4];
    unsigned char cpu_subtype[4];
    unsigned char file_type[4];
    unsigned char num_cmds[4];
    unsigned char sizeof_cmds[4];
    unsigned char flags[4];
    unsigned char reserved[4];

};

#define     MH_MAGIC_64                 0xFEEDFACF

#define     MH_CPU_TYPE_AMD64           0x01000007
#define     MH_CPU_TYPE_ARM64           0x0100000C

#define     MH_CPU_SUBTYPE_I386_ALL     3
#define     MH_OBJECT                   1

struct load_command {

    unsigned char command[4];
    unsigned char command_size[4];

};

#define     LC_SEGMENT_64               0x19
#define     LC_SYMTAB                   0x02

struct segment_command_64 {

    unsigned char command[4];
    unsigned char command_size[4];
    
    char seg_name[16];
    
    unsigned char vm_addr[8];
    unsigned char vm_size[8];
    unsigned char file_off[8];
    unsigned char file_size[8];
    
    unsigned char max_prot[4];
    unsigned char init_prot[4];
    unsigned char num_sects[4];
    unsigned char flags[4];

};

struct section_64 {

    char sect_name[16];
    char seg_name[16];
    
    unsigned char addr[8];
    unsigned char size[8];
    
    unsigned char offset[4];
    unsigned char align[4];
    unsigned char rel_off[4];
    unsigned char num_reloc[4];
    
    unsigned char flags[4];
    
    unsigned char reserved1[4];
    unsigned char reserved2[4];
    unsigned char reserved3[4];

};

struct symtab_command {

    unsigned char command[4];
    unsigned char command_size[4];
    unsigned char symbol_offset[4];
    unsigned char num_symbols[4];
    unsigned char string_offset[4];
    unsigned char string_size[4];

};

#define     MACHO_N_EXT                 0x01

struct nlist_64 {

    unsigned char n_strx[4];
    unsigned char n_type[1];
    unsigned char n_sect[1];
    unsigned char n_desc[2];
    unsigned char n_value[8];

};

#endif      /* _MACHO_H */
