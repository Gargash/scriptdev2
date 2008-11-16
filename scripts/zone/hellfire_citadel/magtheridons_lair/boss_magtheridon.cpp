/* Copyright (C) 2006 - 2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Boss_Magtheridon
SD%Complete: 60
SDComment: In Development
SDCategory: Hellfire Citadel, Magtheridon's lair
EndScriptData */

#include "precompiled.h"
#include "def_magtheridons_lair.h"
#include "WorldPacket.h"

struct Yell
{
    int32 id;
};

static Yell RandomTaunt[]=
{
    {-1544000},
    {-1544001},
    {-1544002},
    {-1544003},
    {-1544004},
    {-1544005},
};

#define SAY_FREED                   -1544006
#define SAY_AGGRO                   -1544007
#define SAY_BANISH                  -1544008
#define SAY_CHAMBER_DESTROY         -1544009
#define SAY_PLAYER_KILLED           -1544010
#define SAY_DEATH                   -1544011

#define EMOTE_BERSERK               -1544012
#define EMOTE_BLASTNOVA             -1544013
#define EMOTE_BEGIN                 -1544014

//Phase 2 Spells
#define SPELL_QUAKE_PROC            30571
#define SPELL_QUAKE                 30576                   //must be cast with 30561 as the proc spell
#define SPELL_BLASTNOVA             30616
#define SPELL_CLEAVE                30619
#define SPELL_BERSERK               27680
#define SPELL_DEBRIS                30631
#define SPELL_CAMERA_SHAKE          36455

//Banish
#define SPELL_SHADOW_CAGE           30205

//Player version of the banish
#define SPELL_SHADOW_CAGE_2         30168

//Spell that is cast on players from the cube
#define SPELL_SHADOW_GRASP          30410
#define SPELL_SHADOW_GRASP_UKN      30166
#define SPELL_SHADOW_GRASP_VIS      30207

//Spawned objects
#define SPELL_COLLAPSE              34233                   //This spell casted by the "cave in" type object

#define SPELL_CONFLAGERATION        35840                   //Actually casted by a creature or object spawned on the ground

//Cubes
#define SPELL_MIND_EXHAUSTIOIN      30509                   //Casted by the cubes when channeling ends

//Channeler spells
//#define MOB_HELLFIRE_CHANNELLER    17256

#define SPELL_SOUL_TRANSFER         30531
#define SPELL_SHADOW_BOLT_VOLLEY    30510
#define SPELL_DARK_MENDING          30528
#define SPELL_HELLFIRE_CHANNELING   31059
#define SPELL_HELLFIRE_CAST_VISUAL  24207
#define SPELL_FEAR                  39176

#define SPELL_BURNING_ABYSSAL       30511

struct MANGOS_DLL_DECL boss_magtheridonAI : public ScriptedAI
{
    boss_magtheridonAI(Creature *c) : ScriptedAI(c)
    {
        pInst = (ScriptedInstance*)m_creature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* pInst;

    uint32 Phase1_Timer;
    uint32 Cleave_Timer;
    uint32 BlastNova_Timer;
    uint32 Quake_Timer;
    uint32 QuakePhase;
    uint32 Collapse_Timer;
    uint32 Berserk_Timer;
    bool Banished;
    bool Phase3;

    uint32 RandChat_Timer;

    void Reset()
    {
        RandChat_Timer = 90000;

        Phase1_Timer = 0;
        Cleave_Timer = 15000;
        Berserk_Timer = 1200000;                            //20 minutes
        BlastNova_Timer = 60000;
        Quake_Timer = 40000;
        QuakePhase = 0;
        Collapse_Timer = 0;
        Banished = false;

        m_creature->setFaction(35);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        m_creature->CastSpell(m_creature, SPELL_SHADOW_CAGE, false);

        if (pInst)
            pInst->SetData(DATA_MAGTHERIDON_EVENT_ENDED, false);
    }

    void KilledUnit(Unit* victim)
    {
        DoScriptText(SAY_PLAYER_KILLED, m_creature);
    }

    void JustDied(Unit* Killer)
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void Aggro(Unit *who)
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void MoveInLineOfSight(Unit* who) {}

    void UpdateAI(const uint32 diff)
    {
        if (!InCombat && !Phase1_Timer)
        {
            if (RandChat_Timer < diff)
            {
                DoScriptText(RandomTaunt[rand()%6].id, m_creature);
                RandChat_Timer = 90000;
            }else RandChat_Timer -= diff;
        }

        if (!InCombat && !Phase1_Timer && pInst && pInst->GetData64(DATA_EVENT_STARTER))
        {
            //Unbanish self after 2 minutes
            Phase1_Timer = 120000;
            DoScriptText(EMOTE_BEGIN, m_creature);
            return;
        }

        //Phase timer
        if (Phase1_Timer)
        {
            if (Phase1_Timer <= diff)
            {
                m_creature->setFaction(14);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

                DoScriptText(SAY_FREED, m_creature);

                m_creature->RemoveAurasDueToSpell(SPELL_SHADOW_CAGE);
                AttackStart(Unit::GetUnit(*m_creature, pInst->GetData64(DATA_EVENT_STARTER)));

                Phase1_Timer = 0;
            }
            else
            {
                if (!Unit::GetUnit(*m_creature, pInst->GetData64(DATA_EVENT_STARTER)))
                {
                    Phase1_Timer = 0;
                    return;
                }

                Phase1_Timer -= diff;
                return;
            }
        }

        //Return since we have no target
        if (!m_creature->SelectHostilTarget() || !m_creature->getVictim())
            return;

        //Interrupt Blast Nova
        if (m_creature->HasAura(SPELL_SHADOW_GRASP_VIS, 0) && m_creature->HasAura(SPELL_BLASTNOVA, 0) && !Banished)
        {
            DoScriptText(SAY_BANISH, m_creature);

            m_creature->RemoveAurasDueToSpell(SPELL_BLASTNOVA);
            m_creature->InterruptNonMeleeSpells(false);
            DoCast(m_creature, SPELL_SHADOW_CAGE_2);
            Banished = true;
        }

        if (Banished && !m_creature->HasAura(SPELL_SHADOW_GRASP_VIS, 0))
        {
            Banished = false;
            m_creature->RemoveAurasDueToSpell(SPELL_SHADOW_CAGE_2);
        }

        //Berserk_Timer
        if (Berserk_Timer < diff)
        {
            DoScriptText(EMOTE_BERSERK, m_creature);
            DoCast(m_creature, SPELL_BERSERK);
            Berserk_Timer = 300000;
        }else Berserk_Timer -= diff;

        //Cleave_Timer
        if (Cleave_Timer < diff)
        {
            DoCast(m_creature->getVictim(),SPELL_CLEAVE);
            Cleave_Timer = 10000;
        }else Cleave_Timer -= diff;

        //Quake_Timer
        if (Quake_Timer < diff)
        {
            int32 i = SPELL_QUAKE_PROC;
            m_creature->CastCustomSpell(m_creature, SPELL_QUAKE, &i, 0, 0, false);

            Quake_Timer = 40000;
        }else Quake_Timer -= diff;

        //BlastNova_Timer
        if (BlastNova_Timer < diff)
        {
            //Inturrupt Quake if it is casting
            m_creature->InterruptNonMeleeSpells(false);

            DoScriptText(EMOTE_BLASTNOVA, m_creature);
            DoCast(m_creature, SPELL_BLASTNOVA);

            BlastNova_Timer = 40000;
        }else BlastNova_Timer -= diff;

        //Phase3 if not already enraged and below 30%
        if (!Phase3 && (m_creature->GetHealth()*100 / m_creature->GetMaxHealth()) < 30)
        {
            Phase3 = true;
            DoScriptText(SAY_CHAMBER_DESTROY, m_creature);
        }

        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL mob_hellfire_channelerAI : public ScriptedAI
{
    mob_hellfire_channelerAI(Creature *c) : ScriptedAI(c)
    {
        pInst = (ScriptedInstance*)m_creature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* pInst;

    uint32 ShadowBoltVolley_Timer;
    uint32 DarkMending_Timer;
    uint32 Fear_Timer;
    uint32 Infernal_Timer;

    bool InfernalSpawned;

    void Reset()
    {
        ShadowBoltVolley_Timer = 8000 + rand()%2000;
        DarkMending_Timer = 30000;
        Fear_Timer = 15000 + rand()%5000;
        Infernal_Timer = 20000 + rand()%5000;

        InfernalSpawned = false;

        //Suprisingly this works very well, but only if the channelers are spawned after magtheridon
        DoCast(m_creature, SPELL_SHADOW_GRASP_VIS);
        if (pInst)
            pInst->SetData(DATA_MAGTHERIDON_EVENT_ENDED, false);
    }

    void Aggro(Unit *who)
    {
        m_creature->InterruptNonMeleeSpells(false);

        if (!pInst || pInst->GetData64(DATA_EVENT_STARTER))
            return;

        pInst->SetData64(DATA_EVENT_STARTER, who->GetGUID());
        pInst->SetData(DATA_MAGTHERIDON_EVENT_STARTED, true);
    }

    void MoveInLineOfSight(Unit*)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        if (!InCombat && pInst && pInst->GetData64(DATA_EVENT_STARTER))
        {
            m_creature->InterruptNonMeleeSpells(false);
            AttackStart(Unit::GetUnit(*m_creature, pInst->GetData64(DATA_EVENT_STARTER)));
            return;
        }

        //Return since we have no target
        if (!m_creature->SelectHostilTarget() || !m_creature->getVictim() )
            return;

        //Shadow bolt volley
        if (ShadowBoltVolley_Timer < diff)
        {
            DoCast(m_creature->getVictim(),SPELL_SHADOW_BOLT_VOLLEY);
            ShadowBoltVolley_Timer = 10000 + (rand()%10000);
        }else ShadowBoltVolley_Timer -= diff;

        //Dark Mending
        if (DarkMending_Timer < diff)
        {
            if ((m_creature->GetHealth()*100 / m_creature->GetMaxHealth()) < 50)
            {
                //Cast on ourselves if we are lower then lowest hp friendly unit
                /*if (pLowestHPTarget && LowestHP < m_creature->GetHealth())
                    DoCast(pLowestHPTarget, SPELL_DARK_MENDING);
                else*/
                DoCast(m_creature, SPELL_DARK_MENDING);
            }

            DarkMending_Timer = 10000 + (rand() % 10000);
        }else DarkMending_Timer -= diff;

        //Fear
        if (Fear_Timer < diff)
        {
            if (Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 1))
                DoCast(target,SPELL_FEAR);

            Fear_Timer = 25000 + (rand()%15000);
        }else Fear_Timer -= diff;

        //Infernal spawning
        if (!InfernalSpawned && Infernal_Timer < diff)
        {
            if (Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0))
                DoCast(target, SPELL_BURNING_ABYSSAL);

            InfernalSpawned = true;
        }else Infernal_Timer -= diff;

        DoMeleeAttackIfReady();
    }

};

//Manticron Cube
bool GOHello_go_Manticron_Cube(Player *player, GameObject* _GO)
{
    ScriptedInstance* pInst = (ScriptedInstance*)_GO->GetInstanceData();

    Unit* pUnit = NULL;
    if (pInst)
        pUnit = Unit::GetUnit(*_GO, pInst->GetData64(DATA_MAGTHERIDON));
    else
    {
        error_log("SD2: Magtheridon: Manticron Cube: NO INSTANCE");
        return true;
    }

    if (!pUnit || !pUnit->isAlive() || !player)
    {
        error_log("SD2: Magtheridon: Mantricon Cube: NO TARGET");
        return true;
    }

    player->InterruptNonMeleeSpells(false);
    player->CastSpell(pUnit, SPELL_SHADOW_GRASP, true);
    player->CastSpell(pUnit, SPELL_SHADOW_GRASP_VIS, false);

    debug_log("SD2: Magtheridon: Mantricon Cube Clicked");
    return true;
}

CreatureAI* GetAI_boss_magtheridon(Creature *_Creature)
{
    return new boss_magtheridonAI (_Creature);
}

CreatureAI* GetAI_mob_hellfire_channeler(Creature *_Creature)
{
    return new mob_hellfire_channelerAI (_Creature);
}

void AddSC_boss_magtheridon()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_magtheridon";
    newscript->GetAI = &GetAI_boss_magtheridon;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_hellfire_channeler";
    newscript->GetAI = &GetAI_mob_hellfire_channeler;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "go_manticron_cube";
    newscript->pGOHello = &GOHello_go_Manticron_Cube;
    newscript->RegisterSelf();
}
