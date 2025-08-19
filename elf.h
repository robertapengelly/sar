/******************************************************************************
 * @file            coff.h
 *****************************************************************************/
#ifndef     _ELF_H
#define     _ELF_H

struct elf32_exec {

    unsigned char e_ident[16];
    unsigned char e_type[2];
    unsigned char e_machine[2];
    unsigned char e_version[4];
    unsigned char e_entry[4];
    unsigned char e_phoff[4];
    unsigned char e_shoff[4];
    unsigned char e_flags[4];
    unsigned char e_ehsize[2];
    unsigned char e_phentsize[2];
    unsigned char e_phnum[2];
    unsigned char e_shentsize[2];
    unsigned char e_shnum[2];
    unsigned char e_shstrndx[2];

};

struct elf64_exec {

    unsigned char e_ident[16];
    unsigned char e_type[2];
    unsigned char e_machine[2];
    unsigned char e_version[4];
    unsigned char e_entry[8];
    unsigned char e_phoff[8];
    unsigned char e_shoff[8];
    unsigned char e_flags[4];
    unsigned char e_ehsize[2];
    unsigned char e_phentsize[2];
    unsigned char e_phnum[2];
    unsigned char e_shentsize[2];
    unsigned char e_shnum[2];
    unsigned char e_shstrndx[2];

};

struct elf32_shdr {
    
    unsigned char sh_name[4];
    unsigned char sh_type[4];
    unsigned char sh_flags[4];
    unsigned char sh_addr[4];
    unsigned char sh_offset[4];
    unsigned char sh_size[4];
    unsigned char sh_link[4];
    unsigned char sh_info[4];
    unsigned char sh_addralign[4];
    unsigned char sh_entsize[4];

};

struct elf64_shdr {
    
    unsigned char sh_name[4];
    unsigned char sh_type[4];
    unsigned char sh_flags[8];
    unsigned char sh_addr[8];
    unsigned char sh_offset[8];
    unsigned char sh_size[8];
    unsigned char sh_link[4];
    unsigned char sh_info[4];
    unsigned char sh_addralign[8];
    unsigned char sh_entsize[8];

};

struct elf32_sym {

    unsigned char st_name[4];
    unsigned char st_value[4];
    unsigned char st_size[4];
    unsigned char st_info[1];
    unsigned char st_other[1];
    unsigned char st_shndx[2];

};

struct elf64_sym {

    unsigned char st_name[4];
    unsigned char st_info[1];
    unsigned char st_other[1];
    unsigned char st_shndx[2];
    unsigned char st_value[8];
    unsigned char st_size[8];

};

#endif      /* _ELF_H */
