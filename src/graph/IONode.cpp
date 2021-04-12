
#include "graph/IONode.h"
#include "graph/GraphNode.h"

namespace Element {
    
IONode::IONode (const IODeviceType type_)
    : NodeObject(0),
      type (type_),
      graph (nullptr)
{ }

IONode::~IONode()
{ }

const String IONode::getName() const
{
    // FIXME:
    // if (auto* const root = dynamic_cast<RootGraph*> (getParentGraph()))
    // {
    //     switch (type)
    //     {
    //         case audioOutputNode:   return root->getAudioOutputDeviceName(); break;
    //         case audioInputNode:    return root->getAudioInputDeviceName();  break;
    //         case midiOutputNode:    return "MIDI Out"; break;
    //         case midiInputNode:     return "MIDI In"; break;
    //         default:                break;
    //     }
    // }

    switch (type)
    {
        case audioOutputNode:   return "Audio Output"; break;
        case audioInputNode:    return "Audio Input"; break;
        case midiOutputNode:    return "Midi Output"; break;
        case midiInputNode:     return "Midi Input"; break;
        default:                break;
    }

    return String();
}

PortType IONode::getPortType() const noexcept
{
    switch (getType())
    {
        case audioInputNode:
        case audioOutputNode:
            return PortType::Audio;
            break;
        case midiInputNode:
        case midiOutputNode:
            return PortType::Midi;
            break;
        default:
            break;
    }
    return PortType::Unknown;
}

void IONode::refreshPorts()
{
    if (auto* const graph = getParentGraph())
    {
        PortCount count;
        count.set (getPortType(), 
            graph->getNumPorts (getPortType(), isInput()), 
            !isInput()
        );

        if (type == midiOutputNode)
        {
            DBG("MIDI Output Node:");
            DBG(" Num IO Node MIDI IN = " << count.get (PortType::Midi, true));
        }

        setPorts (count.toPortList());
    }
}

void IONode::fillInPluginDescription (PluginDescription& d) const
{
    d.name = getName();
    d.uid = d.name.hashCode();
    d.category = "I/O Devices";
    d.pluginFormatName = "Internal";
    d.manufacturerName = "Element";
    d.version = "1.0";
    d.isInstrument = false;
    
    switch (static_cast<int> (this->type))
    {
        case audioInputNode:  d.fileOrIdentifier = "audio.input"; break;
        case audioOutputNode: d.fileOrIdentifier = "audio.output"; break;
        case midiInputNode:   d.fileOrIdentifier = "midi.input"; break;
        case midiOutputNode:  d.fileOrIdentifier = "midi.output"; break;
    }
}

void IONode::prepareToRender (double r, int b)
{
    setRenderDetails (r, b);
    jassert (graph != nullptr);
}

void IONode::releaseResources()
{
}

void IONode::render (AudioSampleBuffer& buffer, MidiPipe& midiPipe) 
{
    jassert (graph != nullptr);
    // jassert (midiPipe.getNumBuffers() > 0);
    auto& midiMessages = *midiPipe.getWriteBuffer (0);
    switch (type)
    {
        case audioOutputNode:
        {
            for (int i = jmin (graph->currentAudioOutputBuffer.getNumChannels(),
                               buffer.getNumChannels()); --i >= 0;)
            {
                graph->currentAudioOutputBuffer.addFrom (i, 0, buffer, i, 0, buffer.getNumSamples());
            }

            break;
        }

        case audioInputNode:
        {
            for (int i = jmin (graph->currentAudioInputBuffer->getNumChannels(),
                               buffer.getNumChannels()); --i >= 0;)
            {
                buffer.copyFrom (i, 0, *graph->currentAudioInputBuffer, i, 0, buffer.getNumSamples());
            }

            break;
        }

        case midiOutputNode:
            graph->currentMidiOutputBuffer.clear();
            graph->currentMidiOutputBuffer.addEvents (midiMessages, 0, buffer.getNumSamples(), 0);
            midiMessages.clear();
            break;

        case midiInputNode:
            midiMessages.clear();
            midiMessages.addEvents (*graph->currentMidiInputBuffer, 0, buffer.getNumSamples(), 0);
            graph->currentMidiInputBuffer->clear();
            break;

        default:
            break;
    }
}

const String IONode::getInputChannelName (int channelIndex) const
{
    switch (type)
    {
        case audioOutputNode:   return "Output " + String (channelIndex + 1);
        case midiOutputNode:    return "Midi Output";
        default:                break;
    }

    return String();
}

const String IONode::getOutputChannelName (int channelIndex) const
{
    switch (type)
    {
        case audioInputNode:    return "Input " + String (channelIndex + 1);
        case midiInputNode:     return "Midi Input";
        default:                break;
    }

    return String();
}


bool IONode::isInput() const   { return type == audioInputNode  || type == midiInputNode; }
bool IONode::isOutput() const  { return type == audioOutputNode || type == midiOutputNode; }

void IONode::setParentGraph (GraphNode* const newGraph)
{
    graph = newGraph;
    // FIXME
    // if (graph != nullptr)
    // {
    //     setPlayConfigDetails (type == audioOutputNode ? graph->getTotalNumOutputChannels() : 0,
    //                           type == audioInputNode ? graph->getTotalNumInputChannels() : 0,
    //                           graph->getSampleRate(), graph->getBlockSize());
    //     updateHostDisplay();
    // }
}

}