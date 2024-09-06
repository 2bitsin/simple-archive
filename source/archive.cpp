#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <format>
#include <vector>
#include <print>

#include "structures.hpp"
#include "helpers.hpp"

struct tree
{ 
  std::unordered_map<std::string, tree> m_children;
  std::size_t m_links_to{ 0 };

  auto insert(std::filesystem::path running,     
    std::size_t index) -> void 
  {
    using namespace std;
    using namespace filesystem;    
    auto const head = *running.begin();
    auto const rest = relative(running, head);
    auto& child = m_children[head.string()];
    if (rest != ".") {
      child.insert(rest, index);
    } else {
      child.m_links_to = index;
    }
  }
  auto recursive_count() -> std::size_t {
    std::size_t total_count = m_children.size();
    for (auto&& [key, entry] : m_children) {      
      total_count += entry.recursive_count(); }
    return total_count;
  }

  auto count() -> std::size_t {
    return m_children.size();
  }
};

struct file_data
{
  std::filesystem::path linkto;
  std::size_t length;
  std::size_t offset;
};

inline auto copy_name(auto& target, auto const& source) {
  using namespace std;
  if (source.size() > sizeof(target.name))
    throw out_of_range(format("Name {} is too long", source));
  target.name_size = static_cast<uint8_t>(source.size());
  auto const last = copy_n(begin(source),
    min(size(source), size(target.name)), 
    begin(target.name));
  fill(last, end(target.name), '\0');
}

auto build_dirents(  
  std::vector<dirent>& dirents,
  std::size_t& offset,
  tree const& directory, 
  std::vector<file_data> const& data_index)
  -> std::size_t
{
  using namespace std;
  if (offset + directory.m_children.size() > dirents.size()) {
    throw out_of_range("Miscalculated directory entry count");
  }  
  auto curr_offset = offset;
  offset += directory.m_children.size();
  size_t index = 0u;
  for (auto [key, entry] : directory.m_children) {
    auto& dirent_r = dirents[curr_offset + index];
    copy_name (dirent_r, key);
    if (entry.m_links_to != 0) {
      auto& data_index_entry = data_index[entry.m_links_to - 1u];
      dirent_r.flags = 0u;
      dirent_r.length = data_index_entry.length / PAGE_SIZE;
      dirent_r.remainder = data_index_entry.length % PAGE_SIZE;
      dirent_r.offset = helpers::pages(data_index_entry.offset, PAGE_SIZE);
    } else {
      dirent_r.flags = DIRENT_DIRECTORY_FLAG;
      dirent_r.length = entry.m_children.size();
      dirent_r.remainder = 0u;
      dirent_r.offset = build_dirents(dirents, offset, entry, data_index);
    }
    index += 1u;
  }
  return curr_offset;
}

auto write_file(std::ofstream& target, std::size_t offset, std::filesystem::path const& from) {
  using namespace std;
  target.seekp(offset);
  ifstream source(from, ios::binary);
  size_t length = file_size(from);
  char buffer[2048];
  while (length > 0) {
    source.read(buffer, min(length, 
      sizeof(buffer)));
    auto const gcount = source.gcount();
    if (!gcount) {
      throw runtime_error(format (
        "I/O error reading {}.", from.string()));
    }
    length -= gcount;
    target.write(buffer, gcount);
  }
}

int main(int argc, char** argv)
{
  using namespace std;
  using namespace string_view_literals;
  using namespace filesystem;

  vector<path> args{ argv, argv+argc }; 
  
  /*
#ifndef NDEBUG
  auto const test_path = args[0].parent_path().parent_path() / "test";
  current_path(test_path);
  if (args.size() == 1) {
    args.emplace_back("helloasm.arc");
    args.emplace_back("helloasm");
  }
#endif
  */

  tree directory;
  std::vector<file_data> data_index;
  using rdi = recursive_directory_iterator;
  auto const root_path = args[2];  
  std::size_t data_offset = 0u;
  for (auto&& item : rdi(root_path))
  {
    auto const node_path = item.path();
    auto const relative_path = relative(node_path, root_path);
    auto const length = file_size(item.path());   
    if (!exists(node_path) || !length) {
      cout << format("Skipping {} ...\n", 
        node_path.string());
      continue; 
    }
    if (is_regular_file(node_path)) {      
      auto const the_size = file_size(item.path());
      data_index.push_back(file_data{
        .linkto = absolute(item.path()),
        .length = the_size,
        .offset = data_offset });
      data_offset += helpers::align(the_size);
      directory.insert(relative_path, data_index.size());
    }
  } 
  vector<dirent> dirents;
  dirents.resize(directory.recursive_count());
  size_t entry_offset = 0u;
  build_dirents(dirents, entry_offset, 
    directory, data_index);

  header the_header;
  copy_name(the_header, ""sv);
  the_header.magic = HEADER_MAGIC;
  the_header.root_length = directory.count();
  the_header.version = 0x0u;
  the_header.data_offset = helpers::pages(dirents.size() * sizeof(dirent) + sizeof(header), PAGE_SIZE);
  the_header.data_length = helpers::pages(data_offset, PAGE_SIZE);
  
  ofstream ofs(args[1], ios::binary);
  ofs.write((char const*)&the_header, sizeof(the_header));
  ofs.write((char const*)dirents.data(), dirents.size()*sizeof(dirent));
  data_offset = the_header.data_offset*PAGE_SIZE;
  for (auto&& data_entry : data_index) {    
    println("Writting {} to 0x{:016x}, size {} ...", data_entry.linkto.string(),
      data_offset + data_entry.offset, data_entry.length);
    write_file(ofs, data_offset+data_entry.offset, data_entry.linkto);
  }
  ofs.close();

  return 0;
}