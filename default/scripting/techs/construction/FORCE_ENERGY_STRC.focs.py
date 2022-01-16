from common.base_prod import TECH_COST_MULTIPLIER
from common.priorities import AFTER_ALL_TARGET_MAX_METERS_PRIORITY

Tech(
    name="CON_FRC_ENRG_STRC",
    description="CON_FRC_ENRG_STRC_DESC",
    short_description="METER_GROWTH_SHORT_DESC",
    category="CONSTRUCTION_CATEGORY",
    researchcost=200 * TECH_COST_MULTIPLIER,
    researchturns=5,
    tags=["PEDIA_CONSTRUCTION_CATEGORY"],
    prerequisites=["LRN_FORCE_FIELD", "CON_ARCH_PSYCH"],
    effectsgroups=[
        EffectsGroup(
            scope=ProductionCenter & OwnedBy(empire=Source.Owner),
            accountinglabel="CON_TECH_ACCOUNTING_LABEL",
            priority=AFTER_ALL_TARGET_MAX_METERS_PRIORITY,
            effects=[
                If(
                    condition=(Value(LocalCandidate.Industry) <= Value(LocalCandidate.TargetIndustry)),
                    effects=SetIndustry(
                        value=Min(
                            float,
                            Value + NamedReal(name="FORCE_ENERGY_RATE_INCREASE", value=3.0),
                            Value(Target.TargetIndustry),
                        )
                    ),
                    else_=SetIndustry(
                        value=Max(
                            float,
                            Value - NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.TargetIndustry),
                        )
                    ),
                ),
                If(
                    condition=(Value(LocalCandidate.Research) <= Value(LocalCandidate.TargetResearch)),
                    effects=SetResearch(
                        value=Min(
                            float,
                            Value + NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.TargetResearch),
                        )
                    ),
                    else_=SetResearch(
                        value=Max(
                            float,
                            Value - NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.TargetResearch),
                        )
                    ),
                ),
                If(
                    condition=(Value(LocalCandidate.Construction) <= Value(LocalCandidate.TargetConstruction)),
                    effects=SetConstruction(
                        value=Min(
                            float,
                            Value + NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.TargetConstruction),
                        )
                    ),
                    else_=SetConstruction(
                        value=Max(
                            float,
                            Value - NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.TargetConstruction),
                        )
                    ),
                ),
                If(
                    condition=(Value(LocalCandidate.Stockpile) <= Value(LocalCandidate.MaxStockpile)),
                    effects=SetStockpile(
                        value=Min(
                            float,
                            Value + NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.MaxStockpile),
                        )
                    ),
                    else_=SetStockpile(
                        value=Max(
                            float,
                            Value - NamedRealLookup(name="FORCE_ENERGY_RATE_INCREASE"),
                            Value(Target.MaxStockpile),
                        )
                    ),
                ),
            ],
        )
    ],
    graphic="icons/tech/force_energy_structures.png",
)