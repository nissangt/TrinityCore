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

#include "ModelInstance.h"
#include "WorldModel.h"
#include "MapTree.h"
#include "VMapDefinitions.h"

namespace VMAP
{
    ModelInstance::ModelInstance(const ModelSpawn& spawn, WorldModel* model): ModelSpawn(spawn), _model(model)
    {
        _invRotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * _rotation.y / 180.f,
                                                        G3D::pi() * _rotation.x / 180.f,
                                                        G3D::pi() * _rotation.z / 180.f).inverse();
        _invScale = 1.0f / _scale;
    }

    bool ModelInstance::IntersectRay(const G3D::Ray& ray, float& maxDist, bool stopAtFirstHit) const
    {
        if (!_model)
            return false;

        float time = ray.intersectionTime(_bounds);
        if (time == G3D::inf())
            return false;

//        std::cout << "Ray crosses bound of '" << name << "'\n";
/*        std::cout << "ray from:" << pRay.origin().x << ", " << pRay.origin().y << ", " << pRay.origin().z
                  << " dir:" << pRay.direction().x << ", " << pRay.direction().y << ", " << pRay.direction().z
                  << " t/tmax:" << time << '/' << pMaxDist;
        std::cout << "\nBound lo:" << iBound.low().x << ", " << iBound.low().y << ", " << iBound.low().z << " hi: "
                  << iBound.high().x << ", " << iBound.high().y << ", " << iBound.high().z << std::endl; */

        // child bounds are defined in object space:
        G3D::Vector3 p = _invRotation * (ray.origin() - _pos) * _invScale;
        G3D::Ray modRay(p, _invRotation * ray.direction());
        float distance = maxDist * _invScale;
        bool hit = _model->IntersectRay(modRay, distance, stopAtFirstHit);
        if (hit)
        {
            distance *= _scale;
            maxDist = distance;
        }
        return hit;
    }

    void ModelInstance::IntersectPoint(const G3D::Vector3& p, AreaInfo& info) const
    {
        if (!_model)
        {
#ifdef VMAP_DEBUG
            std::cout << "<object not loaded>\n";
#endif
            return;
        }

        // M2 files don't contain area info, only WMO files
        if (_flags & MOD_M2)
            return;
        if (!_bounds.contains(p))
            return;

        // child bounds are defined in object space:
        G3D::Vector3 posModel = _invRotation * (p - _pos) * _invScale;
        G3D::Vector3 zDirModel = _invRotation * G3D::Vector3(0.f, 0.f, -1.f);
        float zDist;
        if (_model->IntersectPoint(posModel, zDirModel, zDist, info))
        {
            G3D::Vector3 modelGround = posModel + zDist * zDirModel;
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float worldZ = ((modelGround * _invRotation) * _scale + _pos).z;
            if (info._groundZ < worldZ)
            {
                info._groundZ = worldZ;
                info._adtId = _adtId;
            }
        }
    }

    bool ModelInstance::GetLocationInfo(const G3D::Vector3& p, LocationInfo& info) const
    {
        if (!_model)
        {
#ifdef VMAP_DEBUG
            std::cout << "<object not loaded>\n";
#endif
            return false;
        }

        // M2 files don't contain area info, only WMO files
        if (_flags & MOD_M2)
            return false;
        if (!_bounds.contains(p))
            return false;
        // child bounds are defined in object space:
        G3D::Vector3 posModel = _invRotation * (p - _pos) * _invScale;
        G3D::Vector3 zDirModel = _invRotation * G3D::Vector3(0.0f, 0.0f, -1.0f);
        float zDist;
        if (_model->GetLocationInfo(posModel, zDirModel, zDist, info))
        {
            G3D::Vector3 modelGround = posModel + zDist * zDirModel;
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float worldZ = ((modelGround * _invRotation) * _scale + _pos).z;
            if (info._groundZ < worldZ) // hm...could it be handled automatically with zDist at intersection?
            {
                info._groundZ = worldZ;
                info._hitInstance = this;
                return true;
            }
        }
        return false;
    }

    bool ModelInstance::GetLiquidLevel(const G3D::Vector3& p, LocationInfo& info, float& liqHeight) const
    {
        // child bounds are defined in object space:
        G3D::Vector3 posModel = _invRotation * (p - _pos) * _invScale;
        // G3D::Vector3 zDirModel = iInvRot * Vector3(0.f, 0.f, -1.f);
        float zDist;
        if (info._hitModel->GetLiquidLevel(posModel, zDist))
        {
            // calculate world height (zDist in model coords):
            // assume WMO not tilted (wouldn't make much sense anyway)
            liqHeight = zDist * _scale + _pos.z;
            return true;
        }
        return false;
    }

    bool ModelSpawn::ReadFromFile(FILE* rf, ModelSpawn& spawn)
    {
        uint32 check = 0;
        check += fread(&spawn._flags, sizeof(uint32), 1, rf);
        // EoF?
        if (!check)
        {
            if (ferror(rf))
                std::cout << "Error reading ModelSpawn!\n";
            return false;
        }
        check += fread(&spawn._adtId, sizeof(uint16), 1, rf);
        check += fread(&spawn._id, sizeof(uint32), 1, rf);
        check += fread(&spawn._pos, sizeof(float), 3, rf);
        check += fread(&spawn._rotation, sizeof(float), 3, rf);
        check += fread(&spawn._scale, sizeof(float), 1, rf);
        bool hasBounds = (spawn._flags & MOD_HAS_BOUND);
        if (hasBounds) // only WMOs have bound in MPQ, only available after computation
        {
            G3D::Vector3 low;
            check += fread(&low, sizeof(float), 3, rf);
            G3D::Vector3 high;
            check += fread(&high, sizeof(float), 3, rf);
            spawn._bounds = G3D::AABox(low, high);
        }
        uint32 nameLen;
        check += fread(&nameLen, sizeof(uint32), 1, rf);
        if (check != uint32(hasBounds ? 17 : 11))
        {
            std::cout << "Error reading ModelSpawn!\n";
            return false;
        }
        char nameBuff[500];
        if (nameLen > 500) // file names should never be that long, must be file error
        {
            std::cout << "Error reading ModelSpawn, file name too long!\n";
            return false;
        }
        check = fread(nameBuff, sizeof(char), nameLen, rf);
        if (check != nameLen)
        {
            std::cout << "Error reading ModelSpawn!\n";
            return false;
        }
        spawn._name = std::string(nameBuff, nameLen);
        return true;
    }

    bool ModelSpawn::WriteToFile(FILE* wf, const ModelSpawn& spawn)
    {
        uint32 check = 0;
        check += fwrite(&spawn._flags, sizeof(uint32), 1, wf);
        check += fwrite(&spawn._adtId, sizeof(uint16), 1, wf);
        check += fwrite(&spawn._id, sizeof(uint32), 1, wf);
        check += fwrite(&spawn._pos, sizeof(float), 3, wf);
        check += fwrite(&spawn._rotation, sizeof(float), 3, wf);
        check += fwrite(&spawn._scale, sizeof(float), 1, wf);
        bool hasBounds = (spawn._flags & MOD_HAS_BOUND);
        if (hasBounds) // only WMOs have bound in MPQ, only available after computation
        {
            check += fwrite(&spawn._bounds.low(), sizeof(float), 3, wf);
            check += fwrite(&spawn._bounds.high(), sizeof(float), 3, wf);
        }
        uint32 nameLen = spawn._name.length();
        check += fwrite(&nameLen, sizeof(uint32), 1, wf);
        if (check != uint32(hasBounds ? 17 : 11))
            return false;
        check = fwrite(spawn._name.c_str(), sizeof(char), nameLen, wf);
        if (check != nameLen)
            return false;
        return true;
    }
}
