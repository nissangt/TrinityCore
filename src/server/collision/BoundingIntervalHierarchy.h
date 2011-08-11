/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2010 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BIH_H
#define _BIH_H

#include "G3D/Vector3.h"
#include "G3D/Ray.h"
#include "G3D/AABox.h"

#include "Define.h"

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>

#ifdef __APPLE__
  #define isnan(x) ( std::isnan(x) )
#endif

#define MAX_STACK_SIZE 64

#ifdef _MSC_VER
    #define isnan(x) _isnan(x)
#endif

union IntFloat
{
    uint32 i;
    float f;
};

static inline uint32 FloatToInt(const float f)
{
    IntFloat tmp;
    tmp.f = f;
    return tmp.i;
}

static inline float IntToFloat(const uint32 i)
{
    IntFloat tmp;
    tmp.i = i;
    return tmp.f;
}

struct AABound
{
    G3D::Vector3 lo;
    G3D::Vector3 hi;
};

/** Bounding Interval Hierarchy Class.
    Building and Ray-Intersection functions based on BIH from
    Sunflow, a Java Raytracer, released under MIT/X11 License
    http://sunflow.sourceforge.net/
    Copyright (c) 2003-2007 Christopher Kulla
*/
class BIH
{
public:
    BIH() { }

    uint32 Count() const { return _objects.size(); }

    template <class T, class BoundsFunc>
    void Build(const std::vector<T>& primitives, BoundsFunc& getBounds, uint32 leafSize = 3, bool printStats = false)
    {
        if (primitives.empty())
            return;

        BuildData data;
        data._maxPrims = leafSize;
        data._count = primitives.size();
        data._indices = new uint32[data._count];
        data._bounds = new G3D::AABox[data._count];
        getBounds(primitives[0], _bounds);
        for (uint32 i = 0; i < data._count; ++i)
        {
            data._indices[i] = i;
            getBounds(primitives[i], data._bounds[i]);
            _bounds.merge(data._bounds[i]);
        }
        std::vector<uint32> tree;
        BuildStats stats;
        _BuildHierarchy(tree, data, stats);
        if (printStats)
            stats.Print();

        _objects.resize(data._count);
        for (uint32 i = 0; i < data._count; ++i)
            _objects[i] = data._indices[i];

        _tree = tree;
        delete[] data._bounds;
        delete[] data._indices;
    }

    template <typename RayCallback>
    void IntersectRay(const G3D::Ray& ray, RayCallback& rayCallback, float& maxDist, bool stopAtFirstHit = false) const
    {
        float intervalMin = -1.0f;
        float intervalMax = -1.0f;
        G3D::Vector3 org = ray.origin();
        G3D::Vector3 dir = ray.direction();
        G3D::Vector3 invDir;
        for (uint8 i = 0; i < 3; ++i)
        {
            invDir[i] = 1.0f / dir[i];
            if (G3D::fuzzyNe(dir[i], 0.0f))
            {
                float t1 = (_bounds.low()[i]  - org[i]) * invDir[i];
                float t2 = (_bounds.high()[i] - org[i]) * invDir[i];
                if (t1 > t2)
                    std::swap(t1, t2);
                if (t1 > intervalMin)
                    intervalMin = t1;
                if (t2 < intervalMax || intervalMax < 0.0f)
                    intervalMax = t2;
                // intervalMax can only become smaller for other axis,
                // and intervalMin only larger respectively, so stop early
                if (intervalMax <= 0 || intervalMin >= maxDist)
                    return;
            }
        }

        if (intervalMin > intervalMax)
            return;

        intervalMin = std::max(intervalMin, 0.0f);
        intervalMax = std::min(intervalMax, maxDist);

        uint32 offsetFront[3];
        uint32 offsetBack[3];
        uint32 offsetFront3[3];
        uint32 offsetBack3[3];

        // compute custom offsets from direction sign bit
        for (uint8 i = 0; i < 3; ++i)
        {
            offsetFront[i]  = FloatToInt(dir[i]) >> 31;
            offsetBack[i]   = offsetFront[i] ^ 1;
            offsetFront3[i] = offsetFront[i] * 3;
            offsetBack3[i]  = offsetBack[i]  * 3;
            // avoid always adding 1 during the inner loop
            ++offsetFront[i];
            ++offsetBack[i];
        }

        StackNode stack[MAX_STACK_SIZE];
        uint32 stackPos = 0;
        uint32 nodeIndex = 0;
        while (true)
        {
            while (true)
            {
                uint32 node     = _tree[nodeIndex];
                uint32 axis     = (node & (3 << 30)) >> 30;
                bool BVH2       = node & (1 << 29);
                uint32 offset   = node & ~(7 << 29);
                if (!BVH2)
                {
                    if (axis < 3)
                    {
                        // "normal" interior node
                        float front = (IntToFloat(_tree[nodeIndex + offsetFront[axis]]) - org[axis]) * invDir[axis];
                        float back  = (IntToFloat(_tree[nodeIndex + offsetBack [axis]]) - org[axis]) * invDir[axis];
                        // ray passes between clip zones
                        if (front < intervalMin && back > intervalMax)
                            break;

                        uint32 backIndex = offset + offsetBack3[axis];
                        nodeIndex = backIndex;
                        // ray passes through far node only
                        if (front < intervalMin) {
                            intervalMin = std::max(back, intervalMin);
                            continue;
                        }
                        nodeIndex = offset + offsetFront3[axis]; // front
                        // ray passes through near node only
                        if (back > intervalMax) {
                            intervalMax = std::min(front, intervalMax);
                            continue;
                        }
                        // ray passes through both nodes
                        // push back node
                        stack[stackPos]._node = backIndex;
                        stack[stackPos]._near = std::max(back, intervalMin);
                        stack[stackPos]._far  = intervalMax;
                        ++stackPos;
                        // update ray interval for front node
                        intervalMax = std::min(front, intervalMax);
                    }
                    else
                    {
                        // leaf - test some objects
                        uint32 o = _tree[nodeIndex + 1];
                        while (o > 0)
                        {
                            bool hit = rayCallback(ray, _objects[offset], maxDist, stopAtFirstHit);
                            if (stopAtFirstHit && hit)
                                return;
                            --o;
                            ++offset;
                        }
                        break;
                    }
                }
                else
                {
                    if (axis > 2)
                        return; // should not happen

                    float front = (IntToFloat(_tree[nodeIndex + offsetFront[axis]]) - org[axis]) * invDir[axis];
                    float back  = (IntToFloat(_tree[nodeIndex + offsetBack [axis]]) - org[axis]) * invDir[axis];
                    nodeIndex = offset;
                    intervalMin = std::max(front, intervalMin);
                    intervalMax = std::min(back, intervalMax);
                    if (intervalMin > intervalMax)
                        break;
                }
            } // traversal loop
            while (true)
            {
                // stack is empty?
                if (stackPos == 0)
                    return;

                // move back up the stack
                --stackPos;
                intervalMin = stack[stackPos]._near;
                if (maxDist < intervalMin)
                    continue;

                nodeIndex = stack[stackPos]._node;
                intervalMax = stack[stackPos]._far;
                break;
            }
        }
    }

    template<typename IsectCallback>
    void IntersectPoint(const G3D::Vector3& pos, IsectCallback& isectCallback) const
    {
        if (!_bounds.contains(pos))
            return;

        StackNode stack[MAX_STACK_SIZE];
        uint32 stackPos = 0;
        uint32 nodeIndex = 0;
        while (true)
        {
            while (true)
            {
                uint32 node     = _tree[nodeIndex];
                uint32 axis     = (node & (3 << 30)) >> 30;
                bool BVH2       = node & (1 << 29);
                uint32 offset   = node & ~(7 << 29);
                if (!BVH2)
                {
                    if (axis < 3)
                    {
                        // "normal" interior node
                        float left  = IntToFloat(_tree[nodeIndex + 1]);
                        float right = IntToFloat(_tree[nodeIndex + 2]);
                        // point is between clip zones
                        if (left < pos[axis] && right > pos[axis])
                            break;

                        uint32 rightIndex = offset + 3;
                        nodeIndex = rightIndex;
                        // point is in right node only
                        if (left < pos[axis]) {
                            continue;
                        }
                        nodeIndex = offset; // left
                        // point is in left node only
                        if (right > pos[axis]) {
                            continue;
                        }
                        // point is in both nodes
                        // push back right node
                        stack[stackPos]._node = rightIndex;
                        ++stackPos;
                    }
                    else
                    {
                        // leaf - test some objects
                        uint32 o = _tree[nodeIndex + 1];
                        while (o > 0) {
                            isectCallback(pos, _objects[offset]); // !!!
                            --o;
                            ++offset;
                        }
                        break;
                    }
                }
                else // BVH2 node (empty space cut off left and right)
                {
                    if (axis > 2)
                        return; // should not happen

                    float left  = IntToFloat(_tree[nodeIndex + 1]);
                    float right = IntToFloat(_tree[nodeIndex + 2]);
                    nodeIndex = offset;
                    if (left > pos[axis] || right < pos[axis])
                        break;
                }
            } // traversal loop

            // stack is empty?
            if (stackPos == 0)
                return;

            // move back up the stack
            --stackPos;
            nodeIndex = stack[stackPos]._node;
        }
    }

    bool WriteToFile(FILE* wf) const;
    bool ReadFromFile(FILE* rf);

protected:
    std::vector<uint32> _tree;
    std::vector<uint32> _objects;
    G3D::AABox _bounds;

    struct BuildData
    {
        uint32* _indices;
        G3D::AABox* _bounds;
        uint32 _count;
        uint32 _maxPrims;
    };

    struct StackNode
    {
        uint32 _node;
        float _near;
        float _far;
    };

    class BuildStats
    {
    private:
        uint32 _nodesCount;
        uint32 _leavesCount;
        uint32 _objectsSum;
        uint32 _objectsMin;
        uint32 _objectsMax;
        uint32 _depthSum;
        uint32 _depthMin;
        uint32 _depthMax;
        uint32 _leavesCountN[6];
        uint32 _bvh2count;

    public:
        BuildStats() : _nodesCount(0), _leavesCount(0),
            _objectsSum(0), _objectsMin(0x0FFFFFFF), _objectsMax(0xFFFFFFFF),
            _depthSum(0), _depthMin(0x0FFFFFFF), _depthMax(0xFFFFFFFF), _bvh2count(0)
        {
            memset(_leavesCountN, 0, sizeof(uint32) * 6);
        }

        void IncNodesCount() { ++_nodesCount; }
        void IncBvh2Count() { ++_bvh2count; }
        void UpdateLeaf(uint32 depth, uint32 objects);
        void Print();
    };

    void _BuildHierarchy(std::vector<uint32>& tree, BuildData& data, BuildStats& stats);

    void _CreateNode(std::vector<uint32>& tree, uint32 nodeIndex, uint32 left, uint32 right)
    {
        // write leaf node
        tree[nodeIndex + 0] = (3 << 30) | left;
        tree[nodeIndex + 1] = right - left + 1;
    }

    void _SubDivide(uint32 left, uint32 right, std::vector<uint32>& tree, BuildData& data, AABound& gridBox, AABound& nodeBox, uint32 nodeIndex, uint32 depth, BuildStats& stats);
};

#endif // _BIH_H
