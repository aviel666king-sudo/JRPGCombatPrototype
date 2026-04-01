#include "StatusEffects/VirusSpreadEffect.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"
#include "EngineUtils.h"          // TActorIterator

void UVirusSpreadEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[VirusSpread] Applied to %s. Stacks: %d"),
        Owner ? *Owner->GetName() : TEXT("?"), StackCount);
}

void UVirusSpreadEffect::OnTurnStart_Implementation()
{
    if (!Owner || Owner->IsDead()) { return; }

    // 1. Deal 6% MaxHP Magical damage.
    const float DamageAmount = Owner->GetMaxHP() * DamagePercent;

    FDamagePayload Payload;
    Payload.Source     = Source.Get();
    Payload.BaseDamage = DamageAmount;
    Payload.DamageType = EDamageType::Magical;

    Owner->ApplyDamage(Payload);

    UE_LOG(LogTemp, Log, TEXT("[VirusSpread] Ticked on %s. Dealt %.1f. Stacks: %d"),
        *Owner->GetName(), Payload.ResolvedDamage, StackCount);

    // 2. Spread chance: 15% to apply 1 stack to another living combatant.
    if (FMath::FRand() < SpreadChance)
    {
        UWorld* World = Owner->GetWorld();
        if (World)
        {
            // Collect all living combatants except Owner.
            // TODO: Replace with adjacency system when it exists — use random fallback for now.
            TArray<ACombatantBase*> Candidates;
            for (TActorIterator<ACombatantBase> It(World); It; ++It)
            {
                ACombatantBase* C = *It;
                // Spread only to the same team (enemy-to-enemy, player-to-player).
                if (C && C != Owner && C->IsAlive() && C->GetTeam() == Owner->GetTeam())
                {
                    Candidates.Add(C);
                }
            }

            if (Candidates.Num() > 0)
            {
                ACombatantBase* SpreadTarget = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
                // Apply exactly 1 stack — do not copy the full count.
                SpreadTarget->StatusEffectManager->ApplyEffect(
                    UVirusSpreadEffect::StaticClass(), Source.Get());

                UE_LOG(LogTemp, Log, TEXT("[VirusSpread] Spread 1 stack from %s to %s."),
                    *Owner->GetName(), *SpreadTarget->GetName());
            }
        }
    }

    // 3. Retention check: 20% chance to keep this stack, otherwise consume it.
    if (FMath::FRand() >= RetentionChance)
    {
        --StackCount;

        if (StackCount <= 0)
        {
            Duration = 0;  // Signal PurgeExpired.
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[VirusSpread] Stack retained on %s. Stacks: %d"),
            *Owner->GetName(), StackCount);
    }
}

void UVirusSpreadEffect::OnExpire_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[VirusSpread] Expired on %s."),
        Owner ? *Owner->GetName() : TEXT("?"));
}
