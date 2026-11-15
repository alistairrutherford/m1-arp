#pragma once

#include "BankCache.h"
#include "Engine.h"
#include "Factory.h"
#include "State.h"
#include <atomic>
#include <memory>

class M1ArpAudioProcessor : public juce::AudioProcessor
{
public:
    M1ArpAudioProcessor();
    ~M1ArpAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tailSeconds.load(); }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardState clickState;

    m1::Session& session() { return edit; }
    const m1::Session& session() const { return edit; }
    void publishSession();
    void setCombiMode (bool on);
    bool isCombiMode() const { return edit.combiMode != 0; }
    void setCurrentCombi (int index);
    int getCurrentCombi() const { return edit.currentCombi; }
    void savePreset (const juce::File& file);
    void loadPreset (const juce::File& file);
    int structureRevision() const { return revision.load(); }
    bool bankReady() const { return m1::BankCache::instance().get() != nullptr; }
    float outputPeak() { return peakMeter.exchange (0.0f); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void publish();
    void snapshot (m1::Session& dst) const;
    m1::Performance readPerformance() const;
    void pullPerformanceIntoSession();
    void pushPerformanceFrom (const m1::ArpSettings& arp);
    void refreshTail (const m1::Session& s);
    float paramFloat (const char* id) const;

    m1::Engine engine;
    m1::Session edit;
    m1::Session published;
    m1::Session renderSession;
    mutable std::atomic<uint32_t> seq { 0 };
    std::atomic<const m1::Bank*> bankPtr { nullptr };
    float masterGain = 1.0f;
    std::atomic<bool> resetRequested { false };
    std::atomic<float> tailSeconds { 0.1f };
    std::atomic<int> revision { 0 };
    std::atomic<float> peakMeter { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (M1ArpAudioProcessor)
};
