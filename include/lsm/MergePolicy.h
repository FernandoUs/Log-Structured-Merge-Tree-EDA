#pragma once

#include "LSMComponent.h"
#include "../spatial/SpatialComparators.h"
#include <vector>
#include <memory>
#include <queue>
#include <algorithm>
#include <map>
#include <string>
using namespace std;

namespace lsm
{

    namespace sp = spatial;

    template <typename T>
    class PartitioningStrategy;

    template <typename T>
    struct MergeEntry
    {
        sp::SpatialRecord<T> record;
        int componentIndex;
        size_t vectorIndex;
    };

    template <typename T>
    struct MergeComparator
    {
        sp::ISpatialComparator<T> *comparator;
        bool operator()(const MergeEntry<T> &a, const MergeEntry<T> &b) const
        {
            if (comparator->compare(b.record, a.record))
                return true;
            if (comparator->compare(a.record, b.record))
                return false;
            return a.componentIndex > b.componentIndex;
        }
    };

    template <typename T>
    class MergePolicy
    {
    public:
        virtual ~MergePolicy() = default;
        virtual bool shouldMerge(const vector<LSMComponent<T> *> &components) const = 0;
        virtual vector<LSMComponent<T> *> selectComponentsToMerge(
            const vector<LSMComponent<T> *> &components) const = 0;

        vector<LSMComponent<T> *> mergeComponentsPartitioned(
            const vector<LSMComponent<T> *> &components,
            size_t targetLevel,
            size_t dimensions,
            const string &directory,
            sp::ISpatialComparator<T> *comp,
            class PartitioningStrategy<T> *strategy,
            size_t maxComponentSize) const
        {
            if (components.empty() || !strategy)
                return {};

            MergeComparator<T> pqComp = {comp};
            priority_queue<MergeEntry<T>, vector<MergeEntry<T>>, MergeComparator<T>> pq(pqComp);
            vector<vector<sp::SpatialRecord<T>>> allData;
            allData.reserve(components.size());

            sp::MBR fullWorld(dimensions);
            sp::Point minP(vector<double>(dimensions, -1e9));
            sp::Point maxP(vector<double>(dimensions, 1e9));
            fullWorld.setLower(minP);
            fullWorld.setUpper(maxP);

            for (size_t i = 0; i < components.size(); ++i)
            {
                vector<sp::SpatialRecord<T>> records = components[i]->rangeSearch(fullWorld);
                allData.push_back(records);

                if (!records.empty())
                {
                    MergeEntry<T> entry;
                    entry.record = records[0];
                    entry.componentIndex = (int)i;
                    entry.vectorIndex = 0;
                    pq.push(entry);
                }
            }

            vector<sp::SpatialRecord<T>> mergedRecords;

            while (!pq.empty())
            {
                MergeEntry<T> top = pq.top();
                pq.pop();
                bool isDuplicate = false;
                if (!mergedRecords.empty())
                {
                    sp::SpatialRecord<T> &last = mergedRecords.back();
                    if (last.point == top.record.point)
                    {
                        isDuplicate = true;
                    }
                }

                if (!isDuplicate)
                {
                    mergedRecords.push_back(top.record);
                }

                int cIdx = top.componentIndex;
                size_t nextIdx = top.vectorIndex + 1;

                if (nextIdx < allData[cIdx].size())
                {
                    MergeEntry<T> nextEntry;
                    nextEntry.record = allData[cIdx][nextIdx];
                    nextEntry.componentIndex = cIdx;
                    nextEntry.vectorIndex = nextIdx;
                    pq.push(nextEntry);
                }
            }

            return strategy->partition(mergedRecords, targetLevel, dimensions, maxComponentSize);
        }

        LSMComponent<T> *mergeComponents(
            const vector<LSMComponent<T> *> &components,
            size_t targetLevel,
            size_t dimensions,
            const string &directory,
            sp::ISpatialComparator<T> *comp) const
        {

            if (components.empty())
                return nullptr;
            MergeComparator<T> pqComp = {comp};
            priority_queue<MergeEntry<T>, vector<MergeEntry<T>>, MergeComparator<T>> pq(pqComp);
            vector<vector<sp::SpatialRecord<T>>> allData;
            allData.reserve(components.size());

            sp::MBR fullWorld(dimensions);
            sp::Point minP(vector<double>(dimensions, -1e9));
            sp::Point maxP(vector<double>(dimensions, 1e9));
            fullWorld.setLower(minP);
            fullWorld.setUpper(maxP);

            for (size_t i = 0; i < components.size(); ++i)
            {
                vector<sp::SpatialRecord<T>> records = components[i]->rangeSearch(fullWorld);
                allData.push_back(records);

                if (!records.empty())
                {
                    MergeEntry<T> entry;
                    entry.record = records[0];
                    entry.componentIndex = (int)i;
                    entry.vectorIndex = 0;
                    pq.push(entry);
                }
            }

            vector<sp::SpatialRecord<T>> mergedRecords;

            while (!pq.empty())
            {
                MergeEntry<T> top = pq.top();
                pq.pop();
                bool isDuplicate = false;
                if (!mergedRecords.empty())
                {
                    sp::SpatialRecord<T> &last = mergedRecords.back();
                    if (last.point == top.record.point)
                    {
                        isDuplicate = true;
                    }
                }

                if (!isDuplicate)
                {
                    mergedRecords.push_back(top.record);
                }

                int cIdx = top.componentIndex;
                size_t nextIdx = top.vectorIndex + 1;

                if (nextIdx < allData[cIdx].size())
                {
                    MergeEntry<T> nextEntry;
                    nextEntry.record = allData[cIdx][nextIdx];
                    nextEntry.componentIndex = cIdx;
                    nextEntry.vectorIndex = nextIdx;
                    pq.push(nextEntry);
                }
            }

            LSMComponent<T> *newComp = new LSMComponent<T>(targetLevel, dimensions);
            newComp->build(mergedRecords);
            newComp->saveToDisk(directory);
            return newComp;
        }
    };

    template <typename T>
    class BinomialMergePolicy : public MergePolicy<T>
    {
    private:
        size_t k;

    public:
        explicit BinomialMergePolicy(size_t ratio = 4) : k(ratio) {}

        bool shouldMerge(const vector<LSMComponent<T> *> &components) const override
        {
            return false;
        }

        vector<LSMComponent<T> *> selectComponentsToMerge(
            const vector<LSMComponent<T> *> &components) const override
        {
            return {};
        }
    };

    template <typename T>
    class TieredMergePolicy : public MergePolicy<T>
    {
    private:
        size_t B;

    public:
        explicit TieredMergePolicy(size_t branchingFactor = 4) : B(branchingFactor) {}

        bool shouldMerge(const vector<LSMComponent<T> *> &components) const override
        {
            map<size_t, size_t> countsPerLevel;
            for (const auto *comp : components)
            {
                countsPerLevel[comp->getLevel()]++;
            }
            for (const auto &[level, count] : countsPerLevel)
            {
                if (count >= B)
                    return true;
            }
            return false;
        }

        vector<LSMComponent<T> *> selectComponentsToMerge(
            const vector<LSMComponent<T> *> &components) const override
        {
            if (components.size() < B)
                return {};
            map<size_t, vector<LSMComponent<T> *>> componentsByLevel;
            for (auto *comp : components)
            {
                componentsByLevel[comp->getLevel()].push_back(comp);
            }
            for (auto &[level, levelComps] : componentsByLevel)
            {
                if (levelComps.size() >= B)
                {
                    vector<LSMComponent<T> *> selection;
                    size_t start = levelComps.size() - B;
                    for (size_t i = start; i < levelComps.size(); ++i)
                    {
                        selection.push_back(levelComps[i]);
                    }
                    return selection;
                }
            }

            return {};
        }
    };

    template <typename T>
    class ConcurrentMergePolicy : public MergePolicy<T>
    {
    private:
        size_t minComponents;

    public:
        explicit ConcurrentMergePolicy(size_t minComps = 2) : minComponents(minComps) {}

        bool shouldMerge(const vector<LSMComponent<T> *> &components) const override
        {
            return false;
        }

        vector<LSMComponent<T> *> selectComponentsToMerge(
            const vector<LSMComponent<T> *> &components) const override
        {
            return {};
        }
    };

    template <typename T>
    class LeveledMergePolicy : public MergePolicy<T>
    {
    private:
        size_t sizeRatio;
        size_t baseSize;

    public:
        explicit LeveledMergePolicy(size_t ratio = 10, size_t base = 1000)
            : sizeRatio(ratio), baseSize(base) {}

        size_t getMaxSizeForLevel(size_t level) const
        {
            if (level == 0)
                return baseSize;
            size_t maxSize = baseSize;
            for (size_t i = 0; i < level; ++i)
            {
                maxSize *= sizeRatio;
            }
            return maxSize;
        }

        bool shouldMerge(const vector<LSMComponent<T> *> &components) const override
        {
            map<size_t, size_t> sizePerLevel;

            for (const auto *comp : components)
            {
                sizePerLevel[comp->getLevel()] += comp->size();
            }

            for (const auto &[level, totalSize] : sizePerLevel)
            {
                size_t maxAllowed = getMaxSizeForLevel(level);
                if (totalSize > maxAllowed)
                {
                    return true;
                }
            }

            return false;
        }

        vector<LSMComponent<T> *> selectComponentsToMerge(
            const vector<LSMComponent<T> *> &components) const override
        {
            map<size_t, vector<LSMComponent<T> *>> componentsByLevel;
            map<size_t, size_t> sizePerLevel;

            for (auto *comp : components)
            {
                size_t level = comp->getLevel();
                componentsByLevel[level].push_back(comp);
                sizePerLevel[level] += comp->size();
            }

            for (const auto &[level, totalSize] : sizePerLevel)
            {
                size_t maxAllowed = getMaxSizeForLevel(level);
                if (totalSize > maxAllowed)
                {
                    vector<LSMComponent<T> *> victims;

                    const auto &levelComps = componentsByLevel[level];
                    for (auto *comp : levelComps)
                    {
                        victims.push_back(comp);
                    }

                    sp::MBR totalMBR = victims[0]->getMBR();
                    for (size_t i = 1; i < victims.size(); ++i)
                    {
                        totalMBR.expand(victims[i]->getMBR());
                    }

                    size_t nextLevel = level + 1;
                    if (componentsByLevel.find(nextLevel) != componentsByLevel.end())
                    {
                        for (auto *comp : componentsByLevel[nextLevel])
                        {
                            if (comp->getMBR().intersects(totalMBR))
                            {
                                victims.push_back(comp);
                            }
                        }
                    }

                    return victims;
                }
            }

            return {};
        }
    };

    template <typename T>
    class PolicyFactory
    {
    public:
        static MergePolicy<T> *create(const string &name, int param)
        {
            string n = name;
            transform(n.begin(), n.end(), n.begin(), [](unsigned char c)
                      { return std::tolower(c); });
            if (n == "binomial")
            {
                return new BinomialMergePolicy<T>(param);
            }
            else if (n == "tiered")
            {
                return new TieredMergePolicy<T>(param);
            }
            else if (n == "leveled")
            {
                return new LeveledMergePolicy<T>(param, 1000);
            }
            else if (n == "concurrent")
            {
                return new ConcurrentMergePolicy<T>(param);
            }

            cerr << "Warning: Unknown policy '" << n << "'. Defaulting to Tiered." << endl;
            return new TieredMergePolicy<T>(4);
        }
    };

}