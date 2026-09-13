#include <doctest.h>
#include "model/ModuleReplacement.h"
#include "undo/PatchActions.h"

// Replacing a module for another of its family keeps what can be carried over
// without guessing: cables by connector name (or where only one choice exists),
// parameters by name and range, and assignments on parameters that survive.
// Everything else is dropped and counted, never rewired somewhere plausible.

using ModuleReplacement::ConnectorRef;

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static const ModuleDescriptor& type(const char* name)
{
    auto* d = descriptions().getModuleByName(name);
    REQUIRE(d != nullptr);
    return *d;
}

static ConnectorRef ref(const ModuleDescriptor& d, const char* name, bool isOutput)
{
    for (auto& c : d.connectors)
        if (c.name == name && c.isOutput == isOutput)
            return { c.index, c.isOutput };
    FAIL("no connector " << name << " on " << d.name);
    return {};
}

static juce::String connectorName(const ModuleDescriptor& d, ConnectorRef r)
{
    for (auto& c : d.connectors)
        if (c.index == r.index && c.isOutput == r.isOutput)
            return c.name;
    return {};
}

static int paramIndex(const ModuleDescriptor& d, const char* name)
{
    for (auto& p : d.parameters)
        if (p.paramClass == "parameter" && p.name.equalsIgnoreCase(name))
            return p.index;
    FAIL("no parameter " << name << " on " << d.name);
    return -1;
}

TEST_CASE("a replacement keeps cables by connector name and drops the ones it would have to guess")
{
    const auto& oscA = type("OscA");
    const auto& oscB = type("OscB");
    const auto plan = ModuleReplacement::plan(oscA, oscB,
        { ref(oscA, "out", true), ref(oscA, "pitch 1", false), ref(oscA, "sync", false) });

    REQUIRE(plan.connectors.size() == 3);
    const auto out = plan.mapConnector(ref(oscA, "out", true));
    REQUIRE(out.has_value());
    CHECK(connectorName(oscB, *out) == "out");
    const auto pitch = plan.mapConnector(ref(oscA, "pitch 1", false));
    REQUIRE(pitch.has_value());
    CHECK(connectorName(oscB, *pitch) == "pitch 1");

    // OscB has no sync. Its only unnamed audio input left is PW mod, but OscA
    // has two unnamed audio inputs of its own (sync and pulse width mod), so
    // there is no telling which one PW mod stands for.
    CHECK_FALSE(plan.mapConnector(ref(oscA, "sync", false)).has_value());
    CHECK(plan.droppedConnectors() == 1);
}

TEST_CASE("a single output left on each side carries its cable across, several do not")
{
    const auto& filterD = type("FilterD");
    const auto& filterE = type("FilterE");

    const auto toE = ModuleReplacement::plan(filterD, filterE,
        { ref(filterD, "in", false), ref(filterD, "lp", true) });
    const auto in = toE.mapConnector(ref(filterD, "in", false));
    REQUIRE(in.has_value());
    CHECK(connectorName(filterE, *in) == "in");
    const auto lp = toE.mapConnector(ref(filterD, "lp", true));
    REQUIRE(lp.has_value());
    CHECK(connectorName(filterE, *lp) == "out");
    CHECK(toE.droppedConnectors() == 0);

    // Going the other way, FilterE's one out could be FilterD's hp, bp or lp.
    const auto toD = ModuleReplacement::plan(filterE, filterD, { ref(filterE, "out", true) });
    CHECK_FALSE(toD.mapConnector(ref(filterE, "out", true)).has_value());
}

TEST_CASE("parameters carry over only with the same name and the same range")
{
    const auto& filterD = type("FilterD");
    const auto& filterE = type("FilterE");
    const auto plan = ModuleReplacement::plan(filterD, filterE, {});

    CHECK(plan.mapParameter(paramIndex(filterD, "resonance")) == paramIndex(filterE, "resonance"));
    CHECK(plan.mapParameter(paramIndex(filterD, "kbt")) == paramIndex(filterE, "kbt"));
    // "freq" and "frequency" may well be the same knob, but that is a guess.
    CHECK_FALSE(plan.mapParameter(paramIndex(filterD, "freq")).has_value());
}

TEST_CASE("the replacement menu offers the module's own family and nothing else")
{
    const auto& filterE = type("FilterE");
    const auto list = ModuleReplacement::candidates(descriptions(), filterE);

    REQUIRE_FALSE(list.empty());
    bool hasFilterD = false;
    for (auto* d : list)
    {
        CHECK(d->category == filterE.category);
        CHECK(d->instantiable);
        CHECK(d->index != filterE.index);
        hasFilterD = hasFilterD || d->name == "FilterD";
    }
    CHECK(hasFilterD);
}

TEST_CASE("replacing a module keeps its place, cables, values and surviving assignments, and undoes whole")
{
    const auto& oscA = type("OscA");
    const auto& oscB = type("OscB");
    const auto& output = type("2Output");

    Patch patch;
    auto* osc = patch.createModule(1, oscA.index, 0, 0, "OscA", descriptions());
    auto* out = patch.createModule(1, output.index, 1, 0, "2Output", descriptions());
    REQUIRE(osc != nullptr);
    REQUIRE(out != nullptr);
    const int oscIndex = osc->getContainerIndex();
    auto& area = patch.getPolyVoiceArea();

    const auto oscOut = ref(oscA, "out", true);
    const auto left = ref(output, "out left", false);
    area.addConnection(osc->getConnector(oscOut.index, true), out->getConnector(left.index, false));

    const int coarseA = paramIndex(oscA, "freq coarse");
    const int widthA = paramIndex(oscA, "Pulse width");
    const int muteA = paramIndex(oscA, "Mute");
    osc->getParameter(coarseA)->setValue(50);
    patch.knobAssignments[3] = { true, 1, oscIndex, coarseA };
    patch.morphAssignments.push_back({ 1, oscIndex, widthA, 1, 30 });
    osc->getParameter(widthA)->setMorphGroup(1);
    osc->getParameter(widthA)->setMorphRange(30);
    patch.ctrlAssignments.push_back({ 20, 1, oscIndex, muteA });

    ConnectionManager connection;   // never connected, so the action sends nothing
    std::unique_ptr<PatchSynchronizer> sync;
    UndoContext ctx { patch, connection, sync, descriptions(),
                      [] {}, [] {}, nullptr, nullptr, nullptr, 0 };
    juce::UndoManager undo;

    undo.beginNewTransaction("replace");
    auto* action = new ReplaceModuleAction(ctx, 1, osc, oscB.index);
    CHECK(action->getDroppedCables() == 0);
    CHECK(action->getKeptCables() == 1);
    // The morph sits on Pulse width, which OscB calls PWidth.
    CHECK(action->getDroppedAssignments() == 1);
    REQUIRE(undo.perform(action));

    auto* replaced = area.getModuleByIndex(oscIndex);
    REQUIRE(replaced != nullptr);
    CHECK(replaced->getDescriptor()->name == "OscB");
    CHECK(replaced->getTitle() == "OscB");
    CHECK(replaced->getPosition() == juce::Point<int>(0, 0));
    CHECK(replaced->getParameter(paramIndex(oscB, "Freq coarse"))->getValue() == 50);
    CHECK(area.getConnections().size() == 1);
    CHECK(patch.knobAssignments[3].assigned);
    CHECK(patch.knobAssignments[3].module == oscIndex);
    CHECK(patch.knobAssignments[3].param == paramIndex(oscB, "Freq coarse"));
    CHECK(patch.morphAssignments.empty());
    REQUIRE(patch.ctrlAssignments.size() == 1);
    CHECK(patch.ctrlAssignments[0].param == paramIndex(oscB, "mute"));

    REQUIRE(undo.undo());
    auto* restored = area.getModuleByIndex(oscIndex);
    REQUIRE(restored != nullptr);
    CHECK(restored->getDescriptor()->name == "OscA");
    CHECK(restored->getParameter(coarseA)->getValue() == 50);
    CHECK(area.getConnections().size() == 1);
    CHECK(patch.knobAssignments[3].param == coarseA);
    REQUIRE(patch.morphAssignments.size() == 1);
    CHECK(restored->getParameter(widthA)->getMorphGroup() == 1);
    CHECK(restored->getParameter(widthA)->getMorphRange() == 30);
    REQUIRE(patch.ctrlAssignments.size() == 1);
    CHECK(patch.ctrlAssignments[0].param == muteA);

    REQUIRE(undo.redo());
    REQUIRE(area.getModuleByIndex(oscIndex) != nullptr);
    CHECK(area.getModuleByIndex(oscIndex)->getDescriptor()->name == "OscB");
    CHECK(area.getConnections().size() == 1);
}

TEST_CASE("a module the user named keeps its name through a replacement")
{
    Patch patch;
    auto* osc = patch.createModule(1, type("OscA").index, 0, 0, "Bass", descriptions());
    REQUIRE(osc != nullptr);
    const int index = osc->getContainerIndex();

    ConnectionManager connection;
    std::unique_ptr<PatchSynchronizer> sync;
    UndoContext ctx { patch, connection, sync, descriptions(),
                      [] {}, [] {}, nullptr, nullptr, nullptr, 0 };
    juce::UndoManager undo;
    undo.beginNewTransaction("replace");
    REQUIRE(undo.perform(new ReplaceModuleAction(ctx, 1, osc, type("OscB").index)));
    CHECK(patch.getPolyVoiceArea().getModuleByIndex(index)->getTitle() == "Bass");
}
