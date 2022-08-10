from common.base_prod import TECH_COST_MULTIPLIER
from techs.ship_weapons.ship_weapons import WEAPON_BASE_EFFECTS

Tech(
    name="SHP_WEAPON_4_1",
    description="SHP_WEAPON_4_1_DESC",
    short_description="SHIP_WEAPON_UNLOCK_SHORT_DESC",
    category="SHIP_WEAPONS_CATEGORY",
    researchcost=750 * TECH_COST_MULTIPLIER,
    researchturns=10,
    tags=["PEDIA_SR_WEAPON_TECHS"],
    prerequisites="SHP_WEAPON_3_1",
    unlock=Item(type=UnlockShipPart, name="SR_WEAPON_4_1"),
    effectsgroups=WEAPON_BASE_EFFECTS("SR_WEAPON_4_1"),
    graphic="icons/ship_parts/death-ray-1.png",
)