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

#ifndef _MAPTREE_H
#define _MAPTREE_H

#include "Define.h"
#include "Dynamic/UnorderedMap.h"
#include "BoundingIntervalHierarchy.h"

namespace VMAP
{
    class ModelInstance;
    class GroupModel;
    class VMapManager2;

    struct LocationInfo
    {
        LocationInfo(): _hitInstance(NULL), _hitModel(NULL), _groundZ(-G3D::inf()) { }

        const ModelInstance* _hitInstance;
        const GroupModel* _hitModel;
        float _groundZ;
    };

    class StaticMapTree
    {
        typedef UNORDERED_MAP<uint32, bool> LoadedTiles;
        typedef UNORDERED_MAP<uint32, uint32> LoadedSpawns;

    private:
        uint32 _mapId;
        bool _isTiled;
        BIH _tree;
        ModelInstance* _values; // the tree entries
        uint32 _valuesCount;

        // Store all the map tile idents that are loaded for that map
        // some maps are not splitted into tiles and we have to make sure, not removing the map before all tiles are removed
        // empty tiles have no tile file, hence map with bool instead of just a set (consistency check)
        LoadedTiles _loadedTiles;
        // stores <tree_index, reference_count> to invalidate tree values, unload map, and to be able to report errors
        LoadedSpawns _loadedSpawns;
        std::string _basePath;

    private:
        bool _GetIntersectionTime(const G3D::Ray& ray, float& maxDist, bool stopAtFirstHit) const;

    public:
        static std::string GetFileName(uint32 mapId, uint32 tileX, uint32 tileY);
        static uint32 PackTileId(uint32 tileX, uint32 tileY) { return tileX << 16 | tileY; }
        static void UnpackTileId(uint32 tileId, uint32& tileX, uint32& tileY) { tileX = tileId >> 16; tileY = tileId & 0xFF; }
        static bool CanLoadMap(const std::string& basePath, uint32 mapId, uint32 tileX, uint32 tileY);

    public:
        StaticMapTree(uint32 mapId, const std::string& basePath);
        ~StaticMapTree();

        bool IsInLoS(const G3D::Vector3& pos1, const G3D::Vector3& pos2) const;
        bool GetObjectHitPos(const G3D::Vector3& pos1, const G3D::Vector3& pos2, G3D::Vector3& hitPos, float modifyDist) const;
        float GetHeight(const G3D::Vector3& pos, float maxDist) const;
        bool GetAreaInfo(G3D::Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const;
        bool GetLocationInfo(const G3D::Vector3& pos, LocationInfo& info) const;

        bool InitMap(const std::string& fileName, VMapManager2* vm);
        void UnloadMap(VMapManager2* vm);
        bool LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);
        void UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);

        bool IsTiled() const { return _isTiled; }
        uint32 GetLoadedTilesCount() const { return _loadedTiles.size(); }
    };

    struct AreaInfo
    {
        AreaInfo(): _res(false), _groundZ(-G3D::inf()) { }

        bool _res;
        float _groundZ;
        uint32 _flags;
        int32 _adtId;
        int32 _rootId;
        int32 _groupId;
    };
}                                                           // VMAP

#endif // _MAPTREE_H
