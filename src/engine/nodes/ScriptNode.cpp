/*
    This file is part of Element
    Copyright (C) 2020  Kushview, LLC.  All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "amp.lua.h"
#include "ampui.lua.h"

#include <math.h>
#include "sol/sol.hpp"
#include "element/element.h"
#include "el/factories.hpp"

#include "ElementApp.h"
#include "engine/nodes/ScriptNode.h"
#include "engine/midipipe.hpp"
#include "engine/parameter.hpp"
#include "scripting/bindings.hpp"
#include "scripting/dspscript.hpp"
#include "scripting/script.hpp"
#include "scripting/scriptmanager.hpp"

#define EL_LUA_DBG(x)
// #define EL_LUA_DBG(x) DBG(x)

static const String initScript =
    R"(
require ('el.AudioBuffer')
require ('el.MidiBuffer')
require ('el.MidiMessage')
require ('el.midi')
require ('el.audio')
require ('el.MidiPipe')
)";

namespace element {

//=============================================================================
ScriptNode::ScriptNode() noexcept
    : NodeObject (0)
{
    Lua::initializeState (lua);
    script.reset (new DSPScript (lua.create_table()));
    dspCode.replaceAllContent (String::fromUTF8 (
        scripts::amp_lua, scripts::amp_luaSize));
    loadScript (dspCode.getAllContent());
    edCode.replaceAllContent (String::fromUTF8 (
        scripts::ampui_lua, scripts::ampui_luaSize));
    refreshPorts();
}

ScriptNode::~ScriptNode()
{
    script.reset();
}

void ScriptNode::refreshPorts()
{
    if (script == nullptr)
        return;
    PortList newPorts;
    script->getPorts (newPorts);
    setPorts (newPorts);
}

Parameter::Ptr ScriptNode::getParameter (const PortDescription& port)
{
    jassert (port.type == PortType::Control);
    return script ? script->getParameterObject (port.channel, port.input) : nullptr;
}

Result ScriptNode::loadScript (const String& newCode)
{
    auto result = DSPScript::validate (newCode);
    if (result.failed())
        return result;

    Script loader (lua);
    loader.load (newCode);
    if (loader.hasError())
        return Result::fail (loader.getErrorMessage());

    auto dsp = loader();
    if (! dsp.valid() || dsp.get_type() != sol::type::table)
        return Result::fail ("Could not instantiate script");

    auto newScript = std::make_unique<DSPScript> (dsp);

    if (true)
    {
        if (prepared)
            newScript->prepare (sampleRate, blockSize);
        triggerPortReset();
        ScopedLock sl (lock);
        if (script != nullptr)
            newScript->copyParameterValues (*script);
        script.swap (newScript);
    }

    if (newScript != nullptr)
    {
        newScript->release();
        newScript->cleanup();
        newScript.reset();
    }

    return Result::ok();
}

void ScriptNode::getPluginDescription (PluginDescription& desc) const
{
    desc.name = "Script";
    desc.fileOrIdentifier = EL_INTERNAL_ID_SCRIPT;
    desc.uniqueId = EL_INTERNAL_UID_SCRIPT;
    desc.descriptiveName = "A user scriptable Element node";
    desc.numInputChannels = 0;
    desc.numOutputChannels = 0;
    desc.hasSharedContainer = false;
    desc.isInstrument = false;
    desc.manufacturerName = "Element";
    desc.pluginFormatName = EL_INTERNAL_FORMAT_NAME;
    desc.version = "1.0.0";
}

void ScriptNode::prepareToRender (double rate, int block)
{
    if (prepared)
        return;
    sampleRate = rate;
    blockSize = block;
    script->prepare (sampleRate, blockSize);
    prepared = true;
}

void ScriptNode::releaseResources()
{
    if (! prepared)
        return;
    prepared = false;
    script->release();
}

void ScriptNode::render (AudioSampleBuffer& audio, MidiPipe& midi)
{
    ScopedLock sl (lock);
    script->process (audio, midi);
}

void ScriptNode::setState (const void* data, int size)
{
    const auto state = ValueTree::readFromGZIPData (data, size);
    if (state.isValid())
    {
        dspCode.replaceAllContent (state["dspCode"].toString());
        edCode.replaceAllContent (state["editorCode"].toString());

        auto result = loadScript (dspCode.getAllContent());

        if (result.wasOk())
        {
            if (state.hasProperty ("data"))
            {
                const var& data = state.getProperty ("data");
                if (data.isBinaryData())
                    if (auto* block = data.getBinaryData())
                        script->restore (block->getData(), block->getSize());
            }
        }

        sendChangeMessage();
    }
}

void ScriptNode::getState (MemoryBlock& out)
{
    ValueTree state ("ScriptNode");
    state.setProperty ("dspCode", dspCode.getAllContent(), nullptr)
        .setProperty ("editorCode", edCode.getAllContent(), nullptr);

    MemoryBlock block;
    script->save (block);
    if (block.getSize() > 0)
        state.setProperty ("data", block, nullptr);
    block.reset();

    MemoryOutputStream mo (out, false);
    {
        GZIPCompressorOutputStream gz (mo);
        state.writeToStream (gz);
    }
}

void ScriptNode::setParameter (int index, float value)
{
    ScopedLock sl (lock);
}

} // namespace element
