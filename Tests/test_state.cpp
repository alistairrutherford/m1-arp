#include "State.h"
#include "Sysex.h"
#include "check.h"
#include <cstring>

int main()
{
    std::cout << "State\n";
    m1::Session session;
    session.numPrograms = 1;
    session.currentProgram = 0;
    m1::copyName (session.programs[0].name, "Round Trip");
    session.programs[0].cutoff1 = 42;
    session.programs[0].detune = -12;
    session.programs[0].arp.on = 1;
    session.programs[0].arp.mode = (int) m1::ArpMode::Pattern;
    session.programs[0].arp.steps[3].on = 0;
    session.programs[0].arp.steps[3].octave = -2;
    session.programs[0].arp.steps[3].velocity = 41;
    session.numCombis = 1;
    session.combiMode = 1;
    m1::copyName (session.combis[0].name, "Layer Pad");
    session.combis[0].type = (int) m1::CombiType::Layer;
    session.combis[0].splitKey = 55;
    session.combis[0].timbre[1].program = 3;
    session.combis[0].timbre[1].channel = 4;
    session.combis[0].arp.swing = 62;

    auto xml = m1::sessionToXml (session);
    check (xml != nullptr && xml->hasTagName ("M1ARP"), "state root is M1ARP");
    auto loaded = m1::sessionFromXml (*xml);
    check (std::strcmp (loaded.programs[0].name, "Round Trip") == 0, "program name round-trips");
    check (loaded.programs[0].cutoff1 == 42, "cutoff round-trips");
    check (loaded.programs[0].detune == -12, "detune round-trips");
    check (loaded.programs[0].arp.steps[3].on == 0, "arp step on round-trips");
    check (loaded.programs[0].arp.steps[3].octave == -2, "arp step octave round-trips");
    check (loaded.programs[0].arp.steps[3].velocity == 41, "arp step velocity round-trips");
    check (std::strcmp (loaded.combis[0].name, "Layer Pad") == 0, "combination name round-trips");
    check (loaded.combis[0].type == (int) m1::CombiType::Layer, "combination type round-trips");
    check (loaded.combis[0].timbre[1].program == 3, "timbre program round-trips");
    check (loaded.combis[0].timbre[1].channel == 4, "timbre channel round-trips");
    check (loaded.combis[0].arp.swing == 62, "combination swing round-trips");
    check (loaded.combiMode == 1, "combination mode round-trips");

    xml->removeAttribute ("calibration");
    auto missing = m1::sessionFromXml (*xml);
    check (missing.calibration == 1, "a missing calibration loads as version 1");
    check (missing.programs[0].cutoff1 == 42, "a missing calibration keeps the stored cutoff");

    xml->setAttribute ("calibration", 9);
    if (auto* programs = xml->getChildByName ("Programs"))
        if (auto* program = programs->getChildByName ("Program"))
        {
            program->setAttribute ("cutoff1", 55);
            program->createNewChildElement ("Future")->setAttribute ("x", 7);
        }
    auto newer = m1::sessionFromXml (*xml);
    check (newer.calibration == 9, "a newer calibration number is kept");
    check (newer.programs[0].cutoff1 == 55, "an unknown child does not drop the program");

    xml->setAttribute ("calibration", 0);
    auto nonPositive = m1::sessionFromXml (*xml);
    check (nonPositive.calibration == 1, "a non-positive calibration becomes 1");

    m1::Program program;
    m1::copyName (program.name, "Detune");
    program.detune = -17;
    program.osc1.multisound = 31;
    program.cutoff1 = 77;
    std::vector<uint8_t> dump;
    m1::writeProgramDump (program, dump);
    check (dump.size() > 8 && dump.front() == 0xf0 && dump.back() == 0xf7, "a dump is a SysEx frame");
    check (dump[1] == 0x42 && dump[3] == 0x19 && dump[4] == 0x40, "the dump header is the instrument subset");
    m1::Program read;
    check (m1::readProgramDump (dump.data(), (int) dump.size(), read), "the dump reads back");
    check (std::strcmp (read.name, "Detune") == 0, "SysEx keeps the name");
    check (read.detune == -17, "SysEx keeps the detune");
    check (read.osc1.multisound == 31, "SysEx keeps the multisound");
    check (read.cutoff1 == 77, "SysEx keeps the cutoff");

    return gFailures == 0 ? 0 : 1;
}
