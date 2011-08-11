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
#include "TileAssembler.h"
#include "MapTree.h"
#include "BoundingIntervalHierarchy.h"
#include "VMapDefinitions.h"

#include <set>
#include <iomanip>
#include <sstream>
#include <iomanip>

template<> struct BoundsTrait<VMAP::ModelSpawn*>
{
    static void GetBounds(const VMAP::ModelSpawn* const& o, G3D::AABox& res) { res = o->GetBounds(); }
};

namespace VMAP
{
    bool ReadChunk(FILE* rf, char* dest, const char* compare, uint32 len)
    {
        if (fread(dest, sizeof(char), len, rf) != len)
            return false;
        return memcmp(dest, compare, len) == 0;
    }

    G3D::Vector3 ModelPosition::Transform(const G3D::Vector3& v) const
    {
        G3D::Vector3 out = v * _scale;
        out = _rotation * out;
        return out;
    }

    TileAssembler::TileAssembler(const std::string& srcDir, const std::string& destDir)
    {
        _currentId = 0;
        _filterMethod = NULL;
        _srcDir = srcDir;
        _destDir = destDir;
    }

    // virtual
    TileAssembler::~TileAssembler() { }

    bool TileAssembler::ConvertWorld2()
    {
        std::set<std::string> spawnedModelFiles;
        bool res = ReadMapSpawns();
        if (!res)
            return false;

        // export Map data
        for (MapData::iterator itr = _mapData.begin(); itr != _mapData.end() && res; ++itr)
        {
            // build global map tree
            std::vector<ModelSpawn*> mapSpawns;
            UniqueEntryMap::iterator entry;
            printf("Calculating model bounds for map %u...\n", itr->first);
            for (entry = itr->second->_entries.begin(); entry != itr->second->_entries.end(); ++entry)
            {
                // M2 models don't have a bound set in WDT/ADT placement data, i still think they're not used for LoS at all on retail
                if (entry->second._flags & MOD_M2)
                {
                    if (!CalculateTransformedBound(entry->second))
                        break;
                }
                else if (entry->second._flags & MOD_WORLDSPAWN) // WMO maps and terrain maps use different origin, so we need to adapt :/
                {
                    // TODO: remove extractor hack and uncomment below line:
                    // entry->second.iPos += Vector3(533.33333f*32, 533.33333f*32, 0.f);
                    entry->second._bounds = entry->second._bounds + G3D::Vector3(533.33333f * 32, 533.33333f * 32, 0.0f);
                }
                mapSpawns.push_back(&(entry->second));
                spawnedModelFiles.insert(entry->second._name);
            }

            printf("Creating map tree for map %u...\n", itr->first);
            BIH tree;
            tree.Build(mapSpawns, BoundsTrait<ModelSpawn*>::GetBounds);

            // TODO: possibly move this code to StaticMapTree class
            std::map<uint32, uint32> modelNodeIdx;
            for (uint32 i = 0; i < mapSpawns.size(); ++i)
                modelNodeIdx.insert(std::pair<uint32, uint32>(mapSpawns[i]->_id, i));

            // write map tree file
            std::stringstream fileName;
            fileName << _destDir << "/" << std::setfill('0') << std::setw(3) << itr->first << ".vmtree";
            FILE* mf = fopen(fileName.str().c_str(), "wb");
            if (!mf)
            {
                res = false;
                printf("Cannot open %s\n", fileName.str().c_str());
                break;
            }

            // general info
            if (res && fwrite(VMAP_MAGIC, 1, 8, mf) != 8)
                res = false;

            uint32 tileId = StaticMapTree::PackTileId(65, 65);
            std::pair<TileMap::iterator, TileMap::iterator> range = itr->second->_tiles.equal_range(tileId);
            // only maps without terrain (tiles) have global WMO
            char isTiled = range.first == range.second;
            if (res && fwrite(&isTiled, sizeof(char), 1, mf) != 1)
                res = false;
            // Nodes
            if (res && fwrite("NODE", 4, 1, mf) != 1)
                res = false;
            if (res)
                res = tree.WriteToFile(mf);
            // global map spawns (WDT), if any (most instances)
            if (res && fwrite("GOBJ", 4, 1, mf) != 1)
                res = false;

            for (TileMap::iterator tileItr = range.first; tileItr != range.second && res; ++tileItr)
                res = ModelSpawn::WriteToFile(mf, itr->second->_entries[tileItr->second]);
            fclose(mf);

            // Write map tile files, similar to ADT files, only with extra BSP tree node info
            TileMap& tiles = itr->second->_tiles;
            for (TileMap::iterator tileItr = tiles.begin(); tileItr != tiles.end(); ++tileItr)
            {
                const ModelSpawn &spawn = itr->second->_entries[tileItr->second];
                if (spawn._flags & MOD_WORLDSPAWN) // WDT spawn, saved as tile 65/65 currently...
                    continue;

                uint32 spawnsCount = tiles.count(tileItr->first);

                uint32 x;
                uint32 y;
                StaticMapTree::UnpackTileId(tileItr->first, x, y);

                std::stringstream fileName;
                fileName.fill('0');
                fileName << _destDir << "/" << std::setw(3) << itr->first << "_";
                fileName << std::setw(2) << x << "_" << std::setw(2) << y << ".vmtile";

                FILE* tf = fopen(fileName.str().c_str(), "wb");
                // file header
                if (res && fwrite(VMAP_MAGIC, 1, 8, tf) != 8)
                    res = false;
                // write number of tile spawns
                if (res && fwrite(&spawnsCount, sizeof(uint32), 1, tf) != 1)
                    res = false;
                // write tile spawns
                for (uint32 i = 0; i < spawnsCount; ++i)
                {
                    if (i)
                        ++tileItr;
                    const ModelSpawn& spawn2 = itr->second->_entries[tileItr->second];
                    res &= ModelSpawn::WriteToFile(tf, spawn2);
                    // MapTree nodes to update when loading tile:
                    std::map<uint32, uint32>::iterator nIdx = modelNodeIdx.find(spawn2._id);
                    if (res && fwrite(&nIdx->second, sizeof(uint32), 1, tf) != 1)
                        res = false;
                }
                fclose(tf);
            }
            // break; //test, extract only first map; TODO: remove this line
        }

        // export objects
        std::cout << "\nConverting Model Files" << std::endl;
        for (std::set<std::string>::iterator itr = spawnedModelFiles.begin(); itr != spawnedModelFiles.end(); ++itr)
        {
            std::cout << "Converting " << *itr << std::endl;
            if (!ConvertRawFile(*itr))
            {
                std::cout << "error converting " << *itr << std::endl;
                res = false;
                break;
            }
        }

        // cleanup
        for (MapData::iterator itr = _mapData.begin(); itr != _mapData.end(); ++itr)
            delete itr->second;

        return res;
    }

    bool TileAssembler::ReadMapSpawns()
    {
        std::string fileName = _srcDir + "/dir_bin";
        FILE* df = fopen(fileName.c_str(), "rb");
        if (!df)
        {
            printf("Could not read dir_bin file!\n");
            return false;
        }
        printf("Read coordinate mapping...\n");
        uint32 mapId;
        uint32 tileX;
        uint32 tileY;
        G3D::Vector3 v1;
        G3D::Vector3 v2;
        ModelSpawn spawn;
        uint32 check = 0;
        while (!feof(df))
        {
            check = 0;
            // read mapID, tileX, tileY, Flags, adtID, ID, Pos, Rot, Scale, Bound_lo, Bound_hi, name
            check += fread(&mapId, sizeof(uint32), 1, df);
            if (check == 0) // EoF...
                break;
            check += fread(&tileX, sizeof(uint32), 1, df);
            check += fread(&tileY, sizeof(uint32), 1, df);
            if (!ModelSpawn::ReadFromFile(df, spawn))
                break;

            MapSpawns* current;
            MapData::iterator itr = _mapData.find(mapId);
            if (itr == _mapData.end())
            {
                printf("spawning Map %d\n", mapId);
                _mapData[mapId] = current = new MapSpawns();
            }
            else
                current = itr->second;
            current->_entries.insert(std::pair<uint32, ModelSpawn>(spawn._id, spawn));
            current->_tiles.insert(std::pair<uint32, uint32>(StaticMapTree::PackTileId(tileX, tileY), spawn._id));
        }
        bool res = (ferror(df) == 0);
        fclose(df);
        return res;
    }

    bool TileAssembler::CalculateTransformedBound(ModelSpawn& spawn)
    {
        std::string fileName(_srcDir + "/" + spawn._name);

        ModelPosition pos;
        pos._dir = spawn._rotation;
        pos._scale = spawn._scale;
        pos.Init();

        FILE* rf = fopen(fileName.c_str(), "rb");
        if (!rf)
        {
            printf("ERROR: Can't open model file: %s\n", fileName.c_str());
            return false;
        }

        G3D::AABox bounds;
        bool emptyBounds = true;
        char ident[8];

        int readOperation = 1;

        // temporary use defines to simplify read/check code (close file and return at fail)
        #define READ_OR_RETURN(V, S) if(fread((V), (S), 1, rf) != 1) { \
                                        fclose(rf); printf("readfail, op = %i\n", readOperation); return(false); }readOperation++;
        // only use this for array deletes
        #define READ_OR_RETURN_WITH_DELETE(V, S) if(fread((V), (S), 1, rf) != 1) { \
                                        fclose(rf); printf("readfail, op = %i\n", readOperation); delete[] V; return(false); }readOperation++;

        #define CMP_OR_RETURN(V, S)  if(strcmp((V), (S)) != 0)        { \
                                        fclose(rf); printf("cmpfail, %s!=%s\n", V, S);return(false); }

        READ_OR_RETURN(&ident, 8);
        CMP_OR_RETURN(ident, "VMAP003");

        // we have to read one int. This is needed during the export and we have to skip it here
        uint32 vectorsCount;
        READ_OR_RETURN(&vectorsCount, sizeof(vectorsCount));

        uint32 groups;
        uint32 wmoRootId;
        char blockId[5];
        blockId[4] = 0;
        int blockSize;
        float* vectorArray = 0;

        READ_OR_RETURN(&groups, sizeof(uint32));
        READ_OR_RETURN(&wmoRootId, sizeof(uint32));
        if (groups != 1)
            printf("Warning: '%s' does not seem to be a M2 model!\n", fileName.c_str());

        for (uint32 groupId = 0; groupId < groups; ++groupId) // should be only one for M2 files...
        {
            fseek(rf, 3 * sizeof(uint32) + 6 * sizeof(float), SEEK_CUR);

            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "GRP ");
            READ_OR_RETURN(&blockSize, sizeof(int));
            fseek(rf, blockSize, SEEK_CUR);

            // ---- indexes
            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "INDX");
            READ_OR_RETURN(&blockSize, sizeof(int));
            fseek(rf, blockSize, SEEK_CUR);

            // ---- vectors
            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "VERT");
            READ_OR_RETURN(&blockSize, sizeof(int));
            uint32 vectors;
            READ_OR_RETURN(&vectors, sizeof(uint32));

            if (vectors > 0)
            {
                vectorArray = new float[vectors * 3];
                READ_OR_RETURN_WITH_DELETE(vectorArray, vectors * sizeof(float) * 3);
            }
            else
            {
                std::cout << "error: model '" << spawn._name << "' has no geometry!" << std::endl;
                fclose(rf);
                return false;
            }

            for (uint32 i = 0, index = 0; index < vectors; ++index, i += 3)
            {
                G3D::Vector3 v = G3D::Vector3(vectorArray[i + 0], vectorArray[i + 1], vectorArray[i + 2]);
                v = pos.Transform(v);

                if (emptyBounds)
                {
                    bounds = G3D::AABox(v, v);
                    emptyBounds = false;
                }
                else
                    bounds.merge(v);
            }
            delete[] vectorArray;
            // drop of temporary use defines
            #undef READ_OR_RETURN
            #undef READ_OR_RETURN_WITH_DELETE
            #undef CMP_OR_RETURN
        }
        spawn._bounds = bounds + spawn._pos;
        spawn._flags |= MOD_HAS_BOUND;
        fclose(rf);
        return true;
    }

    struct WMOLiquidHeader
    {
        int xverts, yverts, xtiles, ytiles;
        float pos_x;
        float pos_y;
        float pos_z;
        short type;
    };

    bool TileAssembler::ConvertRawFile(const std::string& fileName)
    {
        bool res = true;
        std::string path(_srcDir);
        if (path.length() > 0)
            path.append("/");
        path.append(fileName);

        FILE* rf = fopen(path.c_str(), "rb");
        if (!rf)
        {
            printf("ERROR: Can't open model file in form: %s", fileName.c_str());
            printf("...                          or form: %s", path.c_str() );
            return false;
        }

        char ident[8];

        int readOperation = 1;

        // temporary use defines to simplify read/check code (close file and return at fail)
        #define READ_OR_RETURN(V, S) if(fread((V), (S), 1, rf) != 1) { \
                                        fclose(rf); printf("readfail, op = %i\n", readOperation); return(false); }readOperation++;
        #define READ_OR_RETURN_WITH_DELETE(V, S) if(fread((V), (S), 1, rf) != 1) { \
                                        fclose(rf); printf("readfail, op = %i\n", readOperation); delete[] V; return(false); }readOperation++;
        #define CMP_OR_RETURN(V, S)  if(strcmp((V), (S)) != 0)        { \
                                        fclose(rf); printf("cmpfail, %s!=%s\n", V, S);return(false); }

        READ_OR_RETURN(&ident, 8);
        CMP_OR_RETURN(ident, "VMAP003");

        // we have to read one int. This is needed during the export and we have to skip it here
        uint32 vectorsCount;
        READ_OR_RETURN(&vectorsCount, sizeof(vectorsCount));

        char blockId[5];
        blockId[4] = 0;
        int blockSize;

        uint32 groups;
        READ_OR_RETURN(&groups, sizeof(uint32));
        uint32 rootWmoId;
        READ_OR_RETURN(&rootWmoId, sizeof(uint32));

        std::vector<GroupModel> groupsArray;
        for (uint32 groupId = 0; groupId < groups; ++groupId)
        {
            std::vector<MeshTriangle> triangles;
            std::vector<G3D::Vector3> vertexArray;

            uint32 flags;
            READ_OR_RETURN(&flags, sizeof(uint32));
            uint32 groupWmoId;
            READ_OR_RETURN(&groupWmoId, sizeof(uint32));

            float box1[3];
            READ_OR_RETURN(box1, sizeof(float) * 3);
            float box2[3];
            READ_OR_RETURN(box2, sizeof(float) * 3);

            uint32 liquidFlags;
            READ_OR_RETURN(&liquidFlags, sizeof(uint32));

            // will this ever be used? what is it good for anyway??
            uint32 branches;
            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "GRP ");
            READ_OR_RETURN(&blockSize, sizeof(int));
            READ_OR_RETURN(&branches, sizeof(uint32));
            for (uint32 branchId = 0; branchId < branches; ++branchId)
            {
                uint32 indexes;
                // indexes for each branch (not used jet)
                READ_OR_RETURN(&indexes, sizeof(uint32));
            }

            // ---- indexes
            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "INDX");
            READ_OR_RETURN(&blockSize, sizeof(int));
            uint32 indexCount;
            READ_OR_RETURN(&indexCount, sizeof(uint32));
            if (indexCount > 0)
            {
                uint16* indices = new uint16[indexCount];
                READ_OR_RETURN_WITH_DELETE(indices, indexCount * sizeof(uint16));
                for (uint32 i = 0; i < indexCount; i += 3)
                    triangles.push_back(MeshTriangle(indices[i], indices[i + 1], indices[i + 2]));
                delete[] indices;
            }

            // ---- vectors
            READ_OR_RETURN(&blockId, 4);
            CMP_OR_RETURN(blockId, "VERT");
            READ_OR_RETURN(&blockSize, sizeof(int));
            uint32 vectorsCount;
            READ_OR_RETURN(&vectorsCount, sizeof(uint32));

            if (vectorsCount > 0)
            {
                float* vectors = new float[vectorsCount * 3];
                READ_OR_RETURN_WITH_DELETE(vectors, vectorsCount * sizeof(float) * 3);
                for (uint32 i = 0; i < vectorsCount; ++i)
                    vertexArray.push_back(G3D::Vector3(vectors + 3 * i));
                delete[] vectors;
            }
            // ----- liquid
            WmoLiquid* liquid = NULL;
            if (liquidFlags & 1)
            {
                WMOLiquidHeader liquidHeader;
                READ_OR_RETURN(&blockId, 4);
                CMP_OR_RETURN(blockId, "LIQU");
                READ_OR_RETURN(&blockSize, sizeof(int));
                READ_OR_RETURN(&liquidHeader, sizeof(WMOLiquidHeader));
                liquid = new WmoLiquid(liquidHeader.xtiles,
                                       liquidHeader.ytiles,
                                       G3D::Vector3(liquidHeader.pos_x, liquidHeader.pos_y, liquidHeader.pos_z),
                                       liquidHeader.type);
                uint32 size = liquidHeader.xverts * liquidHeader.yverts;
                READ_OR_RETURN(liquid->GetHeightStorage(), size * sizeof(float));
                size = liquidHeader.xtiles * liquidHeader.ytiles;
                READ_OR_RETURN(liquid->GetFlagsStorage(), size);
            }

            groupsArray.push_back(GroupModel(flags, groupWmoId, G3D::AABox(G3D::Vector3(box1), G3D::Vector3(box2))));
            groupsArray.back().SetMeshData(vertexArray, triangles);
            groupsArray.back().SetLiquidData(liquid);

            // drop of temporary use defines
            #undef READ_OR_RETURN
            #undef READ_OR_RETURN_WITH_DELETE
            #undef CMP_OR_RETURN

        }
        fclose(rf);

        // write WorldModel
        WorldModel model;
        model.SetRootId(rootWmoId);
        if (!groupsArray.empty())
        {
            model.SetGroupModels(groupsArray);
            res = model.WriteFile(_destDir + "/" + fileName + ".vmo");
        }
        return res;
    }
}
