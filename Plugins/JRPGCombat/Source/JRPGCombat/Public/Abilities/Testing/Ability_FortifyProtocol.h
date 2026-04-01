#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_FortifyProtocol.generated.h"

/**
 * Fortify Protocol — a Skill-category ability (NOT a Protocol-system item).
 * Applies Defense Up to a single ally for 3 turns.
 * Costs 2 AP, ends the turn.
 *
 * Note: despite the name containing "Protocol", this is a character ability
 * in the Skill submenu, NOT one of the three shared-charge Protocols
 * (Healing / Revival / AP).  Those live in the Protocol submenu.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_FortifyProtocol : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_FortifyProtocol()
    {
        DisplayName     = FText::FromString(TEXT("Fortify Protocol"));
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::Skill;
        TargetScope     = ETargetScope::SingleAlly;
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
