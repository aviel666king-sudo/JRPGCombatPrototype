#include "Core/TurnOrderManager.h"
#include "Characters/Base/CombatantBase.h"

// -----------------------------------------------------------------------------
//  Setup
// -----------------------------------------------------------------------------

void UTurnOrderManager::BuildTurnOrder(const TArray<ACombatantBase*>& InCombatants)
{
    AllCombatants.Reset();
    for (ACombatantBase* C : InCombatants)
    {
        if (C) { AllCombatants.Add(C); }
    }

    Queue = BuildSortedEntries();
    TurnNumber = 0;
}

// -----------------------------------------------------------------------------
//  Advancing turns
// -----------------------------------------------------------------------------

ACombatantBase* UTurnOrderManager::PeekNextCombatant() const
{
    if (Queue.IsEmpty()) { return nullptr; }
    return Queue[0].Combatant;
}

ACombatantBase* UTurnOrderManager::PopNextCombatant()
{
    while (true)
    {
        if (Queue.IsEmpty())
        {
            RebuildForNewRound();
        }

        if (Queue.IsEmpty())
        {
            return nullptr;
        }

        FTurnEntry Entry = Queue[0];
        Queue.RemoveAt(0);

        if (!Entry.Combatant || Entry.Combatant->IsDead())
        {
            continue;
        }

        ++TurnNumber;
        return Entry.Combatant;
    }
}

void UTurnOrderManager::RebuildForNewRound()
{
    Queue = BuildSortedEntries();
}

// -----------------------------------------------------------------------------
//  Manipulation
// -----------------------------------------------------------------------------

void UTurnOrderManager::SkipNextTurn(ACombatantBase* Combatant)
{
    // Remove the first queue entry that belongs to this combatant.
    const int32 Index = Queue.IndexOfByPredicate(
        [Combatant](const FTurnEntry& E) { return E.Combatant == Combatant; });

    if (Index != INDEX_NONE)
    {
        Queue.RemoveAt(Index);
    }
}

void UTurnOrderManager::InsertExtraTurn(ACombatantBase* Combatant)
{
    if (!Combatant) { return; }

    FTurnEntry Extra;
    Extra.Combatant  = Combatant;
    Extra.Initiative = Combatant->GetSpeed();  // Acts immediately next.

    // Insert at front so the extra turn happens before anyone else.
    Queue.Insert(Extra, 0);
}

// -----------------------------------------------------------------------------
//  Private helpers
// -----------------------------------------------------------------------------

TArray<FTurnEntry> UTurnOrderManager::BuildSortedEntries() const
{
    TArray<FTurnEntry> Entries;
    Entries.Reserve(AllCombatants.Num());

    for (ACombatantBase* C : AllCombatants)
    {
        if (!C || C->IsDead()) { continue; }

        FTurnEntry Entry;
        Entry.Combatant  = C;
        Entry.Initiative = C->GetEffectiveSpeed();
        Entries.Add(Entry);
    }

    // Sort descending — highest initiative acts first.
    Entries.Sort([](const FTurnEntry& A, const FTurnEntry& B)
    {
        return A.Initiative > B.Initiative;
    });

    return Entries;
}
