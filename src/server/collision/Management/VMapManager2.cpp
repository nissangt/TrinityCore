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

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "VMapManager2.h"
#include "MapTree.h"
#include "ModelInstance.h"
#include "WorldModel.h"
#include "VMapDefinitions.h"
#include "Common.h"
#include "Log.h"
#include <G3D/Vector3.h>
#include <ace/Null_Mutex.h>
#include <ace/Singleton.h>
#include "DisableMgr.h"

namespace VMAP
{
    VMapManager2::VMapManager2() { }

    // virtual
    VMapManager2::~VMapManager2()
    {
        for (InstanceTreeMap::iterator itr = _instanceTrees.begin(); itr != _instanceTrees.end(); ++itr)
            delete itr->second;

        for (ModelFileMap::iterator itr = _loadedModels.begin(); itr != _loadedModels.end(); ++itr)
            delete itr->second.GetModel();
    }

    G3D::Vector3 VMapManager2::ConvertPosition(float x, float y, float z) const
    {
        const float mid = 0.5f * 64.0f * 533.33333333f;
        G3D::Vector3 pos;
        pos.x = mid - x;
        pos.y = mid - y;
        pos.z = z;
        return pos;
    }

    // move to MapTree too?
    std::string VMapManager2::GetFileName(uint32 mapId)
    {
        // Filename format: %03u.ext
        std::stringstream fileName;
        fileName.width(3);
        fileName << std::setfill('0') << mapId << std::string(MAP_FILENAME_EXTENSION2);
        return fileName.str();
    }

    VMapLoadResult VMapManager2::LoadMap(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY)
    {
        VMapLoadResult res = VMAP_LOAD_RESULT_IGNORED;
        if (IsMapLoadingEnabled())
        {
            if (_LoadMap(basePath, mapId, tileX, tileY))
                res = VMAP_LOAD_RESULT_OK;
            else
                res = VMAP_LOAD_RESULT_ERROR;
        }
        return res;
    }

    // load one tile (internal use only)
    bool VMapManager2::_LoadMap(const std::string& basePath, uint32 mapId, uint32 tileX, uint32 tileY)
    {
        InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
        if (itr == _instanceTrees.end())
        {
            std::string fileName = GetFileName(mapId);
            StaticMapTree* tree = new StaticMapTree(mapId, basePath);
            if (!tree->InitMap(fileName, this))
            {
                delete tree;
                return false;
            }
            itr = _instanceTrees.insert(InstanceTreeMap::value_type(mapId, tree)).first;
        }
        return itr->second->LoadMapTile(tileX, tileY, this);
    }

    void VMapManager2::UnloadMap(uint32 mapId)
    {
        InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
        if (itr != _instanceTrees.end())
        {
            itr->second->UnloadMap(this);
            if (itr->second->GetLoadedTilesCount() == 0)
            {
                delete itr->second;
                _instanceTrees.erase(mapId);
            }
        }
    }

    void VMapManager2::UnloadMap(uint32 mapId, uint32 tileX, uint32 tileY)
    {
        InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
        if (itr != _instanceTrees.end())
        {
            itr->second->UnloadMapTile(tileX, tileY, this);
            if (itr->second->GetLoadedTilesCount() == 0)
            {
                delete itr->second;
                _instanceTrees.erase(mapId);
            }
        }
    }

    bool VMapManager2::IsInLoS(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2)
    {
        if (!IsEnabledLoS() || sDisableMgr->IsDisabledFor(DISABLE_TYPE_VMAP, mapId, NULL, VMAP_DISABLE_LOS))
            return true;

        InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
        if (itr != _instanceTrees.end())
        {
            G3D::Vector3 pos1 = ConvertPosition(x1, y1, z1);
            G3D::Vector3 pos2 = ConvertPosition(x2, y2, z2);
            if (pos1 != pos2)
                return itr->second->IsInLoS(pos1, pos2);
        }
        return true;
    }

    /**
    get the hit position and return true if we hit something
    otherwise the result pos will be the dest pos
    */
    bool VMapManager2::GetObjectHitPos(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float &ry, float& rz, float modifyDist)
    {
        if (IsEnabledLoS() && !sDisableMgr->IsDisabledFor(DISABLE_TYPE_VMAP, mapId, NULL, VMAP_DISABLE_LOS))
        {
            InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
            if (itr != _instanceTrees.end())
            {
                G3D::Vector3 pos1 = ConvertPosition(x1, y1, z1);
                G3D::Vector3 pos2 = ConvertPosition(x2, y2, z2);
                G3D::Vector3 resPos;
                bool res = itr->second->GetObjectHitPos(pos1, pos2, resPos, modifyDist);
                resPos = ConvertPosition(resPos.x, resPos.y, resPos.z);
                rx = resPos.x;
                ry = resPos.y;
                rz = resPos.z;
                return res;
            }
        }
        rx = x2;
        ry = y2;
        rz = z2;
        return false;
    }

    /**
    get height or INVALID_HEIGHT if no height available
    */

    float VMapManager2::GetHeight(uint32 mapId, float x, float y, float z, float maxDist)
    {
        if (IsEnabledHeight() && !sDisableMgr->IsDisabledFor(DISABLE_TYPE_VMAP, mapId, NULL, VMAP_DISABLE_HEIGHT))
        {
            InstanceTreeMap::iterator itr = _instanceTrees.find(mapId);
            if (itr != _instanceTrees.end())
            {
                G3D::Vector3 pos = ConvertPosition(x, y, z);
                float height = itr->second->GetHeight(pos, maxDist);
                if (!(height < G3D::inf()))
                    height = VMAP_INVALID_HEIGHT_VALUE; // No height
                return height;
            }
        }
        return VMAP_INVALID_HEIGHT_VALUE;
    }

    bool VMapManager2::GetAreaInfo(uint32 mapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
    {
        if (!sDisableMgr->IsDisabledFor(DISABLE_TYPE_VMAP, mapId, NULL, VMAP_DISABLE_AREAFLAG))
        {
            InstanceTreeMap::const_iterator itr = _instanceTrees.find(mapId);
            if (itr != _instanceTrees.end())
            {
                G3D::Vector3 pos = ConvertPosition(x, y, z);
                bool res = itr->second->GetAreaInfo(pos, flags, adtId, rootId, groupId);
                z = pos.z;
                return res;
            }
        }
        return false;
    }

    bool VMapManager2::GetLiquidLevel(uint32 mapId, float x, float y, float z, uint8 reqLiquidType, float& level, float& floor, uint32& type) const
    {
        if (!sDisableMgr->IsDisabledFor(DISABLE_TYPE_VMAP, mapId, NULL, VMAP_DISABLE_LIQUIDSTATUS))
        {
            InstanceTreeMap::const_iterator itr = _instanceTrees.find(mapId);
            if (itr != _instanceTrees.end())
            {
                LocationInfo info;
                G3D::Vector3 pos = ConvertPosition(x, y, z);
                if (itr->second->GetLocationInfo(pos, info))
                {
                    floor = info._groundZ;
                    ASSERT(floor < std::numeric_limits<float>::max());
                    type = info._hitModel->GetLiquidType();
                    if (reqLiquidType && !(type & reqLiquidType))
                        return false;
                    if (info._hitInstance->GetLiquidLevel(pos, info, level))
                        return true;
                }
            }
        }
        return false;
    }

    WorldModel* VMapManager2::GetModelInstance(const std::string& basePath, const std::string& fileName)
    {
        ModelFileMap::iterator itr = _loadedModels.find(fileName);
        if (itr == _loadedModels.end())
        {
            WorldModel* model = new WorldModel();
            if (!model->ReadFile(basePath + fileName + ".vmo"))
            {
                sLog->outError("VMapManager2: could not load '%s%s.vmo'", basePath.c_str(), fileName.c_str());
                delete model;
                return NULL;
            }
            sLog->outDebug(LOG_FILTER_MAPS, "VMapManager2: loading file '%s%s'", basePath.c_str(), fileName.c_str());
            itr = _loadedModels.insert(std::pair<std::string, ManagedModel>(fileName, ManagedModel())).first;
            itr->second.SetModel(model);
        }
        itr->second.IncRefCount();
        return itr->second.GetModel();
    }

    void VMapManager2::ReleaseModelInstance(const std::string& fileName)
    {
        ModelFileMap::iterator itr = _loadedModels.find(fileName);
        if (itr == _loadedModels.end())
        {
            sLog->outError("VMapManager2: trying to unload non-loaded file '%s'", fileName.c_str());
            return;
        }
        if (itr->second.DecRefCount() == 0)
        {
            sLog->outDebug(LOG_FILTER_MAPS, "VMapManager2: unloading file '%s'", fileName.c_str());
            delete itr->second.GetModel();
            _loadedModels.erase(itr);
        }
    }

    bool VMapManager2::MapExists(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY)
    {
        return StaticMapTree::CanLoadMap(std::string(basePath), mapId, tileX, tileY);
    }

} // namespace VMAP
