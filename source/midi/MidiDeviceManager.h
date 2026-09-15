#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "NmProtocol.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// A synth that lives in the same process as the editor. A host that embeds the editor
// (the nmemu Nord Micro Modular emulator plugin does) registers one; it then shows up in
// the device lists under its own name and the SysEx traffic goes straight to it instead of
// through the operating system's MIDI ports.
class EmbeddedSynth
{
public:
    virtual ~EmbeddedSynth() = default;

    virtual juce::String getName() const = 0;

    // editor -> synth, a complete SysEx frame (F0 ... F7)
    virtual void sendToSynth(const std::vector<uint8_t>& data) = 0;

    // synth -> editor: the synth calls the receiver with every message it emits, from any
    // thread; a null receiver disconnects
    virtual void setReceiver(std::function<void(const juce::MidiMessage&)> receiver) = 0;

    // the identifier the device lists use for the embedded synth
    static const char* deviceId() { return "embedded-synth"; }
};

class MidiDeviceManager : private juce::MidiInputCallback
{
public:
    MidiDeviceManager(NmProtocol& protocol);
    ~MidiDeviceManager() override;

    // Register (or clear with nullptr) the synth embedded in this process. Not owned.
    static void setEmbeddedSynth(EmbeddedSynth* synth);
    static EmbeddedSynth* getEmbeddedSynth();

    // Available MIDI devices
    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();
    static juce::Array<juce::MidiDeviceInfo> getAvailableOutputDevices();

    // Connect to MIDI ports by identifier
    bool connect(const juce::String& inputId, const juce::String& outputId);
    void disconnect();

    bool isConnected() const { return (midiInput != nullptr && midiOutput != nullptr) || embedded != nullptr; }

    // Send raw SysEx data
    void sendSysEx(const std::vector<uint8_t>& data);

    juce::String getInputDeviceName() const;
    juce::String getOutputDeviceName() const;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    NmProtocol& protocol;
    EmbeddedSynth* embedded = nullptr;   // connected to the embedded synth instead of MIDI ports
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiDeviceManager)
};
