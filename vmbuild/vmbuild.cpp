// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>
#include <fstream>
#include <cstring>
#include <iostream>

#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <cassert>

#include "../api/boot/multiboot.h"
#include "../api/util/elf.h"
#include "../api/util/elf_binary.hpp"

#define SECT_SIZE 512
#define SECT_SIZE_ERR  666
#define DISK_SIZE_ERR  999

bool verb = false;
#define INFO_(FROM, TEXT, ...) if (verb) fprintf(stderr, "%13s ] " TEXT "\n", "[ " FROM, ##__VA_ARGS__)
#define INFO(X,...) INFO_("Vmbuild", X, ##__VA_ARGS__)

using namespace std;

// Location of special variables inside the bootloader
static const int bootvar_binary_size     = 4;
static const int bootvar_binary_location = 8;

static bool test {false};

static const string info  {"Create a bootable disk image for IncludeOS.\n"};
static const string usage {"Usage: vmbuild <service_binary> [<bootloader>][-test]\n"};

class Vmbuild_error : public std::runtime_error {
  using runtime_error::runtime_error;
};

string get_bootloader_path(int argc, char** argv) {

  if (argc == 2) {
    // Determine IncludeOS install location from environment, or set to default
    std::string includeos_install;
    if (auto env_install = getenv("INCLUDEOS_INSTALL")) {
      includeos_install = env_install;
    } else {
      includeos_install = std::string{getenv("HOME")} + "/IncludeOS_install";
    }
    return includeos_install + "/bootloader";
  } else {
    return argv[2];
  }
}

int main(int argc, char** argv)
{
  // Verify proper command usage
  if (argc <= 2) {
    cout << info << usage;
    exit(EXIT_FAILURE);
  }

  // VERBOSE=...
  const char* env_verb = getenv("VERBOSE");
  if (env_verb && strlen(env_verb) > 0)
      verb = true;

  const string bootloader_path = get_bootloader_path(argc, argv);

  INFO(">>> Using bootloader %s" , bootloader_path.c_str());

  if (argc > 2)
    const string bootloader_path {argv[2]};

  const string elf_binary_path  {argv[1]};
  const string img_name {elf_binary_path.substr(elf_binary_path.find_last_of("/") + 1, string::npos) + ".img"};

  INFO("Creating image '%s'" , img_name.c_str());

  if (argc > 3) {
    if (string{argv[3]} == "-test") {
      test = true;
      verb = true;
    } else if (string{argv[3]} == "-v"){
      verb = true;
    }
  }

  struct stat stat_boot;
  struct stat stat_binary;

  // Validate boot loader
  // Validate boot loader
  if (stat(bootloader_path.c_str(), &stat_boot) == -1) {
    INFO("Could not open %s, exiting\n" , bootloader_path.c_str());
    return errno;
  }

  if (stat_boot.st_size != SECT_SIZE) {
    INFO("Boot sector not exactly one sector in size (%ld bytes, expected %i)",
         stat_boot.st_size, SECT_SIZE);
    return SECT_SIZE_ERR;
  }
  INFO("Size of bootloader: %ld\t" , stat_boot.st_size);

  // Validate service binary location
  if (stat(elf_binary_path.c_str(), &stat_binary) == -1) {
    fprintf(stderr, "vmbuild: Could not open '%s'\n" , elf_binary_path.c_str());
    return errno;
  }

  intmax_t binary_sectors = stat_binary.st_size / SECT_SIZE;
  if (stat_binary.st_size & (SECT_SIZE-1)) binary_sectors += 1;

  INFO("Size of service: \t%ld bytes" , stat_binary.st_size);

  const decltype(binary_sectors) img_size_sect  {1 + binary_sectors};
  const decltype(binary_sectors) img_size_bytes {img_size_sect * SECT_SIZE};
  assert((img_size_bytes & (SECT_SIZE-1)) == 0);

  INFO("Total disk size: \t%ld bytes, => %ld sectors",
       img_size_bytes, img_size_sect);

  const auto disk_size = img_size_bytes;

  INFO("Creating disk of size %ld sectors / %ld bytes" ,
       (disk_size / SECT_SIZE), disk_size);

  vector<char> disk (disk_size);

  auto* disk_head = disk.data();

  ifstream file_boot {bootloader_path}; //< Load the boot loader into memory

  auto read_bytes = file_boot.read(disk_head, stat_boot.st_size).gcount();
  INFO("Read %ld bytes from boot image", read_bytes);

  ifstream file_binary {elf_binary_path}; //< Load the service into memory

  auto* binary_imgloc = disk_head + SECT_SIZE; //< Location of service code within the image

  read_bytes = file_binary.read(binary_imgloc, stat_binary.st_size).gcount();
  INFO("Read %ld bytes from service image" , read_bytes);

  // only accept ELF binaries
if (binary_imgloc[EI_MAG0] == ELFMAG0
 && binary_imgloc[EI_MAG1] == ELFMAG1
 && binary_imgloc[EI_MAG2] == ELFMAG2
 && binary_imgloc[EI_MAG3] == ELFMAG3)
{
  if (binary_imgloc[EI_CLASS] == ELFCLASS32)
  {
    INFO("Found 32-bit ELF\n");
    Elf_binary<Elf32> binary ({binary_imgloc, stat_binary.st_size});

    // Verify multiboot header
    auto& sh_multiboot = binary.section_header(".multiboot");
    multiboot_header& multiboot = *reinterpret_cast<multiboot_header*>(binary.section_data(sh_multiboot).data());

    INFO("Verifying multiboot header:");
    INFO("Magic value: 0x%x\n" , multiboot.magic);
    if (multiboot.magic != MULTIBOOT_HEADER_MAGIC) {
      printf("Multiboot magic mismatch: 0x%08x vs %#x\n", multiboot.magic, MULTIBOOT_HEADER_MAGIC);
    }
    assert(multiboot.magic == MULTIBOOT_HEADER_MAGIC);

    INFO("Flags: 0x%x" , multiboot.flags);
    INFO("Checksum: 0x%x" , multiboot.checksum);
    INFO("Checksum computed: 0x%x", multiboot.checksum + multiboot.flags + multiboot.magic);

    // Verify multiboot header checksum
    assert(multiboot.checksum + multiboot.flags + multiboot.magic == 0);

    INFO("Header addr: 0x%x" , multiboot.header_addr);
    INFO("Load start: 0x%x" , multiboot.load_addr);
    INFO("Load end: 0x%x" , multiboot.load_end_addr);
    INFO("BSS end: 0x%x" , multiboot.bss_end_addr);
    INFO("Entry: 0x%x" , multiboot.entry_addr);

    // Write binary size and entry point to the bootloader
    *(reinterpret_cast<int*>(disk_head + bootvar_binary_size))     = binary_sectors;
    *(reinterpret_cast<int*>(disk_head + bootvar_binary_location)) = binary.entry();
  }
  else if (binary_imgloc[EI_CLASS] == ELFCLASS64)
  {

    auto* hdr   = (const Elf64_Ehdr*) binary_imgloc;
    auto  entry = hdr->e_entry;
    INFO("Found 64-bit ELF with entry at %p", (void*) entry);

    // Write binary size and entry point to the bootloader
    *(uint32_t*) (disk_head + bootvar_binary_size)     = (uint32_t) binary_sectors;
    *(uint32_t*) (disk_head + bootvar_binary_location) = (uint32_t) entry;
  }
  else
  {
    fprintf(stderr, "ERROR: Unknown ELF format\n");
    std::terminate();
  }
}
else
{
  fprintf(stderr, "ERROR: Not ELF binary\n");
  std::terminate();
}

  if (test) {
    INFO("\nTEST overwriting service with testdata");
    for(int i {0}; i < (img_size_bytes - 512); ++i) {
      disk[(512 + i)] = (i % 256);
    }
  } //< if (test)

  // Write the image
  auto* image = fopen(img_name.c_str(), "w");
  auto  wrote = fwrite(disk_head, 1, disk_size, image);

  INFO("Wrote %ld bytes => %ld sectors to '%s'",
       wrote, (wrote / SECT_SIZE), img_name.c_str());

  fclose(image);
}
