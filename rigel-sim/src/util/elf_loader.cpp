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
uint32_t rigel::CODEPAGE_LIBPAR_BEGIN = 0x0;
uint32_t rigel::CODEPAGE_LIBPAR_END = 0x0;
uint32_t rigel::LOCKS_REGION_BEGIN = 0x0;
uint32_t rigel::LOCKS_REGION_END = 0x0;

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
  Elf_Scn *scn;
  Elf32_Ehdr *ehdr;
  Elf32_Shdr *shdr;
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

	while(1) {
		if((gelf_getphdr(elf, i, &phdr)) == NULL)
			break;
	  //printf("p_type %08x p_offset %08"PRIx64" p_vaddr %08"PRIx64" p_paddr %08"PRIx64" p_filesz %08"PRIx64" p_memsz %08"PRIx64" p_flags %08x p_align %08"PRIx64"\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz, phdr.p_memsz, phdr.p_flags, phdr.p_align);

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

  ehdr = elf32_getehdr(elf);
  printf("Start address is 0x%08x\n", ehdr->e_entry);

  //FIXME Support multiprogramming by only applying this entry point to certain threads.
	//rigel::ENTRY_POINTS.resize(rigel::THREADS_TOTAL);	
	for(int i = 0; i < rigel::THREADS_TOTAL; i++) {
    //rigel::ENTRY_POINTS[i] = ehdr->e_entry;
		rigel::ENTRY_POINTS.push_back(ehdr->e_entry);
	}

	scn = NULL;
  while((scn = elf_nextscn(elf, scn)) != NULL) {
    if((shdr = elf32_getshdr(scn)) == NULL) {
      throw ExitSim("ELFAccess::LoadELF(): Bad ELF", 1);
    }
		printf("ELF section has sh_flags %08x\n", shdr->sh_flags);
    if(shdr->sh_flags & SHF_EXECINSTR) //This section contains instructions
		{
      //NOTE: Setting a few extra words as executable so that we don't bork if
      //the structural core speculatively fetches instructions before resolving a
      //branch.  Let's say 3 stages * issue width
      //FIXME MEM_WORD_SIZE is a bad name in the wrong place, colocate it with the rigel_word_t typedef
      mem->set_executable_region(shdr->sh_addr,
                                 shdr->sh_addr + shdr->sh_size + (3*rigel::ISSUE_WIDTH*rigel::MEM_WORD_SIZE) - 1);
    }
    else if(shdr->sh_flags & SHF_WRITE) {
      mem->set_writable_region(shdr->sh_addr, shdr->sh_addr + shdr->sh_size - 1);
      mem->set_readable_region(shdr->sh_addr, shdr->sh_addr + shdr->sh_size - 1);
    }
    else if(shdr->sh_flags & SHF_ALLOC) {
      mem->set_readable_region(shdr->sh_addr, shdr->sh_addr + shdr->sh_size - 1);
    }
    //If none of the above conditions is satisfied, this section does not allocate space in memory (e.g., symbol/string table)

    // If this section is a symbol table, find the _end symbol and set everything after that (heap+stack) to be RW-.
    if(shdr->sh_type == SHT_SYMTAB)
    {
      //edata points to the symbol table
      Elf_Data *edata = NULL; //Initialize to null so that the first elf_getdata()
                              //call returns the first data section
      while((edata = elf_getdata(scn, edata)) != NULL) {
        // the number of symbols in this data section is the total size over the section's entry size,
        // which is the size of a symbol table entry.
        int num_symbols = edata->d_size / shdr->sh_entsize;
        // loop over symbols
        Elf32_Sym *s = (Elf32_Sym *)edata->d_buf;
        for(i = 0; i < num_symbols; i++) {
          //Get the name of the symbol
          //The name is stored in the string table in the section pointed to
          //by shdr->sh_link, and is at the offset stored in s->st_name.
          char *symbol_name = elf_strptr(elf, shdr->sh_link, s->st_name);
          if(symbol_name != NULL && strcmp(symbol_name, "_end") == 0) {
            //We found the _end symbol
            printf("_end is at 0x%08x, setting the rest of memory to RW- permissions\n", s->st_value);
            mem->set_readable_region(s->st_value, 0xFFFFFFFFU); //FIXME Should use a constant tied to Rigel's address type
            mem->set_writable_region(s->st_value, 0xFFFFFFFFU);  
          }
          s++; //go to next symbol
        }
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

