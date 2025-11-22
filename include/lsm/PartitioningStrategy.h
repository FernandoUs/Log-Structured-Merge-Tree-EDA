#pragma once

#include "../spatial/SpatialComparators.h"
#include "../spatial/MBR.h"
#include "LSMComponent.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

using namespace std;

namespace lsm
{

    namespace sp = spatial;

    template <typename T>
    class PartitioningStrategy
    {
    public:
        virtual ~PartitioningStrategy() = default;

        virtual vector<LSMComponent<T> *> partition(
            const vector<sp::SpatialRecord<T>> &records,
            size_t targetLevel,
            size_t dimensions,
            size_t maxComponentSize) const = 0;
    };

    template <typename T>
    class SizePartitioning : public PartitioningStrategy<T>
    {
    private:
        enum ComparatorType
        {
            SIMPLE,
            HILBERT
        };
        ComparatorType comparatorType;

    public:
        explicit SizePartitioning(bool useHilbert = false)
            : comparatorType(useHilbert ? HILBERT : SIMPLE) {}

        vector<LSMComponent<T> *> partition(
            const vector<sp::SpatialRecord<T>> &records,
            size_t targetLevel,
            size_t dimensions,
            size_t maxComponentSize) const override
        {
            if (records.empty())
                return {};

            vector<sp::SpatialRecord<T>> sortedRecords = records;

            if (comparatorType == HILBERT && dimensions == 2)
            {
                sp::MBR bounds(dimensions);
                for (const auto &rec : sortedRecords)
                {
                    bounds.expand(rec.point);
                }
                sp::HilbertCurveComparator hilbertComp;
                sort(sortedRecords.begin(), sortedRecords.end(),
                     [&hilbertComp, &bounds](const sp::SpatialRecord<T> &a, const sp::SpatialRecord<T> &b)
                     {
                         return hilbertComp(a, b, bounds);
                     });
            }
            else
            {
                sp::SimpleComparator simpleComp;
                sort(sortedRecords.begin(), sortedRecords.end(),
                     [&simpleComp](const sp::SpatialRecord<T> &a, const sp::SpatialRecord<T> &b)
                     {
                         return simpleComp(a, b);
                     });
            }

            vector<LSMComponent<T> *> components;
            for (size_t i = 0; i < sortedRecords.size(); i += maxComponentSize)
            {
                size_t end = min(i + maxComponentSize, sortedRecords.size());
                vector<sp::SpatialRecord<T>> chunk(sortedRecords.begin() + i,
                                                   sortedRecords.begin() + end);

                LSMComponent<T> *comp = new LSMComponent<T>(targetLevel, dimensions);
                comp->build(chunk);
                components.push_back(comp);
            }

            return components;
        }
    };

    template <typename T>
    class STRPartitioning : public PartitioningStrategy<T>
    {
    public:
        vector<LSMComponent<T> *> partition(
            const vector<sp::SpatialRecord<T>> &records,
            size_t targetLevel,
            size_t dimensions,
            size_t maxComponentSize) const override
        {
            if (records.empty())
                return {};
            if (records.size() <= maxComponentSize)
            {
                LSMComponent<T> *comp = new LSMComponent<T>(targetLevel, dimensions);
                comp->build(records);
                return {comp};
            }

            if (dimensions != 2)
            {
                SizePartitioning<T> fallback(false);
                return fallback.partition(records, targetLevel, dimensions, maxComponentSize);
            }

            size_t numSlices = (size_t)ceil(sqrt((double)records.size() / maxComponentSize));
            if (numSlices < 1)
                numSlices = 1;

            vector<sp::SpatialRecord<T>> sortedByX = records;
            sort(sortedByX.begin(), sortedByX.end(),
                 [](const sp::SpatialRecord<T> &a, const sp::SpatialRecord<T> &b)
                 {
                     return a.point[0] < b.point[0];
                 });

            vector<LSMComponent<T> *> components;
            size_t sliceSize = (sortedByX.size() + numSlices - 1) / numSlices;

            for (size_t i = 0; i < sortedByX.size(); i += sliceSize)
            {
                size_t end = min(i + sliceSize, sortedByX.size());
                vector<sp::SpatialRecord<T>> slice(sortedByX.begin() + i, sortedByX.begin() + end);

                sort(slice.begin(), slice.end(),
                     [](const sp::SpatialRecord<T> &a, const sp::SpatialRecord<T> &b)
                     {
                         return a.point[1] < b.point[1];
                     });

                size_t numTiles = (slice.size() + maxComponentSize - 1) / maxComponentSize;
                size_t tileSize = (slice.size() + numTiles - 1) / numTiles;

                for (size_t j = 0; j < slice.size(); j += tileSize)
                {
                    size_t tileEnd = min(j + tileSize, slice.size());
                    vector<sp::SpatialRecord<T>> tile(slice.begin() + j, slice.begin() + tileEnd);

                    LSMComponent<T> *comp = new LSMComponent<T>(targetLevel, dimensions);
                    comp->build(tile);
                    components.push_back(comp);
                }
            }

            return components;
        }

    private:
        vector<LSMComponent<T> *> strPartitionRecursive(
            const vector<sp::SpatialRecord<T>> &records,
            size_t targetLevel,
            size_t dimensions,
            size_t maxComponentSize,
            size_t currentDim) const
        {
            return {};
        }
    };

    template <typename T>
    class RStarGrovePartitioning : public PartitioningStrategy<T>
    {
    private:
        double sampleRatio;

    public:
        explicit RStarGrovePartitioning(double sampling = 0.1)
            : sampleRatio(sampling) {}

        vector<LSMComponent<T> *> partition(
            const vector<sp::SpatialRecord<T>> &records,
            size_t targetLevel,
            size_t dimensions,
            size_t maxComponentSize) const override
        {
            if (records.empty())
                return {};
            if (records.size() <= maxComponentSize)
            {
                LSMComponent<T> *comp = new LSMComponent<T>(targetLevel, dimensions);
                comp->build(records);
                return {comp};
            }

            vector<sp::SpatialRecord<T>> sample = selectSample(records);
            vector<sp::MBR> boundaries = computeBoundaries(sample, dimensions, maxComponentSize);
            return assignToComponents(records, boundaries, targetLevel, dimensions);
        }

    private:
        vector<sp::SpatialRecord<T>> selectSample(
            const vector<sp::SpatialRecord<T>> &records) const
        {
            size_t sampleSize = max((size_t)(records.size() * sampleRatio), (size_t)100);
            sampleSize = min(sampleSize, records.size());

            vector<sp::SpatialRecord<T>> sample;
            sample.reserve(sampleSize);

            size_t step = records.size() / sampleSize;
            if (step < 1)
                step = 1;

            for (size_t i = 0; i < records.size() && sample.size() < sampleSize; i += step)
            {
                sample.push_back(records[i]);
            }

            return sample;
        }

        vector<sp::MBR> computeBoundaries(
            const vector<sp::SpatialRecord<T>> &sample,
            size_t dimensions,
            size_t maxComponentSize) const
        {
            if (sample.empty())
                return {};

            size_t numClusters = (sample.size() + maxComponentSize - 1) / maxComponentSize;
            if (numClusters < 1)
                numClusters = 1;

            vector<sp::Point> centroids;
            for (size_t i = 0; i < numClusters && i < sample.size(); ++i)
            {
                centroids.push_back(sample[i * sample.size() / numClusters].point);
            }

            for (int iter = 0; iter < 10; ++iter)
            {
                vector<vector<sp::Point>> clusters(numClusters);

                for (const auto &rec : sample)
                {
                    double minDist = numeric_limits<double>::max();
                    size_t bestCluster = 0;

                    for (size_t c = 0; c < centroids.size(); ++c)
                    {
                        double dist = rec.point.distanceTo(centroids[c]);
                        if (dist < minDist)
                        {
                            minDist = dist;
                            bestCluster = c;
                        }
                    }

                    clusters[bestCluster].push_back(rec.point);
                }

                for (size_t c = 0; c < numClusters; ++c)
                {
                    if (!clusters[c].empty())
                    {
                        sp::Point newCentroid(dimensions);
                        for (size_t d = 0; d < dimensions; ++d)
                        {
                            double sum = 0.0;
                            for (const auto &pt : clusters[c])
                            {
                                sum += pt[d];
                            }
                            newCentroid[d] = sum / clusters[c].size();
                        }
                        centroids[c] = newCentroid;
                    }
                }
            }

            vector<sp::MBR> boundaries;
            for (size_t c = 0; c < centroids.size(); ++c)
            {
                sp::MBR mbr(dimensions);
                mbr.expand(centroids[c]);
                boundaries.push_back(mbr);
            }

            for (const auto &rec : sample)
            {
                double minDist = numeric_limits<double>::max();
                size_t bestCluster = 0;

                for (size_t c = 0; c < centroids.size(); ++c)
                {
                    double dist = rec.point.distanceTo(centroids[c]);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        bestCluster = c;
                    }
                }

                boundaries[bestCluster].expand(rec.point);
            }

            return boundaries;
        }

        vector<LSMComponent<T> *> assignToComponents(
            const vector<sp::SpatialRecord<T>> &records,
            const vector<sp::MBR> &boundaries,
            size_t targetLevel,
            size_t dimensions) const
        {
            if (boundaries.empty())
                return {};

            vector<vector<sp::SpatialRecord<T>>> partitions(boundaries.size());

            for (const auto &rec : records)
            {
                double minExpansion = numeric_limits<double>::max();
                size_t bestPartition = 0;

                for (size_t p = 0; p < boundaries.size(); ++p)
                {
                    sp::MBR expandedMBR = boundaries[p];
                    double areaBefore = expandedMBR.area();
                    expandedMBR.expand(rec.point);
                    double areaAfter = expandedMBR.area();
                    double expansion = areaAfter - areaBefore;

                    if (expansion < minExpansion ||
                        (expansion == minExpansion && boundaries[p].contains(rec.point)))
                    {
                        minExpansion = expansion;
                        bestPartition = p;
                    }
                }

                partitions[bestPartition].push_back(rec);
            }

            vector<LSMComponent<T> *> components;
            for (const auto &partition : partitions)
            {
                if (!partition.empty())
                {
                    LSMComponent<T> *comp = new LSMComponent<T>(targetLevel, dimensions);
                    comp->build(partition);
                    components.push_back(comp);
                }
            }

            return components;
        }
    };

}