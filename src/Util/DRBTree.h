#ifndef MED_UTIL_DRBTREE_H
#define MED_UTIL_DRBTREE_H

#include <exception>
#include <utility>
#include <stdexcept>

namespace Med {
namespace Util {

class DRBTreeDefs {
public:
  enum class Side { LEFT, RIGHT };
  enum class NodeColor { BLACK, RED };

  static Side other(Side side) {
    return side == Side::LEFT ? Side::RIGHT : Side::LEFT;
  }

  struct OperationOptions {
    /** Which side's accumulated deltas are taken to represent the key of a node.
      *
      * Another way of seeing it is that nodes on this side are taken to have a smaller key, while nodes on the other side have a bigger key.
      */
    Side deltaSide = Side::LEFT;

    /** When multiple nodes with the same key exist, which end of the same-key segment to operate on.
      *
      * When getting a node, the node on that end of the segment will be returned.
      *
      * When attaching a node, the node will become that end of the segment. If null, and there already is a node with that key, an error will be raised.
      */
    bool repeats = false;
    Side repeatedSide = Side::LEFT;

    /** (Only meaningful when getting) If set, when getting a node with a key that doesn't exist in the tree, which adjacent existing node to return instead.
      */
    bool equalOrAdjacent = false;
    Side equalOrAdjacentSide = Side::LEFT;
  };

  class Error : public std::logic_error {
    using std::logic_error::logic_error;
  };
};

/** A sorted map from "numeric" keys to arbitrary values, where some operations that affect many keys are efficient.
 *
 * The main such operation is increasing the keys of all elements from a given element to the end of the tree by the same amount. This operation is on average and at worst O(log N).
 *
 * Search, insertion and deletion are also O(log N) on average and at worst.
 *
 * This map uses the concept of "delta". An element's delta is the difference between its key and the previous node's key.
 *
 * Keys are not stored directly, but computed from deltas.
 */
template<typename Key_, typename Delta_, typename Value_>
class DRBTree : public DRBTreeDefs {
public:
  typedef Key_ Key;
  typedef Delta_ Delta;
  typedef Value_ Value;

  static constexpr Key zeroKey{};
  static constexpr Delta zeroDelta{};

  class Node;

  struct ConstEntry {
    Key key;
    const Node* node;
  };

  struct Entry {
    Key key;
    Node* node;
  };

  DRBTree() {}
  ~DRBTree() {}

  /** Returns true iff the tree has no elements. */
  bool empty() const { return root == nullptr; }

  Delta totalDelta() const { return leftmostExtremeDelta + childrenDelta() + rightmostExtremeDelta; }

  template<typename EntryType_>
  class IteratorTemplate {
  public:
    typedef EntryType_ EntryType;

    IteratorTemplate() : entry{zeroKey, nullptr} {}

    bool isValid() const { return entry.node; }

    bool operator==(const IteratorTemplate& other) const { return entry.node == other.entry.node; }
    bool operator!=(const IteratorTemplate& other) const { return !operator==(other); }

    void operator++() {
      entry.key += entry.node->delta;
      entry.node = entry.node->adjacent(Side::RIGHT);
    }

    void operator--() {
      entry.node = entry.node->adjacent(Side::LEFT);
      if (entry.node) entry.key -= entry.node->delta;
    }

    const EntryType& operator*() const { return entry; }
    const EntryType* operator->() const { return &entry; }

  private:
    friend class DRBTree;
    explicit IteratorTemplate(const EntryType entry) : entry(entry) {}

    EntryType entry;
  };

  typedef IteratorTemplate<ConstEntry> ConstIterator;
  typedef IteratorTemplate<Entry> Iterator;

  ConstIterator extreme(Side side, OperationOptions options) const { return extremeInternal<Iterator>(side, options); }

  Iterator extreme(Side side, OperationOptions options) { return extremeInternal<Iterator>(side, options); }

  ConstIterator begin() const { return beginInternal<ConstIterator>(); }
  ConstIterator end() const { return endInternal<ConstIterator>(); }

  Iterator begin() { return beginInternal<Iterator>(); }
  Iterator end() { return endInternal<Iterator>(); }

  /** Gets a node from the tree. Also returns its key, which might be different from the supplied key if equalOrAdjacentSide is set. */
  Iterator get(const Key& key, const OperationOptions& options) {
    if (root == nullptr) return Iterator({zeroKey, nullptr});
    Node* parent = nullptr;
    Node* current = root;
    // The predecessor of leftmost element of the subtree "current" is the root of.
    Node* predecessorOfSubtree = nullptr;
    // Key at the leftmost element of the subtree "current" is the root of.
    Key keyAtSubtree = zeroKey + extremeDelta(options.deltaSide);
    Key foundKey = zeroKey;
    Side dir = Side::LEFT;
    Node* found = nullptr;
    while (current != nullptr) {
      const Key keyAtNode = keyAtSubtree + current->children.subtreeDelta(options.deltaSide);
      if (key == keyAtNode) {
        found = current;
        foundKey = keyAtNode;
        dir = options.repeatedSide;
      } else
        dir = key < keyAtNode ? options.deltaSide : other(options.deltaSide);

      if (dir != options.deltaSide) {
        predecessorOfSubtree = current;
        keyAtSubtree = keyAtNode + current->delta;
      }

      parent = current;
      current = current->children.get(dir);

      if (current == nullptr && found == nullptr && options.equalOrAdjacent) {
        foundKey = keyAtNode;
        found = parent;
        // TODO: this code is a bit horrible, should be simplified.
        if (dir == options.equalOrAdjacentSide) {
          // parent is on the opposite side
          Key newFoundKey = zeroKey;
          if (options.deltaSide != options.equalOrAdjacentSide) newFoundKey = foundKey + found->delta;
          found = found->adjacent(options.equalOrAdjacentSide);
          bool change = true;
          if (options.deltaSide == options.equalOrAdjacentSide) {
            change = found != nullptr;
            if (change) newFoundKey = foundKey - found->delta;
          }
          if (change) foundKey = newFoundKey;
        }
      }
    }
    return Iterator({foundKey, found});
  }

  /** Attaches a new node to the tree. */
  Iterator attach(Node* node, const Key& key, const OperationOptions& options) {
    if (node->isAttached()) throw new Error("The node is already attached.");
    node->color = NodeColor::RED;
    /** Both the key and the predecessor of a subtree are those of its leftmost element. */
    struct Subtree { Key key; Node* predecessor; };
    Subtree subtree{zeroKey + extremeDelta(options.deltaSide), nullptr};
    if (root == nullptr) {
      // Empty tree insertion
      root = node;
    } else {
      Node* grandparent = nullptr;
      Subtree grandparentSubtree;
      Node* parent = nullptr;
      Subtree parentSubtree;
      Node* current = root;
      bool hasLast = false;
      Side last;
      Side dir = Side::LEFT;
      while (current != node) {
        if (current == nullptr) {
          // Insert at leaf
          current = node;
          parent->children.get(dir) = node;
          node->parent = parent;
        } else if (Node::isRed(current->children.get(Side::LEFT)) &&
            Node::isRed(current->children.get(Side::RIGHT))) {
          current->color = NodeColor::RED;
          current->children.get(Side::LEFT)->color = NodeColor::BLACK;
          current->children.get(Side::RIGHT)->color = NodeColor::BLACK;
        }

        if (Node::isRed(current) && Node::isRed(parent)) {
          if (hasLast && dir == last) {
            rotateSingle(grandparent, other(last));
          } else {
            rotateDouble(grandparent, other(last));
            // After the rotation grandparent and parent end up as children of current, so we must update their  subtree deltas unless current is node (in that case it will have been automatically corrected by the rotation).
            if (current != node) {
              subtree = grandparentSubtree;
              // parent and grandparent subtree information has become invalid, but it shouldn't be needed any more.
              grandparentSubtree = {zeroKey, nullptr};
              parentSubtree = {zeroKey, nullptr};
            }
          }
        }

        if (current != node) {
          hasLast = true;
          last = dir;

          const Key keyAtNode = subtree.key + current->children.subtreeDelta(options.deltaSide);
          dir = keyAtNode < key ? other(options.deltaSide) : options.deltaSide;
          if (keyAtNode == key && current->children.get(dir) != nullptr) {
            if (!options.repeats) {
              throw Error("Trying to insert node with repeated key '" + std::to_string(key) + "'.");
            }
            dir = options.repeatedSide;
          }

          grandparent = parent;
          grandparentSubtree = parentSubtree;
          parent = current;
          parentSubtree = subtree;
          current = current->children.get(dir);
          if (dir != options.deltaSide) {
            // We've moved to a subtree with a different (bigger) leftmost element.
            subtree = Subtree{keyAtNode + parent->delta, parent};
          }
        }
      }
    }

    node->tree = this;
    root->color = NodeColor::BLACK;

    // Set the node's delta.
    if (subtree.predecessor) {
      // At the leaf, subtree
      if (key > subtree.key) {
        // This means we are at the biggest end of the tree and we're increasing the tree's total delta.
        node->setDelta(zeroDelta);
        subtree.predecessor->setDelta(key - subtree.key);
      } else {
        // We have a successor. We split the predecessor's previous delta between the new node and the predecessor, so the new node's key is as requested and the successor's key doesn't change.
        node->setDelta(subtree.key - key);
        subtree.predecessor->setDelta(subtree.predecessor->delta - node->delta);
      }
    } else {
      // Inserting at the tree's leftmost end.
      if (root == node)
        // First node in the tree.
        node->setDelta(zeroDelta);
      else
        node->setDelta(zeroKey + extremeDelta(options.deltaSide) - key);
      extremeDelta(options.deltaSide) = key - zeroKey;
    }
    return Iterator({key, node});
  }

private:
  friend class DRBTreeTest;

  Delta childrenDelta() const { return empty() ? zeroDelta : root->subtreeDelta; }

  template<typename IteratorType>
  IteratorType extremeInternal(Side side, OperationOptions options) {
    typename IteratorType::EntryType entry{zeroKey, nullptr};
    if (!empty()) {
      entry.key += extremeDelta(options.deltaSide);
      if (side != options.deltaSide) entry.key += childrenDelta() + extremeDelta(side);
      entry.node = root->descendantAtEnd(side);
    }
    return IteratorType(entry);
  }

  template<typename IteratorType>
  IteratorType beginInternal() { return extremeInternal<IteratorType>(Side::LEFT, {}); }

  template<typename IteratorType>
  IteratorType endInternal() { return IteratorType({zeroKey, nullptr}); }

  Delta& extremeDelta(Side side) {
    return side == Side::LEFT ? leftmostExtremeDelta : rightmostExtremeDelta;
  }

  class InternalError : public std::logic_error { using std::logic_error::logic_error; };

  class Children {
  public:
    Side sideWithNode(Node* node) {
      if (leftChild == node) return Side::LEFT;
      else if (rightChild == node) return Side::RIGHT;
      else throw InternalError("No side with the requested node!");
    }

    Node*& get(Side side) { return getInternal(this, side); }
    const Node* get(Side side) const { return getInternal(this, side); }

    Delta subtreeDelta(Side side) {
      Node* child = get(side);
      return child != nullptr ? child->subtreeDelta : zeroDelta;
    }

    Delta totalSubtreeDeltas() { return subtreeDelta(Side::LEFT) + subtreeDelta(Side::RIGHT); }

    /** If the child exists, update its subtree delta. */
    void updateSubtreeDelta(Side side) {
      Node* child = get(side);
      if (child != nullptr) child->updateSubtreeDelta();
    }

    /** Requires that this node has at most one child; returns that child, or null if the node has no children.
     */
    Node* onlyChild() {
      if (leftChild != nullptr && rightChild != nullptr)
        throw InternalError("Node has both children!");
      return leftChild != nullptr ? leftChild : rightChild;
    }

    template<typename NodeType>
    class IteratorTemplate {
    public:
      IteratorTemplate() : finished(true) {}
      IteratorTemplate(const Children* children) : children(children) {
        // If the first child doesn't exist, advance to the first child that does.
        if (operator*() == nullptr) operator++();
      }

      bool operator==(const IteratorTemplate& other) const { return finished == other.finished; }
      bool operator!=(const IteratorTemplate& other) const { return !operator==(other); }

      NodeType* operator*() const {
        if (finished) throw InternalError("Dereferencing finished iterator!");
        return children->get(side);
      }

      void operator++() {
        if (finished) throw InternalError("Advancing finished iterator!");
        if (side == Side::LEFT) {
          side = Side::RIGHT;
          if (operator*() != nullptr) return;
        }
        finished = true;
      }

    private:
      const Children* children = nullptr;
      bool finished = false;
      Side side = Side::LEFT;
    };

    typedef IteratorTemplate<Node> Iterator;
    typedef IteratorTemplate<const Node> ConstIterator;

    Iterator begin() { return Iterator(this); }
    Iterator end() { return Iterator(); }

    ConstIterator begin() const { return ConstIterator(this); }
    ConstIterator end() const { return ConstIterator(); }

  private:
    template<typename ChildrenType>
    static auto& getInternal(ChildrenType* children, Side side) {
      return side == Side::LEFT ? children->leftChild : children->rightChild;
    }

    Node* leftChild = nullptr;
    Node* rightChild = nullptr;
  };

  void rotateSingle(Node* oldRoot, Side dir) {
    Node* const newRoot = oldRoot->children.get(other(dir));
    Node* const top = oldRoot->parent;
    Node* const child = newRoot->children.get(dir);
    oldRoot->children.get(other(dir)) = child;
    if (child != nullptr) child->parent = oldRoot;
    if (top == nullptr) root = newRoot;
    else top->children.get(oldRoot->parentSide()) = newRoot;
    newRoot->parent = top;

    newRoot->children.get(dir) = oldRoot;
    oldRoot->parent = newRoot;

    oldRoot->color = NodeColor::RED;
    newRoot->color = NodeColor::BLACK;

    // Note how these two operations don't affect each other.
    oldRoot->children.updateSubtreeDelta(other(dir));
    newRoot->children.updateSubtreeDelta(dir);
  }

  void rotateDouble(Node* oldRoot, Side side) {
    rotateSingle(oldRoot->children.get(other(side)), other(side));
    rotateSingle(oldRoot, side);
  }

  // Detach @detached from the tree and move @moved from its current location to detached's location.
  // If @moved and @detached are the same node, it is simply detached from the tree.
  void moveAndDetach(Node* moved, Node* detached) {
    Node* const child = moved->children.onlyChild();
    // Point the child to the parent.
    if (child != nullptr) child->parent = moved->parent;
    // Point the parent to the child.
    if (moved->parent == nullptr)
      root = child;
    else {
      moved->parent->children.get(moved->parentSide()) = child;
      moved->parent->updateSubtreeDelta();
    }
    if (moved != detached) {
      moved->color = detached->color;
      moved->parent = detached->parent;
      if (moved->parent == nullptr)
        root = moved;
      else {
        moved->parent->children.get(detached->parentSide()) = moved;
        moved->parent->updateSubtreeDelta();
      }
      moved->children = detached->children;
      if (moved->children.get(Side::LEFT)) moved->children.get(Side::LEFT)->parent = moved;
      if (moved->children.get(Side::RIGHT)) moved->children.get(Side::RIGHT)->parent = moved;
      moved->updateSubtreeDelta();
    }

    detached->parent = nullptr;
    detached->children.get(Side::LEFT) = nullptr;
    detached->children.get(Side::RIGHT) = nullptr;
  }

  void detach(Node* node) {
    Node* predecessor = node->adjacent(Side::LEFT);
    if (predecessor) {
      predecessor->delta += node->delta;
      predecessor->updateSubtreeDelta();
    } else {
      extremeDelta(Side::LEFT) += node->delta;
    }
    Node* current = nullptr;
    if (node->children.get(Side::LEFT) != nullptr &&
        node->children.get(Side::RIGHT) != nullptr) {
      // If the node has both children, get the predecessor,
      // which will be the rightmost leaf of the left child.
      current = node->adjacent(Side::LEFT);
    } else if (node == root) {
      // We are going to delete root and it only has one child,
      // so make that child the new root.
      root = node->children.onlyChild();
      node->children.get(Side::LEFT) = nullptr;
      node->children.get(Side::RIGHT) = nullptr;
      if (root != nullptr) {
        root->color = NodeColor::BLACK;
        root->parent = nullptr;
      }
      return;
    } else {
      // The node has at most one child.
      current = node;
    }

    NodeColor savedColor = current->color;
    Node* const parent = current->parent == node ? current : current->parent;

    Side side = current->parentSide();

    // If we got the node's predecessor, move it to the node's position on the tree.
    // In any case, current's only child is attached to current's parent at side.
    moveAndDetach(current, node);

    if (savedColor == NodeColor::RED) return;
    current = parent;
    if (Node::isRed(current->children.get(side))) {
      current->children.get(side)->color = NodeColor::BLACK;
    } else {
      while (true) {
        Node* sibling = current->children.get(other(side));

        // Case reduction, remove red sibling.
        if (Node::isRed(sibling)) {
          rotateSingle(current, side);
          sibling = current->children.get(other(side));
        }

        if (sibling != nullptr) {
          if (!Node::isRed(sibling->children.get(Side::LEFT)) &&
              !Node::isRed(sibling->children.get(Side::RIGHT))) {
            bool done = Node::isRed(current);
            current->color = NodeColor::BLACK;
            sibling->color = NodeColor::RED;
            if (done) break;
          } else {
            savedColor = current->color;

            if (Node::isRed(sibling->children.get(other(side))))
              rotateSingle(current, side);
            else
              rotateDouble(current, side);
            current = current->parent;

            current->color = savedColor;
            current->children.get(Side::LEFT)->color = NodeColor::BLACK;
            current->children.get(Side::RIGHT)->color = NodeColor::BLACK;
            break;
          }
        }

        if (current->parent == nullptr) {
          root = current;
          break;
        } else {
          side = current->parentSide();
          current = current->parent;
        }
      }
    }
  }

  // The tree's root. It's null iff the tree is empty.
  Node* root = nullptr;

  // Delta from the logical left end of the tree to the leftmost actual element. This allows the leftmost element to have a non-zero left-side key. Undefined if the tree is empty.
  Delta leftmostExtremeDelta = zeroDelta;
  // Delta from the logical right end of the tree to the rightmost actual element. This allows the rightmost element to have a non-zero right-side key. Undefined if the tree is empty.
  Delta rightmostExtremeDelta = zeroDelta;
};

template<typename Key, typename Delta, typename Value>
class DRBTree<Key, Delta, Value>::Node {
public:
  Node() : value() {}

  template<typename InitValue>
  explicit Node(InitValue initValue) : value(initValue) {}

  template<typename InitValue>
  explicit Node(const std::initializer_list<InitValue>& initValue) : value(initValue) {}

  typedef DRBTree<Key, Delta, Value> Tree;

  Value value;

  Tree* tree = nullptr;

  /** The node's delta; can be set arbitrarily (but then the node's antecessors' subtree deltas must be updated). */
  Delta delta = zeroDelta;

  /** The node's delta, plus the subtree deltas of its children (if any). */
  Delta subtreeDelta = zeroDelta;

  /** The node's color; see a Red/Black tree explanation for the definition. */
  NodeColor color = NodeColor::RED;

  /** The node's children object, which encapsulates accesses to the children nodes. */
  Children children;

  /** The node's parent; null if this is the tree's root or the node is detached from the tree. */
  Node* parent = nullptr;

  /** Whether this node is currently attached to the tree. */
  bool isAttached() { return tree != nullptr; }

  /** On which side of its parent this node is. Must not be called if the node has no parent. */
  Side parentSide() { return parent->children.sideWithNode(this); }

  Delta nodePlusSubtreeDelta(Side side) { return delta + children.subtreeDelta(side); }

  /** Set the delta for this node. Recomputes subtree delta. */
  void setDelta(Delta newDelta) {
    delta = newDelta;
    updateSubtreeDelta();
  }

  /** Recompute subtreeDelta from the node's delta and the children's subtree deltas, for this node and all its ancestors, until an ancestor with an unchanged subtree delta is found. */
  void updateSubtreeDelta() {
    for (Node* node = this; node != nullptr; node = node->parent) {
      const Delta newSubtreeDelta = node->delta + node->children.totalSubtreeDeltas();
      if (node->subtreeDelta == newSubtreeDelta) break;
      node->subtreeDelta = newSubtreeDelta;
    }
  }

  /** Detach the node form the tree. */
  void detach() {
    if (!isAttached()) throw Error("The node is not attached.");
    tree->detach(this);
  }

  /** Returns this node's key. */
  Key key(Side side) {
    Delta key = children.subtreeDelta(side);
    for (Node* node = this; node->parent != nullptr; node = node->parent) {
      if (node->parentSide() != side) key += node->nodePlusSubtreeDelta(side);
    }
    return zeroKey + key;
  }

  /** Returns the node's key delta, i.e. the difference between this node's key and the next node's key. */
  Delta getDelta() { return delta; }

  /** Returns the descendant of this node at the given end of the key range. */
  Node* descendantAtEnd(Side side) {
    Node* child = nullptr;
    Node* node;
    for (node = this; (child = node->children.get(side)) != nullptr; node = child);
    return node;
  }

  /** Returns the node that is adjacent to this one on the given side. If none, returns null. */
  Node* adjacent(Side side) {
    Node* child = children.get(side);
    // Adjacent is among the descendants.
    if (child != nullptr) return child->descendantAtEnd(other(side));
    // Adjacent is among the ancestors.
    Node* node;
    for (node = this; node->parent != nullptr && node->parentSide() == side; node = node->parent);
    return node->parent;
  }

#if 0
  def subtreeToString(subtreeKey: Key = zeroKey, indentString: String = "", moreSiblings: Boolean = false, fromParent:
Node = null): String = {
    val childrenIndentString = indentString + (if (moreSiblings) "| " else "  ")
    val key = subtreeKey + children.subtreeDelta(Left)
    indentString + (if (moreSiblings) "+-" else "\\-") +
    value.toString +
    "key = " + key + ", delta = " + delta + ", subtreeDelta = " + subtreeDelta + "\n" +
    (if (fromParent != null && fromParent != parent)
      "<bad parent>\n"
      else
      (if (children.leftChild != null)
          children.leftChild.subtreeToString(subtreeKey, childrenIndentString, children.rightChild != null, this)
        else
          "") +
      (if (children.rightChild != null)
          children.rightChild.subtreeToString(key + delta, childrenIndentString, false, this)
        else
          ""))
  }

  override def toString = subtreeToString()
#endif

  static bool isRed(Node* node) { return node != nullptr && node->color == NodeColor::RED; }
};

}  // namespace Util
}  // namespace Med

#endif // MED_UTIL_DRBTREE_H
