#ifndef MED_UTIL_ITERATORHELPER_H
#define MED_UTIL_ITERATORHELPER_H

#include <memory>
#include <stdexcept>

namespace Med {
namespace Util {
  
template<typename Result_>
class IteratorHelper {
public:
  typedef Result_ Result;

  class Impl {
  public:
    // Returns the item pointed to by the iterator.
    virtual Result get() const = 0;
    // Advances the iterator to the next item. Returns false if the advance couldn't be completed because there are no more items.
    virtual bool advance() = 0;
  };
  
  // Constructs a finished iterator.
  IteratorHelper() {}
  
  // Constructs an iterator with the given implementation. impl may be nullptr; in that case a finished iterator is constructed. If impl != nullptr, it must return at least one item.
  IteratorHelper(std::unique_ptr<Impl>&& impl) : impl(std::move(impl)) {}
  
  bool operator==(const IteratorHelper& other) const { return impl == other.impl; }
  bool operator!=(const IteratorHelper& other) const { return !operator==(other); }

  Result operator*() const {
    if (impl == nullptr) throw std::logic_error("Dereferencing finished iterator!");
    return impl->get();
  }
  Result* operator->() const { return &operator*(); }

  void operator++() {
    if (impl == nullptr) throw std::logic_error("Advancing finished iterator!");
    if (!impl->advance()) impl.reset();
  }
  
private:
  // Released when advance() returns false. impl == nullptr means a finished iterator.
  std::unique_ptr<Impl> impl;
};

}  // namespace Util
}  // namespace Med

#endif // MED_UTIL_ITERATORHELPER_H
