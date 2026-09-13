#pragma once

#include "ModuleDescriptions.h"
#include <optional>
#include <utility>
#include <vector>

// Swapping a module for another of its family (a FilterE for a FilterD, say)
// without deleting it, adding the new one and wiring it again by hand. What
// carries over is decided here, apart from the undo action, so it can be tested
// against the real module descriptions.
//
// The rule throughout is to drop rather than guess. A cable left unplugged is
// visible and costs one drag to fix; a cable quietly moved from a sync input to
// an FM input changes the sound and nobody knows why.
namespace ModuleReplacement
{
    struct ConnectorRef
    {
        int index = 0;
        bool isOutput = false;

        bool operator== (const ConnectorRef& other) const
        {
            return index == other.index && isOutput == other.isOutput;
        }
    };

    struct ConnectorMatch
    {
        ConnectorRef from;
        std::optional<ConnectorRef> to;   // empty: the cables on it are dropped
    };

    struct Plan
    {
        // One entry per connector of the old module that has a cable on it.
        std::vector<ConnectorMatch> connectors;
        // Old parameter index -> new parameter index, for "parameter"-class
        // entries with the same name and the same range.
        std::vector<std::pair<int, int>> parameters;

        std::optional<ConnectorRef> mapConnector (ConnectorRef from) const;
        std::optional<int> mapParameter (int fromIndex) const;
        int droppedConnectors() const;
    };

    // `used` names the old module's connectors that carry cables; duplicates
    // are fine.
    //
    // A connector keeps its cables when the new module has one with the same
    // name and direction. Failing that, an output keeps them when it is the only
    // used, unmatched output of its signal type and the new module has exactly
    // one unmatched output of that type left: one output has no second meaning
    // (hp, bp or lp going into a filter's single out). An input needs the same on
    // both sides counting every input, used or not, because inputs do mean
    // different things: an OscA's sync must not become an OscB's FM input.
    Plan plan (const ModuleDescriptor& from, const ModuleDescriptor& to,
               const std::vector<ConnectorRef>& used);

    // The modules `current` can be replaced by: its own family (the descriptor's
    // category), instantiable, not itself, sorted by full name.
    std::vector<const ModuleDescriptor*> candidates (const ModuleDescriptions& descs,
                                                     const ModuleDescriptor& current);
}
