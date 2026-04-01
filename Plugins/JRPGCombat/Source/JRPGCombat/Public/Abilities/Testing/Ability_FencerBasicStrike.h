#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_FencerBasicStrike.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_FencerBasicStrike : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_FencerBasicStrike()
    {
        DisplayName     = FText::FromString(TEXT("Basic Strike"));
        MaxCooldown     = 0;
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::Melee;  // appears on the Melee button
        TargetScope     = ETargetScope::SingleEnemy;
        // No AP cost.
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
