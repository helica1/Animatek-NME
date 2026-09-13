#include "ModuleReplacement.h"
#include <algorithm>

namespace ModuleReplacement
{
namespace
{
    const ConnectorDescriptor* findConnector (const ModuleDescriptor& d, ConnectorRef ref)
    {
        for (auto& c : d.connectors)
            if (c.index == ref.index && c.isOutput == ref.isOutput)
                return &c;
        return nullptr;
    }

    bool hasNamesake (const ModuleDescriptor& d, const ConnectorDescriptor& c)
    {
        for (auto& other : d.connectors)
            if (other.isOutput == c.isOutput && other.name.equalsIgnoreCase (c.name))
                return true;
        return false;
    }

    bool sameKind (const ConnectorDescriptor& a, const ConnectorDescriptor& b)
    {
        return a.isOutput == b.isOutput && a.signalType == b.signalType;
    }
}

std::optional<ConnectorRef> Plan::mapConnector (ConnectorRef from) const
{
    for (auto& match : connectors)
        if (match.from == from)
            return match.to;
    return std::nullopt;
}

std::optional<int> Plan::mapParameter (int fromIndex) const
{
    for (auto& [oldIndex, newIndex] : parameters)
        if (oldIndex == fromIndex)
            return newIndex;
    return std::nullopt;
}

int Plan::droppedConnectors() const
{
    return static_cast<int> (std::count_if (connectors.begin(), connectors.end(),
                                            [] (const ConnectorMatch& m) { return ! m.to.has_value(); }));
}

Plan plan (const ModuleDescriptor& from, const ModuleDescriptor& to,
           const std::vector<ConnectorRef>& used)
{
    // The used connectors in descriptor order, once each, so the plan does not
    // depend on the order the cables happened to be found in.
    std::vector<const ConnectorDescriptor*> usedFrom;
    for (auto& c : from.connectors)
        if (std::find (used.begin(), used.end(), ConnectorRef { c.index, c.isOutput }) != used.end())
            usedFrom.push_back (&c);

    std::vector<const ConnectorDescriptor*> chosen (usedFrom.size(), nullptr);
    std::vector<const ConnectorDescriptor*> taken;
    const auto isTaken = [&taken] (const ConnectorDescriptor* c)
    {
        return std::find (taken.begin(), taken.end(), c) != taken.end();
    };

    // 1. Same name, same direction.
    for (size_t i = 0; i < usedFrom.size(); ++i)
        for (auto& c : to.connectors)
            if (c.isOutput == usedFrom[i]->isOutput && ! isTaken (&c)
                && c.name.equalsIgnoreCase (usedFrom[i]->name))
            {
                chosen[i] = &c;
                taken.push_back (&c);
                break;
            }

    // 2. No name to go by: only where there is nothing to choose between.
    for (size_t i = 0; i < usedFrom.size(); ++i)
    {
        if (chosen[i] != nullptr)
            continue;
        const auto& old = *usedFrom[i];

        std::vector<const ConnectorDescriptor*> newPool;
        for (auto& c : to.connectors)
            if (sameKind (c, old) && ! isTaken (&c) && ! hasNamesake (from, c))
                newPool.push_back (&c);

        int oldPool = 0;
        if (old.isOutput)
        {
            for (size_t j = 0; j < usedFrom.size(); ++j)
                if (chosen[j] == nullptr && sameKind (*usedFrom[j], old))
                    ++oldPool;
        }
        else
        {
            for (auto& c : from.connectors)
                if (sameKind (c, old) && ! hasNamesake (to, c))
                    ++oldPool;
        }

        if (newPool.size() == 1 && oldPool == 1)
        {
            chosen[i] = newPool.front();
            taken.push_back (newPool.front());
        }
    }

    Plan result;
    for (size_t i = 0; i < usedFrom.size(); ++i)
    {
        ConnectorMatch match;
        match.from = { usedFrom[i]->index, usedFrom[i]->isOutput };
        if (chosen[i] != nullptr)
            match.to = ConnectorRef { chosen[i]->index, chosen[i]->isOutput };
        result.connectors.push_back (match);
    }

    for (auto& oldParam : from.parameters)
    {
        if (oldParam.paramClass != "parameter")
            continue;
        for (auto& newParam : to.parameters)
            if (newParam.paramClass == "parameter" && newParam.name.equalsIgnoreCase (oldParam.name)
                && newParam.minValue == oldParam.minValue && newParam.maxValue == oldParam.maxValue)
            {
                result.parameters.push_back ({ oldParam.index, newParam.index });
                break;
            }
    }

    return result;
}

std::vector<const ModuleDescriptor*> candidates (const ModuleDescriptions& descs,
                                                 const ModuleDescriptor& current)
{
    std::vector<const ModuleDescriptor*> result;
    if (current.category.isEmpty())
        return result;

    for (auto& d : descs.getAllModules())
        if (d.index != current.index && d.instantiable && d.category == current.category)
            result.push_back (&d);

    std::sort (result.begin(), result.end(), [] (const ModuleDescriptor* a, const ModuleDescriptor* b)
    {
        return a->fullname.compareNatural (b->fullname) < 0;
    });
    return result;
}
}
