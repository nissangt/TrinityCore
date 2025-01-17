/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
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

/*
 * Scripts for spells with SPELLFAMILY_SHAMAN and SPELLFAMILY_GENERIC spells used by shaman players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_sha_".
 */

#include "ScriptPCH.h"
#include "SpellAuraEffects.h"

enum ShamanSpells
{
    SHAMAN_SPELL_GLYPH_OF_MANA_TIDE     = 55441,
    SHAMAN_SPELL_MANA_TIDE_TOTEM        = 39609,
    SHAMAN_SPELL_FIRE_NOVA_R1           = 1535,
    SHAMAN_SPELL_FIRE_NOVA_TRIGGERED_R1 = 8349,

    //For Earthen Power
    SHAMAN_TOTEM_SPELL_EARTHBIND_TOTEM  = 6474, //Spell casted by totem
    SHAMAN_TOTEM_SPELL_EARTHEN_POWER    = 59566, //Spell witch remove snare effect
};

// this a dirty hack
// TODO: remove me if you will find how to disable this spell for second pet
class spell_sha_bash : public SpellScriptLoader
{
public:
    spell_sha_bash() : SpellScriptLoader("spell_sha_bash") { }

    class spell_sha_bash_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_sha_bash_SpellScript)

        SpellCastResult CheckCooldown()
        {
            if (GetCaster())
                if (Unit * owner = GetCaster()->GetOwner())
                    if (owner->ToPlayer()->HasSpellCooldown(58861))
                        return SPELL_FAILED_DONT_REPORT;
            return SPELL_CAST_OK;
        }

        void HandleAfterHit()
        {
            if (GetCaster())
                if (Unit * owner = GetCaster()->GetOwner())
                    owner->ToPlayer()->AddSpellCooldown(58861, 0, time(NULL) + 45);
        }

        void Register()
        {
            OnCheckCast += SpellCheckCastFn(spell_sha_bash_SpellScript::CheckCooldown);
            AfterHit += SpellHitFn(spell_sha_bash_SpellScript::HandleAfterHit);
        }
    };

    SpellScript * GetSpellScript() const
    {
        return new spell_sha_bash_SpellScript();
    }
};

// 51474 - Astral shift
class spell_sha_astral_shift : public SpellScriptLoader
{
public:
    spell_sha_astral_shift() : SpellScriptLoader("spell_sha_astral_shift") { }

    class spell_sha_astral_shift_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_sha_astral_shift_AuraScript);

        uint32 absorbPct;

        bool Load()
        {
            absorbPct = GetSpellInfo()->Effects[EFFECT_0].CalcValue(GetCaster());
            return true;
        }

        void CalculateAmount(AuraEffect const* /*aurEff*/, int32 & amount, bool & /*canBeRecalculated*/)
        {
            // Set absorbtion amount to unlimited
            amount = -1;
        }

        void Absorb(AuraEffect * /*aurEff*/, DamageInfo & dmgInfo, uint32 & absorbAmount)
        {
            // reduces all damage taken while stun, fear or silence
            if (GetTarget()->GetUInt32Value(UNIT_FIELD_FLAGS) & (UNIT_FLAG_FLEEING | UNIT_FLAG_SILENCED) || (GetTarget()->GetUInt32Value(UNIT_FIELD_FLAGS) & (UNIT_FLAG_STUNNED) && GetTarget()->HasAuraWithMechanic(1<<MECHANIC_STUN)))
                absorbAmount = CalculatePctN(dmgInfo.GetDamage(), absorbPct);
        }

        void Register()
        {
             DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_sha_astral_shift_AuraScript::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
             OnEffectAbsorb += AuraEffectAbsorbFn(spell_sha_astral_shift_AuraScript::Absorb, EFFECT_0);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_sha_astral_shift_AuraScript();
    }
};

// 1535 Fire Nova
class spell_sha_fire_nova : public SpellScriptLoader
{
public:
    spell_sha_fire_nova() : SpellScriptLoader("spell_sha_fire_nova") { }

    class spell_sha_fire_nova_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_sha_fire_nova_SpellScript)
        bool Validate(SpellInfo const* spellEntry)
        {
            if (!sSpellMgr->GetSpellInfo(SHAMAN_SPELL_FIRE_NOVA_R1))
                return false;
            if (sSpellMgr->GetFirstSpellInChain(SHAMAN_SPELL_FIRE_NOVA_R1) != sSpellMgr->GetFirstSpellInChain(spellEntry->Id))
                return false;

            uint8 rank = sSpellMgr->GetSpellRank(spellEntry->Id);
            if (!sSpellMgr->GetSpellWithRank(SHAMAN_SPELL_FIRE_NOVA_TRIGGERED_R1, rank, true))
                return false;
            return true;
        }

        SpellCastResult CheckFireTotem()
        {
            // fire totem
            if (!GetCaster()->m_SummonSlot[1])
            {
                SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_MUST_HAVE_FIRE_TOTEM);
                return SPELL_FAILED_CUSTOM_ERROR;
            }

            return SPELL_CAST_OK;
        }

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            uint8 rank = sSpellMgr->GetSpellRank(GetSpellInfo()->Id);
            if (uint32 spellId = sSpellMgr->GetSpellWithRank(SHAMAN_SPELL_FIRE_NOVA_TRIGGERED_R1, rank))
            {
                Creature* totem = caster->GetMap()->GetCreature(caster->m_SummonSlot[1]);
                if (totem && totem->isTotem())
                    totem->CastSpell(totem, spellId, true);
            }
        }

        void Register()
        {
            OnCheckCast += SpellCheckCastFn(spell_sha_fire_nova_SpellScript::CheckFireTotem);
            OnEffect += SpellEffectFn(spell_sha_fire_nova_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_sha_fire_nova_SpellScript();
    }
};

// 39610 Mana Tide Totem
class spell_sha_mana_tide_totem : public SpellScriptLoader
{
public:
    spell_sha_mana_tide_totem() : SpellScriptLoader("spell_sha_mana_tide_totem") { }

    class spell_sha_mana_tide_totem_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_sha_mana_tide_totem_SpellScript)
        bool Validate(SpellInfo const* /*spellEntry*/)
        {
            if (!sSpellMgr->GetSpellInfo(SHAMAN_SPELL_GLYPH_OF_MANA_TIDE))
                return false;
            if (!sSpellMgr->GetSpellInfo(SHAMAN_SPELL_MANA_TIDE_TOTEM))
                return false;
            return true;
        }

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            if (Unit* unitTarget = GetHitUnit())
            {
                if (unitTarget->getPowerType() == POWER_MANA)
                {
                    int32 effValue = GetEffectValue();
                    // Glyph of Mana Tide
                    if (Unit* owner = caster->GetOwner())
                        if (AuraEffect *dummy = owner->GetAuraEffect(SHAMAN_SPELL_GLYPH_OF_MANA_TIDE, 0))
                            effValue += dummy->GetAmount();
                    // Regenerate 6% of Total Mana Every 3 secs
                    int32 effBasePoints0 = int32(CalculatePctN(unitTarget->GetMaxPower(POWER_MANA), effValue));
                    caster->CastCustomSpell(unitTarget, SHAMAN_SPELL_MANA_TIDE_TOTEM, &effBasePoints0, NULL, NULL, true, NULL, NULL, GetOriginalCaster()->GetGUID());
                }
            }
        }

        void Register()
        {
            OnEffect += SpellEffectFn(spell_sha_mana_tide_totem_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_sha_mana_tide_totem_SpellScript();
    }
};

// 6474 - Earthbind Totem - Fix Talent:Earthen Power
class spell_sha_earthbind_totem : public SpellScriptLoader
{
public:
    spell_sha_earthbind_totem() : SpellScriptLoader("spell_sha_earthbind_totem") { }

    class spell_sha_earthbind_totem_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_sha_earthbind_totem_AuraScript);

        bool Validate(SpellInfo const* /*spellEntry*/)
        {
            if (!sSpellMgr->GetSpellInfo(SHAMAN_TOTEM_SPELL_EARTHBIND_TOTEM))
                return false;
            if (!sSpellMgr->GetSpellInfo(SHAMAN_TOTEM_SPELL_EARTHEN_POWER))
                return false;
            return true;
        }

        void HandleEffectPeriodic(AuraEffect const* aurEff)
        {
            Unit* target = GetTarget();
            if (Unit* caster = aurEff->GetBase()->GetCaster())
                if (AuraEffect* aur = caster->GetDummyAuraEffect(SPELLFAMILY_SHAMAN, 2289, 0))
                    if (roll_chance_i(aur->GetBaseAmount()))
                        target->CastSpell(target, SHAMAN_TOTEM_SPELL_EARTHEN_POWER, true, NULL, aurEff);
        }

        void Register()
        {
             OnEffectPeriodic += AuraEffectPeriodicFn(spell_sha_earthbind_totem_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_sha_earthbind_totem_AuraScript();
    }
};

void AddSC_shaman_spell_scripts()
{
    new spell_sha_bash();
    new spell_sha_astral_shift();
    new spell_sha_fire_nova();
    new spell_sha_mana_tide_totem();
    new spell_sha_earthbind_totem();
}
