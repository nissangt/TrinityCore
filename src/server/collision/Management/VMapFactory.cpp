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

#include <sys/types.h>
#include "G3D/Table.h"
#include "VMapFactory.h"
#include "VMapManager2.h"

namespace VMAP
{    
    IVMapManager* _manager = NULL;
    G3D::Table <uint32, bool>* _ignoredSpells = NULL; // Replace by set? map?

    // TODO: Replace by normal trim
    void ChompAndTrim(std::string& str)
    {
        // 1. Remove new line characters, space, ' and " characters from the end of the string
        while (str.length() > 0)
        {
            char lc = str[str.length() - 1];
            if (lc == '\r' || lc == '\n' || lc == ' ' || lc == '"' || lc == '\'')
                str = str.substr(0, str.length() - 1);
            else
                break;
        }
        // 2. Remove space, ' and " characters from the beginning of the string
        while (str.length() > 0)
        {
            char lc = str[0];
            if (lc == ' ' || lc == '"' || lc == '\'')
                str = str.substr(1, str.length() - 1);
            else
                break;
        }
    }

    // TODO: Replace by normal tokenizer
    // result false, if no more id are found
    bool GetNextId(const std::string& s, std::string::size_type& startPos, uint32& id)
    {
        bool res = false;
        std::string::size_type pos;
        // Find ,
        for (pos = startPos; pos < s.size(); ++pos)
            if (s[pos] == ',')
                break;
        if (pos > startPos)
        {
            std::string sub = s.substr(startPos, pos - startPos);
            startPos = pos + 1;
            ChompAndTrim(sub);
            id = atoi(sub.c_str());
            res = true;
        }
        return res;
    }

    /**
    parameter: String of map ids. Delimiter = ","
    */
    void VMapFactory::SetSpellsIgnoredForLoS(const char* str)
    {
        if (!_ignoredSpells)
            _ignoredSpells = new G3D::Table<uint32, bool>();

        if (str)
        {
            std::string::size_type pos = 0;
            uint32 spellId;
            std::string s(str);
            ChompAndTrim(s);
            while (GetNextId(s, pos, spellId))
                _ignoredSpells->set(spellId, true);
        }
    }

    bool VMapFactory::CheckSpellForLoS(uint32 spellId)
    {
        return !_ignoredSpells->containsKey(spellId);
    }

    // Factory method
    IVMapManager* VMapFactory::GetVMapManager()
    {
        if (!_manager)
            _manager = new VMapManager2();                // should be taken from config ... Please change if you like :-)
        return _manager;
    }

    void VMapFactory::Clear()
    {
        if (_ignoredSpells)
        {
            delete _ignoredSpells;
            _ignoredSpells = NULL;
        }
        if (_manager)
        {
            delete _manager;
            _manager = NULL;
        }
    }
}
