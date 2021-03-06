#include "Planet.h"

#include "Building.h"
#include "Fleet.h"
#include "Ship.h"
#include "System.h"
#include "Predicates.h"
#include "Species.h"
#include "Condition.h"
#include "Universe.h"
#include "ValueRef.h"
#include "../util/Logger.h"
#include "../util/OptionsDB.h"
#include "../util/Random.h"
#include "../util/Directories.h"
#include "../util/SitRepEntry.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"

#include <boost/assign/list_of.hpp>

namespace {
    // high tilt is arbitrarily taken to mean 45 degrees or more
    const float HIGH_TILT_THERESHOLD = 45.0f;

    float SizeRotationFactor(PlanetSize size) {
        switch (size) {
        case SZ_TINY:     return 1.5f;
        case SZ_SMALL:    return 1.25f;
        case SZ_MEDIUM:   return 1.0f;
        case SZ_LARGE:    return 0.75f;
        case SZ_HUGE:     return 0.5f;
        case SZ_GASGIANT: return 0.25f;
        default:          return 1.0f;
        }
        return 1.0f;
    }

    static const std::string EMPTY_STRING;
}


////////////////////////////////////////////////////////////
// Year
////////////////////////////////////////////////////////////
Year::Year(Day d) :
    TypesafeFloat(d / 360.0f)
{}


////////////////////////////////////////////////////////////
// Planet
////////////////////////////////////////////////////////////
Planet::Planet() :
    UniverseObject(),
    PopCenter(),
    ResourceCenter(),
    m_type(PT_TERRAN),
    m_original_type(PT_TERRAN),
    m_size(SZ_MEDIUM),
    m_orbital_period(1.0f),
    m_initial_orbital_position(0.0f),
    m_rotational_period(1.0f),
    m_axial_tilt(23.0f),
    m_just_conquered(false),
    m_is_about_to_be_colonized(false),
    m_is_about_to_be_invaded(false),
    m_is_about_to_be_bombarded(false),
    m_ordered_given_to_empire_id(ALL_EMPIRES),
    m_last_turn_attacked_by_ship(-1),
    m_surface_texture()
{
    //DebugLogger() << "Planet::Planet()";
    // assumes PopCenter and ResourceCenter don't need to be initialized, due to having been re-created
    // in functional form by deserialization.  Also assumes planet-specific meters don't need to be re-added.
}

Planet::Planet(PlanetType type, PlanetSize size) :
    UniverseObject(),
    PopCenter(),
    ResourceCenter(),
    m_type(type),
    m_original_type(type),
    m_size(size),
    m_orbital_period(1.0f),
    m_initial_orbital_position(RandZeroToOne() * 2 * 3.14159f),
    m_rotational_period(1.0f),
    m_axial_tilt(RandZeroToOne() * HIGH_TILT_THERESHOLD),
    m_just_conquered(false),
    m_is_about_to_be_colonized(false),
    m_is_about_to_be_invaded(false),
    m_is_about_to_be_bombarded(false),
    m_ordered_given_to_empire_id(ALL_EMPIRES),
    m_last_turn_attacked_by_ship(-1),
    m_surface_texture()
{
    //DebugLogger() << "Planet::Planet(" << type << ", " << size <<")";
    UniverseObject::Init();
    PopCenter::Init();
    ResourceCenter::Init();
    Planet::Init();

    const double SPIN_STD_DEV = 0.1;
    const double REVERSE_SPIN_CHANCE = 0.06;
    m_rotational_period = RandGaussian(1.0, SPIN_STD_DEV) / SizeRotationFactor(m_size);
    if (RandZeroToOne() < REVERSE_SPIN_CHANCE)
        m_rotational_period = -m_rotational_period;
}

Planet* Planet::Clone(int empire_id) const {
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(this->ID(), empire_id);

    if (!(vis >= VIS_BASIC_VISIBILITY && vis <= VIS_FULL_VISIBILITY))
        return 0;

    Planet* retval = new Planet();
    retval->Copy(TemporaryFromThis(), empire_id);
    return retval;
}

void Planet::Copy(TemporaryPtr<const UniverseObject> copied_object, int empire_id) {
    if (copied_object == this)
        return;
    TemporaryPtr<const Planet> copied_planet = boost::dynamic_pointer_cast<const Planet>(copied_object);
    if (!copied_planet) {
        ErrorLogger() << "Planet::Copy passed an object that wasn't a Planet";
        return;
    }

    int copied_object_id = copied_object->ID();
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(copied_object_id, empire_id);
    std::set<std::string> visible_specials = GetUniverse().GetObjectVisibleSpecialsByEmpire(copied_object_id, empire_id);

    UniverseObject::Copy(copied_object, vis, visible_specials);
    PopCenter::Copy(copied_planet, vis);
    ResourceCenter::Copy(copied_planet, vis);

    if (vis >= VIS_BASIC_VISIBILITY) {
        this->m_name =                      copied_planet->m_name;

        this->m_buildings =                 copied_planet->VisibleContainedObjectIDs(empire_id);
        this->m_type =                      copied_planet->m_type;
        this->m_original_type =             copied_planet->m_original_type;
        this->m_size =                      copied_planet->m_size;
        this->m_orbital_period =            copied_planet->m_orbital_period;
        this->m_initial_orbital_position =  copied_planet->m_initial_orbital_position;
        this->m_rotational_period =         copied_planet->m_rotational_period;
        this->m_axial_tilt =                copied_planet->m_axial_tilt;
        this->m_just_conquered =            copied_planet->m_just_conquered;

        if (vis >= VIS_PARTIAL_VISIBILITY) {
            if (vis >= VIS_FULL_VISIBILITY) {
                this->m_is_about_to_be_colonized =  copied_planet->m_is_about_to_be_colonized;
                this->m_is_about_to_be_invaded   =  copied_planet->m_is_about_to_be_invaded;
                this->m_is_about_to_be_bombarded =  copied_planet->m_is_about_to_be_bombarded;
                this->m_ordered_given_to_empire_id =copied_planet->m_ordered_given_to_empire_id;
                this->m_last_turn_attacked_by_ship= copied_planet->m_last_turn_attacked_by_ship;
            } else {
                // copy system name if at partial visibility, as it won't be copied
                // by UniverseObject::Copy unless at full visibility, but players
                // should know planet names even if they don't own the planet
                GetUniverse().InhibitUniverseObjectSignals(true);
                this->Rename(copied_planet->Name());
                GetUniverse().InhibitUniverseObjectSignals(false);
            }
        }
    }
}

std::set<std::string> Planet::Tags() const {
    const Species* species = GetSpecies(SpeciesName());
    if (!species)
        return std::set<std::string>();
    return species->Tags();
}

bool Planet::HasTag(const std::string& name) const {
    const Species* species = GetSpecies(SpeciesName());

    return species && species->Tags().count(name);
}

UniverseObjectType Planet::ObjectType() const
{ return OBJ_PLANET; }

std::string Planet::Dump() const {
    std::stringstream os;
    os << UniverseObject::Dump();
    os << PopCenter::Dump();
    os << ResourceCenter::Dump();
    os << " type: " << UserString(EnumToString(m_type))
       << " original type: " << UserString(EnumToString(m_original_type))
       << " size: " << UserString(EnumToString(m_size))
       << " rot period: " << m_rotational_period
       << " axis tilt: " << m_axial_tilt
       << " buildings: ";
    for (std::set<int>::const_iterator it = m_buildings.begin(); it != m_buildings.end();) {
        int building_id = *it;
        ++it;
        os << building_id << (it == m_buildings.end() ? "" : ", ");
    }
    if (m_is_about_to_be_colonized)
        os << " (About to be Colonize)";
    if (m_is_about_to_be_invaded)
        os << " (About to be Invaded)";
    if (m_just_conquered)
        os << " (Just Conquered)";
    if (m_is_about_to_be_bombarded)
        os << " (About to be Bombarded)";
    if (m_ordered_given_to_empire_id != ALL_EMPIRES)
        os << " (Ordered to be given to empire with id: " << m_ordered_given_to_empire_id << ")";
    os << " last attacked on turn: " << m_last_turn_attacked_by_ship;

    return os.str();
}

int Planet::SizeAsInt() const {
    switch (m_size) {
    case SZ_GASGIANT:   return 6;   break;
    case SZ_HUGE:       return 5;   break;
    case SZ_LARGE:      return 4;   break;
    case SZ_MEDIUM:     return 3;   break;
    case SZ_ASTEROIDS:  return 3;   break;
    case SZ_SMALL:      return 2;   break;
    case SZ_TINY:       return 1;   break;
    default:            return 0;   break;
    }
}

void Planet::Init() {
    AddMeter(METER_SUPPLY);
    AddMeter(METER_MAX_SUPPLY);
    AddMeter(METER_SHIELD);
    AddMeter(METER_MAX_SHIELD);
    AddMeter(METER_DEFENSE);
    AddMeter(METER_MAX_DEFENSE);
    AddMeter(METER_TROOPS);
    AddMeter(METER_MAX_TROOPS);
    AddMeter(METER_DETECTION);
    AddMeter(METER_REBEL_TROOPS);
}

PlanetEnvironment Planet::EnvironmentForSpecies(const std::string& species_name/* = ""*/) const {
    const Species* species = 0;
    if (species_name.empty()) {
        const std::string& this_planet_species_name = this->SpeciesName();
        if (this_planet_species_name.empty())
            return PE_UNINHABITABLE;
        species = GetSpecies(this_planet_species_name);
    } else {
        species = GetSpecies(species_name);
    }
    if (!species) {
        ErrorLogger() << "Planet::EnvironmentForSpecies couldn't get species with name \"" << species_name << "\"";
        return PE_UNINHABITABLE;
    }
    return species->GetPlanetEnvironment(m_type);
}

PlanetType Planet::NextBetterPlanetTypeForSpecies(const std::string& species_name/* = ""*/) const {
    const Species* species = 0;
    if (species_name.empty()) {
        const std::string& this_planet_species_name = this->SpeciesName();
        if (this_planet_species_name.empty())
            return m_type;
        species = GetSpecies(this_planet_species_name);
    } else {
        species = GetSpecies(species_name);
    }
    if (!species) {
        ErrorLogger() << "Planet::NextBetterPlanetTypeForSpecies couldn't get species with name \"" << species_name << "\"";
        return m_type;
    }
    return species->NextBetterPlanetType(m_type);
}

namespace {
    PlanetType RingNextPlanetType(PlanetType current_type) {
        PlanetType next(PlanetType(int(current_type)+1));
        if (next >= PT_ASTEROIDS)
            next = PT_SWAMP;
        return next;
    }
    PlanetType RingPreviousPlanetType(PlanetType current_type) {
        PlanetType next(PlanetType(int(current_type)-1));
        if (next <= INVALID_PLANET_TYPE)
            next = PT_OCEAN;
        return next;
    }
}

PlanetType Planet::NextCloserToOriginalPlanetType() const {
    if (m_type == INVALID_PLANET_TYPE ||
        m_type == PT_GASGIANT ||
        m_type == PT_ASTEROIDS ||
        m_original_type == INVALID_PLANET_TYPE ||
        m_original_type == PT_GASGIANT ||
        m_original_type == PT_ASTEROIDS)
    { return m_type; }

    if (m_type == m_original_type)
        return m_type;

    PlanetType cur_type = m_type;
    int cw_steps = 0;
    while (cur_type != m_original_type) {
        cw_steps++;
        cur_type = RingNextPlanetType(cur_type);
    }

    cur_type = m_type;
    int ccw_steps = 0;
    while (cur_type != m_original_type) {
        ccw_steps++;
        cur_type = RingPreviousPlanetType(cur_type);
    }

    if (cw_steps <= ccw_steps)
        return RingNextPlanetType(m_type);
    return RingPreviousPlanetType(m_type);
}

namespace {
    PlanetType LoopPlanetTypeIncrement(PlanetType initial_type, int step) {
        // avoid too large steps that would mess up enum arithmatic
        if (std::abs(step) >= PT_ASTEROIDS) {
            DebugLogger() << "LoopPlanetTypeIncrement giving too large step: " << step;
            return initial_type;
        }
        // some types can't be terraformed
        if (initial_type == PT_GASGIANT)
            return PT_GASGIANT;
        if (initial_type == PT_ASTEROIDS)
            return PT_ASTEROIDS;
        if (initial_type == INVALID_PLANET_TYPE)
            return INVALID_PLANET_TYPE;
        if (initial_type == NUM_PLANET_TYPES)
            return NUM_PLANET_TYPES;
        // calculate next planet type, accounting for loop arounds
        PlanetType new_type(PlanetType(initial_type + step));
        if (new_type >= PT_ASTEROIDS)
            new_type = PlanetType(new_type - PT_ASTEROIDS);
        else if (new_type <= INVALID_PLANET_TYPE)
            new_type = PlanetType(new_type + PT_ASTEROIDS);
        return new_type;
    }
}

PlanetType Planet::ClockwiseNextPlanetType() const
{ return LoopPlanetTypeIncrement(m_type, 1); }

PlanetType Planet::CounterClockwiseNextPlanetType() const
{ return LoopPlanetTypeIncrement(m_type, -1); }

int Planet::TypeDifference(PlanetType type1, PlanetType type2) {
    // no distance defined for invalid types
    if (type1 == INVALID_PLANET_TYPE || type2 == INVALID_PLANET_TYPE)
        return 0;
    // if the same, distance is zero
    if (type1 == type2)
        return 0;
    // no distance defined for asteroids or gas giants with anything else
    if (type1 == PT_ASTEROIDS || type1 == PT_GASGIANT || type2 == PT_ASTEROIDS || type2 == PT_GASGIANT)
        return 0;
    // find distance around loop:
    //
    //  0  1  2
    //  8     3
    //  7     4
    //    6 5
    int diff = std::abs(int(type1) - int(type2));
    // raw_dist -> actual dist
    //  0 to 4       0 to 4
    //  5 to 8       4 to 1
    if (diff > 4)
        diff = 9 - diff;
    //std::cout << "typedifference type1: " << int(type1) << "  type2: " << int(type2) << "  diff: " << diff << std::endl;
    return diff;
}

namespace {
    PlanetSize PlanetSizeIncrement(PlanetSize initial_size, int step) {
        // some sizes don't have meaningful increments
        if (initial_size == SZ_GASGIANT)
            return SZ_GASGIANT;
        if (initial_size == SZ_ASTEROIDS)
            return SZ_ASTEROIDS;
        if (initial_size == SZ_NOWORLD)
            return SZ_NOWORLD;
        if (initial_size == INVALID_PLANET_SIZE)
            return INVALID_PLANET_SIZE;
        if (initial_size == NUM_PLANET_SIZES)
            return NUM_PLANET_SIZES;
        // calculate next planet size
        PlanetSize new_type(PlanetSize(initial_size + step));
        if (new_type >= SZ_HUGE)
            return SZ_HUGE;
        if (new_type <= SZ_TINY)
            return SZ_TINY;
        return new_type;
    }
}

PlanetSize Planet::NextLargerPlanetSize() const
{ return PlanetSizeIncrement(m_size, 1); }

PlanetSize Planet::NextSmallerPlanetSize() const
{ return PlanetSizeIncrement(m_size, -1); }

Year Planet::OrbitalPeriod() const
{ return m_orbital_period; }

Radian Planet::InitialOrbitalPosition() const
{ return m_initial_orbital_position; }

Radian Planet::OrbitalPositionOnTurn(int turn) const
{ return m_initial_orbital_position + OrbitalPeriod() * 2.0 * 3.1415926 / 4 * turn; }

Day Planet::RotationalPeriod() const
{ return m_rotational_period; }

Degree Planet::AxialTilt() const
{ return m_axial_tilt; }

TemporaryPtr<UniverseObject> Planet::Accept(const UniverseObjectVisitor& visitor) const
{ return visitor.Visit(boost::const_pointer_cast<Planet>(boost::static_pointer_cast<const Planet>(TemporaryFromThis()))); }

Meter* Planet::GetMeter(MeterType type)
{ return UniverseObject::GetMeter(type); }

const Meter* Planet::GetMeter(MeterType type) const
{ return UniverseObject::GetMeter(type); }

float Planet::InitialMeterValue(MeterType type) const
{ return UniverseObject::InitialMeterValue(type); }

float Planet::CurrentMeterValue(MeterType type) const
{ return UniverseObject::CurrentMeterValue(type); }

float Planet::NextTurnCurrentMeterValue(MeterType type) const {
    MeterType max_meter_type = INVALID_METER_TYPE;
    switch (type) {
    case METER_TARGET_POPULATION:
    case METER_POPULATION:
    case METER_TARGET_HAPPINESS:
    case METER_HAPPINESS:
        return PopCenterNextTurnMeterValue(type);
        break;
    case METER_TARGET_INDUSTRY:
    case METER_TARGET_RESEARCH:
    case METER_TARGET_TRADE:
    case METER_TARGET_CONSTRUCTION:
    case METER_INDUSTRY:
    case METER_RESEARCH:
    case METER_TRADE:
    case METER_CONSTRUCTION:
        return ResourceCenterNextTurnMeterValue(type);
        break;
    case METER_SHIELD:      max_meter_type = METER_MAX_SHIELD;          break;
    case METER_TROOPS:      max_meter_type = METER_MAX_TROOPS;          break;
    case METER_DEFENSE:     max_meter_type = METER_MAX_DEFENSE;         break;
    case METER_SUPPLY:      max_meter_type = METER_MAX_SUPPLY;          break;
        break;
    default:
        return UniverseObject::NextTurnCurrentMeterValue(type);
    }

    const Meter* meter = GetMeter(type);
    if (!meter) {
        throw std::invalid_argument("Planet::NextTurnCurrentMeterValue passed meter type that the Planet does not have, but should: " + boost::lexical_cast<std::string>(type));
    }
    float current_meter_value = meter->Current();

    const Meter* max_meter = GetMeter(max_meter_type);
    if (!max_meter) {
        throw std::runtime_error("Planet::NextTurnCurrentMeterValue dealing with invalid meter type: " + boost::lexical_cast<std::string>(type));
    }
    float max_meter_value = max_meter->Current();

    // being attacked prevents meter growth
    if (LastTurnAttackedByShip() >= CurrentTurn())
        return std::min(current_meter_value, max_meter_value);

    // currently meter growth is one per turn.
    return std::min(current_meter_value + 1.0f, max_meter_value);
}

int Planet::ContainerObjectID() const
{ return this->SystemID(); }

const std::set<int>& Planet::ContainedObjectIDs() const
{ return m_buildings; }

bool Planet::Contains(int object_id) const
{ return object_id != INVALID_OBJECT_ID && m_buildings.find(object_id) != m_buildings.end(); }

bool Planet::ContainedBy(int object_id) const
{ return object_id != INVALID_OBJECT_ID && this->SystemID() == object_id; }

std::vector<std::string> Planet::AvailableFoci() const {
    std::vector<std::string> retval;
    TemporaryPtr<const Planet> this_planet = boost::dynamic_pointer_cast<const Planet>(TemporaryFromThis());
    if (!this_planet)
        return retval;
    ScriptingContext context(this_planet);
    if (const Species* species = GetSpecies(this_planet->SpeciesName())) {
        const std::vector<FocusType>& foci = species->Foci();
        for (std::vector<FocusType>::const_iterator it = foci.begin(); it != foci.end(); ++it) {
            const FocusType& focus_type = *it;
            if (const Condition::ConditionBase* location = focus_type.Location()) {
                if (location->Eval(context, this_planet))
                    retval.push_back(focus_type.Name());
            }
        }
    }

    return retval;
}

const std::string& Planet::FocusIcon(const std::string& focus_name) const {
    if (const Species* species = GetSpecies(this->SpeciesName())) {
        const std::vector<FocusType>& foci = species->Foci();
        for (std::vector<FocusType>::const_iterator it = foci.begin(); it != foci.end(); ++it) {
            const FocusType& focus_type = *it;
            if (focus_type.Name() == focus_name)
                return focus_type.Graphic();
        }
    }
    return EMPTY_STRING;
}

void Planet::SetType(PlanetType type) {
    if (type <= INVALID_PLANET_TYPE)
        type = PT_SWAMP;
    if (NUM_PLANET_TYPES <= type)
        type = PT_GASGIANT;
    m_type = type;
    StateChangedSignal();
}

void Planet::SetSize(PlanetSize size) {
    if (size <= SZ_NOWORLD)
        size = SZ_TINY;
    if (NUM_PLANET_SIZES <= size)
        size = SZ_GASGIANT;
    m_size = size;
    StateChangedSignal();
}

void Planet::SetOrbitalPeriod(unsigned int orbit) {
    assert(orbit < 10);
    const double THIRD_ORBIT_PERIOD = 4;
    const double THIRD_ORBIT_RADIUS = OrbitalRadius(2);
    const double ORBIT_RADIUS = OrbitalRadius(orbit);
    // Kepler's third law.
    m_orbital_period =
        std::sqrt(std::pow(THIRD_ORBIT_PERIOD, 2.0) /
                  std::pow(THIRD_ORBIT_RADIUS, 3.0) *
                  std::pow(ORBIT_RADIUS, 3.0));
}

void Planet::SetRotationalPeriod(Day days)
{ m_rotational_period = days; }

void Planet::SetHighAxialTilt() {
    const double MAX_TILT = 90.0;
    m_axial_tilt = HIGH_TILT_THERESHOLD + RandZeroToOne() * (MAX_TILT - HIGH_TILT_THERESHOLD);
}

void Planet::AddBuilding(int building_id) {
    size_t buildings_size = m_buildings.size();
    m_buildings.insert(building_id);
    if (buildings_size != m_buildings.size())
        StateChangedSignal();
    // expect calling code to set building's planet
}

bool Planet::RemoveBuilding(int building_id) {
    if (m_buildings.find(building_id) != m_buildings.end()) {
        m_buildings.erase(building_id);
        StateChangedSignal();
        return true;
    }
    return false;
}

void Planet::Reset() {
    PopCenter::Reset();
    ResourceCenter::Reset();

    GetMeter(METER_SUPPLY)->Reset();
    GetMeter(METER_MAX_SUPPLY)->Reset();
    GetMeter(METER_SHIELD)->Reset();
    GetMeter(METER_MAX_SHIELD)->Reset();
    GetMeter(METER_DEFENSE)->Reset();
    GetMeter(METER_MAX_DEFENSE)->Reset();
    GetMeter(METER_DETECTION)->Reset();
    GetMeter(METER_REBEL_TROOPS)->Reset();

    if (m_is_about_to_be_colonized && !OwnedBy(ALL_EMPIRES)) {
        for (std::set<int>::const_iterator it = m_buildings.begin(); it != m_buildings.end(); ++it)
            if (TemporaryPtr<Building> building = GetBuilding(*it))
                building->Reset();
    }

    m_just_conquered = false;
    m_is_about_to_be_colonized = false;
    m_is_about_to_be_invaded = false;
    m_is_about_to_be_bombarded = false;
    SetOwner(ALL_EMPIRES);
}

void Planet::Depopulate() {
    PopCenter::Depopulate();

    GetMeter(METER_INDUSTRY)->Reset();
    GetMeter(METER_RESEARCH)->Reset();
    GetMeter(METER_TRADE)->Reset();
    GetMeter(METER_CONSTRUCTION)->Reset();

    ClearFocus();
}

void Planet::Conquer(int conquerer) {
    m_just_conquered = true;

    // deal with things on production queue located at this planet
    Empire::ConquerProductionQueueItemsAtLocation(ID(), conquerer);

    // deal with UniverseObjects (eg. buildings) located on this planet
    std::vector<TemporaryPtr<Building> > buildings = Objects().FindObjects<Building>(m_buildings);
    for (std::vector<TemporaryPtr<Building> >::iterator it = buildings.begin();
         it != buildings.end(); ++it)
    {
        TemporaryPtr<Building> building = *it;
        const BuildingType* type = GetBuildingType(building->BuildingTypeName());

        // determine what to do with building of this type...
        const CaptureResult cap_result = type->GetCaptureResult(building->Owner(), conquerer, this->ID(), false);

        if (cap_result == CR_CAPTURE) {
            // replace ownership
            building->SetOwner(conquerer);
        } else if (cap_result == CR_DESTROY) {
            // destroy object
            //DebugLogger() << "Planet::Conquer destroying object: " << building->Name();
            this->RemoveBuilding(building->ID());
            if (TemporaryPtr<System> system = GetSystem(this->SystemID()))
                system->Remove(building->ID());
            GetUniverse().Destroy(building->ID());
        } else if (cap_result == CR_RETAIN) {
            // do nothing
        }
    }

    // replace ownership
    SetOwner(conquerer);

    GetMeter(METER_SUPPLY)->SetCurrent(0.0f);
    GetMeter(METER_SUPPLY)->BackPropegate();
    GetMeter(METER_INDUSTRY)->SetCurrent(0.0f);
    GetMeter(METER_INDUSTRY)->BackPropegate();
    GetMeter(METER_RESEARCH)->SetCurrent(0.0f);
    GetMeter(METER_RESEARCH)->BackPropegate();
    GetMeter(METER_TRADE)->SetCurrent(0.0f);
    GetMeter(METER_TRADE)->BackPropegate();
    GetMeter(METER_CONSTRUCTION)->SetCurrent(0.0f);
    GetMeter(METER_CONSTRUCTION)->BackPropegate();
    GetMeter(METER_DEFENSE)->SetCurrent(0.0f);
    GetMeter(METER_DEFENSE)->BackPropegate();
    GetMeter(METER_SHIELD)->SetCurrent(0.0f);
    GetMeter(METER_SHIELD)->BackPropegate();
    GetMeter(METER_HAPPINESS)->SetCurrent(0.0f);
    GetMeter(METER_HAPPINESS)->BackPropegate();
}

bool Planet::Colonize(int empire_id, const std::string& species_name, double population) {
    const Species* species = 0;

    // if desired pop > 0, we want a colony, not an outpost, so we need to do some checks
    if (population > 0.0) {
        // check if specified species exists and get reference
        species = GetSpecies(species_name);
        if (!species) {
            ErrorLogger() << "Planet::Colonize couldn't get species already on planet with name: " << species_name;
            return false;
        }
        // check if specified species can colonize this planet
        if (EnvironmentForSpecies(species_name) < PE_HOSTILE) {
            ErrorLogger() << "Planet::Colonize: can't colonize planet already populated by species " << species_name;
            return false;
        }
    }

    // reset the planet to unowned/unpopulated
    if (!OwnedBy(empire_id)) {
        Reset();
    } else {
        PopCenter::Reset();
        for (std::set<int>::const_iterator it = m_buildings.begin(); it != m_buildings.end(); ++it)
            if (TemporaryPtr<Building> building = GetBuilding(*it))
                building->Reset();
        m_just_conquered = false;
        m_is_about_to_be_colonized = false;
        m_is_about_to_be_invaded = false;
        m_is_about_to_be_bombarded = false;
        SetOwner(ALL_EMPIRES);
    }

    // if desired pop > 0, we want a colony, not an outpost, so we have to set the colony species
    if (population > 0.0)
        SetSpecies(species_name);

    // find a default focus. use first defined available focus.
    // AvailableFoci function should return a vector of all names of
    // available foci.
    std::vector<std::string> available_foci = AvailableFoci();
    if (species && !available_foci.empty()) {
        bool found_preference = false;
        for (std::vector<std::string>::const_iterator it = available_foci.begin();
             it != available_foci.end(); ++it)
        {
            if (!it->empty() && *it == species->PreferredFocus()) {
                SetFocus(*it);
                found_preference = true;
                break;
            }
        }

        if (!found_preference)
            SetFocus(*available_foci.begin());
    } else {
        DebugLogger() << "Planet::Colonize unable to find a focus to set for species " << species_name;
    }

    // set colony population
    GetMeter(METER_POPULATION)->SetCurrent(population);
    GetMeter(METER_TARGET_POPULATION)->SetCurrent(population);
    BackPropegateMeters();


    // set specified empire as owner
    SetOwner(empire_id);

    // if there are buildings on the planet, set the specified empire as their owner too
    std::vector<TemporaryPtr<Building> > buildings = Objects().FindObjects<Building>(BuildingIDs());
    for (std::vector<TemporaryPtr<Building> >::iterator building_it = buildings.begin();
         building_it != buildings.end(); ++building_it)
    { (*building_it)->SetOwner(empire_id); }

    return true;
}

void Planet::SetIsAboutToBeColonized(bool b) {
    bool initial_status = m_is_about_to_be_colonized;
    if (b == initial_status) return;
    m_is_about_to_be_colonized = b;
    StateChangedSignal();
}

void Planet::ResetIsAboutToBeColonized()
{ SetIsAboutToBeColonized(false); }

void Planet::SetIsAboutToBeInvaded(bool b) {
    bool initial_status = m_is_about_to_be_invaded;
    if (b == initial_status) return;
    m_is_about_to_be_invaded = b;
    StateChangedSignal();
}

void Planet::ResetIsAboutToBeInvaded()
{ SetIsAboutToBeInvaded(false); }

void Planet::SetIsAboutToBeBombarded(bool b) {
    bool initial_status = m_is_about_to_be_bombarded;
    if (b == initial_status) return;
    m_is_about_to_be_bombarded = b;
    StateChangedSignal();
}

void Planet::ResetIsAboutToBeBombarded()
{ SetIsAboutToBeBombarded(false); }

void Planet::SetGiveToEmpire(int empire_id) {
    if (empire_id == m_ordered_given_to_empire_id) return;
    m_ordered_given_to_empire_id = empire_id;
    StateChangedSignal();
}

void Planet::ClearGiveToEmpire()
{ SetGiveToEmpire(ALL_EMPIRES); }

void Planet::SetLastTurnAttackedByShip(int turn)
{ m_last_turn_attacked_by_ship = turn; }

void Planet::SetSurfaceTexture(const std::string& texture) {
    m_surface_texture = texture;
    StateChangedSignal();
}

void Planet::PopGrowthProductionResearchPhase() {
    UniverseObject::PopGrowthProductionResearchPhase();

    bool just_conquered = m_just_conquered;
    // do not do production if planet was just conquered
    m_just_conquered = false;

    if (!just_conquered)
        ResourceCenterPopGrowthProductionResearchPhase();

    PopCenterPopGrowthProductionResearchPhase();

    // check for colonies without positive population, and change to outposts
    if (!SpeciesName().empty() && GetMeter(METER_POPULATION)->Current() <= 0.0f) {
        if (Empire* empire = GetEmpire(this->Owner())) {
            empire->AddSitRepEntry(CreatePlanetDepopulatedSitRep(this->ID()));

            // record depopulation of planet with species while owned by this empire
            std::map<std::string, int>::iterator species_it = empire->SpeciesPlanetsDepoped().find(SpeciesName());
            if (species_it == empire->SpeciesPlanetsDepoped().end())
                empire->SpeciesPlanetsDepoped()[SpeciesName()] = 1;
            else
                species_it->second++;
        }
        // remove species
        SetSpecies("");
    }

    if (!just_conquered) {
        GetMeter(METER_SHIELD)->SetCurrent(Planet::NextTurnCurrentMeterValue(METER_SHIELD));
        GetMeter(METER_DEFENSE)->SetCurrent(Planet::NextTurnCurrentMeterValue(METER_DEFENSE));
        GetMeter(METER_TROOPS)->SetCurrent(Planet::NextTurnCurrentMeterValue(METER_TROOPS));
        GetMeter(METER_REBEL_TROOPS)->SetCurrent(Planet::NextTurnCurrentMeterValue(METER_REBEL_TROOPS));
        GetMeter(METER_SUPPLY)->SetCurrent(Planet::NextTurnCurrentMeterValue(METER_SUPPLY));
    }

    StateChangedSignal();
}

void Planet::ResetTargetMaxUnpairedMeters() {
    UniverseObject::ResetTargetMaxUnpairedMeters();
    ResourceCenterResetTargetMaxUnpairedMeters();
    PopCenterResetTargetMaxUnpairedMeters();

    // give planets base stealth slightly above zero, so that they can't be
    // seen from a distance without high detection ability
    if (Meter* stealth = GetMeter(METER_STEALTH)) {
        stealth->ResetCurrent();
        //stealth->AddToCurrent(0.01f);
    }

    GetMeter(METER_MAX_SUPPLY)->ResetCurrent();
    GetMeter(METER_MAX_SHIELD)->ResetCurrent();
    GetMeter(METER_MAX_DEFENSE)->ResetCurrent();
    GetMeter(METER_MAX_TROOPS)->ResetCurrent();
    GetMeter(METER_REBEL_TROOPS)->ResetCurrent();
    GetMeter(METER_DETECTION)->ResetCurrent();
}

void Planet::ClampMeters() {
    UniverseObject::ClampMeters();
    ResourceCenterClampMeters();
    PopCenterClampMeters();

    UniverseObject::GetMeter(METER_MAX_SHIELD)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_SHIELD)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_SHIELD)->Current());
    UniverseObject::GetMeter(METER_MAX_DEFENSE)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_DEFENSE)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_DEFENSE)->Current());
    UniverseObject::GetMeter(METER_MAX_TROOPS)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_TROOPS)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_TROOPS)->Current());
    UniverseObject::GetMeter(METER_MAX_SUPPLY)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_SUPPLY)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_SUPPLY)->Current());

    UniverseObject::GetMeter(METER_REBEL_TROOPS)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_DETECTION)->ClampCurrentToRange();
}

// free functions

float PlanetRadius(PlanetSize size) {
    float retval = 0.0f;
    switch (size) {
    case INVALID_PLANET_SIZE: retval = 0.0f; break;
    case SZ_NOWORLD:          retval = 0.0f; break;
    case SZ_TINY:             retval = 2.0f; break;
    case SZ_SMALL:            retval = 3.5f; break;
    default:
    case SZ_MEDIUM:           retval = 5.0f; break;
    case SZ_LARGE:            retval = 7.0f; break;
    case SZ_HUGE:             retval = 9.0f; break;
    case SZ_ASTEROIDS:        retval = 0.0f; break;
    case SZ_GASGIANT:         retval = 11.0f; break; // this one goes to eleven
    case NUM_PLANET_SIZES:    retval = 0.0f; break;
    };
    return retval;
}

float AsteroidBeltRadius()
{ return 12.5f; }
