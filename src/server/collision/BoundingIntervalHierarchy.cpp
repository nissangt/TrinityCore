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

#include "BoundingIntervalHierarchy.h"

void BIH::_BuildHierarchy(std::vector<uint32>& tree, BuildData& data, BuildStats& stats)
{
    // create space for the first node
    tree.push_back(uint32(3 << 30)); // dummy leaf
    tree.insert(tree.end(), 2, 0);
    //tempTree.add(0);

    // seed bbox
    AABound gridBox = { _bounds.low(), _bounds.high() };
    AABound nodeBox = gridBox;
    // seed subdivide function
    _SubDivide(0, data._count - 1, tree, data, gridBox, nodeBox, 0, 1, stats);
}

void BIH::_SubDivide(uint32 left, uint32 right, std::vector<uint32>& tree, BuildData& data, AABound& gridBox, AABound& nodeBox, uint32 nodeIndex, uint32 depth, BuildStats& stats)
{
    if ((right - left + 1) <= data._maxPrims || depth >= MAX_STACK_SIZE)
    {
        // write leaf node
        stats.UpdateLeaf(depth, right - left + 1);
        _CreateNode(tree, nodeIndex, left, right);
        return;
    }
    // calculate extents
    int axis = -1;
    int prevAxis;
    uint32 rightOrig;
    float clipLeft = G3D::fnan();
    float clipRight = G3D::fnan();
    float prevClip = G3D::fnan();
    float split = G3D::fnan();
    float prevSplit;

    bool wasLeft = true;
    while (true)
    {
        prevAxis = axis;
        prevSplit = split;
        // perform quick consistency checks
        G3D::Vector3 d(gridBox.hi - gridBox.lo);
        if (d.x < 0 || d.y < 0 || d.z < 0)
            throw std::logic_error("negative node extents");

        for (uint8 i = 0; i < 3; ++i)
            if (nodeBox.hi[i] < gridBox.lo[i] || nodeBox.lo[i] > gridBox.hi[i])
                throw std::logic_error("invalid node overlap");

        // find longest axis
        axis = d.primaryAxis();
        split = 0.5f * (gridBox.lo[axis] + gridBox.hi[axis]);
        // partition L/R subsets
        clipLeft = -G3D::inf();
        clipRight = G3D::inf();
        rightOrig = right; // save this for later

        float nodeLeft = G3D::inf();
        float nodeRight = -G3D::inf();
        for (uint32 i = left; i <= right; )
        {
            uint32 obj = data._indices[i];
            float minBound = data._bounds[obj].low() [axis];
            float maxBound = data._bounds[obj].high()[axis];
            float center = (minBound + maxBound) * 0.5f;
            if (center <= split)
            {
                // stay left
                ++i;
                if (clipLeft < maxBound)
                    clipLeft = maxBound;
            }
            else
            {
                // move to the right most
                uint32 tmp = data._indices[i];
                data._indices[i] = data._indices[right];
                data._indices[right] = tmp;
                --right;
                if (clipRight > minBound)
                    clipRight = minBound;
            }
            nodeLeft  = std::min(nodeLeft,  minBound);
            nodeRight = std::max(nodeRight, maxBound);
        }
        // check for empty space
        if (nodeLeft > nodeBox.lo[axis] && nodeRight < nodeBox.hi[axis])
        {
            float nodeBoxW = nodeBox.hi[axis] - nodeBox.lo[axis];
            float nodeNewW = nodeRight - nodeLeft;
            // node box is too big compare to space occupied by primitives?
            if (1.3f * nodeNewW < nodeBoxW)
            {
                stats.IncBvh2Count();
                uint32 nextIndex = tree.size();
                // allocate child
                tree.push_back(0);
                tree.push_back(0);
                tree.push_back(0);
                // write bvh2 clip node
                stats.IncNodesCount();
                tree[nodeIndex + 0] = (axis << 30) | (1 << 29) | nextIndex;
                tree[nodeIndex + 1] = FloatToInt(nodeLeft);
                tree[nodeIndex + 2] = FloatToInt(nodeRight);
                // update nodebox and recurse
                nodeBox.lo[axis] = nodeLeft;
                nodeBox.hi[axis] = nodeRight;
                _SubDivide(left, rightOrig, tree, data, gridBox, nodeBox, nextIndex, depth + 1, stats);
                return;
            }
        }
        // ensure we are making progress in the subdivision
        if (right == rightOrig)
        {
            // all left
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split)) {
                // we are stuck here - create a leaf
                stats.UpdateLeaf(depth, right - left + 1);
                _CreateNode(tree, nodeIndex, left, right);
                return;
            }
            if (clipLeft <= split) {
                // keep looping on left half
                gridBox.hi[axis] = split;
                prevClip = clipLeft;
                wasLeft = true;
                continue;
            }
            gridBox.hi[axis] = split;
            prevClip = G3D::fnan();
        }
        else if (left > right)
        {
            // all right
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split)) {
                // we are stuck here - create a leaf
                stats.UpdateLeaf(depth, right - left + 1);
                _CreateNode(tree, nodeIndex, left, right);
                return;
            }
            right = rightOrig;
            if (clipRight >= split) {
                // keep looping on right half
                gridBox.lo[axis] = split;
                prevClip = clipRight;
                wasLeft = false;
                continue;
            }
            gridBox.lo[axis] = split;
            prevClip = G3D::fnan();
        }
        else
        {
            // we are actually splitting stuff
            if (prevAxis != -1 && !isnan(prevClip))
            {
                // second time through - lets create the previous split
                // since it produced empty space
                uint32 nextIndex = tree.size();
                // allocate child node
                tree.push_back(0);
                tree.push_back(0);
                tree.push_back(0);
                if (wasLeft)
                {
                    // create a node with a left child
                    // write leaf node
                    stats.IncNodesCount();
                    tree[nodeIndex + 0] = (prevAxis << 30) | nextIndex;
                    tree[nodeIndex + 1] = FloatToInt(prevClip);
                    tree[nodeIndex + 2] = FloatToInt(G3D::inf());
                }
                else
                {
                    // create a node with a right child
                    // write leaf node
                    stats.IncNodesCount();
                    tree[nodeIndex + 0] = (prevAxis << 30) | (nextIndex - 3);
                    tree[nodeIndex + 1] = FloatToInt(-G3D::inf());
                    tree[nodeIndex + 2] = FloatToInt(prevClip);
                }
                // count stats for the unused leaf
                ++depth;
                stats.UpdateLeaf(depth, 0);
                // now we keep going as we are, with a new nodeIndex:
                nodeIndex = nextIndex;
            }
            break;
        }
    }
    // compute index of child nodes
    uint32 nextIndex = tree.size();
    // allocate left node
    uint32 nl = right - left + 1;
    uint32 nr = rightOrig - (right + 1) + 1;
    if (nl > 0)
    {
        tree.push_back(0);
        tree.push_back(0);
        tree.push_back(0);
    }
    else
        nextIndex -= 3;
    // allocate right node
    if (nr > 0)
    {
        tree.push_back(0);
        tree.push_back(0);
        tree.push_back(0);
    }
    // write leaf node
    stats.IncNodesCount();
    tree[nodeIndex + 0] = (axis << 30) | nextIndex;
    tree[nodeIndex + 1] = FloatToInt(clipLeft);
    tree[nodeIndex + 2] = FloatToInt(clipRight);
    // prepare L/R child boxes
    AABound gridBoxLeft(gridBox);
    AABound gridBoxRight(gridBox);
    AABound nodeBoxLeft(nodeBox);
    AABound nodeBoxRight(nodeBox);
    gridBoxLeft.hi[axis] = gridBoxRight.lo[axis] = split;
    nodeBoxLeft.hi[axis] = clipLeft;
    nodeBoxRight.lo[axis] = clipRight;
    // recurse
    if (nl > 0)
        _SubDivide(left, right, tree, data, gridBoxLeft, nodeBoxLeft, nextIndex, depth + 1, stats);
    else
        stats.UpdateLeaf(depth + 1, 0);
    if (nr > 0)
        _SubDivide(right + 1, rightOrig, tree, data, gridBoxRight, nodeBoxRight, nextIndex + 3, depth + 1, stats);
    else
        stats.UpdateLeaf(depth + 1, 0);
}

bool BIH::WriteToFile(FILE* wf) const
{
    uint32 treeSize = _tree.size();
    uint32 check = 0;
    check += fwrite(&_bounds.low(), sizeof(float), 3, wf);
    check += fwrite(&_bounds.high(), sizeof(float), 3, wf);
    check += fwrite(&treeSize, sizeof(uint32), 1, wf);
    check += fwrite(&_tree[0], sizeof(uint32), treeSize, wf);
    uint32 count = _objects.size();
    check += fwrite(&count, sizeof(uint32), 1, wf);
    check += fwrite(&_objects[0], sizeof(uint32), count, wf);
    return check == (3 + 3 + 2 + treeSize + count);
}

bool BIH::ReadFromFile(FILE* rf)
{
    uint32 check = 0;
    G3D::Vector3 lo;
    check += fread(&lo, sizeof(float), 3, rf);
    G3D::Vector3 hi;
    check += fread(&hi, sizeof(float), 3, rf);
    _bounds = G3D::AABox(lo, hi);
    uint32 treeSize;
    check += fread(&treeSize, sizeof(uint32), 1, rf);
    _tree.resize(treeSize);
    check += fread(&_tree[0], sizeof(uint32), treeSize, rf);
    uint32 count = 0;
    check += fread(&count, sizeof(uint32), 1, rf);
    _objects.resize(count);
    check += fread(&_objects[0], sizeof(uint32), count, rf);
    return check == (3 + 3 + 2 + treeSize + count);
}

void BIH::BuildStats::UpdateLeaf(uint32 depth, uint32 objects)
{
    ++_leavesCount;
    _depthMin = std::min(depth, _depthMin);
    _depthMax = std::max(depth, _depthMax);
    _depthSum += depth;
    _objectsMin = std::min(objects, _objectsMin);
    _objectsMax = std::max(objects, _objectsMax);
    _objectsSum += objects;
    uint32 index = std::min(objects, 5U);
    ++_leavesCountN[index];
}

void BIH::BuildStats::Print()
{
    printf("Tree stats:\n");
    printf("  * Nodes:          %d\n", _nodesCount);
    printf("  * Leaves:         %d\n", _leavesCount);
    printf("  * Objects: min    %d\n", _objectsMin);
    printf("             avg    %.2f\n", (float) _objectsSum / _leavesCount);
    printf("           avg(n>0) %.2f\n", (float) _objectsSum / (_leavesCount - _leavesCountN[0]));
    printf("             max    %d\n", _objectsMax);
    printf("  * Depth:   min    %d\n", _depthMin);
    printf("             avg    %.2f\n", (float) _depthSum / _leavesCount);
    printf("             max    %d\n", _depthMax);
    printf("  * Leaves w/: N=0  %3d%%\n", 100 * _leavesCountN[0] / _leavesCount);
    printf("               N=1  %3d%%\n", 100 * _leavesCountN[1] / _leavesCount);
    printf("               N=2  %3d%%\n", 100 * _leavesCountN[2] / _leavesCount);
    printf("               N=3  %3d%%\n", 100 * _leavesCountN[3] / _leavesCount);
    printf("               N=4  %3d%%\n", 100 * _leavesCountN[4] / _leavesCount);
    printf("               N>4  %3d%%\n", 100 * _leavesCountN[5] / _leavesCount);
    printf("  * BVH2 nodes:     %d (%3d%%)\n", _bvh2count, 100 * _bvh2count / (_nodesCount + _leavesCount - 2 * _bvh2count));
}
