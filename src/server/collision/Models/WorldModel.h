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

#ifndef _WORLDMODEL_H
#define _WORLDMODEL_H

#include <G3D/HashTrait.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include "BoundingIntervalHierarchy.h"

#include "Define.h"

namespace VMAP
{
    class TreeNode;
    struct AreaInfo;
    struct LocationInfo;

    class MeshTriangle
    {
    public:
        MeshTriangle() { }
        MeshTriangle(uint32 a, uint32 b, uint32 c) : _a(a), _b(b), _c(c) { }

        uint32 _a;
        uint32 _b;
        uint32 _c;
    };

    class WmoLiquid
    {
    public:
        WmoLiquid(uint32 width, uint32 height, const G3D::Vector3& corner, uint32 type);
        WmoLiquid(const WmoLiquid& other);
        ~WmoLiquid();

        WmoLiquid& operator=(const WmoLiquid &other);

        bool GetLiquidHeight(const G3D::Vector3& pos, float& liqHeight) const;

        uint32 GetType() const { return _type; }
        float* GetHeightStorage() { return _height; }
        uint8* GetFlagsStorage() { return _flags; }
        uint32 GetFileSize();

        bool WriteToFile(FILE* wf);
        static bool ReadFromFile(FILE* rf, WmoLiquid*& liquid);

    private:
        WmoLiquid(): _height(NULL), _flags(NULL) { }

        uint32 _tilesX;  //!< number of tiles in x direction, each
        uint32 _tilesY;
        G3D::Vector3 _corner; //!< the lower corner
        uint32 _type;    //!< liquid type
        float* _height;  //!< (tilesX + 1)*(tilesY + 1) height values
        uint8* _flags;   //!< info if liquid tile is used
    };

    /*! holding additional info for WMO group files */
    class GroupModel
    {
    public:
        GroupModel(): _liquid(NULL) { }
        GroupModel(const GroupModel& other);
        GroupModel(uint32 flags, uint32 groupId, const G3D::AABox& bounds) :
            _bounds(bounds), _flags(flags), _groupId(groupId), _liquid(NULL) { }
        ~GroupModel() { delete _liquid; }

        //! pass mesh data to object and create BIH. Passed vectors get swapped with old geometry!
        void SetMeshData(std::vector<G3D::Vector3>& vertices, std::vector<MeshTriangle>& triangles);
        void SetLiquidData(WmoLiquid* liquid) { _liquid = liquid; }

        bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const;
        bool IsInsideObject(const G3D::Vector3& pos, const G3D::Vector3& down, float& distZ) const;
        bool GetLiquidLevel(const G3D::Vector3& pos, float& liqHeight) const;
        uint32 GetLiquidType() const;

        const G3D::AABox& GetBounds() const { return _bounds; }
        uint32 GetFlags() const { return _flags; }
        uint32 GetGroupId() const { return _groupId; }
        
        bool WriteToFile(FILE* wf);
        bool ReadFromFile(FILE* rf);
    protected:
        G3D::AABox _bounds;
        uint32 _flags;      // 0x8 outdor; 0x2000 indoor
        uint32 _groupId;
        std::vector<G3D::Vector3> _vertices;
        std::vector<MeshTriangle> _triangles;
        BIH _tree;
        WmoLiquid* _liquid;
    };

    /*! Holds a model (converted M2 or WMO) in its original coordinate space */
    class WorldModel
    {
    public:
        WorldModel(): _rootId(0) {}

        //! pass group models to WorldModel and create BIH. Passed vector is swapped with old geometry!
        void SetGroupModels(std::vector<GroupModel>& models);
        void SetRootId(uint32 rootId) { _rootId = rootId; }

        bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const;
        bool IntersectPoint(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, AreaInfo& info) const;
        bool GetLocationInfo(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, LocationInfo& info) const;

        bool WriteFile(const std::string& fileName);
        bool ReadFile(const std::string& fileName);
    protected:
        uint32 _rootId;
        std::vector<GroupModel> _models;
        BIH _groupTree;
    };
} // namespace VMAP

#endif // _WORLDMODEL_H
