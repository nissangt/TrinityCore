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

#ifndef _MODELINSTANCE_H_
#define _MODELINSTANCE_H_

#include <G3D/Matrix3.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>

#include "Define.h"

namespace VMAP
{
    class WorldModel;
    struct AreaInfo;
    struct LocationInfo;

    enum ModelFlags
    {
        MOD_M2          = 0x1,
        MOD_WORLDSPAWN  = 0x2,
        MOD_HAS_BOUND   = 0x4
    };

    class ModelSpawn
    {
    public:
        uint32 _flags;
        uint16 _adtId;
        uint32 _id;
        G3D::Vector3 _pos;
        G3D::Vector3 _rotation;
        float _scale;
        G3D::AABox _bounds;
        std::string _name;

        const G3D::AABox& GetBounds() const { return _bounds; }

        bool operator==(const ModelSpawn& other) const { return _id == other._id; }

        static bool ReadFromFile(FILE *rf, ModelSpawn &spawn);
        static bool WriteToFile(FILE *rw, const ModelSpawn &spawn);
    };

    class ModelInstance : public ModelSpawn
    {
    public:
        ModelInstance() : _model(NULL) { }
        ModelInstance(const ModelSpawn& spawn, WorldModel* model);

        void SetUnloaded() { _model = NULL; }
        bool IntersectRay(const G3D::Ray& ray, float& maxDist, bool stopAtFirstHit) const;
        void IntersectPoint(const G3D::Vector3& p, AreaInfo& info) const;
        bool GetLocationInfo(const G3D::Vector3& p, LocationInfo& info) const;
        bool GetLiquidLevel(const G3D::Vector3& p, LocationInfo& info, float& liqHeight) const;

    protected:
        G3D::Matrix3 _invRotation;
        float _invScale;
        WorldModel* _model;
    };
} // namespace VMAP

#endif // _MODELINSTANCE
