#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_GunShot.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_GunShot : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_GunShot()
    {
        DisplayName     = FText::FromString(TEXT("Gun Shot"));
        MaxCooldown     = 0;
        bEndsTurn       = false;            // Player retains their turn after firing.
        AbilityCategory = EAbilityCategory::Gun;  // appears on the Gun button
        Element         = EElement::Pierce;
        TargetScope     = ETargetScope::SingleEnemy;
        Costs.Add({ EResourceType::AP, 1.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
