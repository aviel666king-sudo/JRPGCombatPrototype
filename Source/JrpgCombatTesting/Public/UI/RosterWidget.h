#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "RosterWidget.generated.h"

class URosterSubsystem;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UWidgetSwitcher;
class UBorder;
struct FPartyMemberRecord;
enum class EPartyAssignment : uint8;

/** Which equipment slot a switch action targets. */
UENUM()
enum class ESwitchSlot : uint8
{
    MainWeapon,
    Gun,
    Armor,
    Chip,
    ArmorChip,   // the chip socketed into the equipped armor (camp-only re-bind)
};

/** What a roster button does when clicked. */
UENUM()
enum class ERosterAction : uint8
{
    AssignParty1,
    AssignParty2,
    AssignBench,
    OpenDetails,
    BackToRoster,
    OpenSwitch,     // open the switch list for SwitchSlot / SlotIndex
    EquipItem,      // equip Payload into the current switch slot
    UpgradeSlot,    // upgrade the item in SwitchSlot / SlotIndex (camp only)
    BackToDetail,
    Close,
};

/**
 * URosterActionButton
 *
 * Button carrying everything one handler needs to route a click: which member,
 * what action, (for switching) which slot + chip index + the item payload.
 */
UCLASS()
class JRPGCOMBATTESTING_API URosterActionButton : public UButton
{
    GENERATED_BODY()

public:
    int32 MemberIndex = -1;
    ERosterAction Action = ERosterAction::Close;
    ESwitchSlot SwitchSlot = ESwitchSlot::MainWeapon;
    int32 SlotIndex = -1;                  // chip slot when SwitchSlot == Chip
    TWeakObjectPtr<UObject> Payload;       // item to equip for EquipItem

    TFunction<void(URosterActionButton*)> OnActionClicked;

    UFUNCTION()
    void HandleClicked();
};

/**
 * URosterWidget
 *
 * Full-screen party-management screen (Tab). Reads persistent party records
 * from URosterSubsystem so it works in every level. Three pages:
 *   0 Roster  — cards + party assignment + potion footer
 *   1 Detail  — stats + loadout (each slot has a [Change] button) + passives
 *   2 Switch  — list of owned items for the chosen slot; click to equip
 */
UCLASS()
class JRPGCOMBATTESTING_API URosterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    TFunction<void()> OnCloseRequested;

    /** When true (opened from the camp checkpoint), the detail page shows
     *  per-slot Upgrade buttons + the gold / material balance. Set by the pawn
     *  before AddToViewport. */
    bool bUpgradeMode = false;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }
    virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;

private:
    TObjectPtr<UWidgetSwitcher> Switcher;
    TObjectPtr<UVerticalBox>    RosterList;
    TObjectPtr<UVerticalBox>    DetailBox;
    TObjectPtr<UVerticalBox>    SwitchBox;
    TObjectPtr<UTextBlock>      PartyCountText;

    int32 CurrentMemberIndex = -1;
    ESwitchSlot CurrentSwitchSlot = ESwitchSlot::MainWeapon;
    int32 CurrentChipSlot = -1;

    URosterSubsystem* GetRoster() const;

    void RefreshRoster();
    void ShowDetail(int32 MemberIndex);
    void ShowSwitch(ESwitchSlot Slot, int32 ChipSlot);
    void HandleButton(URosterActionButton* Btn);

    UBorder* BuildCharacterCard(int32 MemberIndex);

    void AddDetailHeader(const FPartyMemberRecord& Rec);
    void AddStatsBlock(const FPartyMemberRecord& Rec);
    void AddLoadoutBlock(const FPartyMemberRecord& Rec);

    /** Build one loadout row: "Label: Value   [Change] [Up]". If bChangeCampOnly,
     *  the Change button only appears in camp upgrade mode. */
    void AddLoadoutRow(const FString& Label, const FString& Value,
                       ESwitchSlot Slot, int32 ChipSlot, bool bChangeCampOnly = false);

    URosterActionButton* MakeButton(const TCHAR* Label, ERosterAction Action,
                                    const struct FLinearColor& BG);

    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
    static FText AssignmentLabel(EPartyAssignment Assignment);
    static const TCHAR* WeaponTierName(uint8 Tier);
    static const TCHAR* BuffedStatName(uint8 Stat);
};
