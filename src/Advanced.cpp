#include "Advanced.h"

#include <nlohmann/json.hpp>

namespace Advanced
{
    using MacroTree::Node;

    namespace
    {
        void CountIn(const Node& aFolder, InUse& aOut)
        {
            for (const auto& c : aFolder.Children)
            {
                if (c->IsFolder())
                {
                    CountIn(*c, aOut);
                    continue;
                }
                if (Blocks(*c)) aOut.Linked++;
                if (HasCooldown(*c)) aOut.Cooldowns++;
            }
        }
    }

    bool Blocks(const Node& aNode) { return aNode.IsMacro() && !aNode.LinkFile.empty(); }
    bool HasCooldown(const Node& aNode) { return aNode.IsMacro() && aNode.CooldownSec > 0; }
    bool Uses(const Node& aNode) { return Blocks(aNode) || HasCooldown(aNode); }

    bool Blocked(const Node& aNode, bool aAdvancedOn) { return !aAdvancedOn && Blocks(aNode); }
    int CooldownSec(const Node& aNode, bool aAdvancedOn) { return aAdvancedOn ? aNode.CooldownSec : 0; }

    InUse Count(const MacroTree::Tree& aTree)
    {
        InUse n;
        CountIn(aTree.Root(), n);
        return n;
    }

    bool AnyUses(const MacroTree::Tree& aTree) { return Count(aTree).Any(); }

    bool FromSettings(const nlohmann::json& aSettings)
    {
        if (!aSettings.is_object()) return false;
        const auto it = aSettings.find(kSetting);
        return it != aSettings.end() && it->is_boolean() && it->get<bool>();
    }
}
