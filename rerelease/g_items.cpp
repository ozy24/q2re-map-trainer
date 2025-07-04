// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "bots/bot_includes.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

bool Pickup_Weapon(edict_t *ent, edict_t *other);
void Use_Weapon(edict_t *ent, gitem_t *inv);
void Drop_Weapon(edict_t *ent, gitem_t *inv);

void Weapon_Blaster(edict_t *ent);
void Weapon_Shotgun(edict_t *ent);
void Weapon_SuperShotgun(edict_t *ent);
void Weapon_Machinegun(edict_t *ent);
void Weapon_Chaingun(edict_t *ent);
void Weapon_HyperBlaster(edict_t *ent);
void Weapon_RocketLauncher(edict_t *ent);
void Weapon_Grenade(edict_t *ent);
void Weapon_GrenadeLauncher(edict_t *ent);
void Weapon_Railgun(edict_t *ent);
void Weapon_BFG(edict_t *ent);
// RAFAEL
void Weapon_Ionripper(edict_t *ent);
void Weapon_Phalanx(edict_t *ent);
void Weapon_Trap(edict_t *ent);
// RAFAEL
// ROGUE
void Weapon_ChainFist(edict_t *ent);
void Weapon_Disintegrator(edict_t *ent);
void Weapon_ETF_Rifle(edict_t *ent);
void Weapon_Heatbeam(edict_t *ent);
void Weapon_Prox(edict_t *ent);
void Weapon_Tesla(edict_t *ent);
void Weapon_ProxLauncher(edict_t *ent);
// ROGUE
void Weapon_Beta_Disintegrator(edict_t *ent);

void	   Use_Quad(edict_t *ent, gitem_t *item);
static gtime_t quad_drop_timeout_hack;

// RAFAEL
void	   Use_QuadFire(edict_t *ent, gitem_t *item);
static gtime_t quad_fire_drop_timeout_hack;
// RAFAEL

//======================================================================

/*
===============
GetItemByIndex
===============
*/
gitem_t *GetItemByIndex(item_id_t index)
{
	if (index <= IT_NULL || index >= IT_TOTAL)
		return nullptr;

	return &itemlist[index];
}

static gitem_t *ammolist[AMMO_MAX];

gitem_t *GetItemByAmmo(ammo_t ammo)
{
	return ammolist[ammo];
}

static gitem_t *poweruplist[POWERUP_MAX];

gitem_t *GetItemByPowerup(powerup_t powerup)
{
	return poweruplist[powerup];
}

/*
===============
FindItemByClassname

===============
*/
gitem_t *FindItemByClassname(const char *classname)
{
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++)
	{
		if (!it->classname)
			continue;
		if (!Q_strcasecmp(it->classname, classname))
			return it;
	}

	return nullptr;
}

/*
===============
FindItem

===============
*/
gitem_t *FindItem(const char *pickup_name)
{
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++)
	{
		if (!it->use_name)
			continue;
		if (!Q_strcasecmp(it->use_name, pickup_name))
			return it;
	}

	return nullptr;
}

//======================================================================

THINK(DoRespawn) (edict_t *ent) -> void
{
	if (ent->team)
	{
		edict_t *master;
		int		 count;
		int		 choice;

		master = ent->teammaster;

		// ZOID
		// in ctf, when we are weapons stay, only the master of a team of weapons
		// is spawned
		if (ctf->integer && g_dm_weapons_stay->integer && master->item && (master->item->flags & IF_WEAPON))
			ent = master;
		else
		{
			// ZOID
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			gi.linkentity(ent);

			for (count = 0, ent = master; ent; ent = ent->chain, count++)
				;

			choice = irandom(count);

			for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
				;
		}
	}

	ent->svflags &= ~SVF_NOCLIENT;
	ent->svflags &= ~SVF_RESPAWNING;
	ent->solid = SOLID_TRIGGER;
	gi.linkentity(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;

	// ROGUE
	if (g_dm_random_items->integer)
	{
		item_id_t new_item = DoRandomRespawn(ent);

		// if we've changed entities, then do some sleight of hand.
		// otherwise, the old entity will respawn
		if (new_item)
		{
			ent->item = GetItemByIndex(new_item);

			ent->classname = ent->item->classname;
			ent->s.effects = ent->item->world_model_flags;
			gi.setmodel(ent, ent->item->world_model);
		}
	}
	// ROGUE
}

void SetRespawn(edict_t *ent, gtime_t delay, bool hide_self)
{
	// already respawning
	if (ent->think == DoRespawn && ent->nextthink >= level.time)
		return;

	ent->flags |= FL_RESPAWN;

	if (hide_self)
	{
		ent->svflags |= ( SVF_NOCLIENT | SVF_RESPAWNING );
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);
	}

	ent->nextthink = level.time + delay;
	ent->think = DoRespawn;
}

//======================================================================

bool IsInstantItemsEnabled()
{
	if (deathmatch->integer && g_dm_instant_items->integer)
	{
		return true;
	}

	if (!deathmatch->integer && level.instantitems)
	{
		return true;
	}

	return false;
}

bool Pickup_Powerup(edict_t *ent, edict_t *other)
{
	int quantity;

	quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 0 && quantity >= 3) ||
		(skill->integer == 1 && quantity >= 2) ||
		(skill->integer >= 2 && quantity >= 1))
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	bool is_dropped_from_death = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED);

	if (IsInstantItemsEnabled() ||
		((ent->item->use == Use_Quad) && is_dropped_from_death) ||
		((ent->item->use == Use_QuadFire) && is_dropped_from_death))
	{
		if ((ent->item->use == Use_Quad) && is_dropped_from_death)
			quad_drop_timeout_hack = (ent->nextthink - level.time);
		else if ((ent->item->use == Use_QuadFire) && is_dropped_from_death)
			quad_fire_drop_timeout_hack = (ent->nextthink - level.time);

		if (ent->item->use)
			ent->item->use(other, ent->item);
	}

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && !is_dropped_from_death)
			SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));
	}

	return true;
}

bool Pickup_General(edict_t *ent, edict_t *other)
{
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED))
			SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));
	}

	return true;
}

void Drop_General(edict_t *ent, gitem_t *item)
{
	edict_t *dropped = Drop_Item(ent, item);
	dropped->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svflags &= ~SVF_INSTANCED;
	ent->client->pers.inventory[item->id]--;
}

//======================================================================

void Use_Adrenaline(edict_t *ent, gitem_t *item)
{
	if (!deathmatch->integer)
		ent->max_health += 1;

	if (ent->health < ent->max_health)
		ent->health = ent->max_health;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/n_health.wav"), 1, ATTN_NORM, 0);

	ent->client->pers.inventory[item->id]--;
}

bool Pickup_LegacyHead(edict_t *ent, edict_t *other)
{
	other->max_health += 5;
	other->health += 5;

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

void G_CheckPowerArmor(edict_t *ent)
{
	bool has_enough_cells;

	if (!ent->client->pers.inventory[IT_AMMO_CELLS])
		has_enough_cells = false;
	else if (ent->client->pers.autoshield >= AUTO_SHIELD_AUTO)
		has_enough_cells = (ent->flags & FL_WANTS_POWER_ARMOR) && ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield;
	else
		has_enough_cells = true;

	if (ent->flags & FL_POWER_ARMOR)
	{
		if (!has_enough_cells)
		{
			// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
		}
	}
	else
	{
		// special case for power armor, for auto-shields
		if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL &&
			has_enough_cells && (ent->client->pers.inventory[IT_ITEM_POWER_SCREEN] ||
				ent->client->pers.inventory[IT_ITEM_POWER_SHIELD]))
		{
			ent->flags |= FL_POWER_ARMOR;
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
		}
	}
}

inline bool G_AddAmmoAndCap(edict_t *other, item_id_t item, int32_t max, int32_t quantity)
{
	if (other->client->pers.inventory[item] >= max)
		return false;

	other->client->pers.inventory[item] += quantity;
	if (other->client->pers.inventory[item] > max)
		other->client->pers.inventory[item] = max;

	G_CheckPowerArmor(other);

	return true;
}

inline bool G_AddAmmoAndCapQuantity(edict_t *other, ammo_t ammo)
{
	gitem_t *item = GetItemByAmmo(ammo);
	return G_AddAmmoAndCap(other, item->id, other->client->pers.max_ammo[ammo], item->quantity);
}

inline void G_AdjustAmmoCap(edict_t *other, ammo_t ammo, int16_t new_max)
{
	other->client->pers.max_ammo[ammo] = max(other->client->pers.max_ammo[ammo], new_max);
}

bool Pickup_Bandolier(edict_t *ent, edict_t *other)
{
	G_AdjustAmmoCap(other, AMMO_BULLETS, 250);
	G_AdjustAmmoCap(other, AMMO_SHELLS, 150);
	G_AdjustAmmoCap(other, AMMO_CELLS, 250);
	G_AdjustAmmoCap(other, AMMO_SLUGS, 75);
	G_AdjustAmmoCap(other, AMMO_MAGSLUG, 75);
	G_AdjustAmmoCap(other, AMMO_FLECHETTES, 250);
	G_AdjustAmmoCap(other, AMMO_DISRUPTOR, 21);

	G_AddAmmoAndCapQuantity(other, AMMO_BULLETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SHELLS);

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

bool Pickup_Pack(edict_t *ent, edict_t *other)
{
	G_AdjustAmmoCap(other, AMMO_BULLETS, 300);
	G_AdjustAmmoCap(other, AMMO_SHELLS, 200);
	G_AdjustAmmoCap(other, AMMO_ROCKETS, 100);
	G_AdjustAmmoCap(other, AMMO_GRENADES, 100);
	G_AdjustAmmoCap(other, AMMO_CELLS, 300);
	G_AdjustAmmoCap(other, AMMO_SLUGS, 100);
	G_AdjustAmmoCap(other, AMMO_MAGSLUG, 100);
	G_AdjustAmmoCap(other, AMMO_FLECHETTES, 300);
	G_AdjustAmmoCap(other, AMMO_DISRUPTOR, 30);

	G_AddAmmoAndCapQuantity(other, AMMO_BULLETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SHELLS);
	G_AddAmmoAndCapQuantity(other, AMMO_CELLS);
	G_AddAmmoAndCapQuantity(other, AMMO_GRENADES);
	G_AddAmmoAndCapQuantity(other, AMMO_ROCKETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SLUGS);

	// RAFAEL
	G_AddAmmoAndCapQuantity(other, AMMO_MAGSLUG);
	// RAFAEL

	// ROGUE
	G_AddAmmoAndCapQuantity(other, AMMO_FLECHETTES);
	G_AddAmmoAndCapQuantity(other, AMMO_DISRUPTOR);
	// ROGUE

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

void Use_Quad(edict_t *ent, gitem_t *item)
{
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (quad_drop_timeout_hack)
	{
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0_ms;
	}
	else
	{
		timeout = 30_sec;
	}

	ent->client->quad_time = max(level.time, ent->client->quad_time) + timeout;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}
// =====================================================================

// RAFAEL
void Use_QuadFire(edict_t *ent, gitem_t *item)
{
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (quad_fire_drop_timeout_hack)
	{
		timeout = quad_fire_drop_timeout_hack;
		quad_fire_drop_timeout_hack = 0_ms;
	}
	else
	{
		timeout = 30_sec;
	}

	ent->client->quadfire_time = max(level.time, ent->client->quadfire_time) + timeout;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/quadfire1.wav"), 1, ATTN_NORM, 0);
}
// RAFAEL

//======================================================================

void Use_Breather(edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[item->id]--;

	ent->client->breather_time = max(level.time, ent->client->breather_time) + 30_sec;

	//	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Envirosuit(edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[item->id]--;

	ent->client->enviro_time = max(level.time, ent->client->enviro_time) + 30_sec;

	//	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Invulnerability(edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[item->id]--;

	ent->client->invincible_time = max(level.time, ent->client->invincible_time) + 30_sec;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect.wav"), 1, ATTN_NORM, 0);
}

void Use_Invisibility(edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[item->id]--;

	ent->client->invisible_time = max(level.time, ent->client->invisible_time) + 30_sec;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Silencer(edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[item->id]--;
	ent->client->silencer_shots += 30;

	//	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

bool Pickup_Key(edict_t *ent, edict_t *other)
{
	if (coop->integer)
	{
		if (ent->item->id == IT_KEY_POWER_CUBE || ent->item->id == IT_KEY_EXPLOSIVE_CHARGES)
		{
			if (other->client->pers.power_cubes & ((ent->spawnflags & SPAWNFLAG_EDITOR_MASK).value >> 8))
				return false;
			other->client->pers.inventory[ent->item->id]++;
			other->client->pers.power_cubes |= ((ent->spawnflags & SPAWNFLAG_EDITOR_MASK).value >> 8);
		}
		else
		{
			if (other->client->pers.inventory[ent->item->id])
				return false;
			other->client->pers.inventory[ent->item->id] = 1;
		}
		return true;
	}
	other->client->pers.inventory[ent->item->id]++;
	return true;
}

//======================================================================

bool Add_Ammo(edict_t *ent, gitem_t *item, int count)
{
	if (!ent->client || item->tag < AMMO_BULLETS || item->tag >= AMMO_MAX)
		return false;

	return G_AddAmmoAndCap(ent, item->id, ent->client->pers.max_ammo[item->tag], count);
}

// we just got weapon `item`, check if we should switch to it
void G_CheckAutoSwitch(edict_t *ent, gitem_t *item, bool is_new)
{
	// already using or switching to
	if (ent->client->pers.weapon == item ||
		ent->client->newweapon == item)
		return;
	// need ammo
	else if (item->ammo)
	{
		int32_t required_ammo = (item->flags & IF_AMMO) ? 1 : item->quantity;
		
		if (ent->client->pers.inventory[item->ammo] < required_ammo)
			return;
	}

	// check autoswitch setting
	if (ent->client->pers.autoswitch == auto_switch_t::NEVER)
		return;
	else if ((item->flags & IF_AMMO) && ent->client->pers.autoswitch == auto_switch_t::ALWAYS_NO_AMMO)
		return;
	else if (ent->client->pers.autoswitch == auto_switch_t::SMART)
	{
		bool using_blaster = ent->client->pers.weapon && ent->client->pers.weapon->id == IT_WEAPON_BLASTER;

		// smartness algorithm: in DM, we will always switch if we have the blaster out
		// otherwise leave our active weapon alone
		if (deathmatch->integer && !using_blaster)
			return;
		// in SP, only switch if it's a new weapon, or we have the blaster out
		else if (!deathmatch->integer && !using_blaster && !is_new)
			return;
	}

	// switch!
	ent->client->newweapon = item;
}

bool Pickup_Ammo(edict_t *ent, edict_t *other)
{
	int	 oldcount;
	int	 count;
	bool weapon;

	weapon = (ent->item->flags & IF_WEAPON);
	if (weapon && G_CheckInfiniteAmmo(ent->item))
		count = 1000;
	else if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	oldcount = other->client->pers.inventory[ent->item->id];

	// Map Trainer: Allow target ammo items to be picked up even when at max capacity
	bool is_map_trainer_target = level.map_trainer.initialized && MapTrainer_IsTargetItem(ent);
	
	if (!Add_Ammo(other, ent->item, count) && !is_map_trainer_target)
		return false;
	
	// If Add_Ammo failed but this is a target item, allow pickup anyway (just don't add ammo)
	if (is_map_trainer_target && other->client->pers.inventory[ent->item->id] >= other->client->pers.max_ammo[ent->item->tag])
	{
		// Don't actually add ammo, but allow the pickup for training purposes

	}

	if (weapon)
		G_CheckAutoSwitch(other, ent->item, !oldcount);

	if (!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)) && deathmatch->integer)
		SetRespawn(ent, 30_sec);
	return true;
}

void Drop_Ammo(edict_t *ent, gitem_t *item)
{
	// [Paril-KEX]
	if (G_CheckInfiniteAmmo(item))
		return;

	item_id_t index = item->id;
	edict_t *dropped = Drop_Item(ent, item);
	dropped->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svflags &= ~SVF_INSTANCED;

	if (ent->client->pers.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->pers.inventory[index];

	if (ent->client->pers.weapon && ent->client->pers.weapon == item && (item->flags & IF_AMMO) &&
		ent->client->pers.inventory[index] - dropped->count <= 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_cant_drop_weapon");
		G_FreeEdict(dropped);
		return;
	}

	ent->client->pers.inventory[index] -= dropped->count;
	G_CheckPowerArmor(ent);
}

//======================================================================

THINK(MegaHealth_think) (edict_t *self) -> void
{
	if (self->owner->health > self->owner->max_health
		//ZOID
		&& !CTFHasRegeneration(self->owner)
		//ZOID
		)
	{
		self->nextthink = level.time + 1_sec;
		self->owner->health -= 1;
		return;
	}

	if (!(self->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(self, 20_sec);
	else
		G_FreeEdict(self);
}

bool Pickup_Health(edict_t *ent, edict_t *other)
{
	int health_flags = (ent->style ? ent->style : ent->item->tag);

	// Map Trainer: Allow health items to be picked up even at full health when path training is enabled
	bool is_map_trainer_target = level.map_trainer.initialized && MapTrainer_IsTargetItem(ent);
	bool path_training_enabled = level.map_trainer.training_enabled && level.map_trainer.initialized;

	if (!(health_flags & HEALTH_IGNORE_MAX) && !is_map_trainer_target && !path_training_enabled)
		if (other->health >= other->max_health)
			return false;

	int count = ent->count ? ent->count : ent->item->quantity;

	// ZOID
	if (deathmatch->integer && other->health >= 250 && count > 25)
		return false;
	// ZOID

	other->health += count;

	//ZOID
	if (ctf->integer && other->health > 250 && count > 25)
		other->health = 250;
	//ZOID

	if (!(health_flags & HEALTH_IGNORE_MAX))
	{
		if (other->health > other->max_health)
			other->health = other->max_health;
	}

	if ((ent->item->tag & HEALTH_TIMED)
		//ZOID
		&& !CTFHasRegeneration(other)
		//ZOID
		)
	{
		if (!deathmatch->integer)
		{
			// mega health doesn't need to be special in SP
			// since it never respawns.
			other->client->pers.megahealth_time = 5_sec;
		}
		else
		{
			ent->think = MegaHealth_think;
			ent->nextthink = level.time + 5_sec;
			ent->owner = other;
			ent->flags |= FL_RESPAWN;
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
		}
	}
	else
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
			SetRespawn(ent, 30_sec);
	}

	return true;
}

//======================================================================

item_id_t ArmorIndex(edict_t *ent)
{
	if (ent->svflags & SVF_MONSTER)
		return ent->monsterinfo.armor_type;

	if (ent->client)
	{
		if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0)
			return IT_ARMOR_JACKET;
		else if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > 0)
			return IT_ARMOR_COMBAT;
		else if (ent->client->pers.inventory[IT_ARMOR_BODY] > 0)
			return IT_ARMOR_BODY;
	}

	return IT_NULL;
}

bool Pickup_Armor(edict_t *ent, edict_t *other)
{
	item_id_t			 old_armor_index;
	const gitem_armor_t *oldinfo;
	const gitem_armor_t *newinfo;
	int					 newcount;
	float				 salvage;
	int					 salvagecount;

	// get info on new armor
	newinfo = ent->item->armor_info;

	old_armor_index = ArmorIndex(other);

	// [Paril-KEX] for g_start_items
	int32_t base_count = ent->count ? ent->count : newinfo ? newinfo->base_count : 0;

	// handle armor shards specially
	if (ent->item->id == IT_ARMOR_SHARD)
	{
		if (!old_armor_index)
			other->client->pers.inventory[IT_ARMOR_JACKET] = 2;
		else
			other->client->pers.inventory[old_armor_index] += 2;
	}
	// if player has no armor, just use it
	else if (!old_armor_index)
	{
		other->client->pers.inventory[ent->item->id] = base_count;
	}

	// use the better armor
	else
	{
		// get info on old armor
		if (old_armor_index == IT_ARMOR_JACKET)
			oldinfo = &jacketarmor_info;
		else if (old_armor_index == IT_ARMOR_COMBAT)
			oldinfo = &combatarmor_info;
		else
			oldinfo = &bodyarmor_info;

		if (newinfo->normal_protection > oldinfo->normal_protection)
		{
			// calc new armor values
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = (int) (salvage * other->client->pers.inventory[old_armor_index]);
			newcount = base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			// zero count of old armor so it goes away
			other->client->pers.inventory[old_armor_index] = 0;

			// change armor to new item with computed value
			other->client->pers.inventory[ent->item->id] = newcount;
		}
		else
		{
			// calc new armor values
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = (int) (salvage * base_count);
			newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

			// if we're already maxed out then we don't need the new armor
			// Map Trainer: Allow target armor items to be picked up even when maxed out
			bool is_map_trainer_target = level.map_trainer.initialized && MapTrainer_IsTargetItem(ent);
			// Map Trainer: Allow pickup if free collect is enabled
			bool free_collect_allowed = level.map_trainer.timing_enabled && level.map_trainer.free_collect_enabled;
			if (other->client->pers.inventory[old_armor_index] >= newcount && !is_map_trainer_target && !free_collect_allowed)
				return false;

			// update current armor value
			other->client->pers.inventory[old_armor_index] = newcount;
		}
	}

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(ent, 20_sec);

	return true;
}

//======================================================================

item_id_t PowerArmorType(edict_t *ent)
{
	if (!ent->client)
		return IT_NULL;

	if (!(ent->flags & FL_POWER_ARMOR))
		return IT_NULL;

	if (ent->client->pers.inventory[IT_ITEM_POWER_SHIELD] > 0)
		return IT_ITEM_POWER_SHIELD;

	if (ent->client->pers.inventory[IT_ITEM_POWER_SCREEN] > 0)
		return IT_ITEM_POWER_SCREEN;

	return IT_NULL;
}

void Use_PowerArmor(edict_t *ent, gitem_t *item)
{
	if (ent->flags & FL_POWER_ARMOR)
	{
		ent->flags &= ~(FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
	}
	else
	{
		if (!ent->client->pers.inventory[IT_AMMO_CELLS])
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_cells_power_armor");
			return;
		}

		ent->flags |= FL_POWER_ARMOR;

		if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL &&
			ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield)
			ent->flags |= FL_WANTS_POWER_ARMOR;

		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
	}
}

bool Pickup_PowerArmor(edict_t *ent, edict_t *other)
{
	int quantity;

	quantity = other->client->pers.inventory[ent->item->id];

	other->client->pers.inventory[ent->item->id]++;

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED))
			SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));
		// auto-use for DM only if we didn't already have one
		if (!quantity)
			G_CheckPowerArmor(other);
	}
	else
		G_CheckPowerArmor(other);

	return true;
}

void Drop_PowerArmor(edict_t *ent, gitem_t *item)
{
	if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[item->id] == 1))
		Use_PowerArmor(ent, item);
	Drop_General(ent, item);
}

//======================================================================

bool Entity_IsVisibleToPlayer(edict_t* ent, edict_t* player)
{
	return !ent->item_picked_up_by[player->s.number - 1];
}

/*
===============
Touch_Item
===============
*/
TOUCH(Touch_Item) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	bool taken;

	if (!other->client)
		return;
	if (other->health < 1)
		return; // dead people can't pickup
	if (!ent->item->pickup)
		return; // not a grabbable item?



	// already got this instanced item
	if (coop->integer && P_UseCoopInstancedItems())
	{
		if (ent->item_picked_up_by[other->s.number - 1])
			return;
	}

	// ZOID
	if (CTFMatchSetup())
		return; // can't pick stuff up right now
				// ZOID

	// Map Trainer: Only allow pickup of target items (or any item if first pickup)
	if (level.map_trainer.initialized && level.map_trainer.training_enabled)
	{
		if (!level.map_trainer.first_pickup && !MapTrainer_IsTargetItem(ent))
		{
			// Not the target item - block pickup silently
			return;
		}
	}

	taken = ent->item->pickup(ent, other);

	ValidateSelectedItem(other);

	if (taken)
	{
		// Map Trainer: Check if this is the target item (only if training is enabled)
		if (level.map_trainer.training_enabled)
			MapTrainer_OnItemPickup(ent, other);
		
		// Map Trainer: Item Timing Trainer - start timer for armor, weapon, and powerup pickups
		if (level.map_trainer.timing_enabled && ent->item && ent->item->classname)
		{
			const char *classname = ent->item->classname;
			const char *item_name = nullptr;
			gtime_t respawn_time = 20_sec; // Default respawn time
			
			// Check for armor items (20 second respawn)
			if (Q_strcasecmp(classname, "item_armor_jacket") == 0)
			{
				item_name = "Green Armor";
				respawn_time = 20_sec;
			}
			else if (Q_strcasecmp(classname, "item_armor_combat") == 0)
			{
				item_name = "Yellow Armor";
				respawn_time = 20_sec;
			}
			else if (Q_strcasecmp(classname, "item_armor_body") == 0)
			{
				item_name = "Red Armor";
				respawn_time = 20_sec;
			}
			// Check for power armor items (20 second respawn)
			else if (Q_strcasecmp(classname, "item_power_screen") == 0)
			{
				item_name = "Power Screen";
				respawn_time = 20_sec;
			}
			else if (Q_strcasecmp(classname, "item_power_shield") == 0)
			{
				item_name = "Power Shield";
				respawn_time = 20_sec;
			}
			// Check for major powerups (300 second respawn = 5 minutes)
			else if (Q_strcasecmp(classname, "item_invulnerability") == 0)
			{
				item_name = "Invulnerability";
				respawn_time = 300_sec;
			}
			else if (Q_strcasecmp(classname, "item_invisibility") == 0)
			{
				item_name = "Invisibility";
				respawn_time = 300_sec;
			}
			// Check for lesser powerups (60 second respawn = 1 minute)
			else if (Q_strcasecmp(classname, "item_quad") == 0)
			{
				item_name = "Quad Damage";
				respawn_time = 60_sec;
			}
			else if (Q_strcasecmp(classname, "item_double") == 0)
			{
				item_name = "Double Damage";
				respawn_time = 60_sec;
			}
			else if (Q_strcasecmp(classname, "item_quadfire") == 0)
			{
				item_name = "DualFire Damage";
				respawn_time = 60_sec;
			}
			// Check for megahealth (special 25 second timing - 5 sec decay + 20 sec respawn)
			else if (Q_strcasecmp(classname, "item_health_mega") == 0)
			{
				item_name = "Megahealth";
				respawn_time = 25_sec; // Total time estimate (will be dynamic based on player health)
			}
			// Check for weapon items (30 second respawn - typical weapon respawn time)
			else if (ent->item->pickup == Pickup_Weapon)
			{
				// Get weapon respawn time from cvar (default 30 seconds)
				respawn_time = gtime_t::from_sec(g_weapon_respawn_time ? g_weapon_respawn_time->integer : 30);
				
				// Set friendly names for common weapons
				if (Q_strcasecmp(classname, "weapon_shotgun") == 0)
					item_name = "Shotgun";
				else if (Q_strcasecmp(classname, "weapon_supershotgun") == 0)
					item_name = "Super Shotgun";
				else if (Q_strcasecmp(classname, "weapon_machinegun") == 0)
					item_name = "Machinegun";
				else if (Q_strcasecmp(classname, "weapon_chaingun") == 0)
					item_name = "Chaingun";
				else if (Q_strcasecmp(classname, "weapon_grenadelauncher") == 0)
					item_name = "Grenade Launcher";
				else if (Q_strcasecmp(classname, "weapon_rocketlauncher") == 0)
					item_name = "Rocket Launcher";
				else if (Q_strcasecmp(classname, "weapon_hyperblaster") == 0)
					item_name = "Hyperblaster";
				else if (Q_strcasecmp(classname, "weapon_railgun") == 0)
					item_name = "Railgun";
				else if (Q_strcasecmp(classname, "weapon_bfg10k") == 0)
					item_name = "BFG10K";
				else
					item_name = "Weapon"; // Generic fallback
			}
				
					if (item_name)
		{
			// Create or update timing entry for this item
			map_trainer_t::timing_entry_t* timing_entry = MapTrainer_CreateOrUpdateTimingEntry(
				classname, item_name, ent->s.origin, level.time, respawn_time);
			
			if (timing_entry)
			{
				// Special handling for megahealth
				if (Q_strcasecmp(classname, "item_health_mega") == 0)
				{
					timing_entry->is_megahealth = true;
					timing_entry->megahealth_player = other;
					timing_entry->megahealth_decay_finished = false;
					timing_entry->megahealth_respawn_start = 0_ms;
					// Grace period lasts until health decay is finished (use very large value)
					timing_entry->grace_period_end = level.time + 999999_sec;
				}
				else
				{
					timing_entry->is_megahealth = false;
					timing_entry->megahealth_player = nullptr;
					timing_entry->megahealth_decay_finished = false;
					timing_entry->megahealth_respawn_start = 0_ms;
				}
				
				if (level.map_trainer.timing_debug_enabled)
				{
					gi.LocClient_Print(other, PRINT_HIGH, G_Fmt("[DEBUG] Pickup: {} at ({:.1f}, {:.1f}, {:.1f}) time {:.2f} respawn {:.2f}{}",
						item_name,
						ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
						level.time.seconds(),
						respawn_time.seconds(),
						timing_entry->is_megahealth ? " (MEGAHEALTH)" : ""
					).data());
				}
			}
			else
			{
				gi.LocClient_Print(other, PRINT_HIGH, "Warning: Too many concurrent timings active!");
			}

			// Show pickup message
			if (Q_strcasecmp(classname, "item_health_mega") == 0)
			{
				gi.LocClient_Print(other, PRINT_HIGH, "Megahealth - 20s timer after health < 100");
			}
			else
			{
				gi.LocClient_Print(other, PRINT_HIGH, G_Fmt("{} back in {:.0f} seconds", 
					item_name, respawn_time.seconds()).data());
			}
		}
		}
		
		// flash the screen
		other->client->bonus_alpha = 0.25;

		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(ent->item->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ent->item->id;
		other->client->pickup_msg_time = level.time + 3_sec;

		// change selected item if we still have it
		if (ent->item->use && other->client->pers.inventory[ent->item->id])
		{
			other->client->ps.stats[STAT_SELECTED_ITEM] = other->client->pers.selected_item = ent->item->id;
			other->client->ps.stats[STAT_SELECTED_ITEM_NAME] = 0; // don't set name on pickup item since it's already there
		}

		if (ent->noise_index)
			gi.sound(other, CHAN_ITEM, ent->noise_index, 1, ATTN_NORM, 0);
		else if (ent->item->pickup_sound)
			gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
		
		int32_t player_number = other->s.number - 1;

		if (coop->integer && P_UseCoopInstancedItems() && !ent->item_picked_up_by[player_number])
		{
			ent->item_picked_up_by[player_number] = true;

			// [Paril-KEX] this is to fix a coop quirk where items
			// that send a message on pick up will only print on the
			// player that picked them up, and never anybody else; 
			// when instanced items are enabled we don't need to limit
			// ourselves to this, but it does mean that relays that trigger
			// messages won't work, so we'll have to fix those
			if (ent->message)
				G_PrintActivationMessage(ent, other, false);
		}
	}

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_TARGETS_USED))
	{
		// [Paril-KEX] see above msg; this also disables the message in DM
		// since there's no need to print pickup messages in DM (this wasn't
		// even a documented feature, relays were traditionally used for this)
		const char *message_backup = nullptr;

		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		G_UseTargets(ent, other);
		
		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		ent->spawnflags |= SPAWNFLAG_ITEM_TARGETS_USED;
	}

	if (taken)
	{
		bool should_remove = false;

		if (coop->integer)
		{
			// in coop with instanced items, *only* dropped 
			// player items will ever get deleted permanently.
			if (P_UseCoopInstancedItems())
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER);
			// in coop without instanced items, IF_STAY_COOP items remain
			// if not dropped
			else
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER) || !(ent->item->flags & IF_STAY_COOP);
		}
		else
			should_remove = !deathmatch->integer || ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);

		if (should_remove)
		{
			if (ent->flags & FL_RESPAWN)
				ent->flags &= ~FL_RESPAWN;
			else
				G_FreeEdict(ent);
		}
	}
}

//======================================================================

TOUCH(drop_temp_touch) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (other == ent->owner)
		return;

	Touch_Item(ent, other, tr, other_touching_self);
}

THINK(drop_make_touchable) (edict_t *ent) -> void
{
	ent->touch = Touch_Item;
	if (deathmatch->integer)
	{
		ent->nextthink = level.time + 29_sec;
		ent->think = G_FreeEdict;
	}
}

edict_t *Drop_Item(edict_t *ent, gitem_t *item)
{
	edict_t *dropped;
	vec3_t	 forward, right;
	vec3_t	 offset;

	dropped = G_Spawn();

	dropped->item = item;
	dropped->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	dropped->classname = item->classname;
	dropped->s.effects = item->world_model_flags;
	gi.setmodel(dropped, dropped->item->world_model);
	dropped->s.renderfx = RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE; // PGM
	dropped->mins = { -15, -15, -15 };
	dropped->maxs = { 15, 15, 15 };
	dropped->solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = drop_temp_touch;
	dropped->owner = ent;

	if (ent->client)
	{
		trace_t trace;

		AngleVectors(ent->client->v_angle, forward, right, nullptr);
		offset = { 24, 0, -16 };
		dropped->s.origin = G_ProjectSource(ent->s.origin, offset, forward, right);
		trace = gi.trace(ent->s.origin, dropped->mins, dropped->maxs, dropped->s.origin, ent, CONTENTS_SOLID);
		dropped->s.origin = trace.endpos;
	}
	else
	{
		AngleVectors(ent->s.angles, forward, right, nullptr);
		dropped->s.origin = (ent->absmin + ent->absmax) / 2;
	}

	G_FixStuckObject(dropped, dropped->s.origin);

	dropped->velocity = forward * 100;
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextthink = level.time + 1_sec;

	if (coop->integer && P_UseCoopInstancedItems())
		dropped->svflags |= SVF_INSTANCED;

	gi.linkentity(dropped);
	return dropped;
}

USE(Use_Item) (edict_t *ent, edict_t *other, edict_t *activator) -> void
{
	ent->svflags &= ~SVF_NOCLIENT;
	ent->use = nullptr;

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
	{
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
	}
	else
	{
		ent->solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	gi.linkentity(ent);
}

//======================================================================

/*
================
droptofloor
================
*/
THINK(droptofloor) (edict_t *ent) -> void
{
	trace_t tr;
	vec3_t	dest;

	// [Paril-KEX] scale foodcube based on how much we ingested
	if (strcmp(ent->classname, "item_foodcube") == 0)
	{
		ent->mins = vec3_t { -8, -8, -8 } * ent->s.scale;
		ent->maxs = vec3_t { 8, 8, 8 } * ent->s.scale;
	}
	else
	{
		ent->mins = { -15, -15, -15 };
		ent->maxs = { 15, 15, 15 };
	}

	if (ent->model)
		gi.setmodel(ent, ent->model);
	else
		gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;

	dest = ent->s.origin + vec3_t { 0, 0, -128 };

	tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		if (G_FixStuckObject(ent, ent->s.origin) == stuck_result_t::NO_GOOD_POSITION)
		{
			// RAFAEL
			if (strcmp(ent->classname, "item_foodcube") == 0)
				ent->velocity[2] = 0;
			else
			{
				// RAFAEL
				gi.Com_PrintFmt("{}: droptofloor: startsolid\n", *ent);
				G_FreeEdict(ent);
				return;
				// RAFAEL
			}
			// RAFAEL
		}
	}
	else
		ent->s.origin = tr.endpos;

	if (ent->team)
	{
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = nullptr;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;

		if (ent == ent->teammaster)
		{
			ent->nextthink = level.time + 10_hz;
			ent->think = DoRespawn;
		}
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
	{
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
		ent->s.effects &= ~(EF_ROTATE | EF_BOB);
		ent->s.renderfx &= ~RF_GLOW;
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
	{
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = Use_Item;
	}

	ent->watertype = gi.pointcontents(ent->s.origin);
	gi.linkentity(ent);
}

/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(gitem_t *it)
{
	const char *s, *start;
	char		data[MAX_QPATH];
	ptrdiff_t	len;
	gitem_t	*ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		gi.soundindex(it->pickup_sound);
	if (it->world_model)
		gi.modelindex(it->world_model);
	if (it->view_model)
		gi.modelindex(it->view_model);
	if (it->icon)
		gi.imageindex(it->icon);

	// parse everything for its ammo
	if (it->ammo)
	{
		ammo = GetItemByIndex(it->ammo);
		if (ammo != it)
			PrecacheItem(ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s)
	{
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s - start;
		if (len >= MAX_QPATH || len < 5)
			gi.Com_ErrorFmt("PrecacheItem: {} has bad precache string", it->classname);
		memcpy(data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!strcmp(data + len - 3, "md2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "sp2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "wav"))
			gi.soundindex(data);
		if (!strcmp(data + len - 3, "pcx"))
			gi.imageindex(data);
	}
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem(edict_t *ent, gitem_t *item)
{
	// [Sam-KEX]
	// Paril: allow all keys to be trigger_spawn'd (N64 uses this
	// a few different times)
	if (item->flags & IF_KEY)
	{
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		{
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->use = Use_Item;
		}
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
		{
			ent->solid = SOLID_BBOX;
			ent->touch = nullptr;
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
			ent->s.renderfx &= ~RF_GLOW;
		}
	}
	else if (ent->spawnflags.value >= SPAWNFLAG_ITEM_MAX.value) // PGM
	{
		ent->spawnflags = SPAWNFLAG_NONE;
		gi.Com_PrintFmt("{} has invalid spawnflags set\n", *ent);
	}

	// some items will be prevented in deathmatch
	if (deathmatch->integer)
	{
		// [Kex] In instagib, spawn no pickups!
		if (g_instagib->value)
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor ||
				item->pickup == Pickup_Powerup || item->pickup == Pickup_Sphere || item->pickup == Pickup_Doppleganger ||
				(item->flags & IF_HEALTH) || (item->flags & IF_AMMO) || item->pickup == Pickup_Weapon || item->pickup == Pickup_Pack ||
				item->id == IT_ITEM_BANDOLIER || item->id == IT_ITEM_PACK ||
				item->id == IT_AMMO_NUKE)
			{
				G_FreeEdict(ent);
				return;
			}
		}

		if (g_no_armor->integer)
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_items->integer)
		{
			if (item->pickup == Pickup_Powerup)
			{
				G_FreeEdict(ent);
				return;
			}
			//=====
			// ROGUE
			if (item->pickup == Pickup_Sphere)
			{
				G_FreeEdict(ent);
				return;
			}
			if (item->pickup == Pickup_Doppleganger)
			{
				G_FreeEdict(ent);
				return;
			}
			// ROGUE
			//=====
		}
		if (g_no_health->integer)
		{
			if (item->flags & IF_HEALTH)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (G_CheckInfiniteAmmo(item))
		{
			if (item->flags == IF_AMMO)
			{
				G_FreeEdict(ent);
				return;
			}

			// [Paril-KEX] some item swappage 
			// BFG too strong in Infinite Ammo mode
			if (item->id == IT_WEAPON_BFG)
				item = GetItemByIndex(IT_WEAPON_DISRUPTOR);
		}

		//==========
		// ROGUE
		if (g_no_mines->integer)
		{
			if (item->id == IT_WEAPON_PROXLAUNCHER || item->id == IT_AMMO_PROX || item->id == IT_AMMO_TESLA || item->id == IT_AMMO_TRAP)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_nukes->integer)
		{
			if (item->id == IT_AMMO_NUKE)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_spheres->integer)
		{
			if (item->pickup == Pickup_Sphere)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		// ROGUE
		//==========
	}

	//==========
	// ROGUE
	// DM only items
	if (!deathmatch->integer)
	{
		if (item->pickup == Pickup_Doppleganger || item->pickup == Pickup_Nuke)
		{
			gi.Com_PrintFmt("{} spawned in non-DM; freeing...\n", *ent);
			G_FreeEdict(ent);
			return;
		}
		if ((item->use == Use_Vengeance) || (item->use == Use_Hunter))
		{
			gi.Com_PrintFmt("{} spawned in non-DM; freeing...\n", *ent);
			G_FreeEdict(ent);
			return;
		}
	}
	// ROGUE
	//==========

	// [Paril-KEX] power armor breaks infinite ammo
	if (G_CheckInfiniteAmmo(item))
	{
		if (item->id == IT_ITEM_POWER_SHIELD || item->id == IT_ITEM_POWER_SCREEN)
			item = GetItemByIndex(IT_ARMOR_BODY);
	}

	// ZOID
	// Don't spawn the flags unless enabled
	if (!ctf->integer && (item->id == IT_FLAG1 || item->id == IT_FLAG2))
	{
		G_FreeEdict(ent);
		return;
	}
	// ZOID

	// set final classname now
	ent->classname = item->classname;

	PrecacheItem(item);

	if (coop->integer && (item->id == IT_KEY_POWER_CUBE || item->id == IT_KEY_EXPLOSIVE_CHARGES))
	{
		ent->spawnflags.value |= (1 << (8 + level.power_cubes));
		level.power_cubes++;
	}

	// mark all items as instanced
	if (coop->integer)
	{
		if (P_UseCoopInstancedItems())
			ent->svflags |= SVF_INSTANCED;
	}

	ent->item = item;
	ent->nextthink = level.time + 20_hz; // items start after other solids
	ent->think = droptofloor;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD;
	if (ent->model)
		gi.modelindex(ent->model);

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		SetTriggeredSpawn(ent);

	// ZOID
	// flags are server animated and have special handling
	if (item->id == IT_FLAG1 || item->id == IT_FLAG2)
	{
		ent->think = CTFFlagSetup;
	}
	// ZOID
}

void P_ToggleFlashlight(edict_t *ent, bool state)
{
	if (!!(ent->flags & FL_FLASHLIGHT) == state)
		return;

	ent->flags ^= FL_FLASHLIGHT;

	gi.sound(ent, CHAN_AUTO, gi.soundindex(ent->flags & FL_FLASHLIGHT ? "items/flashlight_on.wav" : "items/flashlight_off.wav"), 1.f, ATTN_STATIC, 0);
}

static void Use_Flashlight(edict_t *ent, gitem_t *inv)
{
	P_ToggleFlashlight(ent, !(ent->flags & FL_FLASHLIGHT));
}

constexpr size_t MAX_TEMP_POI_POINTS = 128;

void Compass_Update(edict_t *ent, bool first)
{
	vec3_t *&points = level.poi_points[ent->s.number - 1];

	// deleted for some reason
	if (!points)
		return;

	if (!ent->client->help_draw_points)
		return;
	if (ent->client->help_draw_time >= level.time)
		return;

	// don't draw too many points
	float distance = (points[ent->client->help_draw_index] - ent->s.origin).length();
	if (distance > 4096 ||
		!gi.inPHS(ent->s.origin, points[ent->client->help_draw_index], false))
	{
		ent->client->help_draw_points = false;
		return;
	}

	gi.WriteByte(svc_help_path);
	gi.WriteByte(first ? 1 : 0);
	gi.WritePosition(points[ent->client->help_draw_index]);
	
	if (ent->client->help_draw_index == ent->client->help_draw_count - 1)
		gi.WriteDir((ent->client->help_poi_location - points[ent->client->help_draw_index]).normalized());
	else
		gi.WriteDir((points[ent->client->help_draw_index + 1] - points[ent->client->help_draw_index]).normalized());
	gi.unicast(ent, false);

	P_SendLevelPOI(ent);

	gi.local_sound(ent, points[ent->client->help_draw_index], world, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NORM, 0.0f, GetUnicastKey());

	// done
	if (ent->client->help_draw_index == ent->client->help_draw_count - 1)
	{
		ent->client->help_draw_points = false;
		return;
	}

	ent->client->help_draw_index++;
	ent->client->help_draw_time = level.time + 200_ms;
}

static void Use_Compass(edict_t *ent, gitem_t *inv)
{
	if (!level.valid_poi)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$no_valid_poi");
		return;
	}

	if (level.current_dynamic_poi)
		level.current_dynamic_poi->use(level.current_dynamic_poi, ent, ent);
	
	ent->client->help_poi_location = level.current_poi;
	ent->client->help_poi_image = level.current_poi_image;

	vec3_t *&points = level.poi_points[ent->s.number - 1];

	if (!points)
		points = (vec3_t *) gi.TagMalloc(sizeof(vec3_t) * (MAX_TEMP_POI_POINTS + 1), TAG_LEVEL);

	PathRequest request;
	request.start = ent->s.origin;
	request.goal = level.current_poi;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.array = points + 1;
	request.pathPoints.count = MAX_TEMP_POI_POINTS;

	PathInfo info;

	if (gi.GetPathToGoal(request, info))
	{
		// TODO: optimize points?
		ent->client->help_draw_points = true;
		ent->client->help_draw_count = min((size_t)info.numPathPoints, MAX_TEMP_POI_POINTS);
		ent->client->help_draw_index = 1;

		// remove points too close to the player so they don't have to backtrack
		for (int i = 1; i < 1 + ent->client->help_draw_count; i++)
		{
			float distance = (points[i] - ent->s.origin).length();
			if (distance > 192)
			{
				break;
			}

			ent->client->help_draw_index = i;
		}

		// create an extra point in front of us if we're facing away from the first real point
		float d = ((*(points + ent->client->help_draw_index)) - ent->s.origin).normalized().dot(ent->client->v_forward);

		if (d < 0.3f)
		{
			vec3_t p = ent->s.origin + (ent->client->v_forward * 64.f);

			trace_t tr = gi.traceline(ent->s.origin + vec3_t{0.f, 0.f, (float) ent->viewheight}, p, nullptr, MASK_SOLID);

			ent->client->help_draw_index--;
			ent->client->help_draw_count++;

			if (tr.fraction < 1.0f)
				tr.endpos += tr.plane.normal * 8.f;

			*(points + ent->client->help_draw_index) = tr.endpos;
		}

		ent->client->help_draw_time = 0_ms;
		Compass_Update(ent, true);
	}
	else
	{
		P_SendLevelPOI(ent);
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.f, ATTN_NORM, 0, GetUnicastKey());
	}
}

//======================================================================

// clang-format off
gitem_t	itemlist[] = 
{
	{ },	// leave index 0 alone

	//
	// ARMOR
	//
	

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16)
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/armor/body/tris.md2"
*/
	{
		/* id */ IT_ARMOR_BODY,
		/* classname */ "item_armor_body", 
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/body/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_bodyarmor",
		/* use_name */   "Body Armor",
		/* pickup_name */   "$item_body_armor",
		/* pickup_name_definite */ "$item_body_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &bodyarmor_info
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ARMOR_COMBAT,
		/* classname */ "item_armor_combat", 
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/combat/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_combatarmor",
		/* use_name */  "Combat Armor",
		/* pickup_name */  "$item_combat_armor",
		/* pickup_name_definite */ "$item_combat_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &combatarmor_info
	},

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ARMOR_JACKET,
		/* classname */ "item_armor_jacket", 
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/jacket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_jacketarmor",
		/* use_name */  "Jacket Armor",
		/* pickup_name */  "$item_jacket_armor",
		/* pickup_name_definite */ "$item_jacket_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &jacketarmor_info
	},

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ARMOR_SHARD,
		/* classname */ "item_armor_shard", 
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar2_pkup.wav",
		/* world_model */ "models/items/armor/shard/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_armor_shard",
		/* use_name */  "Armor Shard",
		/* pickup_name */  "$item_armor_shard",
		/* pickup_name_definite */ "$item_armor_shard_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR
	},

/*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_POWER_SCREEN,
		/* classname */ "item_power_screen", 
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/screen/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powerscreen",
		/* use_name */  "Power Screen",
		/* pickup_name */  "$item_power_screen",
		/* pickup_name_definite */ "$item_power_screen_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SCREEN,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

/*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_POWER_SHIELD,
		/* classname */ "item_power_shield",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/shield/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powershield",
		/* use_name */  "Power Shield",
		/* pickup_name */  "$item_power_shield",
		/* pickup_name_definite */ "$item_power_shield_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SHIELD,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

	//
	// WEAPONS 
	//

/* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		/* id */ IT_WEAPON_GRAPPLE,
		/* classname */ "weapon_grapple", 
		/* pickup */ nullptr,
		/* use */ Use_Weapon,
		/* drop */ nullptr,
		/* weaponthink */ CTFWeapon_Grapple,
		/* pickup_sound */ nullptr,
		/* world_model */ nullptr,
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/grapple/tris.md2",
		/* icon */ "w_grapple",
		/* use_name */  "Grapple",
		/* pickup_name */  "$item_grapple",
		/* pickup_name_definite */ "$item_grapple_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_NO_HASTE | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ "#w_grapple.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/grapple/grfire.wav weapons/grapple/grpull.wav weapons/grapple/grhang.wav weapons/grapple/grreset.wav weapons/grapple/grhit.wav weapons/grapple/grfly.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		/* id */ IT_WEAPON_BLASTER,
		/* classname */ "weapon_blaster", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ nullptr,
		/* weaponthink */ Weapon_Blaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_blast/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_blast/tris.md2",
		/* icon */ "w_blaster",
		/* use_name */  "Blaster",
		/* pickup_name */  "$item_blaster",
		/* pickup_name_definite */ "$item_blaster_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NOT_RANDOM,
		/* vwep_model */ "#w_blaster.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/blastf1a.wav misc/lasfly.wav"
	},

	/*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
	*/
	{
		/* id */ IT_WEAPON_CHAINFIST,
		/* classname */ "weapon_chainfist",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ChainFist,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chainf/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chainf/tris.md2",
		/* icon */ "w_chainfist",
		/* use_name */  "Chainfist",
		/* pickup_name */  "$item_chainfist",
		/* pickup_name_definite */ "$item_chainfist_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NO_HASTE,
		/* vwep_model */ "#w_chainfist.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/sawidle.wav weapons/sawhit.wav weapons/sawslice.wav",
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SHOTGUN,
		/* classname */ "weapon_shotgun", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Shotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg/tris.md2",
		/* icon */ "w_shotgun",
		/* use_name */  "Shotgun",
		/* pickup_name */  "$item_shotgun",
		/* pickup_name_definite */ "$item_shotgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_shotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/shotgf1b.wav weapons/shotgr1b.wav"
	},

/*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_SSHOTGUN,
		/* classname */ "weapon_supershotgun", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_SuperShotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg2/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg2/tris.md2",
		/* icon */ "w_sshotgun",
		/* use_name */  "Super Shotgun",
		/* pickup_name */  "$item_super_shotgun",
		/* pickup_name_definite */ "$item_super_shotgun_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_sshotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/sshotf1b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 10
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_MACHINEGUN,
		/* classname */ "weapon_machinegun", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Machinegun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_machn/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_machn/tris.md2",
		/* icon */ "w_machinegun",
		/* use_name */  "Machinegun",
		/* pickup_name */  "$item_machinegun",
		/* pickup_name_definite */ "$item_machinegun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_machinegun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/machgf1b.wav weapons/machgf2b.wav weapons/machgf3b.wav weapons/machgf4b.wav weapons/machgf5b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

	// ROGUE
/*QUAKED weapon_etf_rifle (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_WEAPON_ETF_RIFLE,
		/* classname */ "weapon_etf_rifle",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ETF_Rifle,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_etf_rifle/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_etf_rifle/tris.md2",
		/* icon */ "w_etf_rifle",
		/* use_name */  "ETF Rifle",
		/* pickup_name */  "$item_etf_rifle",
		/* pickup_name_definite */ "$item_etf_rifle_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_FLECHETTES,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_etfrifle.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/nail1.wav models/proj/flechette/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},
	// ROGUE

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_CHAINGUN,
		/* classname */ "weapon_chaingun", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Chaingun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chain/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chain/tris.md2",
		/* icon */ "w_chaingun",
		/* use_name */  "Chaingun",
		/* pickup_name */  "$item_chaingun",
		/* pickup_name_definite */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_chaingun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/chngnu1a.wav weapons/chngnl1a.wav weapons/machgf3b.wav weapons/chngnd1a.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 60
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_GRENADES,
		/* classname */ "ammo_grenades",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Grenade,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/grenades/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_handgr/tris.md2",
		/* icon */ "a_grenades",
		/* use_name */  "Grenades",
		/* pickup_name */  "$item_grenades",
		/* pickup_name_definite */ "$item_grenades_def",
		/* quantity */ 5,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON,
		/* vwep_model */ "#a_grenades.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */ "weapons/hgrent1a.wav weapons/hgrena1b.wav weapons/hgrenc1b.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/objects/grenade3/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 2
	},

// RAFAEL
/*QUAKED ammo_trap (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_TRAP,
		/* classname */ "ammo_trap",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Trap,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_trap/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_trap/tris.md2",
		/* icon */ "a_trap",
		/* use_name */  "Trap",
		/* pickup_name */  "$item_trap",
		/* pickup_name_definite */ "$item_trap_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_TRAP,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_trap.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TRAP,
		/* precaches */ "misc/fhit3.wav weapons/trapcock.wav weapons/traploop.wav weapons/trapsuck.wav weapons/trapdown.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav models/weapons/z_trap/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},
// RAFAEL

/*QUAKED ammo_tesla (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_AMMO_TESLA,
		/* classname */ "ammo_tesla",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Tesla,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_tesl/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_tesla/tris.md2",
		/* icon */ "a_tesla",
		/* use_name */  "Tesla",
		/* pickup_name */  "$item_tesla",
		/* pickup_name_definite */ "$item_tesla_def",
		/* quantity */ 3,
		/* ammo */ IT_AMMO_TESLA,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_tesla.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TESLA,
		/* precaches */ "weapons/teslaopen.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/weapons/g_tesla/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_launch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_GLAUNCHER,
		/* classname */ "weapon_grenadelauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_GrenadeLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_launch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_launch/tris.md2",
		/* icon */ "w_glauncher",
		/* use_name */  "Grenade Launcher",
		/* pickup_name */  "$item_grenade_launcher",
		/* pickup_name_definite */ "$item_grenade_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_glauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "models/objects/grenade4/tris.md2 weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav"
	},

	// ROGUE
/*QUAKED weapon_proxlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_WEAPON_PROXLAUNCHER,
		/* classname */ "weapon_proxlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ProxLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_plaunch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_plaunch/tris.md2",
		/* icon */ "w_proxlaunch",
		/* use_name */  "Prox Launcher",
		/* pickup_name */  "$item_prox_launcher",
		/* pickup_name_definite */ "$item_prox_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_PROX,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav weapons/proxwarn.wav weapons/proxopen.wav",
	},
	// ROGUE

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_RLAUNCHER,
		/* classname */ "weapon_rocketlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_RocketLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rocket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rocket/tris.md2",
		/* icon */ "w_rlauncher",
		/* use_name */  "Rocket Launcher",
		/* pickup_name */  "$item_rocket_launcher",
		/* pickup_name_definite */ "$item_rocket_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROCKETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON|IF_STAY_COOP,
		/* vwep_model */ "#w_rlauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "models/objects/rocket/tris.md2 weapons/rockfly.wav weapons/rocklf1a.wav weapons/rocklr1b.wav models/objects/debris2/tris.md2"
	},

/*QUAKED weapon_hyperblaster (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_HYPERBLASTER,
		/* classname */ "weapon_hyperblaster", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_HyperBlaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_hyperb/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_hyperb/tris.md2",
		/* icon */ "w_hyperblaster",
		/* use_name */  "HyperBlaster",
		/* pickup_name */  "$item_hyperblaster",
		/* pickup_name_definite */ "$item_hyperblaster_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON|IF_STAY_COOP,
		/* vwep_model */ "#w_hyperblaster.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/hyprbu1a.wav weapons/hyprbl1a.wav weapons/hyprbf1a.wav weapons/hyprbd1a.wav misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

// RAFAEL
/*QUAKED weapon_boomer (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_IONRIPPER,
		/* classname */ "weapon_boomer",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Ionripper,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_boom/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_boomer/tris.md2",
		/* icon */ "w_ripper",
		/* use_name */  "Ionripper",
		/* pickup_name */  "$item_ionripper",
		/* pickup_name_definite */ "$item_ionripper_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_ripper.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/rippfire.wav models/objects/boomrang/tris.md2 misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},
// RAFAEL

// ROGUE
	/*QUAKED weapon_plasmabeam (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
	*/ 
	{
		/* id */ IT_WEAPON_PLASMABEAM,
		/* classname */ "weapon_plasmabeam",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Heatbeam,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_beamer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_beamer/tris.md2",
		/* icon */ "w_heatbeam",
		/* use_name */  "Plasma Beam",
		/* pickup_name */  "$item_plasma_beam",
		/* pickup_name_definite */ "$item_plasma_beam_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plasma.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/bfg__l1a.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},
//rogue

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_RAILGUN,
		/* classname */ "weapon_railgun", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Railgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rail/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rail/tris.md2",
		/* icon */ "w_railgun",
		/* use_name */  "Railgun",
		/* pickup_name */  "$item_railgun",
		/* pickup_name_definite */ "$item_railgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SLUGS,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON|IF_STAY_COOP,
		/* vwep_model */ "#w_railgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/rg_hum.wav"
	},

// RAFAEL 14-APR-98
/*QUAKED weapon_phalanx (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_PHALANX,
		/* classname */ "weapon_phalanx",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Phalanx,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotx/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotx/tris.md2",
		/* icon */ "w_phallanx",
		/* use_name */  "Phalanx",
		/* pickup_name */  "$item_phalanx",
		/* pickup_name_definite */ "$item_phalanx_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_MAGSLUG,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_phalanx.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/plasshot.wav sprites/s_photon.sp2 weapons/rockfly.wav"
	},
// RAFAEL

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_BFG,
		/* classname */ "weapon_bfg",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_BFG,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_bfg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_bfg/tris.md2",
		/* icon */ "w_bfg",
		/* use_name */  "BFG10K",
		/* pickup_name */  "$item_bfg10k",
		/* pickup_name_definite */ "$item_bfg10k_def",
		/* quantity */ 50,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON|IF_STAY_COOP,
		/* vwep_model */ "#w_bfg.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "sprites/s_bfg1.sp2 sprites/s_bfg2.sp2 sprites/s_bfg3.sp2 weapons/bfg__f1y.wav weapons/bfg__l1a.wav weapons/bfg__x1b.wav weapons/bfg_hum.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},

	// =========================
	// ROGUE WEAPONS
	/*QUAKED weapon_disintegrator (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
	*/
	{
		/* id */ IT_WEAPON_DISRUPTOR,
		/* classname */ "weapon_disintegrator",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Disintegrator,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_dist/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_dist/tris.md2",
		/* icon */ "w_disintegrator",
		/* use_name */  "Disruptor",
		/* pickup_name */  "$item_disruptor",
		/* pickup_name_definite */ "$item_disruptor_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROUNDS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_disrupt.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "models/proj/disintegrator/tris.md2 weapons/disrupt.wav weapons/disint2.wav weapons/disrupthit.wav",
	},

	// ROGUE WEAPONS
	// =========================

#if 0 // sorry little guy
	{
		/* id */ IT_WEAPON_DISINTEGRATOR,
		/* classname */ "weapon_beta_disintegrator",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Beta_Disintegrator,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_disint/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_disint/tris.md2",
		/* icon */ "w_bfg",
		/* use_name */  "Disintegrator",
		/* pickup_name */  "$item_disintegrator",
		/* pickup_name_definite */ "$item_disintegrator_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROUNDS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_bfg.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "",
	},
#endif

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_SHELLS,
		/* classname */ "ammo_shells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/shells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Shells",
		/* pickup_name */  "$item_shells",
		/* pickup_name_definite */ "$item_shells_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_BULLETS,
		/* classname */ "ammo_bullets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/bullets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Bullets",
		/* pickup_name */  "$item_bullets",
		/* pickup_name_definite */ "$item_bullets_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_CELLS,
		/* classname */ "ammo_cells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/cells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Cells",
		/* pickup_name */  "$item_cells",
		/* pickup_name_definite */ "$item_cells_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/ammo/rockets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS,
		/* classname */ "ammo_rockets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/rockets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_rockets",
		/* use_name */  "Rockets",
		/* pickup_name */  "$item_rockets",
		/* pickup_name_definite */ "$item_rockets_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_SLUGS,
		/* classname */ "ammo_slugs",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/slugs/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Slugs",
		/* pickup_name */  "$item_slugs",
		/* pickup_name_definite */ "$item_slugs_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

// RAFAEL
/*QUAKED ammo_magslug (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_MAGSLUG,
		/* classname */ "ammo_magslug",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/objects/ammo/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_mslugs",
		/* use_name */  "Mag Slug",
		/* pickup_name */  "$item_mag_slug",
		/* pickup_name_definite */ "$item_mag_slug_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAGSLUG
	},
// RAFAEL

// =======================================
// ROGUE AMMO

/*QUAKED ammo_flechettes (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_AMMO_FLECHETTES,
		/* classname */ "ammo_flechettes",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_flechette/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_flechettes",
		/* use_name */  "Flechettes",
		/* pickup_name */  "$item_flechettes",
		/* pickup_name_definite */ "$item_flechettes_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_FLECHETTES
	},

/*QUAKED ammo_prox (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_AMMO_PROX,
		/* classname */ "ammo_prox",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_prox/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_prox",
		/* use_name */  "Prox",
		/* pickup_name */  "$item_prox",
		/* pickup_name_definite */ "$item_prox_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "models/weapons/g_prox/tris.md2 weapons/proxwarn.wav"
	},

/*QUAKED ammo_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_AMMO_NUKE,
		/* classname */ "ammo_nuke",
		/* pickup */ Pickup_Nuke,
		/* use */ Use_Nuke,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_nuke",
		/* use_name */  "A-M Bomb",
		/* pickup_name */  "$item_am_bomb",
		/* pickup_name_definite */ "$item_am_bomb_def",
		/* quantity */ 300,
		/* ammo */ IT_AMMO_NUKE,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_AM_BOMB,
		/* precaches */ "weapons/nukewarn2.wav world/rumble.wav"
	},

/*QUAKED ammo_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_AMMO_ROUNDS,
		/* classname */ "ammo_disruptor",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_disr/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_disruptor",
		/* use_name */  "Rounds",
		/* pickup_name */  "$item_rounds",
		/* pickup_name_definite */ "$item_rounds_def",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_DISRUPTOR
	},
// ROGUE AMMO
// =======================================

	//
	// POWERUP ITEMS
	//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_QUAD,
		/* classname */ "item_quad", 
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Quad,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quaddama/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quad",
		/* use_name */  "Quad Damage",
		/* pickup_name */  "$item_quad_damage",
		/* pickup_name_definite */ "$item_quad_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_QUAD,
		/* precaches */ "items/damage.wav items/damage2.wav items/damage3.wav ctf/tech2x.wav"
	},

// RAFAEL
/*QUAKED item_quadfire (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_QUADFIRE,
		/* classname */ "item_quadfire", 
		/* pickup */ Pickup_Powerup,
		/* use */ Use_QuadFire,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quadfire/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quadfire",
		/* use_name */  "DualFire Damage",
		/* pickup_name */  "$item_dualfire_damage",
		/* pickup_name_definite */ "$item_dualfire_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_QUADFIRE,
		/* precaches */ "items/quadfire1.wav items/quadfire2.wav items/quadfire3.wav"
	},
// RAFAEL

/*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_INVULNERABILITY,
		/* classname */ "item_invulnerability",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Invulnerability,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/invulner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_invulnerability",
		/* use_name */  "Invulnerability",
		/* pickup_name */  "$item_invulnerability",
		/* pickup_name_definite */ "$item_invulnerability_def",
		/* quantity */ 300,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_INVULNERABILITY,
		/* precaches */ "items/protect.wav items/protect2.wav items/protect4.wav"
	},

/*QUAKED item_invisibility (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_INVISIBILITY,
		/* classname */ "item_invisibility",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Invisibility,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/cloaker/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_cloaker",
		/* use_name */  "Invisibility",
		/* pickup_name */  "$item_invisibility",
		/* pickup_name_definite */ "$item_invisibility_def",
		/* quantity */ 300,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_INVISIBILITY,
	},

/*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/silencer/tris.md2"
*/
	{
		/* id */ IT_ITEM_SILENCER,
		/* classname */ "item_silencer",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Silencer,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/silencer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_silencer",
		/* use_name */  "Silencer",
		/* pickup_name */  "$item_silencer",
		/* pickup_name_definite */ "$item_silencer_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SILENCER,
	},

/*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_REBREATHER,
		/* classname */ "item_breather",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Breather,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/breather/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_rebreather",
		/* use_name */  "Rebreather",
		/* pickup_name */  "$item_rebreather",
		/* pickup_name_definite */ "$item_rebreather_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_REBREATHER,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_ENVIROSUIT,
		/* classname */ "item_enviro",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Envirosuit,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/enviro/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_envirosuit",
		/* use_name */  "Environment Suit",
		/* pickup_name */  "$item_environment_suit",
		/* pickup_name_definite */ "$item_environment_suit_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ENVIROSUIT,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16)
Special item that gives +2 to maximum health
model="models/items/c_head/tris.md2"
*/
	{
		/* id */ IT_ITEM_ANCIENT_HEAD,
		/* classname */ "item_ancient_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/c_head/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Ancient Head",
		/* pickup_name */  "$item_ancient_head",
		/* pickup_name_definite */ "$item_ancient_head_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

	/*QUAKED item_legacy_head (.3 .3 1) (-16 -16 -16) (16 16 16)
	Special item that gives +5 to maximum health
	model="models/items/legacyhead/tris.md2"
	*/
	{
		/* id */ IT_ITEM_LEGACY_HEAD,
		/* classname */ "item_legacy_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/legacyhead/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Legacy Head",
		/* pickup_name */  "$item_legacy_head",
		/* pickup_name_definite */ "$item_legacy_head_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16)
gives +1 to maximum health
*/
	{
		/* id */ IT_ITEM_ADRENALINE,
		/* classname */ "item_adrenaline",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Adrenaline,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/adrenal/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_adrenaline",
		/* use_name */  "Adrenaline",
		/* pickup_name */  "$item_adrenaline",
		/* pickup_name_definite */ "$item_adrenaline_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ADRENALINE,
		/* precache */ "items/n_health.wav"
	},

/*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_BANDOLIER,
		/* classname */ "item_bandolier",
		/* pickup */ Pickup_Bandolier,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/band/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_bandolier",
		/* use_name */  "Bandolier",
		/* pickup_name */  "$item_bandolier",
		/* pickup_name_definite */ "$item_bandolier_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_PACK,
		/* classname */ "item_pack",
		/* pickup */ Pickup_Pack,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/pack/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_pack",
		/* use_name */  "Ammo Pack",
		/* pickup_name */  "$item_ammo_pack",
		/* pickup_name_definite */ "$item_ammo_pack_def",
		/* quantity */ 180,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP
	},


// ======================================
// PGM

/*QUAKED item_ir_goggles (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
gives +1 to maximum health
*/
	{
		/* id */ IT_ITEM_IR_GOGGLES,
		/* classname */ "item_ir_goggles",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_IR,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/goggles/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_ir",
		/* use_name */  "IR Goggles",
		/* pickup_name */  "$item_ir_goggles",
		/* pickup_name_definite */ "$item_ir_goggles_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_IR_GOGGLES,
		/* precaches */ "misc/ir_start.wav"
	},

/*QUAKED item_double (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_ITEM_DOUBLE,
		/* classname */ "item_double", 
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Double,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/ddamage/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_double",
		/* use_name */  "Double Damage",
		/* pickup_name */  "$item_double_damage",
		/* pickup_name_definite */ "$item_double_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOUBLE,
		/* precaches */ "misc/ddamage1.wav misc/ddamage2.wav misc/ddamage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_sphere_vengeance (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_ITEM_SPHERE_VENGEANCE,
		/* classname */ "item_sphere_vengeance", 
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Vengeance,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/vengnce/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_vengeance",
		/* use_name */  "vengeance sphere",
		/* pickup_name */  "$item_vengeance_sphere",
		/* pickup_name_definite */ "$item_vengeance_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_VENGEANCE,
		/* precaches */ "spheres/v_idle.wav"
	},

/*QUAKED item_sphere_hunter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_ITEM_SPHERE_HUNTER,
		/* classname */ "item_sphere_hunter", 
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Hunter,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/hunter/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_hunter",
		/* use_name */  "hunter sphere",
		/* pickup_name */  "$item_hunter_sphere",
		/* pickup_name_definite */ "$item_hunter_sphere_def",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_HUNTER,
		/* precaches */ "spheres/h_idle.wav spheres/h_active.wav spheres/h_lurk.wav"
	},

/*QUAKED item_sphere_defender (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_ITEM_SPHERE_DEFENDER,
		/* classname */ "item_sphere_defender", 
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Defender,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/defender/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_defender",
		/* use_name */  "defender sphere",
		/* pickup_name */  "$item_defender_sphere",
		/* pickup_name_definite */ "$item_defender_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_DEFENDER,
		/* precaches */ "models/objects/laser/tris.md2 models/items/shell/tris.md2 spheres/d_idle.wav"
	},

/*QUAKED item_doppleganger (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_ITEM_DOPPELGANGER,
		/* classname */ "item_doppleganger",
		/* pickup */ Pickup_Doppleganger,
		/* use */ Use_Doppleganger,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/dopple/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_doppleganger",
		/* use_name */  "Doppelganger",
		/* pickup_name */  "$item_doppleganger",
		/* pickup_name_definite */ "$item_doppleganger_def",
		/* quantity */ 90,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOPPELGANGER,
		/* precaches */ "models/objects/dopplebase/tris.md2 models/items/spawngro3/tris.md2 medic_commander/monsterspawn1.wav models/items/hunter/tris.md2 models/items/vengnce/tris.md2",
	},

	{
		/* id */ IT_ITEM_TAG_TOKEN,
		/* classname */ nullptr,
		/* pickup */ Tag_PickupToken,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/tagtoken/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB | EF_TAGTRAIL,
		/* view_model */ nullptr,
		/* icon */ "i_tagtoken",
		/* use_name */  "Tag Token",
		/* pickup_name */  "$item_tag_token",
		/* pickup_name_definite */ "$item_tag_token_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_NOT_GIVEABLE
	},

// PGM
// ======================================

	//
	// KEYS
	//
/*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16)
key for computer centers
*/
	{
		/* id */ IT_KEY_DATA_CD,
		/* classname */ "key_data_cd",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/data_cd/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_datacd",
		/* use_name */  "Data CD",
		/* pickup_name */  "$item_data_cd",
		/* pickup_name_definite */ "$item_data_cd_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH
warehouse circuits
*/
	{
		/* id */ IT_KEY_POWER_CUBE,
		/* classname */ "key_power_cube",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/power/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_powercube",
		/* use_name */  "Power Cube",
		/* pickup_name */  "$item_power_cube",
		/* pickup_name_definite */ "$item_power_cube_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_explosive_charges (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH
warehouse circuits
*/
	{
		/* id */ IT_KEY_EXPLOSIVE_CHARGES,
		/* classname */ "key_explosive_charges",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/charge/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_charges",
		/* use_name */  "Explosive Charges",
		/* pickup_name */  "$item_explosive_charges",
		/* pickup_name_definite */ "$item_explosive_charges_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_yellow_key (0 .5 .8) (-16 -16 -16) (16 16 16)
normal door key - yellow
[Sam-KEX] New key type for Q2 N64
*/
	{
		/* id */ IT_KEY_YELLOW,
		/* classname */ "key_yellow_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/yellow_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_yellow_key",
		/* use_name */  "Yellow Key",
		/* pickup_name */  "$item_yellow_key",
		/* pickup_name_definite */ "$item_yellow_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_core (0 .5 .8) (-16 -16 -16) (16 16 16)
key for N64
*/
	{
		/* id */ IT_KEY_POWER_CORE,
		/* classname */ "key_power_core",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/power_core/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Power Core",
		/* pickup_name */  "$item_power_core",
		/* pickup_name_definite */ "$item_power_core_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16)
key for the entrance of jail3
*/
	{
		/* id */ IT_KEY_PYRAMID,
		/* classname */ "key_pyramid",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pyramid/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Pyramid Key",
		/* pickup_name */  "$item_pyramid_key",
		/* pickup_name_definite */ "$item_pyramid_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16)
key for the city computer
model="models/items/keys/spinner/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_SPINNER,
		/* classname */ "key_data_spinner",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/spinner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_dataspin",
		/* use_name */  "Data Spinner",
		/* pickup_name */  "$item_data_spinner",
		/* pickup_name_definite */ "$item_data_spinner_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16)
security pass for the security level
model="models/items/keys/pass/tris.md2"
*/
	{
		/* id */ IT_KEY_PASS,
		/* classname */ "key_pass",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pass/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_security",
		/* use_name */  "Security Pass",
		/* pickup_name */  "$item_security_pass",
		/* pickup_name_definite */ "$item_security_pass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16)
normal door key - blue
*/
	{
		/* id */ IT_KEY_BLUE_KEY,
		/* classname */ "key_blue_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_bluekey",
		/* use_name */  "Blue Key",
		/* pickup_name */  "$item_blue_key",
		/* pickup_name_definite */ "$item_blue_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16)
normal door key - red
*/
	{
		/* id */ IT_KEY_RED_KEY,
		/* classname */ "key_red_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/red_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_redkey",
		/* use_name */  "Red Key",
		/* pickup_name */  "$item_red_key",
		/* pickup_name_definite */ "$item_red_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

// RAFAEL
/*QUAKED key_green_key (0 .5 .8) (-16 -16 -16) (16 16 16)
normal door key - blue
*/
	{
		/* id */ IT_KEY_GREEN_KEY,
		/* classname */ "key_green_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/green_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_green",
		/* use_name */  "Green Key",
		/* pickup_name */  "$item_green_key",
		/* pickup_name_definite */ "$item_green_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},
// RAFAEL

/*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16)
tank commander's head
*/
	{
		/* id */ IT_KEY_COMMANDER_HEAD,
		/* classname */ "key_commander_head",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/monsters/commandr/head/tris.md2",
		/* world_model_flags */ EF_GIB,
		/* view_model */ nullptr,
		/* icon */ "k_comhead",
		/* use_name */  "Commander's Head",
		/* pickup_name */  "$item_commanders_head",
		/* pickup_name_definite */ "$item_commanders_head_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},

/*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_KEY_AIRSTRIKE,
		/* classname */ "key_airstrike_target",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/target/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_airstrike",
		/* use_name */  "Airstrike Marker",
		/* pickup_name */  "$item_airstrike_marker",
		/* pickup_name_definite */ "$item_airstrike_marker_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY
	},
	
// ======================================
// PGM

/*QUAKED key_nuke_container (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_KEY_NUKE_CONTAINER,
		/* classname */ "key_nuke_container",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_contain",
		/* use_name */  "Antimatter Pod",
		/* pickup_name */  "$item_antimatter_pod",
		/* pickup_name_definite */ "$item_antimatter_pod_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY,
	},

/*QUAKED key_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
	{
		/* id */ IT_KEY_NUKE,
		/* classname */ "key_nuke",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_nuke",
		/* use_name */  "Antimatter Bomb",
		/* pickup_name */  "$item_antimatter_bomb",
		/* pickup_name_definite */ "$item_antimatter_bomb_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP|IF_KEY,
	},

// PGM
//

// PGM
// ======================================

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/healing/stimpack/tris.md2"
*/
	// Paril: split the healths up so they are always valid classnames
	{
		/* id */ IT_HEALTH_SMALL,
		/* classname */ "item_health_small",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/s_health.wav",
		/* world_model */ "models/items/healing/stimpack/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_stimpack",
		/* pickup_name_definite */ "$item_stimpack_def",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX
	},
/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/healing/medium/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEDIUM,
		/* classname */ "item_health",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/items/healing/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_small_medkit",
		/* pickup_name_definite */ "$item_small_medkit_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},
/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/healing/large/tris.md2"
*/
	{
		/* id */ IT_HEALTH_LARGE,
		/* classname */ "item_health_large",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/l_health.wav",
		/* world_model */ "models/items/healing/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_large_medkit",
		/* pickup_name_definite */ "$item_large_medkit",
		/* quantity */ 25,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},
/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/mega_h/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEGA,
		/* classname */ "item_health_mega",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/m_health.wav",
		/* world_model */ "models/items/mega_h/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_megahealth",
		/* use_name */  "Health",
		/* pickup_name */  "$item_mega_health",
		/* pickup_name_definite */ "$item_mega_health_def",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX | HEALTH_TIMED
	},

//ZOID
/*QUAKED item_flag_team1 (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		/* id */ IT_FLAG1,
		/* classname */ "item_flag_team1",
		/* pickup */ CTFPickup_Flag,
		/* use */ nullptr,
		/* drop */ CTFDrop_Flag, //Should this be null if we don't want players to drop it manually?
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag1.md2",
		/* world_model_flags */ EF_FLAG1,
		/* view_model */ nullptr,
		/* icon */ "i_ctf1",
		/* use_name */  "Red Flag",
		/* pickup_name */  "$item_red_flag",
		/* pickup_name_definite */ "$item_red_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team2 (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		/* id */ IT_FLAG2,
		/* classname */ "item_flag_team2",
		/* pickup */ CTFPickup_Flag,
		/* use */ nullptr,
		/* drop */ CTFDrop_Flag,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag2.md2",
		/* world_model_flags */ EF_FLAG2,
		/* view_model */ nullptr,
		/* icon */ "i_ctf2",
		/* use_name */  "Blue Flag",
		/* pickup_name */  "$item_blue_flag",
		/* pickup_name_definite */ "$item_blue_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/* Resistance Tech */
	{
		/* id */ IT_TECH_RESISTANCE,
		/* classname */ "item_tech1",
		/* pickup */ CTFPickup_Tech,
		/* use */ nullptr,
		/* drop */ CTFDrop_Tech,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/resistance/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech1",
		/* use_name */  "Disruptor Shield",
		/* pickup_name */  "$item_disruptor_shield",
		/* pickup_name_definite */ "$item_disruptor_shield_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH1,
		/* precaches */ "ctf/tech1.wav"
	},

/* Strength Tech */
	{
		/* id */ IT_TECH_STRENGTH,
		/* classname */ "item_tech2",
		/* pickup */ CTFPickup_Tech,
		/* use */ nullptr,
		/* drop */ CTFDrop_Tech,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/strength/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech2",
		/* use_name */  "Power Amplifier",
		/* pickup_name */  "$item_power_amplifier",
		/* pickup_name_definite */ "$item_power_amplifier_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH2,
		/* precaches */ "ctf/tech2.wav ctf/tech2x.wav"
	},

/* Haste Tech */
	{
		/* id */ IT_TECH_HASTE,
		/* classname */ "item_tech3",
		/* pickup */ CTFPickup_Tech,
		/* use */ nullptr,
		/* drop */ CTFDrop_Tech,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/haste/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech3",
		/* use_name */  "Time Accel",
		/* pickup_name */  "$item_time_accel",
		/* pickup_name_definite */ "$item_time_accel_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH3,
		/* precaches */ "ctf/tech3.wav"
	},

/* Regeneration Tech */
	{
		/* id */ IT_TECH_REGENERATION,
		/* classname */ "item_tech4",
		/* pickup */ CTFPickup_Tech,
		/* use */ nullptr,
		/* drop */ CTFDrop_Tech,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/regeneration/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech4",
		/* use_name */  "AutoDoc",
		/* pickup_name */  "$item_autodoc",
		/* pickup_name_definite */ "$item_autodoc_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH4,
		/* precaches */ "ctf/tech4.wav"
	},

	{
		/* id */ IT_ITEM_FLASHLIGHT,
		/* classname */ "item_flashlight", 
		/* pickup */ Pickup_General,
		/* use */ Use_Flashlight,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/flashlight/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_torch",
		/* use_name */  "Flashlight",
		/* pickup_name */  "$item_flashlight",
		/* pickup_name_definite */ "$item_flashlight_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_FLASHLIGHT,
		/* precaches */ "items/flashlight_on.wav items/flashlight_off.wav",
		/* sort_id */ -1
	},

	{
		/* id */ IT_ITEM_COMPASS,
		/* classname */ "item_compass", 
		/* pickup */ nullptr,
		/* use */ Use_Compass,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ nullptr,
		/* world_model */ nullptr,
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_compass",
		/* use_name */  "Compass",
		/* pickup_name */  "$item_compass",
		/* pickup_name_definite */ "$item_compass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_COMPASS,
		/* precaches */ "misc/help_marker.wav",
		/* sort_id */ -2
	}
};
// clang-format on

// ==================== MAP TRAINER SYSTEM ====================

void MapTrainer_Init()
{
	// Force weapon stay off for training mode
	gi.cvar_set("g_dm_weapons_stay", "0");
	
	level.map_trainer.items = nullptr;
	level.map_trainer.item_count = 0;
	level.map_trainer.current_target_index = -1;
	level.map_trainer.previous_target_index = -1;
	level.map_trainer.initialized = false;
	level.map_trainer.first_pickup = true;
	level.map_trainer.welcome_message_shown = false;
	level.map_trainer.welcome_message_time = 0_ms;
	
	// Initialize all item category toggles to enabled by default
	level.map_trainer.weapons_enabled = true;
	level.map_trainer.ammo_enabled = true;
	level.map_trainer.health_enabled = true;
	level.map_trainer.armor_enabled = true;
	level.map_trainer.powerups_enabled = true;
	
	// Initialize speedometer as enabled by default
	level.map_trainer.speedometer_enabled = true;
	
	// Initialize training as disabled by default
	level.map_trainer.training_enabled = false;
	
	// Initialize combine health packs as disabled by default (OFF = separated, ON = combined)
	level.map_trainer.combine_health_packs = false;
	
	// Initialize timing trainer as disabled by default
	level.map_trainer.timing_enabled = false;
	
	// Initialize free collect as enabled by default
	level.map_trainer.free_collect_enabled = true;
	// Initialize debug prints as disabled by default
	level.map_trainer.timing_debug_enabled = false;
	// Initialize timing entries array
	level.map_trainer.timing_entry_count = 0;
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		level.map_trainer.timing_entries[i].active = false;
		level.map_trainer.timing_entries[i].pickup_time = 0_ms;
		level.map_trainer.timing_entries[i].position = {};
		level.map_trainer.timing_entries[i].respawn_time = 20_sec;
		level.map_trainer.timing_entries[i].grace_period_end = 0_ms;
		level.map_trainer.timing_entries[i].item_name = nullptr;
		level.map_trainer.timing_entries[i].item_classname = nullptr;
	}
}

// Forward declaration for MapTrainer_FriendlyNameFromPickup
static void MapTrainer_FriendlyNameFromPickup(const char* pickup_name, char* out, size_t out_size);

void MapTrainer_BuildItemList(const char *mapname)
{
	// Clean up previous data
	if (level.map_trainer.items)
	{
		delete[] level.map_trainer.items;
		level.map_trainer.items = nullptr;
	}

	std::vector<map_trainer_item_t> temp_items;

	// Enumerate all in-game entities (skip world and clients)
	for (uint32_t i = 1; i < globals.num_edicts; i++)
	{
		edict_t *ent = &g_edicts[i];
		if (!ent->inuse || !ent->classname || !ent->item)
			continue;

		// Skip entities with invalid classnames
		if (Q_strcasecmp(ent->classname, "noclass") == 0 || Q_strcasecmp(ent->classname, "freed") == 0)
			continue;

		// Only include actual items (entities with an item field)
		// Only include items that are in the enabled categories
		if (!MapTrainer_IsItemCategoryEnabled(ent->classname))
			continue;

		// Get friendly name from the actual item definition that the game uses
		char friendly_name[64];
		if (ent->item && ent->item->pickup_name)
			MapTrainer_FriendlyNameFromPickup(ent->item->pickup_name, friendly_name, sizeof(friendly_name));
		else
			Q_strlcpy(friendly_name, ent->classname, sizeof(friendly_name));

		map_trainer_item_t item;
		Q_strlcpy(item.friendly_name, friendly_name, sizeof(item.friendly_name));
		Q_strlcpy(item.class_name, ent->classname, sizeof(item.class_name));
		item.position = ent->s.origin;

		temp_items.push_back(item);
	}

	level.map_trainer.item_count = static_cast<int32_t>(temp_items.size());
	if (level.map_trainer.item_count > 0)
	{
		level.map_trainer.items = new map_trainer_item_t[level.map_trainer.item_count];
		for (size_t i = 0; i < temp_items.size(); i++)
		{
			level.map_trainer.items[i] = temp_items[i];
		}
		level.map_trainer.initialized = true;
		MapTrainer_BuildUniqueItemsList();
	}
	else
	{
		level.map_trainer.initialized = false;
	}
}

bool MapTrainer_IsCombinableHealthPack(const char *class_name)
{
	// These health packs can be combined when the option is enabled
	// Mega health is NOT included in this list
	return (Q_strcasecmp(class_name, "item_health_small") == 0 ||
			Q_strcasecmp(class_name, "item_health") == 0 ||
			Q_strcasecmp(class_name, "item_health_large") == 0);
}

const char* MapTrainer_GetNormalizedClassName(const char *class_name)
{
	// If combine health packs is enabled, normalize health pack class names (combine them)
	if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(class_name))
	{
		return "item_health_combined"; // Virtual class name for combined health packs
	}
	return class_name;
}

const char* MapTrainer_GetDisplayFriendlyName(const char *class_name, const char *original_friendly_name)
{
	// If combine health packs is enabled, use generic name for combinable health packs
	if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(class_name))
	{
		return "Health Pack"; // Generic display name for combined health packs
	}
	return original_friendly_name;
}

void MapTrainer_BuildUniqueItemsList()
{
	if (!level.map_trainer.initialized || level.map_trainer.item_count == 0)
		return;
	
	// Allocate memory for unique items (worst case: all items are unique)
	level.map_trainer.unique_items = (map_trainer_unique_item_t*)gi.TagMalloc(
		sizeof(map_trainer_unique_item_t) * level.map_trainer.item_count, TAG_LEVEL);
	level.map_trainer.unique_item_count = 0;
	
	// Process each item to build unique list
	for (size_t i = 0; i < static_cast<size_t>(level.map_trainer.item_count); i++)
	{
		map_trainer_item_t *current_item = &level.map_trainer.items[i];
		bool found_existing = false;
		
		// Get normalized class name (handles health pack combining)
		const char *normalized_class_name = MapTrainer_GetNormalizedClassName(current_item->class_name);
		
		// Check if this normalized class_name already exists in unique list
		for (size_t j = 0; j < static_cast<size_t>(level.map_trainer.unique_item_count); j++)
		{
			if (Q_strcasecmp(level.map_trainer.unique_items[j].class_name, normalized_class_name) == 0)
			{
				// Found existing unique item, add this instance to it
				map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[j];
				
				// Reallocate indices array to fit one more
				int32_t *new_indices = (int32_t*)gi.TagMalloc(
					sizeof(int32_t) * (unique_item->instance_count + 1), TAG_LEVEL);
				
				// Copy existing indices
				if (unique_item->item_indices)
				{
					memcpy(new_indices, unique_item->item_indices, sizeof(int32_t) * unique_item->instance_count);
					gi.TagFree(unique_item->item_indices);
				}
				
				// Add new index
				new_indices[unique_item->instance_count] = static_cast<int32_t>(i);
				unique_item->item_indices = new_indices;
				unique_item->instance_count++;
				
				found_existing = true;
				break;
			}
		}
		
		if (!found_existing)
		{
			// Create new unique item entry
			map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[level.map_trainer.unique_item_count];
			
			strcpy(unique_item->class_name, normalized_class_name);
			// For combined health packs, use a generic friendly name
			if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(current_item->class_name))
			{
				strcpy(unique_item->friendly_name, "health pack");
			}
			else
			{
				strcpy(unique_item->friendly_name, current_item->friendly_name);
			}
			
			// Allocate indices array with first instance
			unique_item->item_indices = (int32_t*)gi.TagMalloc(sizeof(int32_t), TAG_LEVEL);
			unique_item->item_indices[0] = static_cast<int32_t>(i);
			unique_item->instance_count = 1;
			
			level.map_trainer.unique_item_count++;
		}
	}
	
	// Debug output removed to reduce verbosity
}

// Determine what category an item belongs to based on its class name
bool MapTrainer_IsItemCategoryEnabled(const char *class_name)
{
	// Weapons
	if (strstr(class_name, "weapon_") == class_name ||
		Q_strcasecmp(class_name, "item_quad") == 0) // Quad damage is a weapon powerup but treat as weapon
	{
		return level.map_trainer.weapons_enabled;
	}
	
	// Ammo
	if (strstr(class_name, "ammo_") == class_name)
	{
		return level.map_trainer.ammo_enabled;
	}
	
	// Health items (including combined health pack virtual class)
	if (strstr(class_name, "item_health") == class_name ||
		Q_strcasecmp(class_name, "item_health_small") == 0 ||
		Q_strcasecmp(class_name, "item_health_large") == 0 ||
		Q_strcasecmp(class_name, "item_health_mega") == 0 ||
		Q_strcasecmp(class_name, "item_health_combined") == 0)
	{
		return level.map_trainer.health_enabled;
	}
	
	// Armor items
	if (strstr(class_name, "item_armor") == class_name ||
		Q_strcasecmp(class_name, "item_jacket_armor") == 0 ||
		Q_strcasecmp(class_name, "item_combat_armor") == 0 ||
		Q_strcasecmp(class_name, "item_body_armor") == 0 ||
		Q_strcasecmp(class_name, "item_power_screen") == 0 ||
		Q_strcasecmp(class_name, "item_power_shield") == 0)
	{
		return level.map_trainer.armor_enabled;
	}
	
	// Powerups (everything else)
	if (strstr(class_name, "item_") == class_name)
	{
		return level.map_trainer.powerups_enabled;
	}
	
	// Default to enabled for unknown items
	return true;
}

// Check if an item entity is available (not respawning)
bool MapTrainer_IsItemAvailable(const char *class_name, const vec3_t &position)
{
	// Find all entities with matching classname and check if any are available
	edict_t *ent = nullptr;
	while ((ent = G_FindByString<&edict_t::classname>(ent, class_name)) != nullptr)
	{
		vec3_t diff = ent->s.origin - position;
		float distance = diff.length();
		
		// Check if this entity is at the expected position (within reasonable tolerance)
		if (distance < 128.0f) // Allow some tolerance for position matching
		{
			// Check if this item is available (not respawning)
			if (!(ent->svflags & SVF_RESPAWNING) && ent->solid != SOLID_NOT)
			{
				return true; // Found an available instance
			}
		}
	}
	
	return false; // No available instances found
}

void MapTrainer_PickNewTarget()
{
	if (!level.map_trainer.initialized || level.map_trainer.unique_item_count == 0)
		return;
	
	// Get the normalized class name of the previous target to avoid picking the same type
	const char *previous_class_name = nullptr;
	if (level.map_trainer.previous_target_index >= 0)
	{
		previous_class_name = MapTrainer_GetNormalizedClassName(level.map_trainer.items[level.map_trainer.previous_target_index].class_name);
	}
	
	// Build a list of available unique item types
	std::vector<int32_t> available_unique_types;
	for (int32_t i = 0; i < level.map_trainer.unique_item_count; i++)
	{
		map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[i];
		
		// Skip if this is the same type as previous (if we have more than one type)
		if (level.map_trainer.unique_item_count > 1 && previous_class_name != nullptr &&
			Q_strcasecmp(unique_item->class_name, previous_class_name) == 0)
		{
			continue;
		}
		
		// Check if this item category is enabled
		if (!MapTrainer_IsItemCategoryEnabled(unique_item->class_name))
		{
			continue;
		}
		
		// Check if any instance of this item type is available
		bool has_available_instance = false;
		for (int32_t j = 0; j < unique_item->instance_count; j++)
		{
			int32_t item_index = unique_item->item_indices[j];
			map_trainer_item_t *item = &level.map_trainer.items[item_index];
			
			if (MapTrainer_IsItemAvailable(item->class_name, item->position))
			{
				has_available_instance = true;
				break;
			}
		}
		
		if (has_available_instance)
		{
			available_unique_types.push_back(i);
		}
	}
	
	// If no available types found, wait and try again later
	if (available_unique_types.empty())
	{
		return;
	}
	
	// Step 1: Pick a random available unique item type (equal weighting for all types)
	int32_t random_index = irandom(static_cast<int32_t>(available_unique_types.size()));
	int32_t unique_type_index = available_unique_types[random_index];
	map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[unique_type_index];
	
	// Step 2: Pick a random available instance of that item type
	std::vector<int32_t> available_instances;
	for (int32_t j = 0; j < unique_item->instance_count; j++)
	{
		int32_t item_index = unique_item->item_indices[j];
		map_trainer_item_t *item = &level.map_trainer.items[item_index];
		
		if (MapTrainer_IsItemAvailable(item->class_name, item->position))
		{
			available_instances.push_back(item_index);
		}
	}
	
	if (available_instances.empty())
	{
		return;
	}
	
	int32_t instance_random_index = irandom(static_cast<int32_t>(available_instances.size()));
	int32_t new_target = available_instances[instance_random_index];
	
	// Only update previous_target_index if we have a valid current target
	if (level.map_trainer.current_target_index >= 0)
		level.map_trainer.previous_target_index = level.map_trainer.current_target_index;
	
	level.map_trainer.current_target_index = new_target;
	
	map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
	
	// Always show "travel from X to Y" if we have a previous target (and training is enabled)
	if (level.map_trainer.previous_target_index >= 0 && level.map_trainer.training_enabled)
	{
		map_trainer_item_t *previous = &level.map_trainer.items[level.map_trainer.previous_target_index];
		
		// Get display-friendly names (handles health pack combining)
		const char *previous_display_name = MapTrainer_GetDisplayFriendlyName(previous->class_name, previous->friendly_name);
		const char *target_display_name = MapTrainer_GetDisplayFriendlyName(target->class_name, target->friendly_name);
		
		// Send message to all players
		for (uint32_t i = 0; i < game.maxclients; i++)
		{
			edict_t *player = &g_edicts[1 + i];
			if (!player->inuse || !player->client)
				continue;
			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Travel from {} to {}", previous_display_name, target_display_name).data());
		}
	}
	else
	{
		// This shouldn't happen after first pickup
	}
}

bool MapTrainer_IsTargetItem(edict_t *ent)
{
	if (!level.map_trainer.initialized || level.map_trainer.current_target_index < 0)
		return false;
		
	if (!ent->item || !ent->item->classname)
		return false;
		
	map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
	
	// Get normalized class names for comparison (handles health pack combining)
	const char *ent_normalized = MapTrainer_GetNormalizedClassName(ent->item->classname);
	const char *target_normalized = MapTrainer_GetNormalizedClassName(target->class_name);
	
	// Check if normalized class names match
	if (Q_strcasecmp(ent_normalized, target_normalized) == 0)
	{
		return true; // Accept any matching class name (including combined health packs)
	}
	
	return false;
}

void MapTrainer_OnItemPickup(edict_t *item_ent, edict_t *player)
{
	if (!level.map_trainer.initialized)
		return;
	
	const char *item_name = item_ent->item ? item_ent->item->classname : "unknown";
	
	// If this is the first pickup, any item becomes the source
	if (level.map_trainer.first_pickup)
	{
		// Find this item in our CSV data to get its friendly name and store as source
		int32_t source_index = -1;
		for (int32_t i = 0; i < level.map_trainer.item_count; i++)
		{
			if (Q_strcasecmp(level.map_trainer.items[i].class_name, item_name) == 0)
			{
				source_index = i;
				break;
			}
		}
		
		// Mark that we've had our first pickup
		level.map_trainer.first_pickup = false;
		
		// Set the source item BEFORE picking new target
		level.map_trainer.previous_target_index = source_index;
		
		// Pick new target
		MapTrainer_PickNewTarget();
	}
	else if (MapTrainer_IsTargetItem(item_ent))
	{
		// Target item picked up!
		// The current target becomes the new source
		level.map_trainer.previous_target_index = level.map_trainer.current_target_index;
		
		// Pick new target
		MapTrainer_PickNewTarget();
	}
}

void MapTrainer_ShowWelcomeMessage(edict_t *player)
{
	if (level.map_trainer.training_enabled && level.map_trainer.initialized)
		{
			gi.LocClient_Print(player, PRINT_CENTER, "CSV file loaded.\nPlease pick up an item to begin.");
		}
	else if (level.map_trainer.training_enabled && !level.map_trainer.initialized)
		{
		gi.LocClient_Print(player, PRINT_CENTER, "CSV file not found for this map.\nTraining mode disabled.");
		// Auto-disable training if CSV file wasn't found
		level.map_trainer.training_enabled = false;
	}
	else
	{
		gi.LocClient_Print(player, PRINT_CENTER, "Welcome to the Q2RE Map Trainer.\nPress Tab to open the training menu.");
	}
}

// ==================== MAP TRAINER MENU SYSTEM ====================

void MapTrainer_MenuClose(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
}

void MapTrainer_RestartPathTraining()
{
	if (level.map_trainer.training_enabled)
	{
		// Rebuild the item list with new category settings
		MapTrainer_BuildItemList(level.mapname);
		
		// Reset the training state so player can pick up any item to begin
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
	}
}

void MapTrainer_ToggleWeapons(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.weapons_enabled = !level.map_trainer.weapons_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleAmmo(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.ammo_enabled = !level.map_trainer.ammo_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleHealth(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.health_enabled = !level.map_trainer.health_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleArmor(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.armor_enabled = !level.map_trainer.armor_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_TogglePowerups(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.powerups_enabled = !level.map_trainer.powerups_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleSpeedometer(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.speedometer_enabled = !level.map_trainer.speedometer_enabled;
	PMenu_Update(ent);
}

void MapTrainer_ToggleTraining(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.training_enabled = !level.map_trainer.training_enabled;
	
	// If training mode was just turned ON, load CSV and reset the training state
	if (level.map_trainer.training_enabled)
	{
		// Mutual exclusion: disable timing trainer if it's enabled
		if (level.map_trainer.timing_enabled)
		{
			level.map_trainer.timing_enabled = false;
			// Reset all active timings when disabled
			for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
			{
				level.map_trainer.timing_entries[i].active = false;
			}
			level.map_trainer.timing_entry_count = 0;
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer automatically disabled.");
		}
		
		MapTrainer_BuildItemList(level.mapname);
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
		
		// Give immediate feedback about item loading
		if (level.map_trainer.initialized)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer enabled. Items loaded from map.");
		}
		else
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "No items found in map. Item Path Trainer disabled.");
			level.map_trainer.training_enabled = false; // Auto-disable if no items
		}
	}
	else
	{
		// Training mode turned OFF - clean up CSV data
		if (level.map_trainer.items)
		{
			delete[] level.map_trainer.items;
			level.map_trainer.items = nullptr;
		}
		level.map_trainer.item_count = 0;
		level.map_trainer.initialized = false;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
		
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleCombineHealthPacks(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.combine_health_packs = !level.map_trainer.combine_health_packs;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleTiming(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.timing_enabled = !level.map_trainer.timing_enabled;
	
	if (level.map_trainer.timing_enabled)
	{
		// Mutual exclusion: disable path trainer if it's enabled
		if (level.map_trainer.training_enabled)
		{
			level.map_trainer.training_enabled = false;
			// Clean up CSV data
			if (level.map_trainer.items)
			{
				delete[] level.map_trainer.items;
				level.map_trainer.items = nullptr;
			}
			level.map_trainer.item_count = 0;
			level.map_trainer.initialized = false;
			level.map_trainer.current_target_index = -1;
			level.map_trainer.previous_target_index = -1;
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer automatically disabled.");
		}
		
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer disabled.");
		// Reset all active timings when disabled
		for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
		{
			level.map_trainer.timing_entries[i].active = false;
		}
		level.map_trainer.timing_entry_count = 0;
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleFreeCollect(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.free_collect_enabled = !level.map_trainer.free_collect_enabled;
	
	if (level.map_trainer.free_collect_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Free Collect enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Free Collect disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleTimingDebug(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.timing_debug_enabled = !level.map_trainer.timing_debug_enabled;
	
	if (level.map_trainer.timing_debug_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Timing Debug enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Timing Debug disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_SavePosition(edict_t *ent, pmenuhnd_t *p)
{
	// Execute savepos command and close menu
	gi.AddCommandString("savepos\n");
	PMenu_Close(ent);
}

void MapTrainer_LoadPosition(edict_t *ent, pmenuhnd_t *p)
{
	// Execute loadpos command and close menu
	gi.AddCommandString("loadpos\n");
	PMenu_Close(ent);
}

// Forward declaration
void MapTrainer_OpenMenu(edict_t *ent);

void MapTrainer_BackToMainMenu(edict_t *ent, pmenuhnd_t *p)
{
	MapTrainer_OpenMenu(ent);
}

void MapTrainer_UpdateItemPathingSubmenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update toggle display text
	Q_strlcpy(entries[2].text, G_Fmt("Path Trainer: {}", level.map_trainer.training_enabled ? "Enabled" : "Disabled").data(), sizeof(entries[2].text));
	
	if (level.map_trainer.training_enabled)
	{
		// Show item category options when training is enabled
		Q_strlcpy(entries[3].text, G_Fmt("Weapons: {}", level.map_trainer.weapons_enabled ? "ON" : "OFF").data(), sizeof(entries[3].text));
		Q_strlcpy(entries[4].text, G_Fmt("Ammo: {}", level.map_trainer.ammo_enabled ? "ON" : "OFF").data(), sizeof(entries[4].text));
		Q_strlcpy(entries[5].text, G_Fmt("Health: {}", level.map_trainer.health_enabled ? "ON" : "OFF").data(), sizeof(entries[5].text));
		Q_strlcpy(entries[6].text, G_Fmt("Armor: {}", level.map_trainer.armor_enabled ? "ON" : "OFF").data(), sizeof(entries[6].text));
		Q_strlcpy(entries[7].text, G_Fmt("Powerups: {}", level.map_trainer.powerups_enabled ? "ON" : "OFF").data(), sizeof(entries[7].text));
		Q_strlcpy(entries[8].text, G_Fmt("Combine Health Packs: {}", level.map_trainer.combine_health_packs ? "ON" : "OFF").data(), sizeof(entries[8].text));
		
		// Re-enable the function pointers
		entries[3].SelectFunc = MapTrainer_ToggleWeapons;
		entries[4].SelectFunc = MapTrainer_ToggleAmmo;
		entries[5].SelectFunc = MapTrainer_ToggleHealth;
		entries[6].SelectFunc = MapTrainer_ToggleArmor;
		entries[7].SelectFunc = MapTrainer_TogglePowerups;
		entries[8].SelectFunc = MapTrainer_ToggleCombineHealthPacks;
	}
	else
	{
		// Hide item category options when training is disabled
		Q_strlcpy(entries[3].text, "", sizeof(entries[3].text));
		Q_strlcpy(entries[4].text, "", sizeof(entries[4].text));
		Q_strlcpy(entries[5].text, "", sizeof(entries[5].text));
		Q_strlcpy(entries[6].text, "", sizeof(entries[6].text));
		Q_strlcpy(entries[7].text, "", sizeof(entries[7].text));
		Q_strlcpy(entries[8].text, "", sizeof(entries[8].text));
		Q_strlcpy(entries[9].text, "", sizeof(entries[9].text)); // Hide the blank line too
		
		// Disable the function pointers
		entries[3].SelectFunc = nullptr;
		entries[4].SelectFunc = nullptr;
		entries[5].SelectFunc = nullptr;
		entries[6].SelectFunc = nullptr;
		entries[7].SelectFunc = nullptr;
		entries[8].SelectFunc = nullptr;
	}
}

pmenu_t maptrainer_itempathing_submenu[] = {
	{ "Item Pathing Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Training Mode: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleTraining },
	{ "Weapons: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleWeapons },
	{ "Ammo: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleAmmo },
	{ "Health: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleHealth },
	{ "Armor: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleArmor },
	{ "Powerups: ON", PMENU_ALIGN_LEFT, MapTrainer_TogglePowerups },
			{ "Combine Health Packs: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleCombineHealthPacks },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "v0.87 beta by ozy", PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenItemPathingSubmenu(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Open(ent, maptrainer_itempathing_submenu, -1, sizeof(maptrainer_itempathing_submenu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateItemPathingSubmenu);
}

void MapTrainer_UpdateItemTimingSubmenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update toggle display text
	Q_strlcpy(entries[2].text, G_Fmt("Timing Trainer: {}", level.map_trainer.timing_enabled ? "Enabled" : "Disabled").data(), sizeof(entries[2].text));
	Q_strlcpy(entries[3].text, G_Fmt("Free Collect: {}", level.map_trainer.free_collect_enabled ? "ON" : "OFF").data(), sizeof(entries[3].text));
	Q_strlcpy(entries[4].text, G_Fmt("Debug Prints: {}", level.map_trainer.timing_debug_enabled ? "ON" : "OFF").data(), sizeof(entries[4].text));
}

pmenu_t maptrainer_jumptrainer_submenu[] = {
	{ "Jump Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Save Position (savepos)", PMENU_ALIGN_LEFT, MapTrainer_SavePosition },
	{ "Load Position (loadpos)", PMENU_ALIGN_LEFT, MapTrainer_LoadPosition },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "v0.91 beta by ozy", PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenJumpTrainerSubmenu(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Open(ent, maptrainer_jumptrainer_submenu, -1, sizeof(maptrainer_jumptrainer_submenu) / sizeof(pmenu_t), nullptr, nullptr);
}

pmenu_t maptrainer_itemtiming_submenu[] = {
	{ "Item Timing Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Timing Trainer: Disabled", PMENU_ALIGN_LEFT, MapTrainer_ToggleTiming },
	{ "Free Collect: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleFreeCollect },
	{ "Debug Prints: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleTimingDebug },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "v0.87 beta by ozy", PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenItemTimingSubmenu(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Open(ent, maptrainer_itemtiming_submenu, -1, sizeof(maptrainer_itemtiming_submenu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateItemTimingSubmenu);
}

void MapTrainer_UpdateMenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update speedometer display text (index 6 in the main menu - after adding Item Jump Trainer)
	Q_strlcpy(entries[6].text, G_Fmt("Speedometer: {}", level.map_trainer.speedometer_enabled ? "ON" : "OFF").data(), sizeof(entries[6].text));
}

pmenu_t maptrainer_menu[] = {
	{ "Settings", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Item Path Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenItemPathingSubmenu },
	{ "Item Timing Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenItemTimingSubmenu },
	{ "Item Jump Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenJumpTrainerSubmenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Speedometer: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleSpeedometer },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Close Menu", PMENU_ALIGN_LEFT, MapTrainer_MenuClose },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "v0.91 beta by ozy", PMENU_ALIGN_CENTER, nullptr }
	
};

void MapTrainer_OpenMenu(edict_t *ent)
{
	PMenu_Open(ent, maptrainer_menu, -1, sizeof(maptrainer_menu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateMenu);
}

void Cmd_MapTrainerMenu_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	MapTrainer_OpenMenu(ent);
}

// ==================== SPEEDOMETER SYSTEM ====================

// Helper function to find existing timing entry for an item
map_trainer_t::timing_entry_t* MapTrainer_FindTimingEntry(const char *classname)
{
	if (!classname)
		return nullptr;
		
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		if (level.map_trainer.timing_entries[i].active && 
			level.map_trainer.timing_entries[i].item_classname &&
			Q_strcasecmp(level.map_trainer.timing_entries[i].item_classname, classname) == 0)
		{
			return &level.map_trainer.timing_entries[i];
		}
	}
	return nullptr;
}

// Helper function to create or update timing entry for an item
map_trainer_t::timing_entry_t* MapTrainer_CreateOrUpdateTimingEntry(const char *classname, const char *item_name, 
	const vec3_t &position, gtime_t pickup_time, gtime_t respawn_time)
{
	if (!classname || !item_name)
		return nullptr;
		
	// First try to find existing entry
	map_trainer_t::timing_entry_t* entry = MapTrainer_FindTimingEntry(classname);
	
	// If not found, create new entry
	if (!entry)
	{
		// Find first inactive slot
		for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
		{
			if (!level.map_trainer.timing_entries[i].active)
			{
				entry = &level.map_trainer.timing_entries[i];
				level.map_trainer.timing_entry_count++;
				break;
			}
		}
		
		// If no free slots, return nullptr (could implement LRU replacement later)
		if (!entry)
			return nullptr;
	}
	
	// Update entry data
	entry->active = true;
	entry->pickup_time = pickup_time;
	entry->position = position;
	entry->respawn_time = respawn_time;
	entry->grace_period_end = pickup_time + 5_sec;
	entry->item_name = item_name;
	entry->item_classname = classname;
	
	// Initialize megahealth fields to default values
	entry->is_megahealth = false;
	entry->megahealth_player = nullptr;
	entry->megahealth_decay_finished = false;
	entry->megahealth_respawn_start = 0_ms;
	
	return entry;
}

void MapTrainer_CheckArmorTiming(edict_t *player)
{
	if (!level.map_trainer.timing_enabled || !player->client)
		return;

	// Check all active timing entries
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		map_trainer_t::timing_entry_t* entry = &level.map_trainer.timing_entries[i];
		
		if (!entry->active)
			continue;
			
		// Skip megahealth entries (handled by separate function)
		if (entry->is_megahealth)
			continue;

		// Check if we're still in the grace period (5 seconds after pickup)
		if (level.time < entry->grace_period_end)
			continue;

		vec3_t diff = player->s.origin - entry->position;
		float distance = diff.length();

		if (distance <= 64.0f) // Within pickup radius
		{
			gtime_t current_time = level.time;
			gtime_t expected_respawn_time = entry->pickup_time + entry->respawn_time;
			float time_diff = (current_time - expected_respawn_time).seconds();

			if (level.map_trainer.timing_debug_enabled)
			{
				// Print debug info every time player is in pickup range
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] In range of {}: player({:.1f},{:.1f},{:.1f}) item({:.1f},{:.1f},{:.1f}) dist {:.1f}",
					entry->item_name ? entry->item_name : "?",
					player->s.origin[0], player->s.origin[1], player->s.origin[2],
					entry->position[0], entry->position[1], entry->position[2],
					distance
				).data());

				// Print concise debug info only when in radius and timing check is triggered
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] {}: player({:.1f},{:.1f},{:.1f}) item({:.1f},{:.1f},{:.1f}) diff {:+.2f}",
					entry->item_name ? entry->item_name : "?",
					player->s.origin[0], player->s.origin[1], player->s.origin[2],
					entry->position[0], entry->position[1], entry->position[2],
					time_diff
				).data());
			}

			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("{}: {:+.2f}", entry->item_name ? entry->item_name : "?", time_diff).data());

			// Reset this timing entry after showing result
			entry->active = false;
			level.map_trainer.timing_entry_count--;
		}
	}
}

void MapTrainer_CheckMegahealthTiming(edict_t *player)
{
	if (!level.map_trainer.timing_enabled || !player->client)
		return;

	// Check all active megahealth timing entries
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		map_trainer_t::timing_entry_t* entry = &level.map_trainer.timing_entries[i];
		
		if (!entry->active || !entry->is_megahealth)
			continue;
			
		// Verify this entry belongs to this player
		if (entry->megahealth_player != player)
			continue;

		// Phase 1: Check if health decay has finished
		if (!entry->megahealth_decay_finished)
		{
			// Check if player's health has dropped to max_health or below
			if (player->health <= player->max_health)
			{
				// Health decay finished, start the 20-second respawn timer
				entry->megahealth_decay_finished = true;
				entry->megahealth_respawn_start = level.time;
				entry->grace_period_end = level.time; // End grace period now
				
				if (level.map_trainer.timing_debug_enabled)
				{
					gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] Megahealth decay finished at {:.2f}, starting 20s respawn timer",
						level.time.seconds()).data());
				}
			}
			// Still in decay phase, continue monitoring
			continue;
		}

		// Phase 2: Check if player is near the megahealth spawn point for timing results
		vec3_t diff = player->s.origin - entry->position;
		float distance = diff.length();

		if (distance <= 64.0f) // Within pickup radius
		{
			gtime_t current_time = level.time;
			gtime_t expected_respawn_time = entry->megahealth_respawn_start + 20_sec;
			float time_diff = (current_time - expected_respawn_time).seconds();

			if (level.map_trainer.timing_debug_enabled)
			{
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] Megahealth timing check: decay_start={:.2f} respawn_start={:.2f} expected={:.2f} now={:.2f} diff={:+.2f}",
					entry->pickup_time.seconds(),
					entry->megahealth_respawn_start.seconds(), 
					expected_respawn_time.seconds(),
					current_time.seconds(),
					time_diff
				).data());
			}

			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Megahealth: {:+.2f}", time_diff).data());

			// Reset this timing entry after showing result
			entry->active = false;
			level.map_trainer.timing_entry_count--;
		}
	}
}

void MapTrainer_UpdateSpeedometer(edict_t *player)
{
	if (!player || !player->client) return;
	
	if (!level.map_trainer.speedometer_enabled) {
		// Clear speedometer when disabled
		player->client->ps.stats[STAT_SPEEDOMETER] = 0;
		return;
	}
	
	// Calculate horizontal speed (ignoring Z component for movement-focused display)
	gvec3_t horizontal_velocity = player->velocity;
	horizontal_velocity[2] = 0; // Remove vertical component
	
	float speed = horizontal_velocity.length();
	
	// Set the speed as a player stat (clamped to 16-bit signed integer range)
	player->client->ps.stats[STAT_SPEEDOMETER] = static_cast<int16_t>(std::min(speed, 32767.0f));
}

// ==================== PRACTICE SPAWN COMMANDS ====================

void Cmd_SetSpawn_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	// Save current position and angles
	level.map_trainer.practice_spawn_origin = ent->s.origin;
	level.map_trainer.practice_spawn_angles = ent->client->ps.viewangles;
	level.map_trainer.practice_spawn_set = true;
	
	gi.LocClient_Print(ent, PRINT_HIGH, "Position saved");
}

void Cmd_WarpSpawn_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	if (!level.map_trainer.practice_spawn_set)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No saved position. Use 'savepos' first.");
		return;
	}
	
	// Teleport player to saved position
	ent->s.origin = level.map_trainer.practice_spawn_origin;
	ent->client->ps.pmove.origin = level.map_trainer.practice_spawn_origin;
	
	// Set saved view angles - need to set multiple angle fields to ensure it takes effect
	ent->client->ps.viewangles = level.map_trainer.practice_spawn_angles;
	ent->client->v_angle = level.map_trainer.practice_spawn_angles;
	ent->s.angles = level.map_trainer.practice_spawn_angles;
	
	// Force the client to use these angles by setting the delta angles
	// This prevents the client from overriding with their current mouse position
	ent->client->ps.pmove.delta_angles = level.map_trainer.practice_spawn_angles - ent->client->resp.cmd_angles;
	
	// Clear velocity to prevent momentum from carrying over
	ent->velocity = vec3_origin;
	ent->client->ps.pmove.velocity = vec3_origin;
	
	// Link entity at new position
	gi.linkentity(ent);
	
	gi.LocClient_Print(ent, PRINT_HIGH, "Position loaded");
}

// ==================== END MAP TRAINER SYSTEM ====================

void InitItems()
{
	// validate item integrity
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		if (itemlist[i].id != i)
			gi.Com_ErrorFmt("Item {} has wrong enum ID {} (should be {})", itemlist[i].pickup_name, (int32_t) itemlist[i].id, (int32_t) i);

	// set up weapon chains
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!itemlist[i].chain)
			continue;

		gitem_t *item = &itemlist[i];

		// already initialized
		if (item->chain_next)
			continue;

		gitem_t *chain_item = &itemlist[item->chain];

		if (!chain_item)
			gi.Com_ErrorFmt("Invalid item chain {} for {}", (int32_t) item->chain, item->pickup_name);

		// set up initial chain
		if (!chain_item->chain_next)
			chain_item->chain_next = chain_item;

		// if we're not the first in chain, add us now
		if (chain_item != item)
		{
			gitem_t *c;

			// end of chain is one whose chain_next points to chain_item
			for (c = chain_item; c->chain_next != chain_item; c = c->chain_next)
				continue;

			// splice us in
			item->chain_next = chain_item;
			c->chain_next = item;
		}
	}

	// set up ammo
	for (auto &it : itemlist)
	{
		if ((it.flags & IF_AMMO) && it.tag >= AMMO_BULLETS && it.tag < AMMO_MAX)
			ammolist[it.tag] = &it;
		else if ((it.flags & IF_POWERUP_WHEEL) && !(it.flags & IF_WEAPON) && it.tag >= POWERUP_SCREEN && it.tag < POWERUP_MAX)
			poweruplist[it.tag] = &it;
	}

	// in coop or DM with Weapons' Stay, remove drop ptr
	for (auto &it : itemlist)
	{
		if (coop->integer)
		{
			if (!P_UseCoopInstancedItems() && (it.flags & IF_STAY_COOP))
				it.drop = nullptr;
		}
	}
}

// [Paril-KEX]
inline bool G_CanDropItem(const gitem_t &item)
{
	if (!item.drop)
		return false;
	else if ((item.flags & IF_WEAPON) && !(item.flags & IF_AMMO) && deathmatch->integer && g_dm_weapons_stay->integer)
		return false;

	return true;
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames()
{
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		gi.configstring(CS_ITEMS + i, itemlist[i].pickup_name);

	// [Paril-KEX] set ammo wheel indices first
	int32_t cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_AMMO))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_AMMO + cs_index, G_Fmt("{}|{}", (int32_t) i, gi.imageindex(itemlist[i].icon)).data());
		itemlist[i].ammo_wheel_index = cs_index;
		cs_index++;
	}

	// set weapon wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		int32_t min_ammo = (itemlist[i].flags & IF_AMMO) ? 1 : itemlist[i].quantity;

		gi.configstring(CS_WHEEL_WEAPONS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}|{}|{}",
			(int32_t) i,
			gi.imageindex(itemlist[i].icon),
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1,
			min_ammo,
			(itemlist[i].flags & IF_POWERUP_WHEEL) ? 1 : 0,
			itemlist[i].sort_id,
			itemlist[i].quantity_warn,
			G_CanDropItem(itemlist[i]) ? 1 : 0
		).data());
		itemlist[i].weapon_wheel_index = cs_index;
		cs_index++;
	}

	// set powerup wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_POWERUP_WHEEL) || (itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_POWERUPS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}",
			(int32_t) i,
			gi.imageindex(itemlist[i].icon),
			(itemlist[i].flags & IF_POWERUP_ONOFF) ? 1 : 0,
			itemlist[i].sort_id,
			G_CanDropItem(itemlist[i]) ? 1 : 0,
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1
		).data());
		itemlist[i].powerup_wheel_index = cs_index;
		cs_index++;
	}
}

// Helper to convert $item_hyperblaster to Hyperblaster
static void MapTrainer_FriendlyNameFromPickup(const char* pickup_name, char* out, size_t out_size)
{
	if (!pickup_name || !*pickup_name)
	{
		Q_strlcpy(out, "Item", out_size);
		return;
	}
	if (pickup_name[0] == '$')
	{
		// Skip the '$' and 'item_' prefix if present
		const char* name = pickup_name + 1;
		if (strncmp(name, "item_", 5) == 0)
			name += 5;
		// Capitalize first letter, lowercase the rest, and replace underscores with spaces
		if (*name)
		{
			char buf[64];
			Q_strlcpy(buf, name, sizeof(buf));
			buf[0] = toupper(buf[0]);
			for (size_t i = 1; buf[i]; ++i)
			{
				if (buf[i] == '_')
					buf[i] = ' ';
				else
					buf[i] = tolower(buf[i]);
			}
			Q_strlcpy(out, buf, out_size);
			return;
		}
	}
	// Fallback: just copy as is
	Q_strlcpy(out, pickup_name, out_size);
}
