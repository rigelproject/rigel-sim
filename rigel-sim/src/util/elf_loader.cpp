////////////////////////////////////////////////////////////////////////////////
// 'elf_loader.cpp' 
////////////////////////////////////////////////////////////////////////////////
//
// Handle inserting Rigel ELF file into the target's memory.
//
////////////////////////////////////////////////////////////////////////////////

//FIXME  Use C++ strings, less repetition and hard-coding, support more ELF features,
//align everything properly,
//Explicitly zero-fill the bss sections instead of relying on a default 0 value
//in the backng store, etc.

#include <elf.h>                        // for Elf32_Shdr, Elf32_Ehdr, etc
#include <fcntl.h>                      // for SEEK_SET
#include <libelf.h>              // for Elf_Data, elf32_getehdr, etc
#include <gelf.h>
#include <stdint.h>                     // for uint32_t
#include <inttypes.h>                   // for e.g. PRIx64
#include <stdio.h>                      // for fprintf, stderr, fclose, etc
#include <stdlib.h>                     // for NULL, exit, free
#include <string.h>                     // for strcmp, strlen, strncmp
#include <algorithm>                    // for max
#include <string>                       // for string
#include <vector>
#include "sim.h"            // for DUMP_ELF_IMAGE, etc
#include "util/util.h"           // for ExitSim, ELFAccess
#include "memory/backing_store.h"

std::vector<uint32_t> rigel::ENTRY_POINTS;

////////////////////////////////////////////////////////////////////////////////
// LoadELF
// 
// Put binary into memory at the proper location as stipulated by the ELF file.
// Uses the GNU libelf to do the dirty work
// 
// PARAMETERS:
// bin_name:	Name of file to open and load
// 
////////////////////////////////////////////////////////////////////////////////
void ELFAccess::LoadELF(std::string bin_name, rigel::GlobalBackingStoreType *mem) {
  Elf *elf;
  FILE *fp;
  int cnt;
  Elf_Scn * scn;
  Elf32_Ehdr * ehdr;
  Elf32_Shdr * shdr;
	GElf_Phdr phdr;

  // Ignore LoadELF on dumps

  if ((fp = fopen(bin_name.c_str(), "rb")) == NULL) {
    throw ExitSim("ELFAccess::LoadELF(): fopen() Failed", 1);
  }

  elf_version(EV_CURRENT);
  if ((elf = elf_begin(fileno(fp), ELF_C_READ, NULL)) == NULL) {
    std::string s("ELFAccess::LoadELF(): elf_begin() Failed -- ");
    s += elf_errmsg(-1);
    throw ExitSim(s.c_str(), 1);
  }

  FILE *dump_elf_img = NULL;
  if( rigel::DUMP_ELF_IMAGE ) {
    if ((dump_elf_img = fopen("elfimage.hex", "w")) == NULL) {
      throw ExitSim("elfimage.hex: fopen() Failed", 1);
    }
  }
  int i = 0;

  //Start highwater mark at 0, set to max physical address of executable program segments.
  //FIXME Do something more intelligent to track code addresses, like have an interval data structure.
  //That way we can support JITs, interspersed code and data sections, multiprogramming, etc.
  rigel::CODEPAGE_HIGHWATER_MARK = 0;

	while(1) {
		if((gelf_getphdr(elf, i, &phdr)) == NULL)
			break;
	  printf("p_type %08x p_offset %08"PRIx64" p_vaddr %08"PRIx64" p_paddr %08"PRIx64" p_filesz %08"PRIx64" p_memsz %08"PRIx64" p_flags %08x p_align %08"PRIx64"\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz, phdr.p_memsz, phdr.p_flags, phdr.p_align);

	  uint32_t *buf = new uint32_t[(phdr.p_filesz+sizeof(uint32_t)-1)/sizeof(uint32_t)];
    fseek(fp, phdr.p_offset, SEEK_SET);
    unsigned int bytes_read = fread((void *)buf, 1, phdr.p_filesz, fp);
    if (bytes_read != phdr.p_filesz)
    {
      fprintf(stderr, "Error: load_elf() read %u of %"PRIu64" bytes\n", bytes_read, phdr.p_filesz);
      exit(1);
    }
    // Fix endianness... FIXME should probably check ELF header for this first
    for ( uint32_t j = 0; j < (phdr.p_memsz+3)/4; j++) 
    {
      uint32_t write_addr = phdr.p_paddr + 4*j;
      // Write the data to memory.
      uint32_t wdata;
      if(j >= ((phdr.p_filesz+3)/4))
        wdata = 0;
      else
        wdata = buf[j];
      mem->write_word( write_addr, wdata );
      if ( rigel::DUMP_ELF_IMAGE ) {
        fprintf( dump_elf_img, "%08x %08x\n", write_addr, wdata );
      }

    }
  
    delete [] buf;
    buf = NULL;


    i++;
	}
  printf("Executable has %d phdrs\n", i);
  ehdr = elf32_getehdr(elf);
  printf("Start address is 0x%08x\n", ehdr->e_entry);

  //FIXME Support multiprogramming by only applying this entry point to certain threads.
	//rigel::ENTRY_POINTS.resize(rigel::THREADS_TOTAL);	
	for(int i = 0; i < rigel::THREADS_TOTAL; i++) {
    //rigel::ENTRY_POINTS[i] = ehdr->e_entry;
		rigel::ENTRY_POINTS.push_back(ehdr->e_entry);
	}

  ehdr = elf32_getehdr(elf);
  scn = elf_getscn(elf,  ehdr->e_shstrndx); 
  for (cnt = 1; (scn = elf_nextscn(elf, scn)); cnt++) {
    if ((shdr = elf32_getshdr(scn)) == NULL) {
      throw ExitSim("ELFAccess::LoadELF(): Bad ELF", 1);
    }
    if(shdr->sh_flags & SHF_EXECINSTR) //This section contains instructions
		{
			if(shdr->sh_addr + shdr->sh_size > rigel::CODEPAGE_HIGHWATER_MARK) {
				fprintf(stderr, "Setting CODEPAGE_HIGHWATER_MARK to %08x\n", shdr->sh_addr+shdr->sh_size);
				rigel::CODEPAGE_HIGHWATER_MARK = shdr->sh_addr+shdr->sh_size;
			}
		}
	}	

    //fprintf(stderr, "sh_type %08X sh_flags %08X sh_addr %08X sh_offset %08X sh_size %08X sh_link %08X sh_info %08X sh_addralign %08X sh_entsize %08X\n", shdr->sh_type, shdr->sh_flags, shdr->sh_addr, shdr->sh_offset, shdr->sh_size, shdr->sh_link, shdr->sh_info, shdr->sh_addralign, shdr->sh_entsize);
    //fprintf(stderr, "p_type %08X p_offset %08X p_vaddr %08X p_paddr %08X p_filesz %08X p_memsz %08X p_flags %08X p_align %08X\n", shdr->p_type, shdr->p_offset, shdr->p_vaddr, shdr->p_paddr, shdr->p_filesz, shdr->p_memsz, shdr->p_flags, shdr->p_align);   
 
  free(elf); elf = NULL;
  fclose(fp);

  if( rigel::DUMP_ELF_IMAGE ) {
    fclose(dump_elf_img);
    throw ExitSim("DUMP_ELF_IMAGE completed!", 0);
  }

}

