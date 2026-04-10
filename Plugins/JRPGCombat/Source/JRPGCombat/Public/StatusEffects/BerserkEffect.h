#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "BerserkEffect.generated.h"

/**
 * Berserk: Increases outgoing damage by 30%. Grants an extra immediate turn after acting.
 * Applied to a player character: the character acts automatically (handled by BattleManager).
 * The extra-turn flag is set at OnApply and reset after ConsumeExtraTurnGrant is called.
 */
UCLASS()
class JRPGCOMBAT_API UBerserkEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UBerserkEffect()
    {
        DisplayName   = FText::FromString("Berserk");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 6;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** +30% outgoing damage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Berserk")
    float DamageBonus = 0.30f;

    /** Set to true each turn start, consumed once by BattleManager at turn end. */
    bool bExtraTurnReady = false;

    virtual bool GrantsExtraTurn() const override { return bExtraTurnReady; }
    virtual void ConsumeExtraTurnGrant() override { bExtraTurnReady = false; }
    virtual void OnBeforeDealDamage_Implementation(FDamagePayload& Payload) override;
    virtual void OnApply_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
    virtual void OnTurnEnd_Implementation() override;
};
