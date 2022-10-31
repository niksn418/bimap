#pragma once

#include <cstddef>

#include "intrusive_set.h"

namespace bimap_details {
inline constexpr struct left_tag_t {
} left_tag{};
inline constexpr struct right_tag_t {
} right_tag{};

template <typename Left, typename Right>
struct left_extract;

template <typename Left, typename Right>
struct right_extract;

template <typename Left, typename Right>
struct Node : intrusive_set::set_element<left_extract<Left, Right>>,
              intrusive_set::set_element<right_extract<Left, Right>> {
  Left left;
  Right right;

  template <typename T, typename R>
  Node(T&& left, R&& right)
      : left(std::forward<T>(left)), right(std::forward<R>(right)) {}

  template <bool t = std::is_default_constructible_v<Right>,
            std::enable_if_t<t, int> = 0>
  Node(left_tag_t, Left const& left) : left(left), right() {}

  template <bool t = std::is_default_constructible_v<Left>,
            std::enable_if_t<t, int> = 0>
  Node(right_tag_t, Right const& right) : left(), right(right) {}
};

template <typename Left, typename Right>
struct left_extract {
  using type = Left;

  static const type& get(const Node<Left, Right>& value) {
    return value.left;
  }
};

template <typename Left, typename Right>
struct right_extract {
  using type = Right;

  static const type& get(const Node<Left, Right>& value) {
    return value.right;
  }
};

template <typename Left, typename Right, typename Comp>
using left_set =
    intrusive_set::set<Node<Left, Right>, Comp, left_extract<Left, Right>>;

template <typename Left, typename Right, typename Comp>
using right_set =
    intrusive_set::set<Node<Left, Right>, Comp, right_extract<Left, Right>>;
} // namespace bimap_details

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap : private bimap_details::left_set<Left, Right, CompareLeft>,
               private bimap_details::right_set<Left, Right, CompareRight> {
  using left_t = Left;
  using right_t = Right;
  using node_t = bimap_details::Node<Left, Right>;

private:
  size_t m_size = 0;

  using left_set = bimap_details::left_set<Left, Right, CompareLeft>;
  using right_set = bimap_details::right_set<Left, Right, CompareRight>;

  using left_set_iterator = typename left_set::const_iterator;
  using right_set_iterator = typename right_set::const_iterator;

  using left_extract = bimap_details::left_extract<Left, Right>;
  using right_extract = bimap_details::right_extract<Left, Right>;

  template <typename Value, typename Extract, typename BaseIterator>
  struct iterator : protected BaseIterator {
    using value_type = const Value;
    using reference = value_type&;
    using pointer = value_type*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

    // Элемент на который сейчас ссылается итератор.
    // Разыменование итератора end_left() неопределено.
    // Разыменование невалидного итератора неопределено.
    reference operator*() const {
      return Extract::get(get_node(*this));
    }

    pointer operator->() const {
      return &operator*();
    }

    // Переход к следующему по величине left'у.
    // Инкремент итератора end_left() неопределен.
    // Инкремент невалидного итератора неопределен.
    iterator& operator++() {
      iterator_base::operator++();
      return *this;
    }

    iterator operator++(int) {
      iterator old = *this;
      operator++();
      return old;
    }

    // Переход к предыдущему по величине left'у.
    // Декремент итератора begin_left() неопределен.
    // Декремент невалидного итератора неопределен.
    iterator& operator--() {
      iterator_base::operator--();
      return *this;
    }

    iterator operator--(int) {
      iterator old = *this;
      operator--();
      return old;
    }

    // left_iterator ссылается на левый элемент некоторой пары.
    // Эта функция возвращает итератор на правый элемент той же пары.
    // end_left().flip() возращает end_right().
    // end_right().flip() возвращает end_left().
    // flip() невалидного итератора неопределен.
    auto flip() const {
      return flip_iterator(*this);
    }

    friend bool operator==(const iterator& lhs, const iterator& rhs) {
      return static_cast<const iterator_base&>(lhs) ==
             static_cast<const iterator_base&>(rhs);
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) {
      return !(lhs == rhs);
    }

  private:
    using iterator_base = BaseIterator;

    friend bimap;

    iterator(iterator_base it) : iterator_base(it) {}

    auto get_pointer() {
      return iterator_base::get_pointer();
    }

    iterator(intrusive_set::set_element<Extract>* ptr) : iterator_base({ptr}) {}
  };

  template <typename Iterator>
  static node_t const& get_node(Iterator it) {
    return it.iterator_base::operator*();
  }

public:
  using left_iterator = iterator<left_t, left_extract, left_set_iterator>;
  using right_iterator = iterator<right_t, right_extract, right_set_iterator>;

  // Создает bimap не содержащий ни одной пары.
  bimap(CompareLeft compare_left = CompareLeft(),
        CompareRight compare_right = CompareRight())
      : left_set(std::move(compare_left)), right_set(std::move(compare_right)) {
  }

  // Конструкторы от других и присваивания
  bimap(bimap const& other) : bimap() {
    for (auto it = other.begin_left(); it != other.end_left(); ++it) {
      insert(*it, *it.flip());
    }
  }

  bimap(bimap&&) noexcept = default;

  bimap& operator=(bimap const& other) {
    if (this != &other) {
      bimap(other).swap(*this);
    }
    return *this;
  }

  bimap& operator=(bimap&& other) noexcept {
    if (this != &other) {
      bimap(std::move(other)).swap(*this);
    }
    return *this;
  }

  // Деструктор. Вызывается при удалении объектов bimap.
  // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
  // (включая итераторы ссылающиеся на элементы следующие за последними).
  ~bimap() {
    erase_left(begin_left(), end_left());
  }

  // Вставка пары (left, right), возвращает итератор на left.
  // Если такой left или такой right уже присутствуют в bimap, вставка не
  // производится и возвращается end_left().
  left_iterator insert(left_t const& left, right_t const& right) {
    return generic_insert(left, right);
  }

  left_iterator insert(left_t const& left, right_t&& right) {
    return generic_insert(left, std::move(right));
  }

  left_iterator insert(left_t&& left, right_t const& right) {
    return generic_insert(std::move(left), right);
  }

  left_iterator insert(left_t&& left, right_t&& right) {
    return generic_insert(std::move(left), std::move(right));
  }

  // Удаляет элемент и соответствующий ему парный.
  // erase невалидного итератора неопределен.
  // erase(end_left()) и erase(end_right()) неопределены.
  // Пусть it ссылается на некоторый элемент e.
  // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
  left_iterator erase_left(left_iterator it) {
    return erase(it);
  }

  // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе
  // не делает ничего Возвращает была ли пара удалена
  bool erase_left(left_t const& left) {
    auto it = find_left(left);
    if (it == end_left()) {
      return false;
    }
    erase_left(it);
    return true;
  }

  right_iterator erase_right(right_iterator it) {
    return erase(it);
  }

  bool erase_right(right_t const& right) {
    auto it = find_right(right);
    if (it == end_right()) {
      return false;
    }
    erase_right(it);
    return true;
  }

  // erase от ренжа, удаляет [first, last), возвращает итератор на последний
  // элемент за удаленной последовательностью
  left_iterator erase_left(left_iterator first, left_iterator last) {
    while (first != last) {
      first = erase_left(first);
    }
    return last;
  }

  right_iterator erase_right(right_iterator first, right_iterator last) {
    while (first != last) {
      first = erase_right(first);
    }
    return last;
  }

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(left_t const& left) const {
    return {left_set::find(left)};
  }

  right_iterator find_right(right_t const& right) const {
    return {right_set::find(right)};
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует -- бросает std::out_of_range
  right_t const& at_left(left_t const& key) const {
    auto it = find_left(key);
    if (it == end_left()) {
      throw std::out_of_range("bimap::at_left: no element found");
    }
    return *it.flip();
  }

  left_t const& at_right(right_t const& key) const {
    auto it = find_right(key);
    if (it == end_right()) {
      throw std::out_of_range("bimap::at_right: no element found");
    }
    return *it.flip();
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый (смотри тесты)
  template <bool t = std::is_default_constructible_v<right_t>,
            std::enable_if_t<t, int> = 0>
  right_t const& at_left_or_default(left_t const& key) {
    auto it = find_left(key);
    if (it == end_left()) {
      auto* u = new node_t(bimap_details::left_tag, key);
      erase_right(u->right);
      insert_node(u);
      return u->right;
    } else {
      return *it.flip();
    }
  }

  template <bool t = std::is_default_constructible_v<left_t>,
            std::enable_if_t<t, int> = 0>
  left_t const& at_right_or_default(right_t const& key) {
    auto it = find_right(key);
    if (it == end_right()) {
      auto* u = new node_t(bimap_details::right_tag, key);
      erase_left(u->left);
      insert_node(u);
      return u->left;
    } else {
      return *it.flip();
    }
  }

  // lower и upper bound'ы по каждой стороне
  // Возвращают итераторы на соответствующие элементы
  // Смотри std::lower_bound, std::upper_bound.
  left_iterator lower_bound_left(left_t const& left) const {
    return {left_set::lower_bound(left)};
  }

  left_iterator upper_bound_left(left_t const& left) const {
    return {left_set::upper_bound(left)};
  }

  right_iterator lower_bound_right(right_t const& right) const {
    return {right_set::lower_bound(right)};
  }

  right_iterator upper_bound_right(right_t const& right) const {
    return {right_set::upper_bound(right)};
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const {
    return {left_set::begin()};
  }
  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const {
    return {left_set::end()};
  }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const {
    return {right_set::begin()};
  }
  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const {
    return {right_set::end()};
  }

  // Проверка на пустоту
  bool empty() const {
    return size() == 0;
  }

  // Возвращает размер бимапы (кол-во пар)
  std::size_t size() const {
    return m_size;
  }
  // операторы сравнения
  friend bool operator==(bimap const& a, bimap const& b) {
    if (a.size() != b.size()) {
      return false;
    }
    for (auto it1 = a.begin_left(), it2 = b.begin_left();
         it1 != a.end_left(), it2 != b.end_left(); ++it1, ++it2) {
      node_t const& node1 = get_node(it1);
      node_t const& node2 = get_node(it2);
      if (!a.left_set::equal(node1, node2) ||
          !a.right_set::equal(node1, node2)) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(bimap const& a, bimap const& b) {
    return !(a == b);
  }

  void swap(bimap& other) noexcept {
    std::swap(m_size, other.m_size);
    left_set::swap(other);
    right_set::swap(other);
  }

private:
  template <typename T, typename R>
  left_iterator generic_insert(T&& left, R&& right) {
    auto* u = new node_t(std::forward<T>(left), std::forward<R>(right));
    return insert_node(u);
  }

  template <typename Iterator>
  Iterator erase(Iterator it) {
    auto result = std::next(it);
    delete &get_node(it);
    --m_size;
    return result;
  }

  left_iterator insert_node(node_t* u) {
    auto left_it = left_set::insert(*u);
    if (&(*left_it) != u) {
      delete u;
      return end_left();
    }
    auto right_it = right_set::insert(*u);
    if (&(*right_it) != u) {
      delete u;
      return end_left();
    }
    ++m_size;
    return {left_it};
  }

  static right_iterator flip_iterator(left_iterator it) {
    auto* ptr = it.get_pointer();
    if (ptr->is_root()) {
      return {static_cast<right_set*>(
          static_cast<bimap*>(static_cast<left_set*>(ptr)))};
    }
    return {static_cast<node_t*>(it.get_pointer())};
  }

  static left_iterator flip_iterator(right_iterator it) {
    auto* ptr = it.get_pointer();
    if (ptr->is_root()) {
      return {static_cast<left_set*>(
          static_cast<bimap*>(static_cast<right_set*>(ptr)))};
    }
    return {static_cast<node_t*>(it.get_pointer())};
  }
};
