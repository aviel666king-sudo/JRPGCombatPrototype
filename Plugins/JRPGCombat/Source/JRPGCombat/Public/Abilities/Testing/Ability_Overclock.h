#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_Overclock.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_Overclock : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_Overclock()
    {
        DisplayName     = FText::FromString(TEXT("Overclock"));
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::Skill;
        TargetScope     = ETargetScope::Self;
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
