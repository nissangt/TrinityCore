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

#include "MapTree.h"
#include "ModelInstance.h"
#include "VMapManager2.h"
#include "VMapDefinitions.h"
#include "Log.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

#ifndef NO_CORE_FUNCS
    #include "Errors.h"
#else
    #define ASSERT(x)
#endif

namespace VMAP
{
    class MapRayCallback
    {
    public:
        MapRayCallback(ModelInstance* values) : _values(values), _hit(false) { }
        bool operator()(const G3D::Ray& ray, uint32 entry, float& distance, bool stopAtFirstHit = true)
        {
            bool res = _values[entry].IntersectRay(ray, distance, stopAtFirstHit);
            if (res)
                _hit = true;
            return res;
        }
        bool DidHit() const { return _hit; }
    protected:
        ModelInstance* _values;
        bool _hit;
    };

    class AreaInfoCallback
    {
    public:
        AreaInfoCallback(ModelInstance* values) : _values(values) { }
        void operator()(const G3D::Vector3& point, uint32 entry)
        {
#ifdef VMAP_DEBUG
            sLog->outDebug(LOG_FILTER_MAPS, "AreaInfoCallback: trying to intersect '%s'", _values[entry].name.c_str());
#endif
            _values[entry].IntersectPoint(point, _info);
        }
        const AreaInfo& GetInfo() const { return _info; }
    protected:
        ModelInstance* _values;
        AreaInfo _info;
    };

    class LocationInfoCallback
    {
    public:
        LocationInfoCallback(ModelInstance* values, LocationInfo& info) : _values(values), _info(info), _res(false) {}
        void operator()(const G3D::Vector3& point, uint32 entry)
        {
#ifdef VMAP_DEBUG
            sLog->outDebug(LOG_FILTER_MAPS, "LocationInfoCallback: trying to intersect '%s'", _values[entry].name.c_str());
#endif
            if (_values[entry].GetLocationInfo(point, _info))
                _res = true;
        }
        bool GetResult() const { return _res; }
    protected:
        ModelInstance* _values;
        LocationInfo& _info;
        bool _res;
    };

    // static
    std::string StaticMapTree::GetFileName(uint32 mapId, uint32 tileX, uint32 tileY)
    {
        // Filename format: %03u_%02u_%02u.vmtile - mapId, tileY, tileX
        std::stringstream fileName;
        fileName.fill('0');
        fileName << std::setw(3) << mapId << "_";
        fileName << std::setw(2) << tileY << "_" << std::setw(2) << tileX << ".vmtile";
        return fileName.str();
    }

    bool StaticMapTree::GetAreaInfo(G3D::Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
    {
        AreaInfoCallback callback(_values);
        _tree.IntersectPoint(pos, callback);
        const AreaInfo& info = callback.GetInfo();
        if (info._res)
        {
            flags   = info._flags;
            adtId   = info._adtId;
            rootId  = info._rootId;
            groupId = info._groupId;
            pos.z   = info._groundZ;
            return true;
        }
        return false;
    }

    bool StaticMapTree::GetLocationInfo(const G3D::Vector3& pos, LocationInfo& info) const
    {
        LocationInfoCallback callback(_values, info);
        _tree.IntersectPoint(pos, callback);
        return callback.GetResult();
    }

    StaticMapTree::StaticMapTree(uint32 mapId, const std::string& basePath) : _mapId(mapId), _values(NULL), _basePath(basePath)
    {
<<<<<<< HEAD
        if (iBasePath.length() > 0 && (iBasePath[iBasePath.length()-1] != '/' || iBasePath[iBasePath.length()-1] != '\\'))
        {
            iBasePath.push_back('/');
        }
=======
        if (_basePath.length() > 0 && (_basePath[_basePath.length() - 1] != '/' || _basePath[_basePath.length() - 1] != '\\'))
            _basePath.append("/");
>>>>>>> 3eb2e3abbd87a7924b9641f007ba95a3aef72d9b
    }

    //! Make sure to call UnloadMap() to unregister acquired model references before destroying
    StaticMapTree::~StaticMapTree()
    {
        delete[] _values;
    }

    /**
    If intersection is found within maxDist, sets maxDist to intersection distance and returns true.
    Else, maxDist is not modified and returns false;
    */
    bool StaticMapTree::_GetIntersectionTime(const G3D::Ray& ray, float& maxDist, bool stopAtFirstHit) const
    {
        float distance = maxDist;
        MapRayCallback callback(_values);
        _tree.IntersectRay(ray, callback, distance, stopAtFirstHit);
        if (callback.DidHit())
            maxDist = distance;
        return callback.DidHit();
    }

    bool StaticMapTree::IsInLoS(const G3D::Vector3& pos1, const G3D::Vector3& pos2) const
    {
        float maxDist = (pos2 - pos1).magnitude();
        // valid map coords should *never ever* produce float overflow, but this would produce NaNs too
        ASSERT(maxDist < std::numeric_limits<float>::max());
        // prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
            return true;
        // direction with length of 1
        G3D::Ray ray = G3D::Ray::fromOriginAndDirection(pos1, (pos2 - pos1) / maxDist);
        return !_GetIntersectionTime(ray, maxDist, true);
    }

    /**
    When moving from pos1 to pos2 check if we hit an object. Return true and the position if we hit one
    Return the hit pos or the original dest pos
    */
    bool StaticMapTree::GetObjectHitPos(const G3D::Vector3& pos1, const G3D::Vector3& pos2, G3D::Vector3& hitPos, float modifyDist) const
    {
        float maxDist = (pos2 - pos1).magnitude();
        // valid map coords should *never ever* produce float overflow, but this would produce NaNs too
        ASSERT(maxDist < std::numeric_limits<float>::max());
        // prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
        {
            hitPos = pos2;
            return false;
        }

        bool res = false;
        G3D::Vector3 dir = (pos2 - pos1) / maxDist;              // direction with length of 1
        G3D::Ray ray(pos1, dir);
        float dist = maxDist;
        if (_GetIntersectionTime(ray, dist, false))
        {
            hitPos = pos1 + dir * dist;
            if (modifyDist < 0)
            {
                if ((hitPos - pos1).magnitude() > -modifyDist)
                    hitPos = hitPos + dir * modifyDist;
                else
                    hitPos = pos1;
            }
            else
                hitPos = hitPos + dir * modifyDist;
            res = true;
        }
        else
        {
            hitPos = pos2;
            res = false;
        }
        return res;
    }

    float StaticMapTree::GetHeight(const G3D::Vector3& pos, float maxDist) const
    {
        float height = G3D::inf();
        G3D::Vector3 dir = G3D::Vector3(0, 0, -1);
        G3D::Ray ray(pos, dir);   // direction with length of 1
        float dist = maxDist;
        if (_GetIntersectionTime(ray, dist, false))
            height = pos.z - dist;
        return height;
    }

    // static
    bool StaticMapTree::CanLoadMap(const std::string& basePath, uint32 mapId, uint32 tileX, uint32 tileY)
    {
<<<<<<< HEAD
        std::string basePath = vmapPath;
        if (basePath.length() > 0 && (basePath[basePath.length()-1] != '/' || basePath[basePath.length()-1] != '\\'))
            basePath.push_back('/');
        std::string fullname = basePath + VMapManager2::getMapFileName(mapID);
        bool success = true;
        FILE *rf = fopen(fullname.c_str(), "rb");
=======
        std::string path(basePath);
        if (path.length() > 0 && (path[path.length() - 1] != '/' || path[path.length() - 1] != '\\'))
            path.append("/");

        std::string fileName = path + VMapManager2::GetFileName(mapId);
        bool res = true;
        FILE* rf = fopen(fileName.c_str(), "rb");
>>>>>>> 3eb2e3abbd87a7924b9641f007ba95a3aef72d9b
        if (!rf)
            return false;

        // TODO: check magic number when implemented...
        char tiled;
        char chunk[8];
        if (!ReadChunk(rf, chunk, VMAP_MAGIC, 8) || fread(&tiled, sizeof(char), 1, rf) != 1)
        {
            fclose(rf);
            return false;
        }
        fclose(rf);
        if (tiled)
        {
            std::string tileFile = path + GetFileName(mapId, tileX, tileY);
            FILE* tf = fopen(tileFile.c_str(), "rb");
            if (!tf)
                res = false;
            else
            {
                if (!ReadChunk(tf, chunk, VMAP_MAGIC, 8))
                    res = false;
                fclose(tf);
            }
        }
        return res;
    }

    bool StaticMapTree::InitMap(const std::string& fileName, VMapManager2* vm)
    {
        sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::InitMap() : initializing StaticMapTree '%s'", fileName.c_str());

        bool res = true;
        std::string fullName = _basePath + fileName;
        FILE* rf = fopen(fullName.c_str(), "rb");
        if (!rf)
            return false;

        char chunk[8];
        // general info
        if (!ReadChunk(rf, chunk, VMAP_MAGIC, 8))
            res = false;

        char tiled = '\0';
        if (res && fread(&tiled, sizeof(char), 1, rf) != 1)
            res = false;
        _isTiled = bool(tiled);

        // Nodes
        if (res && !ReadChunk(rf, chunk, "NODE", 4))
            res = false;
        if (res)
            res = _tree.ReadFromFile(rf);
        if (res)
        {
            _valuesCount = _tree.Count();
            _values = new ModelInstance[_valuesCount];
        }

        if (res && !ReadChunk(rf, chunk, "GOBJ", 4))
            res = false;

        // global model spawns
        // only non-tiled maps have them, and if so exactly one (so far at least...)
#ifdef VMAP_DEBUG
        sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::InitMap() : map isTiled: %u", static_cast<uint32>(_isTiled));
#endif
        ModelSpawn spawn;
        if (!_isTiled && ModelSpawn::ReadFromFile(rf, spawn))
        {
            WorldModel* model = vm->GetModelInstance(_basePath, spawn._name);
            sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::InitMap() : loading %s", spawn._name.c_str());
            if (model)
            {
                // assume that global model always is the first and only tree value (could be improved...)
                _values[0] = ModelInstance(spawn, model);
                _loadedSpawns[0] = 1;
            }
            else
            {
                res = false;
                sLog->outError("StaticMapTree::InitMap() : could not acquire WorldModel pointer for '%s'", spawn._name.c_str());
            }
        }
        fclose(rf);
        return res;
    }

    void StaticMapTree::UnloadMap(VMapManager2* vm)
    {
        for (LoadedSpawns::iterator itr = _loadedSpawns.begin(); itr != _loadedSpawns.end(); ++itr)
        {
            _values[itr->first].SetUnloaded();
            std::string name = _values[itr->first]._name;
            for (uint32 refCount = 0; refCount < itr->second; ++refCount)
                vm->ReleaseModelInstance(name);
        }
        _loadedSpawns.clear();
        _loadedTiles.clear();
    }

    bool StaticMapTree::LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        if (!_isTiled)
        {
            // currently, core creates grids for all maps, whether it has terrain tiles or not
            // so we need "fake" tile loads to know when we can unload map geometry
            _loadedTiles[PackTileId(tileX, tileY)] = false;
            return true;
        }
        if (!_values)
        {
            sLog->outError("StaticMapTree::LoadMapTile() : tree has not been initialized [%u, %u]", tileX, tileY);
            return false;
        }

        bool res = true;
        std::string tileFile = _basePath + GetFileName(_mapId, tileX, tileY);
        FILE* tf = fopen(tileFile.c_str(), "rb");
        if (tf)
        {
            char chunk[8];
            if (!ReadChunk(tf, chunk, VMAP_MAGIC, 8))
                res = false;

            uint32 spawnsCount = 0;
            if (res && fread(&spawnsCount, sizeof(uint32), 1, tf) != 1)
                res = false;
            for (uint32 i = 0; i < spawnsCount && res; ++i)
            {
                // read model spawns
                ModelSpawn spawn;
                res = ModelSpawn::ReadFromFile(tf, spawn);
                if (res)
                {
                    // acquire model instance
                    WorldModel* model = vm->GetModelInstance(_basePath, spawn._name);
                    if (!model)
                        sLog->outError("StaticMapTree::LoadMapTile() : could not acquire WorldModel pointer [%u, %u]", tileX, tileY);

                    // update tree
                    uint32 refValue;
                    if (fread(&refValue, sizeof(uint32), 1, tf) == 1)
                    {
                        if (!_loadedSpawns.count(refValue))
                        {
#ifdef VMAP_DEBUG
                            if (refValue > _valuesCount)
                            {
                                sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::LoadMapTile() : invalid tree element (%u/%u)", refValue, _valuesCount);
                                continue;
                            }
#endif
                            _values[refValue] = ModelInstance(spawn, model);
                            _loadedSpawns[refValue] = 1;
                        }
                        else
                        {
                            ++_loadedSpawns[refValue];
#ifdef VMAP_DEBUG
                            if (_values[refValue].ID != spawn.ID)
                                sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::LoadMapTile() : trying to load wrong spawn in node");
                            else if (_values[refValue].name != spawn.name)
                                sLog->outDebug(LOG_FILTER_MAPS, "StaticMapTree::LoadMapTile() : name collision on GUID=%u", spawn.ID);
#endif
                        }
                    }
                    else
                        res = false;
                }
            }
            _loadedTiles[PackTileId(tileX, tileY)] = true;
            fclose(tf);
        }
        else
            _loadedTiles[PackTileId(tileX, tileY)] = false;
        return res;
    }

    void StaticMapTree::UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        uint32 tileId = PackTileId(tileX, tileY);
        LoadedTiles::iterator itr = _loadedTiles.find(tileId);
        if (itr == _loadedTiles.end())
        {
            sLog->outError("StaticMapTree::UnloadMapTile() : trying to unload non-loaded tile - Map:%u X:%u Y:%u", _mapId, tileX, tileY);
            return;
        }
        if (itr->second) // file associated with tile
        {
            std::string tileFile = _basePath + GetFileName(_mapId, tileX, tileY);
            FILE* tf = fopen(tileFile.c_str(), "rb");
            if (tf)
            {
                bool res = true;
                char chunk[8];
                if (!ReadChunk(tf, chunk, VMAP_MAGIC, 8))
                    res = false;

                uint32 spawnsCount = 0;
                if (fread(&spawnsCount, sizeof(uint32), 1, tf) != 1)
                    res = false;
                for (uint32 i = 0; i < spawnsCount && res; ++i)
                {
                    // read model spawns
                    ModelSpawn spawn;
                    res = ModelSpawn::ReadFromFile(tf, spawn);
                    if (res)
                    {
                        // Release model instance
                        vm->ReleaseModelInstance(spawn._name);

                        // update tree
                        uint32 refNode;
                        if (fread(&refNode, sizeof(uint32), 1, tf) != 1)
                            res = false;
                        else
                        {
                            if (!_loadedSpawns.count(refNode))
                                sLog->outError("StaticMapTree::UnloadMapTile() : trying to unload non-referenced model '%s' (ID:%u)", spawn._name.c_str(), spawn._id);
                            else if (--_loadedSpawns[refNode] == 0)
                            {
                                _values[refNode].SetUnloaded();
                                _loadedSpawns.erase(refNode);
                            }
                        }
                    }
                }
                fclose(tf);
            }
        }
        _loadedTiles.erase(itr);
    }
}
