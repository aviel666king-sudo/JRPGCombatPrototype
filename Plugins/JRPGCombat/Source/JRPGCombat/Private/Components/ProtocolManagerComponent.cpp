#include "Components/ProtocolManagerComponent.h"

UProtocolManagerComponent::UProtocolManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UProtocolManagerComponent::InitializeCharges()
{
    CurrentHealingCharges = HealingCharges;
    CurrentRevivalCharges = RevivalCharges;
    CurrentAPCharges      = APCharges;
}

bool UProtocolManagerComponent::CanSpend(EProtocolType Type) const
{
    return GetCurrentCharges(Type) > 0;
}

void UProtocolManagerComponent::SpendCharge(EProtocolType Type)
{
    switch (Type)
    {
        case EProtocolType::Healing:
            CurrentHealingCharges = FMath::Max(0, CurrentHealingCharges - 1);
            break;
        case EProtocolType::Revival:
            CurrentRevivalCharges = FMath::Max(0, CurrentRevivalCharges - 1);
            break;
        case EProtocolType::AP:
            CurrentAPCharges = FMath::Max(0, CurrentAPCharges - 1);
            break;
    }
}

int32 UProtocolManagerComponent::GetCurrentCharges(EProtocolType Type) const
{
    switch (Type)
    {
        case EProtocolType::Healing: return CurrentHealingCharges;
        case EProtocolType::Revival: return CurrentRevivalCharges;
        case EProtocolType::AP:      return CurrentAPCharges;
    }
    return 0;
}

int32 UProtocolManagerComponent::GetMaxCharges(EProtocolType Type) const
{
    switch (Type)
    {
        case EProtocolType::Healing: return HealingCharges;
        case EProtocolType::Revival: return RevivalCharges;
        case EProtocolType::AP:      return APCharges;
    }
    return 0;
}

TArray<FProtocolInfo> UProtocolManagerComponent::GetAllProtocolInfo() const
{
    TArray<FProtocolInfo> Result;
    Result.Reserve(3);

    auto Make = [&](EProtocolType Type, const FString& Name)
    {
        FProtocolInfo Info;
        Info.Type        = Type;
        Info.DisplayName = FText::FromString(Name);
        Info.Charges     = GetCurrentCharges(Type);
        Info.MaxCharges  = GetMaxCharges(Type);
        Result.Add(Info);
    };

    // NOTE: Display names are placeholders until you approve final names.
    Make(EProtocolType::Healing, TEXT("Healing Protocol"));
    Make(EProtocolType::Revival, TEXT("Revival Protocol"));
    Make(EProtocolType::AP,      TEXT("AP Protocol"));

    return Result;
}
