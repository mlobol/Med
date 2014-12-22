#include "drbtree.h"

#include <list>
#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace med {
namespace editor {

class DRBTreeTest : public ::testing::Test {
protected:

  template<typename Tree>
  void checkInvariants(const Tree& tree) {
    class InvariantChecker {
    public:
      /** Checks the tree structure. */
      void checkTreeStructure(const typename Tree::Node* node) {
        for (const typename Tree::Node* child : node->children) {
          EXPECT_EQ(node, child->parent);
          checkTreeStructure(child);
        }
      }
      
      /** Checks that the children of all red descendants of this node are black. */
      void checkChildrenColor(const typename Tree::Node* node) {
        for (const typename Tree::Node* child : node->children) {
          if (node->color == DRBTreeDefs::NodeColor::RED)
            EXPECT_EQ(DRBTreeDefs::NodeColor::BLACK, child->color);
          checkChildrenColor(child);
        }
      }
      
      /** Checks that the number of black nodes from this node to every one of its descendant leaves is the same. Returns that number, plus 1 if the node is black. */
      int checkBlacksToLeaf(const typename Tree::Node* node) {
        if (node == nullptr) return 0;
        std::set<int> childrenBlacksToLeaf;
        for (DRBTreeDefs::Side side : {DRBTreeDefs::Side::LEFT, DRBTreeDefs::Side::RIGHT})
          childrenBlacksToLeaf.insert(checkBlacksToLeaf(node->children.get(side)));
        EXPECT_EQ(1, childrenBlacksToLeaf.size());
        if (childrenBlacksToLeaf.empty()) return 0;
        return *childrenBlacksToLeaf.begin() + (node->color == DRBTreeDefs::NodeColor::BLACK ? 1 : 0);
      }
      
      /** Checks that the subtree deltas of this node and all its descendants are correct. */ 
      void checkSubtreeDeltas(const typename Tree::Node* node) {
        typename Tree::Delta childrenSubtreeDeltas = 0;
        for (const typename Tree::Node* child : node->children)
          childrenSubtreeDeltas += child->subtreeDelta;
        EXPECT_EQ(childrenSubtreeDeltas + node->delta, node->subtreeDelta);
        for (const typename Tree::Node* child : node->children)
          checkSubtreeDeltas(child);
      }
    };
    
    InvariantChecker checker;
    if (tree.root != nullptr) {
      // The root must be black.
      EXPECT_EQ(DRBTreeDefs::NodeColor::BLACK, tree.root->color);
      checker.checkTreeStructure(tree.root);
      checker.checkChildrenColor(tree.root);
      checker.checkBlacksToLeaf(tree.root);
      checker.checkSubtreeDeltas(tree.root);
    }
  }

  
  template<typename Keys>
  void buildAndTestTree(const Keys& keys) {
    typedef typename Keys::value_type Key;
    struct Value {
      Key keyAtInsertion;
    };
    typedef DRBTree<Key, Key, std::unique_ptr<Value>> Tree;
    Tree tree;
    std::set<Key> insertedKeys;
    for (const Key& key : keys) {
      typename Tree::Node* node = new typename Tree::Node(new Value{key});
      tree.attach(node, key, {});
      insertedKeys.insert(key);
      checkInvariants(tree);
      std::set<Key> actualKeys;
      for (typename Tree::Entry treeEntry : tree) {
        actualKeys.insert(treeEntry.key);
        EXPECT_EQ(treeEntry.node->value->keyAtInsertion, treeEntry.key);
        typename Tree::Entry found = tree.get(treeEntry.key, {});
        EXPECT_EQ(treeEntry.key, found.key);
        EXPECT_EQ(treeEntry.node, found.node);
      }
      EXPECT_THAT(actualKeys, testing::ContainerEq(insertedKeys));
    }
  }
  
  template<typename Type>
  void buildAndTestTreeWithKeys(const std::initializer_list<Type>& keys) {
    buildAndTestTree<std::initializer_list<Type>>(keys);
  }
};

TEST_F(DRBTreeTest, IncreasingArithmeticProgression) {
  buildAndTestTreeWithKeys({1, 2, 3, 4, 5, 6, 7, 8, 9});
}

TEST_F(DRBTreeTest, DecreasingArithmeticProgression) {
  buildAndTestTreeWithKeys({9, 8, 7, 6, 5, 4, 3, 2, 1});
}

TEST_F(DRBTreeTest, IncreasingSquares) {
  buildAndTestTreeWithKeys({1, 4, 9, 16, 25, 36, 49, 64, 81});
}

TEST_F(DRBTreeTest, DecreasingSquares) {
  buildAndTestTreeWithKeys({81, 64, 49, 36, 25, 16, 9, 4, 1});
}

TEST_F(DRBTreeTest, DoubleRotationAtRoot) {
  buildAndTestTreeWithKeys({10, 51, 12, 73, 95, 34, 45, 26, 87, 78, 69});
}

TEST_F(DRBTreeTest, Permutations) {
  std::vector<int> keys = {10, 51, 12, 73, 95, 34, 45};
  do {
    buildAndTestTree(keys);
  } while (std::next_permutation(keys.begin(), keys.end()));
}
  
}  // namespace editor
}  // namespace med

