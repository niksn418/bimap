#pragma once

#include <cstddef>

#include "intrusive_set.h"

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap {
  using left_t = Left;
  using right_t = Right;

private:
  size_t m_size = 0;

  struct left_comp;
  struct right_comp;
  struct left_extract;
  struct right_extract;

  struct left_tag {};
  struct right_tag {};

  struct Node : intrusive_set::set_element<left_comp, left_extract>,
                intrusive_set::set_element<right_comp, right_extract> {
    left_t left;
    right_t right;

    template <typename T, typename R>
    Node(T&& left, R&& right)
        : intrusive_set::set_element<left_comp, left_extract>(),
          intrusive_set::set_element<right_comp, right_extract>(),
          left(std::forward<T>(left)), right(std::forward<R>(right)) {}

    template <bool t = std::is_default_constructible_v<right_t>,
              std::enable_if_t<t, int> = 0>
    Node(left_tag, left_t const& left) : left(left), right() {}

    template <bool t = std::is_default_constructible_v<left_t>,
              std::enable_if_t<t, int> = 0>
    Node(right_tag, right_t const& right) : left(), right(right) {}
  };

  struct left_comp : CompareLeft {
    left_comp(CompareLeft&& comp) : CompareLeft(std::move(comp)) {}

    bool operator()(left_t const& left, left_t const& right) const {
      return CompareLeft::operator()(left, right);
    }
  };

  struct right_comp : CompareRight {
    right_comp(CompareRight&& comp) : CompareRight(std::move(comp)) {}

    bool operator()(right_t const& left, right_t const& right) const {
      return CompareRight::operator()(left, right);
    }
  };

  struct left_extract {
    using type = left_t;

    const type& operator()(const Node& value) const {
      return value.left;
    }
  };

  struct right_extract {
    using type = right_t;

    const type& operator()(const Node& value) const {
      return value.right;
    }
  };

  using left_set_t = intrusive_set::set<Node, left_comp, left_extract>;
  using right_set_t = intrusive_set::set<Node, right_comp, right_extract>;

  using left_set_iterator = typename left_set_t::const_iterator;
  using right_set_iterator = typename right_set_t::const_iterator;

  left_set_t left_set;
  right_set_t right_set;

  template <bool is_left>
  struct iterator {
    using value_type = std::conditional_t<is_left, const left_t, const right_t>;
    using reference = value_type&;
    using pointer = value_type*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

    // Элемент на который сейчас ссылается итератор.
    // Разыменование итератора end_left() неопределено.
    // Разыменование невалидного итератора неопределено.
    reference operator*() const {
      if constexpr (is_left) {
        return (*it).left;
      } else {
        return (*it).right;
      }
    }

    pointer operator->() const {
      return &operator*();
    }

    // Переход к следующему по величине left'у.
    // Инкремент итератора end_left() неопределен.
    // Инкремент невалидного итератора неопределен.
    iterator& operator++() {
      ++it;
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
      --it;
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
    iterator<!is_left> flip() const {
      if constexpr (is_left) {
        if (*this == set->end_left()) {
          return set->end_right();
        }
      } else {
        if (*this == set->end_right()) {
          return set->end_left();
        }
      }
      return {{const_cast<Node&>(*it)}, set};
    }

    friend bool operator==(const iterator& lhs, const iterator& rhs) {
      return lhs.it == rhs.it;
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) {
      return !(lhs == rhs);
    }

  private:
    using iterator_base =
        std::conditional_t<is_left, left_set_iterator, right_set_iterator>;
    iterator_base it;
    bimap const* set;

    friend bimap;

    iterator(iterator_base it, bimap const* set) : it(it), set(set) {}
  };

  template <typename Iterator>
  Iterator create_iterator(typename Iterator::iterator_base it) const {
    return {it, this};
  }

  template <typename Iterator>
  static Node const& get_node(Iterator it) {
    return *it.it;
  }

public:
  using node_t = Node;

  using left_iterator = iterator<true>;
  using right_iterator = iterator<false>;

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
    return create_iterator<left_iterator>(left_set.find(left));
  }

  right_iterator find_right(right_t const& right) const {
    return create_iterator<right_iterator>(right_set.find(right));
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
      Node* u = new Node(left_tag(), key);
      auto it1 = find_right(u->right);
      if (it1 != end_right()) {
        erase_right(it1);
      }
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
      Node* u = new Node(right_tag(), key);
      auto it1 = find_left(u->left);
      if (it1 != end_left()) {
        erase_left(it1);
      }
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
    return create_iterator<left_iterator>(left_set.lower_bound(left));
  }

  left_iterator upper_bound_left(left_t const& left) const {
    return create_iterator<left_iterator>(left_set.upper_bound(left));
  }

  right_iterator lower_bound_right(right_t const& right) const {
    return create_iterator<right_iterator>(right_set.lower_bound(right));
  }

  right_iterator upper_bound_right(right_t const& right) const {
    return create_iterator<right_iterator>(right_set.upper_bound(right));
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const {
    return create_iterator<left_iterator>(left_set.begin());
  }
  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const {
    return create_iterator<left_iterator>(left_set.end());
  }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const {
    return create_iterator<right_iterator>(right_set.begin());
  }
  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const {
    return create_iterator<right_iterator>(right_set.end());
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
      Node const& node1 = get_node(it1);
      Node const& node2 = get_node(it2);
      if (!a.left_set.equal(node1, node2) || !a.right_set.equal(node1, node2)) {
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
    left_set.swap(other.left_set);
    right_set.swap(other.right_set);
  }

private:
  template <typename T, typename R>
  left_iterator generic_insert(T&& left, R&& right) {
    Node* u = new Node(std::forward<T>(left), std::forward<R>(right));
    return insert_node(u);
  }

  template <typename Iterator>
  Iterator erase(Iterator it) {
    auto result = std::next(it);
    delete &get_node(it);
    --m_size;
    return result;
  }

  left_iterator insert_node(Node* u) {
    auto left_it = left_set.insert(*u);
    if (&(*left_it) != u) {
      delete u;
      return end_left();
    }
    auto right_it = right_set.insert(*u);
    if (&(*right_it) != u) {
      delete u;
      return end_left();
    }
    ++m_size;
    return create_iterator<left_iterator>(left_it);
  }
};
