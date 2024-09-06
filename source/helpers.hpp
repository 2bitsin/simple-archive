#pragma once

namespace helpers
{
  inline constexpr auto align(std::size_t offset, std::size_t by = PAGE_SIZE) -> std::size_t {
    by -= 1u;
    return (offset + by) & ~by;
  }
  inline constexpr auto pages(std::size_t length, std::size_t pgsize = PAGE_SIZE) -> std::size_t {
    return (length + pgsize - 1u) / pgsize;
  }
}