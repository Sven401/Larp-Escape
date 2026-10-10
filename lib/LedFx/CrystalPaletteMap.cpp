#include "CrystalPaletteMap.h"

#include <Pallets.h>

namespace larp
{
namespace led
{
const TProgmemRGBPalette16 *paletteForCrystal(domain::RoleId role, domain::GridPosition position)
{
    using domain::GridColumn;
    using domain::GridPosition;
    using domain::RoleId;

    switch (role)
    {
    case RoleId::Puppenspielerin:
        return position == GridPosition{4, GridColumn::E} ? &Purple_p : nullptr;
    case RoleId::Krieger:
        return position == GridPosition{6, GridColumn::B} ? &Red_p : nullptr;
    case RoleId::Narr:
        return position == GridPosition{9, GridColumn::B} ? &Orange_p : nullptr;
    case RoleId::Abenteurerin:
        return position == GridPosition{2, GridColumn::E} ? &Green_p : nullptr;
    case RoleId::Ueberlebende:
        return position == GridPosition{7, GridColumn::G} ? &White_p : nullptr;
    case RoleId::Wissende:
        return position == GridPosition{2, GridColumn::G} ? &Blue_p : nullptr;
    }
    return nullptr;
}
} // namespace led
} // namespace larp
