#pragma once

#include "Point.h"
#include "MBR.h"
#include "SpatialComparators.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace spatial {


template<typename T>
class RTreeNode {
public:
    MBR mbr;
    vector<RTreeNode<T>*> children; 
    vector<SpatialRecord<T>> records;
    bool isLeaf;

    RTreeNode(bool leaf = true) : isLeaf(leaf) {}

    ~RTreeNode() {
        for (RTreeNode<T>* child : children) {
            delete child;
        }
    }

    void updateMBR() {
    }
};

template<typename T>
class RTree {
private:
    RTreeNode<T>* root; 
    size_t maxEntriesPerNode;
    size_t minEntriesPerNode;
    size_t dimensions;

    RTreeNode<T>* bulkLoad(vector<SpatialRecord<T>>& records, size_t dim = 0) {
        return new RTreeNode<T>(true);
    }

    void rangeSearchRecursive(RTreeNode<T>* node,
                             const MBR& queryBox,
                             vector<SpatialRecord<T>>& results) const {
    }

public:
    RTree(size_t dims = 2, size_t maxEntries = 50, size_t minEntries = 20)
        : root(nullptr),
          maxEntriesPerNode(maxEntries),
          minEntriesPerNode(minEntries),
          dimensions(dims) {}

    ~RTree() {
        delete root;
    }

    void build(vector<SpatialRecord<T>> records) {
        delete root;
        root = new RTreeNode<T>(true);
    }

    vector<SpatialRecord<T>> rangeSearch(const MBR& queryBox) const {
        return {};
    }

    MBR getTotalMBR() const {
        return MBR(dimensions);
    }

    bool isEmpty() const {
        return true;
    }

    size_t size() const {
        return 0;
    }

private:
    size_t countRecords(RTreeNode<T>* node) const {
        return 0;
    }
};

}