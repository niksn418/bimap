#include "intrusive_set.h"

namespace intrusive_set {
set_base::set_base() : set_base(nullptr, nullptr, nullptr) {}

set_base::set_base(set_base&& other) : set_base() {
  swap(other);
}

set_base& set_base::operator=(set_base&& other) {
  swap(other);
  other.unlink();
  return *this;
}

void set_base::unlink() {
  if (left == nullptr && right == nullptr) {
    replace(nullptr);
  } else if (left == nullptr) {
    replace(right);
    right->parent = parent;
  } else if (right == nullptr) {
    replace(left);
    left->parent = parent;
  } else {
    set_base* v = right->minimum();
    swap(*v);
    unlink();
  }
  left = right = parent = nullptr;
}

set_base::~set_base() {
  unlink();
}

void set_base::swap(set_base& other) {
  using std::swap;
  if (parent == &other) {
    swap_with_parent();
  } else if (other.parent == this) {
    other.swap_with_parent();
  } else {
    replace(&other);
    other.replace(this);
    swap(parent, other.parent);
    swap(left, other.left);
    swap(right, other.right);
    fix_children();
    other.fix_children();
  }
}

set_base* set_base::minimum() {
  set_base* cur = this;
  while (cur->left != nullptr) {
    cur = cur->left;
  }
  return cur;
}

set_base* set_base::maximum() {
  set_base* cur = this;
  while (cur->right != nullptr) {
    cur = cur->right;
  }
  return cur;
}

set_base::set_base(set_base* left, set_base* right, set_base* parent)
    : left(left), right(right), parent(parent) {}

void set_base::replace(set_base* value) {
  if (parent != nullptr) {
    if (parent->left == this) {
      parent->left = value;
    } else {
      parent->right = value;
    }
  }
}

void set_base::fix_children() {
  if (left != nullptr) {
    left->parent = this;
  }
  if (right != nullptr) {
    right->parent = this;
  }
}

void set_base::assign_children(set_base* new_left, set_base* new_right) {
  left = new_left;
  right = new_right;
}

void set_base::swap_with_parent() {
  auto old_left = left, old_right = right;
  if (parent->left == this) {
    assign_children(parent, parent->right);
  } else {
    assign_children(parent->left, parent);
  }
  parent->replace(this);
  parent->assign_children(old_left, old_right);
  parent->fix_children();
  parent = parent->parent;
  fix_children();
}
} // namespace intrusive_set
