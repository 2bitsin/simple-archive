#pragma once

#include <cstdint>
#include <cstddef>

#pragma pack(push, 1)
static constexpr uint32_t HEADER_MAGIC = '99so';
static constexpr uint32_t PAGE_SIZE = 0x1000u;

static constexpr uint32_t DIRENT_DIRECTORY_FLAG = 0x1u;
struct header
{
  uint32_t  magic;
  uint16_t  version;
  uint16_t  root_length;
  uint32_t  data_offset;
  uint32_t  data_length;
  uint8_t   name_size;
  uint8_t   name[47];
};
static_assert(sizeof(header) == 64, "dirent size must be 128 bytes");

struct dirent
{
  uint32_t  flags;
  uint32_t  offset;
  uint32_t  length;
  uint16_t  remainder;
  uint8_t   name_size;
  char      name[49];
};
static_assert(sizeof(dirent) == 64, "dirent size must be 128 bytes");
#pragma pack(pop)