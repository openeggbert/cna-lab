#include "CnaTamagotchi/Domain/CreatureCatalog.hpp"

#include <array>
#include <iostream>

using namespace CnaTamagotchi::Domain;

namespace {

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void testEveryLifeStageResolvesToAnOriginalForm()
{
    PetState pet{};
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Egg, "egg must render as egg");

    pet.lifeStage = LifeStage::Hatchling;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Pipple,
        "hatchling must resolve to Pipple");

    pet.lifeStage = LifeStage::Child;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Sproutlet,
        "child must resolve to Sproutlet");

    pet.lifeStage = LifeStage::Farewell;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Farewell,
        "farewell must use its own non-living display form");
}

void testCareChangesTeenAndAdultOutcome()
{
    PetState wellCaredFor{};
    wellCaredFor.lifeStage = LifeStage::Teen;
    wellCaredFor.needs.discipline = 100;
    expect(CreatureCatalog::formFor(wellCaredFor) == CreatureForm::Flitwing,
        "well-cared-for teen must become Flitwing");

    PetState neglected{};
    neglected.lifeStage = LifeStage::Adult;
    neglected.careMistakes = 8;
    neglected.needs.discipline = 0;
    neglected.weight = 30;
    expect(CreatureCatalog::formFor(neglected) == CreatureForm::Duskroot,
        "neglected adult must reach the low-care form");

    PetState excellent{};
    excellent.lifeStage = LifeStage::Adult;
    excellent.ageMinutes = 12 * 24 * 60;
    excellent.needs.discipline = 100;
    expect(CreatureCatalog::formFor(excellent) == CreatureForm::Moonmote,
        "excellent long-lived adult must reach the rare form");
}

void testSecondEggLineUsesIndependentForms()
{
    PetState pet{};
    pet.species = PetSpecies::Mossling;
    pet.lifeStage = LifeStage::Hatchling;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Budbit,
        "Mossling hatchling must use the second-line baby form");

    pet.lifeStage = LifeStage::Teen;
    pet.needs.discipline = 100;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Lilyloop,
        "well-cared-for Mossling teen must use Lilyloop");

    pet.lifeStage = LifeStage::Adult;
    pet.careMistakes = 8;
    pet.needs.discipline = 0;
    pet.weight = 30;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Rootslug,
        "neglected Mossling adult must reach the low-care form");

    pet = PetState{};
    pet.species = PetSpecies::Mossling;
    pet.lifeStage = LifeStage::Adult;
    pet.ageMinutes = 12 * 24 * 60;
    pet.needs.discipline = 100;
    expect(CreatureCatalog::formFor(pet) == CreatureForm::Starbloom,
        "excellent long-lived Mossling must reach the rare form");
}

void testEverySpriteHasAStableName()
{
    constexpr std::array<CreatureForm, 24> forms{{
        CreatureForm::Egg, CreatureForm::Pipple, CreatureForm::Sproutlet,
        CreatureForm::Flitwing, CreatureForm::Tumblepuff, CreatureForm::Skywhistle,
        CreatureForm::Mossmuzzle, CreatureForm::Ripplefin, CreatureForm::Pebbleback,
        CreatureForm::Bramblepaw, CreatureForm::Duskroot, CreatureForm::Moonmote,
        CreatureForm::Budbit, CreatureForm::Fernkin, CreatureForm::Lilyloop,
        CreatureForm::Thornhop, CreatureForm::Reedhare, CreatureForm::Cloverowl,
        CreatureForm::Bloomtail, CreatureForm::Sedgehog, CreatureForm::Nectarmoth,
        CreatureForm::Rootslug, CreatureForm::Starbloom,
        CreatureForm::Farewell,
    }};

    for (const CreatureForm form : forms) {
        expect(!CreatureCatalog::nameFor(form).empty(), "every form needs a name");
        expect(CreatureCatalog::spriteFor(form).rows.size() == 10U,
            "every form needs a 10-row LCD sprite");
    }
}

} // namespace

int main()
{
    testEveryLifeStageResolvesToAnOriginalForm();
    testCareChangesTeenAndAdultOutcome();
    testSecondEggLineUsesIndependentForms();
    testEverySpriteHasAStableName();

    if (failures == 0) {
        std::cout << "CreatureCatalogTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
