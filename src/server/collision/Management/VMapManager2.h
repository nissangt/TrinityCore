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

#ifndef _VMAPMANAGER2_H
#define _VMAPMANAGER2_H

#include "IVMapManager.h"
#include "Dynamic/UnorderedMap.h"
#include "Define.h"

#define MAP_FILENAME_EXTENSION2 ".vmtree"

#define FILENAMEBUFFER_SIZE 500

/**
This is the main Class to manage loading and unloading of maps, line of sight, height calculation and so on.
For each map or map tile to load it reads a directory file that contains the ModelContainer files used by this map or map tile.
Each global map or instance has its own dynamic BSP-Tree.
The loaded ModelContainers are included in one of these BSP-Trees.
Additionally a table to match map ids and map names is used.
*/

namespace G3D
{
    class Vector3;
}

namespace VMAP
{
    class StaticMapTree;
    class WorldModel;

    class ManagedModel
    {
    public:
        ManagedModel() : _model(NULL), _refCount(0) { }

        void SetModel(WorldModel* model) { _model = model; }
        WorldModel* GetModel() { return _model; }

        void IncRefCount() { ++_refCount; }
        int32 DecRefCount() { return --_refCount; }

    protected:
        WorldModel* _model;
        int32 _refCount;
    };

    typedef UNORDERED_MAP<uint32, StaticMapTree*> InstanceTreeMap;
    typedef UNORDERED_MAP<std::string, ManagedModel> ModelFileMap;

    class VMapManager2 : public IVMapManager
    {
    protected:
        // Tree to check collision
        ModelFileMap _loadedModels;
        InstanceTreeMap _instanceTrees;

        bool _LoadMap(const std::string& basePath, uint32 mapId, uint32 tileX, uint32 tileY);

    public:
        static std::string GetFileName(uint32 mapId);

        // public for debug
        G3D::Vector3 ConvertPosition(float x, float y, float z) const;

        VMapManager2();
        virtual ~VMapManager2();

        // IVMapManager
        VMapLoadResult LoadMap(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY);

        bool MapExists(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY);

        void UnloadMap(uint32 mapId, uint32 tileX, uint32 tileY);
        void UnloadMap(uint32 mapId);

        bool IsInLoS(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2);
        float GetHeight(uint32 mapId, float x, float y, float z, float maxDist);
        /**
        fill the hit pos and return true, if an object was hit
        */
        bool GetObjectHitPos(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float &ry, float& rz, float modifyDist);

        bool ProcessCommand(char* /*command*/) { return false; } // for debug and extensions

        bool GetAreaInfo(uint32 mapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const;
        bool GetLiquidLevel(uint32 mapId, float x, float y, float z, uint8 reqLiquidType, float& level, float& floor, uint32& type) const;

        // what's the use of this? o.O
        virtual std::string GetFileName(uint32 mapId, uint32 /*tileX*/, uint32 /*tileY*/) const
        {
            return GetFileName(mapId);
        }

        WorldModel* GetModelInstance(const std::string& basePath, const std::string& fileName);
        void ReleaseModelInstance(const std::string& fileName);
    };
}

#endif
