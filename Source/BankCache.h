#pragma once

// Process-wide built-in bank. Synthesising the bank takes about a second in a
// Release build, so it is built once per process on a background thread,
// shared by every plugin instance, and written to ~/Library/Caches so later
// launches only read it back. A user bank file (bank.m1bk in the preset
// folder) is applied on top, slot by slot.

#include "Bank.h"
#include <JuceHeader.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace m1
{

// Bump whenever BankBuilder.h changes what it renders.
inline constexpr int kBuiltinBankVersion = 2;

class BankCache
{
public:
    static BankCache& instance()
    {
        static BankCache cache;
        return cache;
    }

    void start()
    {
        std::call_once (once, [this] { worker = std::thread ([this] { build(); }); });
    }

    // Never blocks. Null until the bank is ready.
    const Bank* get() const { return ready.load (std::memory_order_acquire); }

    // Blocks up to `ms` milliseconds. Not for the audio thread.
    const Bank* wait (int ms)
    {
        start();
        std::unique_lock<std::mutex> lock (mutex);
        cv.wait_for (lock, std::chrono::milliseconds (ms), [this] { return get() != nullptr; });
        return get();
    }

    static juce::File presetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Audio/Presets/Alistair Rutherford/M1 ARP");
    }

    ~BankCache()
    {
        if (worker.joinable())
            worker.join();
    }

private:
    BankCache() = default;

    void build()
    {
        auto bank = std::make_unique<Bank>();
        const auto cacheDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                  .getChildFile ("Library/Caches/com.alistairrutherford.m1arp");
        const auto cacheFile = cacheDir.getChildFile ("builtin-v" + juce::String (kBuiltinBankVersion) + ".m1bk");

        bool loaded = false;
        if (cacheFile.existsAsFile())
        {
            loaded = bank->loadFile (cacheFile.getFullPathName().toStdString());
            for (int i = 0; loaded && i < kMultisounds; ++i)
                loaded = ! bank->multi[i].zones.empty();
        }
        if (! loaded)
        {
            *bank = Bank::builtin();
            if (cacheDir.createDirectory())
            {
                for (const auto& old : cacheDir.findChildFiles (juce::File::findFiles, false, "builtin-v*.m1bk"))
                    old.deleteFile();
                const auto tmp = cacheFile.getSiblingFile (cacheFile.getFileName() + ".tmp");
                if (bank->writeFile (tmp.getFullPathName().toStdString()))
                    tmp.moveFileTo (cacheFile);
                else
                    tmp.deleteFile();
            }
        }

        const auto userBank = presetDirectory().getChildFile ("bank.m1bk");
        if (userBank.existsAsFile())
            bank->loadFile (userBank.getFullPathName().toStdString());

        owned = std::move (bank);
        {
            std::lock_guard<std::mutex> lock (mutex);
            ready.store (owned.get(), std::memory_order_release);
        }
        cv.notify_all();
    }

    std::once_flag once;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable cv;
    std::unique_ptr<Bank> owned;
    std::atomic<const Bank*> ready { nullptr };
};

} // namespace m1
