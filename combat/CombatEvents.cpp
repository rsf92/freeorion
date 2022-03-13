#include "CombatEvents.h"

#include "../universe/Universe.h"

#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/AppInterface.h"
#include "../util/VarText.h"
#include "../util/GameRules.h"

#include "../Empire/Empire.h"

#if __has_include(<charconv>)
  #include <charconv>
#endif
#include <sstream>

namespace {
    // would be better in CombatSystem, but that is server-only, and rules need
    // to exist on client and server.
    void AddRules(GameRules& rules) {
        rules.Add<int>(UserStringNop("RULE_NUM_COMBAT_ROUNDS"),
                       UserStringNop("RULE_NUM_COMBAT_ROUNDS_DESC"),
                       "", 4, true, RangedValidator<int>(2, 20));
        rules.Add<bool>(UserStringNop("RULE_AGGRESSIVE_SHIPS_COMBAT_VISIBLE"),
                        UserStringNop("RULE_AGGRESSIVE_SHIPS_COMBAT_VISIBLE_DESC"),
                        "", false, true);

    }
    bool temp_bool = RegisterGameRules(&AddRules);


    // TODO: Move this code into a common non UI linked location, so that
    // there is no duplicated code between server and clientUI.
    std::string_view LinkTag(UniverseObjectType obj_type) {
        switch (obj_type) {
        case UniverseObjectType::OBJ_SHIP:
            return VarText::SHIP_ID_TAG;
        case UniverseObjectType::OBJ_FLEET:
            return VarText::FLEET_ID_TAG;
        case UniverseObjectType::OBJ_PLANET:
            return VarText::PLANET_ID_TAG;
        case UniverseObjectType::OBJ_BUILDING:
            return VarText::BUILDING_ID_TAG;
        case UniverseObjectType::OBJ_SYSTEM:
            return VarText::SYSTEM_ID_TAG;
        case UniverseObjectType::OBJ_FIELD:
        case UniverseObjectType::OBJ_FIGHTER:
            [[fallthrough]];
        default:
            return "";
        }
    }

    std::string WrapWithTagAndId(std::string_view meat, std::string_view tag, int id) {
        std::string retval;
        retval.reserve(1  + 2* tag.size() + 1 + 10 + 1 + meat.size() + 2 + 1); // guesstimate
        retval.append("<").append(tag).append(" ").append(std::to_string(id)).append(">")
              .append(meat).append("</").append(tag).append(">");
        return retval;
    }

    std::string WrapUserStringWithTag(std::string_view table_id, std::string_view tag) {
        std::string retval;
        const auto& us{UserString(table_id)};
        retval.reserve(1 + 2*tag.size() + 1 + table_id.size() + 1 + us.size() + 2 + 1 + 5);
        retval.append("<").append(tag).append(" ").append(table_id).append(">")
              .append(us).append("</").append(tag).append(">");
        return retval;
    }

    //Copied pasted from Font.cpp due to Font not being linked into AI and server code
    std::string WrapColorTag(std::string_view text, EmpireColor c) {
        static constexpr auto lim = std::numeric_limits<EmpireColor::value_type>::max();
        static_assert(lim < 1000); // ensure no more than 3 characters will be consumed per number
        assert(c.size() >= 4);

        std::string retval;

#if defined(__cpp_lib_to_chars)
        std::array<std::string::value_type, 6 + 4*4 + 1> buffer{"<rgba "}; // rest should be nulls
        auto result = std::to_chars(buffer.data() + 6, buffer.data() + 9, static_cast<int>(c[0]));
        *result.ptr = ' ';
        result = std::to_chars(result.ptr + 1, result.ptr + 4, static_cast<int>(c[1]));
        *result.ptr = ' ';
        result = std::to_chars(result.ptr + 1, result.ptr + 4, static_cast<int>(c[2]));
        *result.ptr = ' ';
        result = std::to_chars(result.ptr + 1, result.ptr + 4, static_cast<int>(c[3]));
        *result.ptr = '>';
        retval.reserve(buffer.size() + text.size() + 7 + 1);
        retval.append(buffer.data()).append(text).append("</rgba>");
#else
        retval.reserve(6 + 4*4 + text.size() + 7 + 4);
        retval.append("<rgba ")
              .append(std::to_string(static_cast<int>(std::get<0>(c)))).append(" ")
              .append(std::to_string(static_cast<int>(std::get<1>(c)))).append(" ")
              .append(std::to_string(static_cast<int>(std::get<2>(c)))).append(" ")
              .append(std::to_string(static_cast<int>(std::get<3>(c)))).append(">")
              .append(text).append("</rgba>");
#endif
        return retval;
    }

    std::string EmpireColorWrappedText(int empire_id, std::string_view text,
                                       const ScriptingContext& context)
    {
        // TODO: refactor this to somewhere that links with the UI code.
        // Hardcoded default color becauses not linked with UI code.
        auto empire = context.GetEmpire(empire_id);
        return WrapColorTag(text, empire ? empire->Color() : EmpireColor{{80, 255, 128, 255}});
    }

    /// Creates a link tag of the appropriate type for object_id,
    /// with the content being the public name from the point of view of empire_id.
    /// Returns UserString("ENC_COMBAT_UNKNOWN_OBJECT") if object_id is not found.
    std::string PublicNameLink(int empire_id, int object_id, const Universe& u) {
        if (auto object = u.Objects().get(object_id)) {
            const auto& name = object->PublicName(empire_id, u);
            const auto& tag = LinkTag(object->ObjectType());
            return WrapWithTagAndId(name, tag, object_id);
        } else {
            return UserString("ENC_COMBAT_UNKNOWN_OBJECT");
        }
    }

    /// Creates a link tag of the appropriate type for either a fighter or another object.
    std::string FighterOrPublicNameLink(int viewing_empire_id, int object_id,
                                        int object_empire_id, const ScriptingContext& context)
    {
        if (object_id >= 0)   // ship
            return PublicNameLink(viewing_empire_id, object_id, context.ContextUniverse());
        else                  // fighter
            return EmpireColorWrappedText(object_empire_id, UserString("OBJ_FIGHTER"), context);
    }

    std::string EmpireLink(int empire_id, const ScriptingContext& context) {
        if (empire_id == ALL_EMPIRES) {
            return UserString("NEUTRAL");
        } else if (auto empire = context.GetEmpire(empire_id)) {
            return EmpireColorWrappedText(
                empire_id, WrapWithTagAndId(empire->Name(), VarText::EMPIRE_ID_TAG, empire_id),
                context);
        } else {
            return UserString("ENC_COMBAT_UNKNOWN_OBJECT");
        }
    }

    std::string ShipPartLink(std::string_view part) {
        return part.empty() ? UserString("ENC_COMBAT_UNKNOWN_OBJECT")
            : WrapUserStringWithTag(part, VarText::SHIP_PART_TAG);
    }
}


//////////////////////////////////////////
///////// BoutBeginEvent//////////////////
//////////////////////////////////////////
BoutBeginEvent::BoutBeginEvent(int bout_) :
    bout(bout_)
{}

std::string BoutBeginEvent::DebugString(const ScriptingContext&) const
{ return std::string{"Bout "}.append(std::to_string(bout)).append(" begins."); }

std::string BoutBeginEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext&) const
{ return str(FlexibleFormat(UserString("ENC_ROUND_BEGIN")) % bout); }


//////////////////////////////////////////
///////// BoutEvent /////////////
//////////////////////////////////////////
BoutEvent::BoutEvent(int _bout):
    bout(_bout)
{}

void BoutEvent::AddEvent(CombatEventPtr event)
{ events.push_back(std::move(event)); }

std::string BoutEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "Bout " << bout << " has " << events.size() << " events";
    return ss.str();
}

std::string BoutEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext&) const
{ return str(FlexibleFormat(UserString("ENC_ROUND_BEGIN")) % bout); }

std::vector<ConstCombatEventPtr> BoutEvent::SubEvents(int viewing_empire_id) const
{ return std::vector<ConstCombatEventPtr>{events.begin(), events.end()}; }


//////////////////////////////////////////
///////// SimultaneousEvents ///////////////////
//////////////////////////////////////////
void SimultaneousEvents::AddEvent(CombatEventPtr event)
{ events.push_back(std::move(event)); }

// The following two function definitions were moved to the header because the linker refuses to find them otherwise...

//std::string SimultaneousEvents::DebugString(const ScriptingContext& context) const {
//    std::stringstream ss;
//    ss << "SimultaneousEvents has " << events.size() << " events";
//    return ss.str();
//}

//std::string SimultaneousEvents::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const
//{ return ""; }

std::vector<ConstCombatEventPtr> SimultaneousEvents::SubEvents(int viewing_empire_id) const {
    // Sort the events by viewing empire, then ALL_EMPIRES and then
    // other empires.
    std::multimap<int, ConstCombatEventPtr> empire_to_event;
    typedef std::multimap<int, ConstCombatEventPtr>::iterator iterator;
    typedef std::pair<iterator, iterator> range;

    for (CombatEventPtr event : events) {
        boost::optional<int> maybe_faction = event->PrincipalFaction(viewing_empire_id);
        int faction = maybe_faction.get_value_or(ALL_EMPIRES);
        empire_to_event.emplace(faction, event);
    }

    std::vector<ConstCombatEventPtr> ordered_events;
    ordered_events.reserve(empire_to_event.size());

    range viewing_empire_events = empire_to_event.equal_range(viewing_empire_id);
    range all_empire_events = empire_to_event.equal_range(ALL_EMPIRES);

    for (iterator it = viewing_empire_events.first; it != viewing_empire_events.second; ++it)
        ordered_events.push_back(it->second);

    for (iterator it = all_empire_events.first; it != all_empire_events.second; ++it)
        ordered_events.push_back(it->second);

    for (auto& entry : empire_to_event) {
        if (entry.first != viewing_empire_id && entry.first != ALL_EMPIRES)
            ordered_events.push_back(entry.second);
    }

    return ordered_events;
}


//////////////////////////////////////////
///////// InitialStealthEvent /////////////
//////////////////////////////////////////
InitialStealthEvent::InitialStealthEvent(const EmpireToObjectVisibilityMap& x) :
    empire_to_object_visibility(x)
{}

std::string InitialStealthEvent::DebugString(const ScriptingContext& context) const {
    std::stringstream ss;
    ss << "InitialStealthEvent: ";
    for (auto& [empire_id, empire_object_vis] : empire_to_object_visibility) {
        ss << " Viewing Empire: " << EmpireLink(empire_id, context) << "\n";

        for (auto& [object_id, object_vis] : empire_object_vis) {
            (void)object_vis; // quiet warning
            const auto obj = context.ContextObjects().get(object_id);
            int owner_id = obj ? obj->Owner() : ALL_EMPIRES;
            if (owner_id == ALL_EMPIRES)
                continue;
            ss << FighterOrPublicNameLink(ALL_EMPIRES, object_id, owner_id, context);
        }
        ss << "\n";
    }
    return ss.str();
}

std::string InitialStealthEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    DebugLogger() << "CombatLogDescription for InitialStealthEvent viewing empire empire: " << viewing_empire_id;

    std::string desc;

    for (auto& [detector_empire_id, visible_objects] : empire_to_object_visibility) {
        DebugLogger() << "CombatLogDescription for InitialStealthEvent for detector empire: " << detector_empire_id;

        if (visible_objects.empty()) {
            DebugLogger() << " ... no object info recorded for detector empire: " << detector_empire_id;
            continue;
        }

        // Check Visibility of objects, report those that are not visible.
        std::vector<std::string> cloaked_attackers;
        for (auto& [object_id, object_vis] : visible_objects) {
            const auto obj = context.ContextObjects().get(object_id);
            const auto& name = obj ? obj->Name() : UserString("UNKNOWN");
            DebugLogger() << " ... object: " << name << " (" << object_id << ") has vis: " << object_vis;
            if (object_vis > Visibility::VIS_NO_VISIBILITY)
                continue;

            // all empires specifies empire to use for link color if this is a fighter
            cloaked_attackers.push_back(FighterOrPublicNameLink(
                viewing_empire_id, object_id, ALL_EMPIRES, context));
        }

        if (!cloaked_attackers.empty()) {
            desc += "\n"; //< Add \n at start of the report and between each empire
            if (detector_empire_id != ALL_EMPIRES) {
                std::vector<std::string> detector_empire_link{EmpireLink(detector_empire_id, context)};
                desc += FlexibleFormatList(detector_empire_link, cloaked_attackers,
                                           UserString("ENC_COMBAT_INITIAL_STEALTH_LIST")).str();
            } else {
                desc += FlexibleFormatList(cloaked_attackers, UserString("ENC_COMBAT_NEUTRAL_INITIAL_STEALTH_LIST")).str();
            }
        }
    }

    return desc;
}


//////////////////////////////////////////
///////// StealthChangeEvent /////////////
//////////////////////////////////////////
StealthChangeEvent::StealthChangeEvent(int bout_) :
    bout(bout_)
{}

StealthChangeEvent::StealthChangeEventDetail::StealthChangeEventDetail() :
    StealthChangeEventDetail(INVALID_OBJECT_ID, INVALID_OBJECT_ID, ALL_EMPIRES, ALL_EMPIRES, Visibility::VIS_NO_VISIBILITY)
{}

StealthChangeEvent::StealthChangeEventDetail::StealthChangeEventDetail(
    int attacker_id_, int target_id_, int attacker_empire_, int target_empire_, Visibility new_visibility_) :
    attacker_id(attacker_id_),
    target_id(target_id_),
    attacker_empire_id(attacker_empire_),
    target_empire_id(target_empire_),
    visibility(new_visibility_)
{}

std::string StealthChangeEvent::StealthChangeEventDetail::DebugString(const ScriptingContext& context) const {
    std::stringstream ss;
    ss << "StealthChangeDetailEvent"
       <<  FighterOrPublicNameLink(ALL_EMPIRES, attacker_id, attacker_empire_id, context)
       << "->" << visibility << " ";
    return ss.str();
}

std::string StealthChangeEvent::StealthChangeEventDetail::CombatLogDescription(
    int viewing_empire_id, const ScriptingContext& context) const
{
    std::string attacker_link = FighterOrPublicNameLink(viewing_empire_id, attacker_id, attacker_empire_id, context);
    std::string target_link = FighterOrPublicNameLink(viewing_empire_id, target_id, target_empire_id, context);
    std::string empire_link = EmpireLink(target_empire_id, context);
    const std::string& template_str = UserString("ENC_COMBAT_STEALTH_DECLOAK_ATTACK");

    return str(FlexibleFormat(template_str)
               % attacker_link
               % target_link
               % empire_link);
}


void StealthChangeEvent::AddEvent(int attacker_id_, int target_id_, int attacker_empire_,
                                  int target_empire_, Visibility new_visibility_)
{
    events[target_empire_].push_back(
        std::make_shared<StealthChangeEventDetail>(
            attacker_id_, target_id_, attacker_empire_, target_empire_, new_visibility_));
}

std::string StealthChangeEvent::DebugString(const ScriptingContext& context) const {
    std::stringstream ss;
    ss << "StealthChangeEvent";
    if (events.size() > 4) {
        ss << events.size() << " empires.";
    } else {
        for (auto& [target_empire_id, stealth_change_events] : events) {
            ss << "Target Empire: " << EmpireLink(target_empire_id, context) << "\n";

            if (stealth_change_events.size() > 4) {
                ss << stealth_change_events.size() << " events.";
            } else {
                for (const auto& event : stealth_change_events)
                    ss << event->DebugString(context);
            }
        }
    }
    return ss.str();
}

std::string StealthChangeEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    std::string desc;

    if (events.empty())
        return desc;

    for (const auto& [target_empire_id, stealth_change_events] : events) {
        std::vector<std::string> uncloaked_attackers;
        uncloaked_attackers.reserve(stealth_change_events.size());
        for (const auto& event : stealth_change_events)
            uncloaked_attackers.push_back(FighterOrPublicNameLink(
                viewing_empire_id, event->attacker_id, event->attacker_empire_id, context));

        if (!uncloaked_attackers.empty()) {
            if (!desc.empty())
                desc += "\n";
            std::vector<std::string> target_empire_link{EmpireLink(target_empire_id, context)};

            desc += FlexibleFormatList(target_empire_link, uncloaked_attackers,
                                       UserString("ENC_COMBAT_STEALTH_DECLOAK_ATTACK_MANY_EVENTS"),
                                       UserString("ENC_COMBAT_STEALTH_DECLOAK_ATTACK_1_EVENTS")).str();
        }
    }

    return desc;
}

bool StealthChangeEvent::AreSubEventsEmpty(int viewing_empire_id) const
{ return events.empty(); }

std::vector<ConstCombatEventPtr> StealthChangeEvent::SubEvents(int viewing_empire_id) const {
    std::vector<ConstCombatEventPtr> all_events;
    all_events.reserve(events.size());  // underestimate probably
    for (const auto& target : events)
        for (const auto& event : target.second)
            all_events.push_back(std::dynamic_pointer_cast<CombatEvent>(event));
    return all_events;
}


//////////////////////////////////////////
///////// Attack Event////////////////////
//////////////////////////////////////////
WeaponFireEvent::WeaponFireEvent(
    int bout_, int round_, int attacker_id_, int target_id_, std::string weapon_name_,
    const std::tuple<float, float, float>& power_shield_damage,
    int attacker_owner_id_, int target_owner_id_) :
    bout(bout_),
    round(round_),
    attacker_id(attacker_id_),
    target_id(target_id_),
    weapon_name(std::move(weapon_name_)),
    attacker_owner_id(attacker_owner_id_),
    target_owner_id(target_owner_id_)
{ std::tie(power, shield, damage) = power_shield_damage; }

std::string WeaponFireEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "rnd: " << round << " : "
       << attacker_id << " -> " << target_id << " : " << weapon_name << " "
       << power << " - " << shield << " = " << damage << "   attacker owner: " << attacker_owner_id;
    return ss.str();
}

std::string WeaponFireEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    std::string attacker_link = FighterOrPublicNameLink(viewing_empire_id, attacker_id, attacker_owner_id, context);
    std::string target_link = FighterOrPublicNameLink(viewing_empire_id, target_id, target_owner_id, context);

    const std::string& template_str = UserString("ENC_COMBAT_ATTACK_STR");

    return str(FlexibleFormat(template_str)
               % attacker_link
               % target_link
               % damage
               % bout
               % round);
}

std::string WeaponFireEvent::CombatLogDetails(int viewing_empire_id) const {
    const std::string& template_str = UserString("ENC_COMBAT_ATTACK_DETAILS");

    if (shield >= 0)
        return str(FlexibleFormat(template_str)
                   % ShipPartLink(weapon_name)
                   % power
                   % shield
                   % damage);
    else
        return str(FlexibleFormat(template_str)
                   % ShipPartLink(weapon_name)
                   % power
                   % UserString("ENC_COMBAT_SHIELD_PIERCED")
                   % damage);
}

boost::optional<int> WeaponFireEvent::PrincipalFaction(int viewing_empire_id) const
{ return attacker_id; }


//////////////////////////////////////////
///////// IncapacitationEvent/////////////
//////////////////////////////////////////

IncapacitationEvent::IncapacitationEvent() :
    bout(-1),
    object_id(INVALID_OBJECT_ID),
    object_owner_id(ALL_EMPIRES)
{}

IncapacitationEvent::IncapacitationEvent(int bout_, int object_id_, int object_owner_id_) :
    bout(bout_),
    object_id(object_id_),
    object_owner_id(object_owner_id_)
{}

std::string IncapacitationEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "incapacitation of " << object_id << " owned by " << object_owner_id << " at bout " << bout;
    return ss.str();
}


std::string IncapacitationEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    auto object = context.ContextObjects().get(object_id);
    std::string template_str, object_str;
    int owner_id = object_owner_id;

    if (!object && object_id < 0) {
        template_str = UserString("ENC_COMBAT_FIGHTER_INCAPACITATED_STR");
        object_str = UserString("OBJ_FIGHTER");

    } else if (!object) {
        template_str = UserString("ENC_COMBAT_UNKNOWN_DESTROYED_STR");
        object_str = UserString("ENC_COMBAT_UNKNOWN_OBJECT");

    } else if (object->ObjectType() == UniverseObjectType::OBJ_PLANET) {
        template_str = UserString("ENC_COMBAT_PLANET_INCAPACITATED_STR");
        object_str = PublicNameLink(viewing_empire_id, object_id, context.ContextUniverse());

    } else {    // ships or other to-be-determined objects...
        template_str = UserString("ENC_COMBAT_DESTROYED_STR");
        object_str = PublicNameLink(viewing_empire_id, object_id, context.ContextUniverse());
    }

    std::string owner_string = " ";
    if (auto owner = context.GetEmpire(owner_id))
        owner_string += owner->Name() + " ";

    std::string object_link = FighterOrPublicNameLink(viewing_empire_id, object_id, object_owner_id, context);

    return str(FlexibleFormat(template_str) % owner_string % object_link);
}

boost::optional<int> IncapacitationEvent::PrincipalFaction(int viewing_empire_id) const
{ return object_owner_id; }


//////////////////////////////////////////
///////// FightersAttackFightersEvent ////
//////////////////////////////////////////
FightersAttackFightersEvent::FightersAttackFightersEvent(int bout_) :
    bout(bout_)
{}

void FightersAttackFightersEvent::AddEvent(int attacker_empire_, int target_empire_)
{ events[{attacker_empire_, target_empire_}] += 1; }

std::string FightersAttackFightersEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "FightersAttackFightersEvent: ";
    for (auto& [index, event]: events) {
        ss << event << " repeated fighters from empire " << index.first
           << " attacking fighters from empire " << index.second << ", ";
    }
    return ss.str();
}

std::string FightersAttackFightersEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    if (events.empty())
        return "";

    const auto& events_to_show = events;
    auto num_events_remaining = events.size();
    std::stringstream ss;

    // Use show_events_for_empire to show events in this order: viewing empire, ALL_EMPIRES and
    // then the remainder.
    auto show_events_for_empire =
        [&ss, &num_events_remaining, &events_to_show, &viewing_empire_id, &context]
        (boost::optional<int> show_attacker)
    {
            int attacker_empire;
            int target_empire;
            for (const auto& index_and_event : events_to_show) {
                std::tie(attacker_empire, target_empire) = index_and_event.first;

                // Skip if this is not the particular attacker requested
                if (show_attacker && *show_attacker != attacker_empire)
                    continue;

                // Skip if no particular attacker was requested and this empire is the viewing
                // empire or ALL_EMPIRES
                if (!show_attacker && (attacker_empire == viewing_empire_id || attacker_empire == ALL_EMPIRES))
                    continue;

                auto count = std::to_string(index_and_event.second);
                const auto&& attacker_link = FighterOrPublicNameLink(
                    viewing_empire_id, INVALID_OBJECT_ID, attacker_empire, context);
                const auto&& target_link = FighterOrPublicNameLink(
                    viewing_empire_id, INVALID_OBJECT_ID, target_empire, context);
                const std::string& template_str = UserString("ENC_COMBAT_ATTACK_REPEATED_STR");

                ss << str(FlexibleFormat(template_str) % count % attacker_link % target_link);
                if (--num_events_remaining > 0)
                    ss << "\n";
            }
        };

    // Sort the events by viewing empire, then ALL_EMPIRES and then other empires.
    show_events_for_empire(viewing_empire_id);
    show_events_for_empire(ALL_EMPIRES);
    show_events_for_empire(boost::none);

    return ss.str();
}


//////////////////////////////////////////
/////////// FighterLaunchEvent ///////////
//////////////////////////////////////////
FighterLaunchEvent::FighterLaunchEvent(int bout_, int launched_from_id_, int fighter_owner_empire_id_, int number_launched_) :
    bout(bout_),
    fighter_owner_empire_id(fighter_owner_empire_id_),
    launched_from_id(launched_from_id_),
    number_launched(number_launched_)
{}

std::string FighterLaunchEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "launch from object " << launched_from_id
       << " of " << number_launched
       << " fighter(s) of empire " << fighter_owner_empire_id
       << " at bout " << bout;
    return ss.str();
}

std::string FighterLaunchEvent::CombatLogDescription(int viewing_empire_id,
                                                     const ScriptingContext& context) const
{
    std::string launched_from_link = PublicNameLink(viewing_empire_id, launched_from_id,
                                                    context.ContextUniverse());
    std::string empire_coloured_fighter = EmpireColorWrappedText(
        fighter_owner_empire_id, UserString("OBJ_FIGHTER"), context);

    // launching negative fighters indicates recovery of them by the ship
    const std::string& template_str = (number_launched >= 0 ?
                                       UserString("ENC_COMBAT_LAUNCH_STR") :
                                       UserString("ENC_COMBAT_RECOVER_STR"));

   return str(FlexibleFormat(template_str)
              % launched_from_link
              % empire_coloured_fighter
              % std::abs(number_launched));
}

boost::optional<int> FighterLaunchEvent::PrincipalFaction(int viewing_empire_id) const
{ return fighter_owner_empire_id; }


//////////////////////////////////////////
///////// FightersDestroyedEvent ////
//////////////////////////////////////////
FightersDestroyedEvent::FightersDestroyedEvent(int bout_) :
    bout(bout_)
{}

void FightersDestroyedEvent::AddEvent(int target_empire_)
{ events[target_empire_] += 1; }

std::string FightersDestroyedEvent::DebugString(const ScriptingContext&) const {
    std::stringstream ss;
    ss << "FightersDestroyedEvent: ";
    for (auto& [empire_id, count] : events) {
        ss << count << " repeated fighters from empire " << empire_id << " destroyed.";
    }
    return ss.str();
}

std::string FightersDestroyedEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    if (events.empty())
        return "";

    const auto& events_to_show = events;
    auto num_events_remaining = events.size();
    std::stringstream ss;

    // Use show_events_for_empire to show events in this order: viewing empire,
    // ALL_EMPIRES and then the remainder.
    auto show_events_for_empire =
        [&ss, &num_events_remaining, &events_to_show, &viewing_empire_id, &context]
        (boost::optional<int> show_empire_id)
    {
            for (auto& [target_empire_id, count] : events_to_show) {
                // Skip if this is not the particular attacker requested
                if (show_empire_id && *show_empire_id != target_empire_id)
                    continue;

                // Skip if no particular empire was requested and this empire is the viewing
                // empire or ALL_EMPIRES
                if (!show_empire_id && (target_empire_id == viewing_empire_id || target_empire_id == ALL_EMPIRES))
                    continue;

                auto count_str{std::to_string(count)};
                auto target_empire_link{EmpireLink(target_empire_id, context)};
                const auto target_link{FighterOrPublicNameLink(
                    viewing_empire_id, INVALID_OBJECT_ID, target_empire_id, context)};

                if (count == 1) {
                    const std::string& template_str = UserString("ENC_COMBAT_FIGHTER_INCAPACITATED_STR");
                    ss << str(FlexibleFormat(template_str) % target_empire_link % target_link);
                } else {
                    const std::string& template_str = UserString("ENC_COMBAT_FIGHTER_INCAPACITATED_REPEATED_STR");
                    ss << str(FlexibleFormat(template_str) % count_str % target_empire_link % target_link);
                }
                if (--num_events_remaining > 0)
                    ss << "\n";
            }
        };

    // Sort the events by viewing empire, then ALL_EMPIRES and then other empires.
    show_events_for_empire(viewing_empire_id);
    show_events_for_empire(ALL_EMPIRES);
    show_events_for_empire(boost::none);

    return ss.str();
}


//////////////////////////////////////////
///////// WeaponsPlatformEvent /////////////
//////////////////////////////////////////
WeaponsPlatformEvent::WeaponsPlatformEvent(int bout_, int attacker_id_, int attacker_owner_id_) :
    bout(bout_),
    attacker_id(attacker_id_),
    attacker_owner_id(attacker_owner_id_)
{}

void WeaponsPlatformEvent::AddEvent(int round_, int target_id_, int target_owner_id_,
                                    const std::string& weapon_name_, float power_,
                                    float shield_, float damage_)
{
    events[target_id_].push_back(
        std::make_shared<WeaponFireEvent>(
            bout, round_, attacker_id, target_id_, weapon_name_,
            std::tie(power_, shield_, damage_),
            attacker_owner_id, target_owner_id_));
}

std::string WeaponsPlatformEvent::DebugString(const ScriptingContext& context) const {
    std::stringstream desc;
    desc << "WeaponsPlatformEvent bout = " << bout << " attacker_id = "
        << attacker_id << " attacker_owner = "<< attacker_owner_id;
    for (auto& [ignored, event_vec] : events) {
        (void)ignored;
        for (const auto& attack : event_vec)
            desc << "\n" << attack->DebugString(context);
    }
    return desc.str();
}

std::string WeaponsPlatformEvent::CombatLogDescription(int viewing_empire_id, const ScriptingContext& context) const {
    if (events.empty())
        return "";

    std::vector<std::string> damaged_target_links;
    std::vector<std::string> undamaged_target_links;

    for (const auto& target : events) {
        if (target.second.empty())
            continue;

        const auto& fire_event(*target.second.begin());
        std::string target_public_name{
            FighterOrPublicNameLink(viewing_empire_id, target.first, fire_event->target_owner_id, context)};

        double damage = 0.0f;
        for (auto attack_it : target.second)
            damage += attack_it->damage;

        if (damage <= 0.0f) {
            undamaged_target_links.push_back(std::move(target_public_name));
        } else {
            damaged_target_links.push_back(
                str(FlexibleFormat(UserString("ENC_COMBAT_PLATFORM_TARGET_AND_DAMAGE"))
                    % target_public_name % damage));
        }
    }

    std::string desc;

    std::vector<std::string> attacker_link{FighterOrPublicNameLink(
        viewing_empire_id, attacker_id, attacker_owner_id, context)};

    if (!damaged_target_links.empty() ) {
        desc += FlexibleFormatList(attacker_link, damaged_target_links,
                                   UserString("ENC_COMBAT_PLATFORM_DAMAGE_MANY_EVENTS"),
                                   UserString("ENC_COMBAT_PLATFORM_DAMAGE_1_EVENTS")).str();

        if (!undamaged_target_links.empty())
            desc += "\n";
    }
    if (!undamaged_target_links.empty()) {
        desc += FlexibleFormatList(attacker_link, undamaged_target_links,
                                   UserString("ENC_COMBAT_PLATFORM_NO_DAMAGE_MANY_EVENTS"),
                                   UserString("ENC_COMBAT_PLATFORM_NO_DAMAGE_1_EVENTS")).str();
    }
    return desc;
}

bool WeaponsPlatformEvent::AreSubEventsEmpty(int viewing_empire_id) const
{ return events.empty(); }

std::vector<ConstCombatEventPtr> WeaponsPlatformEvent::SubEvents(int viewing_empire_id) const {
    std::vector<ConstCombatEventPtr> all_events;
    all_events.reserve(events.size());  // underestimate probably
    for (const auto& target : events)
        for (const auto& event : target.second)
            all_events.push_back(std::dynamic_pointer_cast<CombatEvent>(event));
    return all_events;
}

boost::optional<int> WeaponsPlatformEvent::PrincipalFaction(int viewing_empire_id) const
{ return attacker_owner_id; }
