/**
 * The type system is a fundamental part of the functions system. It is essentially a runtime RTTI
 * (run-time type information) system that can support multiple execution backends (e.g. C++, LLVM,
 * GLSL).
 *
 * The Type class is a container for a specific type. A type is identified by its pointer at
 * run-time. Every type also has a name, but that should only be used for e.g. debugging and not as
 * identifier.
 *
 * A Type instance can contain an arbitrary amount of type extensions. By having multiple
 * extensions for the same type, it can be used by multiple execution backends.
 *
 * Type extensions are identified by their C++ type. So, every type can have each extension type at
 * most once.
 *
 * A type owns its extensions. They can be dynamically added, but not removed. The extensions are
 * freed whenever the type is freed.
 *
 * Types are reference counted. They will be freed automatically, when nobody uses them anymore.
 */

#pragma once

#include <string>
#include <mutex>
#include "BLI_composition.hpp"
#include "BLI_shared.hpp"
#include "BLI_string_ref.hpp"

namespace FN {

using namespace BLI;

class Type;

class TypeExtension {
 private:
  Type *m_owner = nullptr;
  friend Type;

  void set_owner(Type *owner);

 public:
  virtual ~TypeExtension();

  Type *owner() const;
};

class Type final : public RefCountedBase {
 public:
  Type() = delete;
  Type(StringRef name);

  /**
   * Get the name of the type.
   */
  const StringRefNull name() const;

  /**
   * Return true, when the type has an extension of type T. Otherwise false.
   */
  template<typename T> bool has_extension() const;

  /**
   * Return the extension of type T or nullptr when the extension does not exist on this type.
   */
  template<typename T> T *extension() const;

  /**
   * Add a new extension of type T to the type. It will be constructed using the args passed to
   * this function. When this function is called multiple types with the same T, only the first
   * call will change the type.
   */
  template<typename T, typename... Args> bool add_extension(Args &&... args);

 private:
  std::string m_name;
  Composition m_extensions;
  mutable std::mutex m_extension_mutex;
};

using SharedType = AutoRefCount<Type>;

/* Type inline functions
 ****************************************/

inline Type::Type(StringRef name) : m_name(name.to_std_string())
{
}

inline const StringRefNull Type::name() const
{
  return m_name;
}

template<typename T> inline bool Type::has_extension() const
{
  std::lock_guard<std::mutex> lock(m_extension_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<TypeExtension, T>::value), "");
  return m_extensions.has<T>();
}

template<typename T> inline T *Type::extension() const
{
  /* TODO: Check if we really need a lock here.
   *   Since extensions can't be removed, it might be
   *   to access existing extensions without a lock. */
  std::lock_guard<std::mutex> lock(m_extension_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<TypeExtension, T>::value), "");
  return m_extensions.get<T>();
}

template<typename T, typename... Args> inline bool Type::add_extension(Args &&... args)
{
  std::lock_guard<std::mutex> lock(m_extension_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<TypeExtension, T>::value), "");

  if (m_extensions.has<T>()) {
    return false;
  }
  else {
    T *new_extension = new T(std::forward<Args>(args)...);
    new_extension->set_owner(this);
    m_extensions.add(new_extension);
    return true;
  }
}

inline bool operator==(const Type &a, const Type &b)
{
  return &a == &b;
}

/* Type Extension inline functions
 ****************************************/

inline void TypeExtension::set_owner(Type *owner)
{
  m_owner = owner;
}

inline Type *TypeExtension::owner() const
{
  return m_owner;
}

} /* namespace FN */

/* Make Type hashable using std::hash.
 ****************************************/

namespace std {
template<> struct hash<FN::Type> {
  typedef FN::Type argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    return std::hash<void *>{}((void *)&v);
  }
};
}  // namespace std
