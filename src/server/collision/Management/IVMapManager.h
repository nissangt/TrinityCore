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

#ifndef _IVMAPMANAGER_H
#define _IVMAPMANAGER_H

#include <string>
#include "Define.h"

/**
This is the minimum interface to the VMapMamager.
*/
namespace VMAP
{
    enum VMapLoadResult
    {
        VMAP_LOAD_RESULT_ERROR,
        VMAP_LOAD_RESULT_OK,
        VMAP_LOAD_RESULT_IGNORED,
    };

    #define VMAP_INVALID_HEIGHT       -100000.0f            // for check
    #define VMAP_INVALID_HEIGHT_VALUE -200000.0f            // real assigned value in unknown height case

    class IVMapManager
    {
    private:
        bool _enableLoS;
        bool _enableHeight;

    public:
        IVMapManager() : _enableLoS(true), _enableHeight(true) { }

        virtual ~IVMapManager(void) { }

        virtual VMapLoadResult LoadMap(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY) = 0;

        virtual bool MapExists(const char* basePath, uint32 mapId, uint32 tileX, uint32 tileY) = 0;

        virtual void UnloadMap(uint32 mapId, uint32 tileX, uint32 tileY) = 0;
        virtual void UnloadMap(uint32 mapId) = 0;

        virtual bool IsInLoS(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2) = 0;
        virtual float GetHeight(uint32 mapId, float x, float y, float z, float maxDist) = 0;

        /**
        test if we hit an object. return true if we hit one. rx, ry, rz will hold the hit position or the dest position, if no intersection was found
        return a position, that is pReduceDist closer to the origin
        */
        virtual bool GetObjectHitPos(uint32 mapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float &ry, float& rz, float modifyDist) = 0;
        /**
        send debug commands
        */
        virtual bool ProcessCommand(char* command) = 0;
        
        virtual std::string GetFileName(uint32 mapId, uint32 tileX, uint32 tileY) const = 0;
        /**
         Query world model area info.
         \param z gets adjusted to the ground height for which this are info is valid
         */
        virtual bool GetAreaInfo(uint32 mapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const = 0;
        virtual bool GetLiquidLevel(uint32 mapId, float x, float y, float z, uint8 reqLiquidType, float& level, float& floor, uint32& type) const = 0;

        /**
        Enable/disable LOS calculation
        It is enabled by default. If it is enabled in mid game the maps have to loaded manualy
        */
        void SetEnableLoS(bool value) { _enableLoS = value; }
        /**
        Enable/disable model height calculation
        It is enabled by default. If it is enabled in mid game the maps have to loaded manualy
        */
        void SetEnableHeight(bool value) { _enableHeight = value; }

        bool IsEnabledLoS() const { return _enableLoS; }
        bool IsEnabledHeight() const { return _enableHeight; }
        bool IsMapLoadingEnabled() const { return _enableLoS || _enableHeight; }
    };
}

#endif
