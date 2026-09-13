#include <doctest.h>
#include "mcp/McpRules.h"

// An MCP client names knobs in text. Getting a name wrong must fail loudly,
// never land on a neighbouring knob.

TEST_CASE("knobs are found by their panel names, loosely spelled")
{
    CHECK(McpRules::knobFromName("Knob 1") == 0);
    CHECK(McpRules::knobFromName("knob7") == 6);
    CHECK(McpRules::knobFromName("KNOB 18") == 17);
    CHECK(McpRules::knobFromName("Pedal") == 19);
    CHECK(McpRules::knobFromName("After touch") == 20);
    CHECK(McpRules::knobFromName("aftertouch") == 20);
    CHECK(McpRules::knobFromName("On/Off switch") == 22);
    CHECK(McpRules::knobFromName("on-off") == 22);

    // Every name the editor itself prints resolves back to its own index.
    for (int k = 0; k < KnobAssignmentMessage::numKnobs; ++k)
        if (KnobAssignmentMessage::isValidKnob(k))
            CHECK(McpRules::knobFromName(KnobAssignmentMessage::getKnobName(k)) == k);
}

TEST_CASE("ambiguous or impossible knob names are refused")
{
    CHECK_FALSE(McpRules::knobFromName("7").has_value());       // Knob 7, or index 7?
    CHECK_FALSE(McpRules::knobFromName("Knob 0").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob 19").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob 7a").has_value());
    CHECK_FALSE(McpRules::knobFromName("(unused)").has_value());
    CHECK_FALSE(McpRules::knobFromName("").has_value());
}

TEST_CASE("assignment ranges match what the editor's own menus offer")
{
    CHECK(McpRules::isValidMorphGroup(0));
    CHECK(McpRules::isValidMorphGroup(3));
    CHECK_FALSE(McpRules::isValidMorphGroup(4));
    CHECK_FALSE(McpRules::isValidMorphGroup(-1));

    CHECK(McpRules::isValidMorphRange(-127));
    CHECK(McpRules::isValidMorphRange(127));
    CHECK_FALSE(McpRules::isValidMorphRange(128));

    CHECK(McpRules::isValidMidiCc(0));
    CHECK(McpRules::isValidMidiCc(119));
    CHECK_FALSE(McpRules::isValidMidiCc(120));
}
