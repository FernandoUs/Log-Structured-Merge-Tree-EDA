#pragma once

#include "Point.h"
#include "MBR.h"
#include "SpatialComparators.h"
#include <vector>
#include <algorithm>

using namespace std;

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
        for (auto* child : children) {
            delete child;
        }
    }

    void updateMBR() {
        if (isLeaf) {
            if (records.empty()) {
                mbr = MBR(0);
                return;
            }
            mbr = MBR(records[0].point.dimensions());
            for (const auto &r : records) {
                if (!mbr.isValid()) mbr = MBR(r.point.dimensions());
                mbr.expand(r.point);
            }
        } else {
            if (children.empty()) {
                mbr = MBR(0);
                return;
            }
            mbr = children[0]->mbr;
            for (size_t i = 1; i < children.size(); ++i) {
                mbr.expand(children[i]->mbr);
            }
        }
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
        if (records.empty()) return nullptr;

        sort(records.begin(), records.end(), [](const SpatialRecord<T>& a, const SpatialRecord<T>& b) {
            return a.point[0] < b.point[0];
        });

        vector<RTreeNode<T>*> currentLevelNodes;

        size_t numLeaves = (records.size() + maxEntriesPerNode - 1) / maxEntriesPerNode;
        auto it = records.begin();

        for (size_t i = 0; i < numLeaves; ++i) {
            RTreeNode<T>* leaf = new RTreeNode<T>(true);
            
            size_t count = 0;
            while (it != records.end() && count < maxEntriesPerNode) {
                leaf->records.push_back(*it);
                it++;
                count++;
            }
            leaf->updateMBR();
            currentLevelNodes.push_back(leaf);
        }

        while (currentLevelNodes.size() > 1) {
            currentLevelNodes = buildLevel(currentLevelNodes);
        }

        return currentLevelNodes[0];
    }

    void rangeSearchRecursive(RTreeNode<T>* node,
                             const MBR& queryBox,
                             vector<SpatialRecord<T>>& results) const {
        if (!node) return;

        if (!node->mbr.isValid()) return;
        if (!node->mbr.intersects(queryBox)) return;

        if (node->isLeaf) {
            for (const auto &r : node->records) {
                if (!r.isTombstone && queryBox.contains(r.point)) {
                    results.push_back(r);
                }
            }
        } else {
            for (auto *child : node->children) {
                rangeSearchRecursive(child, queryBox, results);
            }
        }
    }

    void collectRecordsRecursive(RTreeNode<T>* node, vector<SpatialRecord<T>>& result) const {
        if (!node) return;

        if (node->isLeaf) {
            result.insert(result.end(), node->records.begin(), node->records.end());
        } else {
            for (auto* child : node->children) {
                collectRecordsRecursive(child, result);
            }
        }
    }

    size_t countRecords(RTreeNode<T>* node) const {
        if (!node) return 0;
        if (node->isLeaf) return node->records.size();
        size_t sum = 0;
        for (auto *c : node->children) sum += countRecords(c);
        return sum;
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
    vector<RTreeNode<T>*> buildLevel(vector<RTreeNode<T>*>& nodes) {
        vector<RTreeNode<T>*> nextLevel;
        size_t numParents = (nodes.size() + maxEntriesPerNode - 1) / maxEntriesPerNode;

        nextLevel.reserve(numParents);

        auto it = nodes.begin();
        for (size_t i = 0; i < numParents; ++i) {
            RTreeNode<T>* parent = new RTreeNode<T>(false);

            size_t count = 0;
            while (it != nodes.end() && count < maxEntriesPerNode) {
                parent->children.push_back(*it);
                it++;
                count++;
            }

            parent->updateMBR();
            nextLevel.push_back(parent);
        }
        return nextLevel;
    }

    void build(vector<SpatialRecord<T>> records) {
        delete root;
        if (records.empty()) {
            root = nullptr;
            return;
        }
        root = bulkLoad(records, 0);
    }

    vector<SpatialRecord<T>> rangeSearch(const MBR& queryBox) const {
        vector<SpatialRecord<T>> results;
        if (!root) return results;
        rangeSearchRecursive(root, queryBox, results);
        return results;
    }
    vector<SpatialRecord<T>> getAllRecords() const {
        vector<SpatialRecord<T>> allRecs;
        if (root) {
            collectRecordsRecursive(root, allRecs);
        }
        return allRecs;
    }

    MBR getTotalMBR() const {
        if (!root) return MBR(dimensions);
        return root->mbr;
    }

    bool isEmpty() const {
        return root == nullptr || countRecords(root) == 0;
    }

    size_t size() const {
        if (!root) return 0;
        return countRecords(root);
    }
};

}