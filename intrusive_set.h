#include <iterator>
#include <type_traits>

namespace intrusive_set {

template <typename T, typename Comp, typename KeyExtract>
struct set;

struct set_base {
  set_base();
  set_base(set_base&& other);

  set_base& operator=(set_base&& other);

  void unlink();

  ~set_base();

  void swap(set_base& other);

  set_base* minimum();

  set_base* maximum();

private:
  set_base* left;
  set_base* right;
  set_base* parent;

  set_base(set_base* left, set_base* right, set_base* parent);

  void replace(set_base* value);

  void fix_children();

  void assign_children(set_base* new_left, set_base* new_right);

  void swap_with_parent();

  template <typename T, typename Comp, typename KeyExtract>
  friend struct set;
};

template <typename Comp, typename KeyExtract>
struct set_element : public set_base {
  using set_base::set_base;
};

template <typename T, typename Comp, typename KeyExtract>
struct set : private Comp {
  set(Comp comp = Comp())
      : Comp(std::move(comp)), sentinel(nullptr, nullptr, nullptr) {}

  set(set&& other) = default;

  set& operator=(set&& other) {
    set(std::move(other)).swap(*this);
    return *this;
  }

  ~set() {
    for (auto it = begin(); it != end();) {
      it = erase(it);
    }
  }

  bool empty() const {
    return root() == nullptr;
  }

  void swap(set& other) {
    sentinel.swap(other.sentinel);
  }

  bool equal(T const& left, T const& right) const {
    key_t const &left_key = get_key(left), right_key = get_key(right);
    return !compare(left_key, right_key) && !compare(right_key, left_key);
  }

private:
  using key_t = typename KeyExtract::type;

  set_element<Comp, KeyExtract> sentinel;

  bool compare(key_t const& left, key_t const& right) const {
    return Comp::operator()(left, right);
  }

  static key_t const& get_key(T const& value) {
    return KeyExtract::get(value);
  }

  static T& get_value(set_base* element) {
    return *static_cast<T*>(
        static_cast<set_element<Comp, KeyExtract>*>(element));
  }

  static set_base* get_base(T& element) {
    return &static_cast<set_base&>(
        static_cast<set_element<Comp, KeyExtract>&>(element));
  }

  set_base* const& root() const {
    return sentinel.left;
  }

  set_base*& root() {
    return sentinel.left;
  }

  template <bool is_const>
  struct iterator_base {
    using value_type = std::conditional_t<is_const, const T, T>;
    using reference = value_type&;
    using pointer = value_type*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

    iterator_base() = default;

    iterator_base(T& value) : value(get_base(value)) {}

    template <bool t = is_const, std::enable_if_t<t, int> = 0>
    iterator_base(iterator_base<false> const& other) : value(other.value) {}

    reference operator*() const {
      return get_value(value);
    }

    pointer operator->() const {
      return &operator*();
    }

    iterator_base& operator++() {
      if (value->right != nullptr) {
        value = value->right->minimum();
      } else {
        while (value->parent->right == value) {
          value = value->parent;
        }
        value = value->parent;
      }
      return *this;
    }

    iterator_base operator++(int) {
      iterator_base old = *this;
      operator++();
      return old;
    }

    iterator_base& operator--() {
      if (value->left != nullptr) {
        value = value->left->maximum();
      } else {
        while (value->parent != nullptr && value->parent->left == value) {
          value = value->parent;
        }
        if (value->parent != nullptr) {
          value = value->parent;
        }
      }
      return *this;
    }

    iterator_base operator--(int) {
      iterator_base old = *this;
      operator--();
      return old;
    }

    friend bool operator==(iterator_base const& lhs, iterator_base const& rhs) {
      return lhs.value == rhs.value;
    }

    friend bool operator!=(iterator_base const& lhs, iterator_base const& rhs) {
      return !(lhs == rhs);
    }

  private:
    set_base* value;

    friend set;

    iterator_base(set_base* value) : value(value) {}
  };

public:
  using iterator = iterator_base<false>;
  using const_iterator = iterator_base<true>;

  iterator begin() {
    return {empty() ? end() : root()->minimum()};
  }

  const_iterator begin() const {
    return {empty() ? end() : root()->minimum()};
  }

  iterator end() {
    return {&sentinel};
  }

  const_iterator end() const {
    return {const_cast<set_element<Comp, KeyExtract>*>(&sentinel)};
  }

  iterator insert(T& element) {
    set_base** cur = &root();
    set_base* parent = &sentinel;
    key_t const& key = get_key(element);
    while (*cur != nullptr) {
      parent = *cur;
      if (compare(key, get_key(get_value(*cur)))) {
        cur = &(*cur)->left;
      } else if (compare(get_key(get_value(*cur)), key)) {
        cur = &(*cur)->right;
      } else {
        return {*cur};
      }
    }
    auto* element_base = get_base(element);
    *cur = element_base;
    element_base->parent = parent;
    return {element_base};
  }

  iterator erase(const_iterator pos) {
    iterator result(std::next(pos).value);
    pos.value->unlink();
    return result;
  }

  const_iterator find(key_t const& key) const {
    auto it = lower_bound(key);
    if (it != end() && compare(key, get_key(*it))) {
      return end();
    }
    return it;
  }

  const_iterator lower_bound(key_t const& key) const {
    set_base* cur = root();
    set_base* parent = end().value;
    while (cur != nullptr) {
      if (compare(key, get_key(get_value(cur)))) {
        parent = cur;
        cur = cur->left;
      } else if (compare(get_key(get_value(cur)), key)) {
        cur = cur->right;
      } else {
        return {cur};
      }
    }
    return {parent};
  }

  const_iterator upper_bound(key_t const& key) const {
    auto it = lower_bound(key);
    if (it != end() && !compare(key, get_key(*it))) {
      ++it;
    }
    return it;
  }
};
} // namespace intrusive_set
