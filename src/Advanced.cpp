#include "Advanced.h"

#include <nlohmann/json.hpp>

namespace Advanced
{
    using MacroTree::Node;

    namespace
    {
        bool AnyIn(const Node& aFolder)
        {
            for (const auto& c : aFolder.Children)
                if (c->IsFolder() ? AnyIn(*c) : Uses(*c)) return true;
            return false;
        }
    }

    bool Uses(const Node& aNode) { return aNode.IsMacro() && !aNode.LinkFile.empty(); }

    bool Blocked(const Node& aNode, bool aAdvancedOn) { return !aAdvancedOn && Uses(aNode); }

    bool AnyUses(const MacroTree::Tree& aTree) { return AnyIn(aTree.Root()); }

    bool FromSettings(const nlohmann::json& aSettings, const MacroTree::Tree& aTree)
    {
        if (aSettings.is_object())
        {
            const auto it = aSettings.find(kSetting);
            if (it != aSettings.end() && it->is_boolean()) return it->get<bool>();
        }
        return AnyUses(aTree);
    }
}
