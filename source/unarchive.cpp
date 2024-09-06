#include <filesystem>
#include <iostream>
#include <fstream>
#include <format>
#include <span>

#include "structures.hpp"
#include "helpers.hpp"

auto load_blob(std::filesystem::path the_path) 
  -> std::vector<std::uint8_t>
{
  using namespace std;
  using namespace filesystem;
  ifstream from{ the_path, 
    ios::binary };
  if (!exists(the_path)) {
    throw runtime_error(format(
      "{} not found.", 
      the_path.string()));
  }
  auto const length = file_size(the_path);
  if (!length) {
    return {};
  }
  vector<uint8_t> blob(length);
  from.read((char*)blob.data(), blob.size());
  return blob;
}

struct directory
{
  using page_t = uint8_t [PAGE_SIZE];

  std::span<header const> m_header;
  std::span<dirent const> m_dirent;
  std::span<page_t const> m_data;

  directory(std::span<std::uint8_t const> blob)  
  {
    using namespace std;
    if (blob.size() < sizeof(header)) 
      throw runtime_error("Bad archive - header corrupted or not present");    
    m_header = span{ (header const*)blob.data(), 1u };
    if (m_header[0].magic != HEADER_MAGIC)
      throw runtime_error("Bad archive - bad signature");
    if (m_header[0].version != 0)
      throw runtime_error("Bad archive - bad version");
    if (m_header[0].root_length == 0)
      throw runtime_error("Bad archive - root directory empty");
    if (m_header[0].root_length*sizeof(dirent) > m_header[0].data_offset*PAGE_SIZE)
      throw runtime_error("Bad archive - data offset incorrect");
    if (m_header[0].name_size > sizeof(m_header[0].name))
      throw runtime_error("Bad archive - comment length out of range");
    if (blob.size() < (m_header[0].data_offset + m_header[0].data_length - 1)*PAGE_SIZE)
      throw runtime_error("Bad archive - archive corrupted");
    m_dirent = span{ (dirent const*)blob.data(), 
      m_header[0].data_offset*(PAGE_SIZE/sizeof(dirent)) };
    m_dirent = m_dirent.subspan(1);
    m_data = std::span{ (page_t const*)blob.data(), blob.size() / PAGE_SIZE };
    m_data = m_data.subspan(m_header[0].data_offset);
  }
  
  auto extract_aux(std::filesystem::path dest, std::uint32_t offset, std::uint32_t length) -> void
  {
    using namespace std;
    using namespace filesystem;
    create_directories(dest);
    for(dirent const& entry : m_dirent.subspan(offset, length))
    {
      std::string name{ entry.name, entry.name_size };
      auto target = dest / name;
      if (entry.flags & DIRENT_DIRECTORY_FLAG) {        
        extract_aux(target,
          entry.offset, entry.length);
      } else {
        ofstream sink{ target, ios::binary };
        sink.write((char*)m_data[entry.offset], 
          entry.length*PAGE_SIZE + entry.remainder);
      }
    }
  }

  auto extract(std::filesystem::path dest) -> void
  {
    extract_aux(dest, 0, m_header[0].root_length);
  }
};


int main(int argc, char** argv) try
{
  using namespace std;
  using namespace filesystem;

  vector<path> args{ argv, argv+argc };

/*
#ifndef NDEBUG
  auto const test_path = args[0].parent_path().parent_path() / "test";
  current_path(test_path);
  if (args.size() < 2) {
    args.emplace_back("helloasm.arc");
    args.emplace_back("helloasm.new");
  }
#endif
*/

  auto blob = load_blob(args[1]);
  if (blob.size() % PAGE_SIZE) {
    blob.resize(helpers::align(blob.size()), 0);
  }
  directory the_directory{ blob };
  the_directory.extract(args[2]);


  return 0;
}
catch (std::exception const& ex)
{
  std::cout << "ERROR:" << ex.what() << "\n";
  return -1;
}