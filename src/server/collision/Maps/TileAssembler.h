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

#ifndef _TILEASSEMBLER_H_
#define _TILEASSEMBLER_H_

#include <G3D/Vector3.h>
#include <G3D/Matrix3.h>
#include <map>

#include "ModelInstance.h"

namespace VMAP
{
    /**
    This Class is used to convert raw vector data into balanced BSP-Trees.
    To start the conversion call convertWorld().
    */
    class ModelPosition
    {
    private:
        G3D::Matrix3 _rotation;

    public:
        G3D::Vector3 _pos;
        G3D::Vector3 _dir;
        float _scale;

        void Init()
        {
            _rotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * _dir.y / 180.f,
                                                         G3D::pi() * _dir.x / 180.f,
                                                         G3D::pi() * _dir.z / 180.f);
        }
        G3D::Vector3 Transform(const G3D::Vector3& v) const;
        void MoveToBasePos(const G3D::Vector3& basePos) { _pos -= basePos; }
    };

    typedef std::map<uint32, ModelSpawn> UniqueEntryMap;
    typedef std::multimap<uint32, uint32> TileMap;
    typedef bool (*FilterMethod)(char*);

    struct MapSpawns
    {
        UniqueEntryMap _entries;
        TileMap _tiles;
    };

    typedef std::map<uint32, MapSpawns*> MapData;

    class TileAssembler
    {
    private:
        std::string _destDir;
        std::string _srcDir;
        FilterMethod* _filterMethod;
        G3D::Table<std::string, uint32> _uniqueIds;
        uint32 _currentId;
        MapData _mapData;

    public:
        TileAssembler(const std::string& srcDir, const std::string& destDir);
        virtual ~TileAssembler();

        bool ConvertWorld2();
        bool ReadMapSpawns();
        bool CalculateTransformedBound(ModelSpawn& spawn);
        bool ConvertRawFile(const std::string& fileName);

        void SetModelNameFilterMethod(FilterMethod* filterMethod) { _filterMethod = filterMethod; }
        std::string GetDirEntryNameFromModName(uint32 mapId, const std::string& modName);
    };
}                                                           // VMAP

#endif                                                      /*_TILEASSEMBLER_H_*/
