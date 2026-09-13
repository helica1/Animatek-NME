#include <doctest.h>
#include "undo/PatchActions.h"

// A morph assignment is held twice: in the patch's morphAssignments list, which
// is what gets saved and sent, and in each Parameter's own group and range,
// which is what the canvas and the inspector draw. The canvas copies one into
// the other only when a patch is set, so the undo actions have to keep them in
// step themselves: otherwise undoing a morph leaves it drawn on the knob, and
// an edit made without touching the parameter first (the MCP bridge) is never
// drawn at all.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

TEST_CASE("morph assign, range change and their undo keep the parameter's own fields in step")
{
    Patch patch;
    const auto* desc = descriptions().getModuleByName("Constant");
    REQUIRE(desc != nullptr);
    auto* module = patch.createModule(1, desc->index, 0, 0, "Constant", descriptions());
    REQUIRE(module != nullptr);
    const int index = module->getContainerIndex();

    Parameter* param = nullptr;
    for (auto& candidate : module->getParameters())
        if (candidate.getDescriptor()->paramClass == "parameter")
        {
            param = &candidate;
            break;
        }
    REQUIRE(param != nullptr);
    const int paramId = param->getDescriptor()->index;

    ConnectionManager connection;   // never connected, so the actions send nothing
    std::unique_ptr<PatchSynchronizer> sync;
    UndoContext ctx { patch, connection, sync, descriptions(),
                      [] {}, [] {}, nullptr, nullptr, nullptr, 0 };
    juce::UndoManager undo;

    undo.beginNewTransaction("assign");
    REQUIRE(undo.perform(new MorphAssignAction(ctx, 1, index, paramId, 2, -1, 0)));
    CHECK(param->getMorphGroup() == 2);
    CHECK(param->getMorphRange() == 0);
    REQUIRE(patch.morphAssignments.size() == 1);
    CHECK(patch.morphAssignments[0].morph == 2);

    undo.beginNewTransaction("range");
    REQUIRE(undo.perform(new MorphRangeChangeAction(ctx, 1, index, paramId, 0, -40)));
    CHECK(param->getMorphRange() == -40);
    CHECK(patch.morphAssignments[0].range == -40);

    REQUIRE(undo.undo());
    CHECK(param->getMorphRange() == 0);
    CHECK(patch.morphAssignments[0].range == 0);

    REQUIRE(undo.undo());
    CHECK(param->getMorphGroup() == -1);
    CHECK(param->getMorphRange() == 0);
    CHECK(patch.morphAssignments.empty());

    REQUIRE(undo.redo());
    REQUIRE(undo.redo());
    CHECK(param->getMorphGroup() == 2);
    CHECK(param->getMorphRange() == -40);
    REQUIRE(patch.morphAssignments.size() == 1);
    CHECK(patch.morphAssignments[0].range == -40);
}
