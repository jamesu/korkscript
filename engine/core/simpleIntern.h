#pragma once
#include <cctype>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class SimpleStringInterner
{
public:
  using Entry = const char*;

   SimpleStringInterner()
   {
    EmptyString = internSV("", true);
  }

  Entry internSV(std::string_view s, bool caseSens = true) 
  {
    if (s.data() == nullptr) return EmptyString;

    // Find existing first (like your old code)
    if (Entry e = find(s, caseSens)) return e;

    // Store original-cased string (stable pointer)
    mStorage.emplace_back(s);
    const std::string& stored = mStorage.back();
    Entry ptr = stored.c_str();

    Node n;
    n.ptr = ptr;
    n.len = stored.size();
    n.hashFold = hashFolded(stored);

    mBuckets[Key{n.hashFold, n.len}].push_back(n);
    return ptr;
  }

  Entry lookupSV(std::string_view s, bool caseSens = true) const 
  {
    if (s.data() == nullptr) return EmptyString;
    return find(s, caseSens);
  }

  Entry empty() const { return EmptyString; }

private:
   
  static inline unsigned char toLower(unsigned char c) noexcept
  {
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
  }

  static std::uint64_t hashFolded(std::string_view s) noexcept 
  {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
      h ^= toLower(c);
      h *= 1099511628211ull;
    }
    return h;
  }

  struct Key 
  {
    std::uint64_t h;
    std::size_t len;
  };

  struct KeyHash 
  {
    std::size_t operator()(const Key& k) const noexcept 
    {
      std::uint64_t x = k.h ^ (std::uint64_t(k.len) + 0x9e3779b97f4a7c15ull + (k.h << 6) + (k.h >> 2));
      return static_cast<std::size_t>(x);
    }
  };

  struct KeyEq 
  {
    bool operator()(const Key& a, const Key& b) const noexcept 
    {
      return a.h == b.h && a.len == b.len;
    }
  };

  struct Node 
  {
    Entry ptr{};
    std::size_t len{};
    std::uint64_t hashFold{};
  };

  static bool equalsExact(std::string_view a, std::string_view b) noexcept
  {
    return a == b;
  }

  static bool equalsFolded(std::string_view a, std::string_view b) noexcept
  {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (toLower(static_cast<unsigned char>(a[i])) != toLower(static_cast<unsigned char>(b[i])))
      {
        return false;
      }
    }
    return true;
  }

  Entry find(std::string_view s, bool caseSens) const
  {
    const std::uint64_t hf = hashFolded(s);
    auto it = mBuckets.find(Key{hf, s.size()});
    if (it == mBuckets.end()) return nullptr;

    for (const Node& n : it->second) 
    {
      std::string_view stored{n.ptr, n.len};
      if (caseSens) 
      {
        if (equalsExact(stored, s)) 
        {
            return n.ptr;
        }
      } else 
      {
        if (equalsFolded(stored, s)) 
          {
            return n.ptr;
          }
      }
    }
    return nullptr;
  }

private:
    std::deque<std::string> mStorage;
    std::unordered_map<Key, std::vector<Node>, KeyHash, KeyEq> mBuckets;
    Entry EmptyString;
};

