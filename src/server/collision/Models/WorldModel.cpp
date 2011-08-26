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

#include "WorldModel.h"
#include "VMapDefinitions.h"
#include "MapTree.h"

template<> struct BoundsTrait<VMAP::GroupModel>
{
    static void GetBounds(const VMAP::GroupModel& o, G3D::AABox& out) { out = o.GetBounds(); }
};

namespace VMAP
{
    bool IntersectTriangle(const MeshTriangle& tri, std::vector<G3D::Vector3>::const_iterator points, const G3D::Ray& ray, float& distance)
    {
        static const float EPS = 1e-5f;

        // See RTR2 ch. 13.7 for the algorithm.
        const G3D::Vector3 e1 = points[tri._b] - points[tri._a];
        const G3D::Vector3 e2 = points[tri._c] - points[tri._a];
        const G3D::Vector3 p(ray.direction().cross(e2));
        const float a = e1.dot(p);

        if (abs(a) < EPS)
            // Determinant is ill-conditioned; abort early
            return false;

        const float f = 1.0f / a;
        const G3D::Vector3 s(ray.origin() - points[tri._a]);
        const float u = f * s.dot(p);

        if (u < 0.0f || u > 1.0f)
            // We hit the plane of the geometry, but outside the geometry
            return false;

        const G3D::Vector3 q(s.cross(e1));
        const float v = f * ray.direction().dot(q);

        if (v < 0.0f || (u + v) > 1.0f)
            // We hit the plane of the triangle, but outside the triangle
            return false;

        const float t = f * e2.dot(q);
        if (t > 0.0f && t < distance)
        {
            // This is a new hit, closer than the previous one
            distance = t;

            /* baryCoord[0] = 1.0 - u - v;
            baryCoord[1] = u;
            baryCoord[2] = v; */

            return true;
        }
        // This hit is after the previous hit, so ignore it
        return false;
    }

    class TriBoundFunc
    {
    public:
        TriBoundFunc(std::vector<G3D::Vector3>& vert) : _itr(vert.begin()) { }
        void operator()(const MeshTriangle& tri, G3D::AABox& out) const
        {
            G3D::Vector3 lo = _itr[tri._a];
            G3D::Vector3 hi = lo;

            lo = (lo.min(_itr[tri._b])).min(_itr[tri._c]);
            hi = (hi.max(_itr[tri._b])).max(_itr[tri._c]);

            out = G3D::AABox(lo, hi);
        }
    protected:
        const std::vector<G3D::Vector3>::const_iterator _itr;
    };

    // ===================== WmoLiquid ==================================
    WmoLiquid::WmoLiquid(uint32 width, uint32 height, const G3D::Vector3& corner, uint32 type):
        _tilesX(width), _tilesY(height), _corner(corner), _type(type)
    {
        _height = new float[(width + 1) * (height + 1)];
        _flags = new uint8[width * height];
    }

    WmoLiquid::WmoLiquid(const WmoLiquid& other): _height(NULL), _flags(NULL)
    {
        *this = other; // use assignment operator...
    }

    WmoLiquid::~WmoLiquid()
    {
        delete[] _height;
        delete[] _flags;
    }

    WmoLiquid& WmoLiquid::operator=(const WmoLiquid &other)
    {
        if (this == &other)
            return *this;
        _tilesX = other._tilesX;
        _tilesY = other._tilesY;
        _corner = other._corner;
        _type = other._type;
        delete[] _height;
        delete[] _flags;
        if (other._height)
        {
            _height = new float[(_tilesX + 1) * (_tilesY + 1)];
            memcpy(_height, other._height, (_tilesX + 1) * (_tilesY + 1) * sizeof(float));
        }
        else
            _height = NULL;
        if (other._flags)
        {
            _flags = new uint8[_tilesX * _tilesY];
            memcpy(_flags, other._flags, _tilesX * _tilesY);
        }
        else
            _flags = NULL;
        return *this;
    }

    bool WmoLiquid::GetLiquidHeight(const G3D::Vector3& pos, float& liqHeight) const
    {
        float txF = (pos.x - _corner.x) / LIQUID_TILE_SIZE;
        uint32 tx = uint32(txF);
        if (txF < 0.0f || tx >= _tilesX)
            return false;

        float tyF = (pos.y - _corner.y) / LIQUID_TILE_SIZE;
        uint32 ty = uint32(tyF);
        if (tyF < 0.0f || ty >= _tilesY)
            return false;

        // check if tile shall be used for liquid level
        // checking for 0x08 *might* be enough, but disabled tiles always are 0x?F:
        if ((_flags[tx + ty * _tilesX] & 0x0F) == 0x0F)
            return false;

        // (dx, dy) coordinates inside tile, in [0,1]^2
        float dx = txF - float(tx);
        float dy = tyF - float(ty);

        /* Tesselate tile to two triangles (not sure if client does it exactly like this)

            ^ dy
            |
          1 x---------x (1,1)
            | (b)   / |
            |     /   |
            |   /     |
            | /   (a) |
            x---------x---> dx
          0           1
        */

        const uint32 rowOffset = _tilesX + 1;
        if (dx > dy) // case (a)
        {
            float sx = _height[tx + 1 +  ty      * rowOffset] - _height[tx     + ty * rowOffset];
            float sy = _height[tx + 1 + (ty + 1) * rowOffset] - _height[tx + 1 + ty * rowOffset];
            liqHeight = _height[tx + ty * rowOffset] + dx * sx + dy * sy;
        }
        else // case (b)
        {
            float sx = _height[tx + 1 + (ty + 1) * rowOffset] - _height[tx + (ty + 1) * rowOffset];
            float sy = _height[tx     + (ty + 1) * rowOffset] - _height[tx +  ty      * rowOffset];
            liqHeight = _height[tx + ty * rowOffset] + dx * sx + dy * sy;
        }
        return true;
    }

    uint32 WmoLiquid::GetFileSize()
    {
        return 2 * sizeof(uint32) + sizeof(G3D::Vector3) +
                (_tilesX + 1) * (_tilesY + 1) * sizeof(float) +
                _tilesX * _tilesY;
    }

    bool WmoLiquid::WriteToFile(FILE* wf)
    {
        bool res = true;
        if (res && fwrite(&_tilesX, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_tilesY, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_corner, sizeof(G3D::Vector3), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_type, sizeof(uint32), 1, wf) != 1)
            res = false;
        uint32 size = (_tilesX + 1) * (_tilesY + 1);
        if (res && fwrite(_height, sizeof(float), size, wf) != size)
            res = false;
        size = _tilesX * _tilesY;
        if (res && fwrite(_flags, sizeof(uint8), size, wf) != size)
            res = false;
        return res;
    }

    bool WmoLiquid::ReadFromFile(FILE* rf, WmoLiquid*& out)
    {
        bool res = true;
        WmoLiquid* liquid = new WmoLiquid();
        if (res && fread(&liquid->_tilesX, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&liquid->_tilesY, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&liquid->_corner, sizeof(G3D::Vector3), 1, rf) != 1)
            res = false;
        if (res && fread(&liquid->_type, sizeof(uint32), 1, rf) != 1)
            res = false;
        uint32 size = (liquid->_tilesX + 1) * (liquid->_tilesY + 1);
        liquid->_height = new float[size];
        if (res && fread(liquid->_height, sizeof(float), size, rf) != size)
            res = false;
        size = liquid->_tilesX * liquid->_tilesY;
        liquid->_flags = new uint8[size];
        if (res && fread(liquid->_flags, sizeof(uint8), size, rf) != size)
            res = false;
        if (!res)
            delete liquid;
        out = liquid;
        return res;
    }

    // ===================== GroupModel ==================================
    GroupModel::GroupModel(const GroupModel& other):
        _bounds(other._bounds), _flags(other._flags), _groupId(other._groupId),
        _vertices(other._vertices), _triangles(other._triangles), _tree(other._tree), _liquid(NULL)
    {
        if (other._liquid)
            _liquid = new WmoLiquid(*other._liquid);
    }

    void GroupModel::SetMeshData(std::vector<G3D::Vector3>& vertices, std::vector<MeshTriangle>& triangles)
    {
        _vertices.swap(vertices);
        _triangles.swap(triangles);

        TriBoundFunc func(_vertices);
        _tree.Build(_triangles, func);
    }

    bool GroupModel::WriteToFile(FILE* wf)
    {
        bool res = true;
        if (res && fwrite(&_bounds, sizeof(G3D::AABox), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_flags, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_groupId, sizeof(uint32), 1, wf) != 1)
            res = false;

        // write vertices
        if (res && fwrite("VERT", 1, 4, wf) != 4)
            res = false;

        uint32 count = _vertices.size();
        uint32 chunkSize = sizeof(uint32) + sizeof(G3D::Vector3) * count;
        if (res && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&count, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (!count) // models without (collision) geometry end here, unsure if they are useful
            return res;

        if (res && fwrite(&_vertices[0], sizeof(G3D::Vector3), count, wf) != count)
            res = false;

        // write triangle mesh
        if (res && fwrite("TRIM", 1, 4, wf) != 4)
            res = false;
        count = _triangles.size();
        chunkSize = sizeof(uint32) + sizeof(MeshTriangle) * count;
        if (res && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&count, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_triangles[0], sizeof(MeshTriangle), count, wf) != count)
            res = false;

        // write mesh BIH
        if (res && fwrite("MBIH", 1, 4, wf) != 4)
            res = false;
        if (res)
            res = _tree.WriteToFile(wf);

        // write liquid data
        if (res && fwrite("LIQU", 1, 4, wf) != 4)
            res = false;
        if (!_liquid)
        {
            chunkSize = 0;
            if (res && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1)
                res = false;
            return res;
        }
        chunkSize = _liquid->GetFileSize();
        if (res && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res)
            res = _liquid->WriteToFile(wf);
        return res;
    }

    bool GroupModel::ReadFromFile(FILE* rf)
    {
        char chunk[8];
        bool res = true;
        uint32 chunkSize = 0;
        uint32 count = 0;
        _triangles.clear();
        _vertices.clear();
        if (_liquid)
        {
            delete _liquid;
            _liquid = NULL;
        }

        if (res && fread(&_bounds, sizeof(G3D::AABox), 1, rf) != 1)
            res = false;
        if (res && fread(&_flags, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&_groupId, sizeof(uint32), 1, rf) != 1)
            res = false;

        // read vertices
        if (res && !ReadChunk(rf, chunk, "VERT", 4))
            res = false;
        if (res && fread(&chunkSize, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&count, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (!count) // models without (collision) geometry end here, unsure if they are useful
            return res;

        if (res)
            _vertices.resize(count);
        if (res && fread(&_vertices[0], sizeof(G3D::Vector3), count, rf) != count)
            res = false;

        // read triangle mesh
        if (res && !ReadChunk(rf, chunk, "TRIM", 4))
            res = false;
        if (res && fread(&chunkSize, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&count, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res)
            _triangles.resize(count);
        if (res && fread(&_triangles[0], sizeof(MeshTriangle), count, rf) != count)
            res = false;

        // read mesh BIH
        if (res && !ReadChunk(rf, chunk, "MBIH", 4))
            res = false;
        if (res)
            res = _tree.ReadFromFile(rf);

        // write liquid data
        if (res && !ReadChunk(rf, chunk, "LIQU", 4))
            res = false;
        if (res && fread(&chunkSize, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && chunkSize > 0)
            res = WmoLiquid::ReadFromFile(rf, _liquid);
        return res;
    }

    class GModelRayCallback
    {
    public:
        GModelRayCallback(const std::vector<MeshTriangle>& triangles, const std::vector<G3D::Vector3>& vertices) :
            _vertItr(vertices.begin()), _triItr(triangles.begin()), _hit(false) { }
        bool operator()(const G3D::Ray& ray, uint32 entry, float& distance, bool /*stopAtFirstHit*/)
        {
            bool res = IntersectTriangle(_triItr[entry], _vertItr, ray, distance);
            if (res)
                _hit = true;
            return _hit;
        }
        bool DidHit() const { return _hit; }
    protected:
        std::vector<G3D::Vector3>::const_iterator _vertItr;
        std::vector<MeshTriangle>::const_iterator _triItr;
        bool _hit;
    };

    bool GroupModel::IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const
    {
        if (_triangles.empty())
            return false;

        GModelRayCallback callback(_triangles, _vertices);
        _tree.IntersectRay(ray, callback, distance, stopAtFirstHit);
        return callback.DidHit();
    }

    bool GroupModel::IsInsideObject(const G3D::Vector3& pos, const G3D::Vector3& down, float& zDist) const
    {
        if (_triangles.empty() || !_bounds.contains(pos))
            return false;

        GModelRayCallback callback(_triangles, _vertices);
        G3D::Vector3 rPos = pos - 0.1f * down;
        float dist = G3D::inf();
        G3D::Ray ray(rPos, down);
        bool hit = IntersectRay(ray, dist, false);
        if (hit)
            zDist = dist - 0.1f;
        return hit;
    }

    bool GroupModel::GetLiquidLevel(const G3D::Vector3& pos, float& liqHeight) const
    {
        if (_liquid)
            return _liquid->GetLiquidHeight(pos, liqHeight);
        return false;
    }

    uint32 GroupModel::GetLiquidType() const
    {
        // convert to type mask, matching MAP_LIQUID_TYPE_* defines in Map.h
        if (_liquid)
            return (1 << _liquid->GetType());
        return 0;
    }

    // ===================== WorldModel ==================================
    void WorldModel::SetGroupModels(std::vector<GroupModel>& models)
    {
        _models.swap(models);
        _groupTree.Build(_models, BoundsTrait<GroupModel>::GetBounds, 1);
    }

    class WModelRayCallBack
    {
    public:
        WModelRayCallBack(const std::vector<GroupModel>& models): _itr(models.begin()), _hit(false) { }
        bool operator()(const G3D::Ray& ray, uint32 entry, float& distance, bool stopAtFirstHit)
        {
            bool res = _itr[entry].IntersectRay(ray, distance, stopAtFirstHit);
            if (res)
                _hit = true;
            return _hit;
        }
        bool DidHit() const { return _hit; }
    protected:
        std::vector<GroupModel>::const_iterator _itr;
        bool _hit;
    };

    bool WorldModel::IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const
    {
        // small M2 workaround, maybe better make separate class with virtual intersection funcs
        // in any case, there's no need to use a bound tree if we only have one submodel
        if (_models.size() == 1)
            return _models[0].IntersectRay(ray, distance, stopAtFirstHit);

        WModelRayCallBack callback(_models);
        _groupTree.IntersectRay(ray, callback, distance, stopAtFirstHit);
        return callback.DidHit();
    }

    class WModelAreaCallback
    {
    public:
        WModelAreaCallback(const std::vector<GroupModel>& values, const G3D::Vector3& down) :
            _begin(values.begin()), _end(values.end()), _minVol(G3D::inf()), _zDist(G3D::inf()), _zVec(down) { }
        void operator()(const G3D::Vector3& point, uint32 entry)
        {
            float groupZ;
            //float pVol = prims[entry].GetBound().volume();
            //if(pVol < minVol)
            //{
                /* if (prims[entry].iBound.contains(point)) */
                if (_begin[entry].IsInsideObject(point, _zVec, groupZ))
                {
                    //minVol = pVol;
                    //hit = prims + entry;
                    if (groupZ < _zDist)
                    {
                        _zDist = groupZ;
                        _end = _begin + entry;
                    }
#ifdef VMAP_DEBUG
                    const GroupModel& gm = _begin[entry];
                    const G3D::Vector3& lo = gm.GetBounds().low();
                    const G3D::Vector3& hi = gm.GetBounds().high();
                    printf("%10u %8X %7.3f, %7.3f, %7.3f | %7.3f, %7.3f, %7.3f | z=%f, p_z=%f\n",
                           gm.GetGroupId(), gm.GetFlags(), lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, groupZ, point.z);
#endif
                }
            //}
            //std::cout << "trying to intersect '" << prims[entry].name << "'\n";
        }
        std::vector<GroupModel>::const_iterator GetEnd() const { return _end; }
        float GetZDist() const { return _zDist; }
    protected:
        std::vector<GroupModel>::const_iterator _begin;
        std::vector<GroupModel>::const_iterator _end;
        float _minVol;
        float _zDist;
        G3D::Vector3 _zVec;
    };

    bool WorldModel::IntersectPoint(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, AreaInfo& info) const
    {
        if (_models.empty())
            return false;

        WModelAreaCallback callback(_models, down);
        _groupTree.IntersectPoint(p, callback);
        if (callback.GetEnd() != _models.end())
        {
            info._rootId = _rootId;
            info._groupId = callback.GetEnd()->GetGroupId();
            info._flags = callback.GetEnd()->GetFlags();
            info._res = true;
            dist = callback.GetZDist();
            return true;
        }
        return false;
    }

    bool WorldModel::GetLocationInfo(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, LocationInfo& info) const
    {
        if (_models.empty())
            return false;

        WModelAreaCallback callback(_models, down);
        _groupTree.IntersectPoint(p, callback);
        if (callback.GetEnd() != _models.end())
        {
            info._hitModel = &(*callback.GetEnd());
            dist = callback.GetZDist();
            return true;
        }
        return false;
    }

    bool WorldModel::WriteFile(const std::string& fileName)
    {
        FILE* wf = fopen(fileName.c_str(), "wb");
        if (!wf)
            return false;

        bool res = true;
        res = (fwrite(VMAP_MAGIC, 1, 8, wf) == 8);
        if (res && fwrite("WMOD", 1, 4, wf) != 4)
            res = false;
        uint32 chunkSize = sizeof(uint32) + sizeof(uint32);
        if (res && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1)
            res = false;
        if (res && fwrite(&_rootId, sizeof(uint32), 1, wf) != 1)
            res = false;

        // write group models
        uint32 count = _models.size();
        if (count)
        {
            if (res && fwrite("GMOD", 1, 4, wf) != 4)
                res = false;
            //chunkSize = sizeof(uint32)+ sizeof(GroupModel)*count;
            //if (result && fwrite(&chunkSize, sizeof(uint32), 1, wf) != 1) result = false;
            if (res && fwrite(&count, sizeof(uint32), 1, wf) != 1)
                res = false;
            for (uint32 i = 0; i < count && res; ++i)
                res = _models[i].WriteToFile(wf);

            // write group BIH
            if (res && fwrite("GBIH", 1, 4, wf) != 4)
                res = false;
            if (res)
                res = _groupTree.WriteToFile(wf);
        }
        fclose(wf);
        return res;
    }

    bool WorldModel::ReadFile(const std::string& fileName)
    {
        FILE* rf = fopen(fileName.c_str(), "rb");
        if (!rf)
            return false;

        bool res = true;
        char chunk[8];                          // Ignore the added magic header
        if (!ReadChunk(rf, chunk, VMAP_MAGIC, 8))
            res = false;
        if (res && !ReadChunk(rf, chunk, "WMOD", 4))
            res = false;

        uint32 chunkSize = 0;
        if (res && fread(&chunkSize, sizeof(uint32), 1, rf) != 1)
            res = false;
        if (res && fread(&_rootId, sizeof(uint32), 1, rf) != 1)
            res = false;

        // read group models
        if (res && ReadChunk(rf, chunk, "GMOD", 4))
        {
            //if (fread(&chunkSize, sizeof(uint32), 1, rf) != 1) result = false;
            uint32 count = 0;
            if (res && fread(&count, sizeof(uint32), 1, rf) != 1)
                res = false;
            if (res)
                _models.resize(count);
            //if (result && fread(&groupModels[0], sizeof(GroupModel), count, rf) != count) result = false;
            for (uint32 i = 0; i < count && res; ++i)
                res = _models[i].ReadFromFile(rf);

            // read group BIH
            if (res && !ReadChunk(rf, chunk, "GBIH", 4))
                res = false;
            if (res)
                res = _groupTree.ReadFromFile(rf);
        }
        fclose(rf);
        return res;
    }
}
