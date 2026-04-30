#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_Percee.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_Percee : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_Percee()
    {
        DisplayName       = FText::FromString(TEXT("Perc\u00e9e"));  // Percée with accent
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;  // damage skill — blue strip active
        AbilityCategory   = EAbilityCategory::Skill;
        Element           = EElement::Pierce;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
