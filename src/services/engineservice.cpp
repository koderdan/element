/*
    This file is part of Element
    Copyright (C) 2019  Kushview, LLC.  All rights reserved.

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

#include "ElementApp.h"
#include "services.hpp"
#include "services/guiservice.hpp"
#include "engine/graphmanager.hpp"
#include "engine/nodes/MidiDeviceProcessor.h"
#include "engine/rootgraph.hpp"
#include "session/devicemanager.hpp"
#include "session/pluginmanager.hpp"
#include "session/node.hpp"
#include "context.hpp"
#include "settings.hpp"

#include "services/engineservice.hpp"

namespace element {

struct RootGraphHolder
{
    RootGraphHolder (const Node& n, Context& world)
        : plugins (world.getPluginManager()),
          devices (world.getDeviceManager()),
          model (n)
    {
    }

    ~RootGraphHolder()
    {
        jassert (! attached());
        controller = nullptr;
        model.getValueTree().removeProperty (Tags::object, 0);
        node = nullptr;
        model = Node();
    }

    bool attached() const { return node && controller; }

    /** This will create a root graph processor/controller and load it if not
        done already. Properties are set from the model, so make sure they are
        correct before calling this 
     */
    bool attach (AudioEnginePtr engine)
    {
        jassert (engine);
        if (! engine)
            return false;

        if (attached())
            return true;

        node = new RootGraph();

        if (auto* root = getRootGraph())
        {
            const auto modeStr = model.getProperty (Tags::renderMode, "single").toString().trim().toLowerCase();
            const auto mode = modeStr == "single" ? RootGraph::SingleGraph : RootGraph::Parallel;
            const auto channels = model.getMidiChannels();
            const auto program = (int) model.getProperty ("midiProgram", -1);

            root->setPlayConfigFor (devices);
            root->setRenderMode (mode);
            root->setMidiChannels (channels);
            root->setMidiProgram (program);

            if (engine->addGraph (root))
            {
                controller = std::make_unique<RootGraphManager> (*root, plugins);
                model.setProperty (Tags::object, node.get());
                controller->setNodeModel (model);
                resetIONodePorts();
            }
        }

        return attached();
    }

    bool detach (AudioEnginePtr engine)
    {
        if (! engine)
            return false;

        if (! attached())
            return true;

        bool wasRemoved = false;
        if (auto* g = getRootGraph())
            wasRemoved = engine->removeGraph (g);

        if (wasRemoved)
        {
            controller = nullptr;
            node = nullptr;
        }

        return wasRemoved;
    }

    RootGraphManager* getController() const { return controller.get(); }
    RootGraph* getRootGraph() const { return dynamic_cast<RootGraph*> (node ? node.get() : nullptr); }

    bool hasController() const { return nullptr != controller; }

    void resetIONodePorts()
    {
        const ValueTree nodes = model.getNodesValueTree();
        for (int i = nodes.getNumChildren(); --i >= 0;)
        {
            Node model (nodes.getChild (i), false);
            NodeObjectPtr node = model.getObject();
            if (node && (node->isAudioIONode() || node->isMidiIONode()))
                model.resetPorts();
        }
    }

private:
    friend class EngineService;
    friend class EngineService::RootGraphs;
    PluginManager& plugins;
    DeviceManager& devices;
    std::unique_ptr<RootGraphManager> controller;
    Node model;
    NodeObjectPtr node;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RootGraphHolder);
};

class EngineService::RootGraphs
{
public:
    RootGraphs (EngineService& e) : owner (e) {}
    ~RootGraphs() {}

    RootGraphHolder* add (RootGraphHolder* item)
    {
        jassert (! graphs.contains (item));
        return graphs.add (item);
    }

    void clear()
    {
        detachAll();
        graphs.clear();
    }

    RootGraphHolder* findByEngineIndex (const int index) const
    {
        if (index >= 0)
            for (auto* const n : graphs)
                if (auto* p = n->getRootGraph())
                    if (p->getEngineIndex() == index)
                        return n;
        return 0;
    }

    RootGraphHolder* findFor (const Node& node) const
    {
        for (auto* const n : graphs)
            if (n->model == node)
                return n;
        return nullptr;
    }

    /** Returns the active graph according to the engine */
    RootGraphHolder* findActiveInEngine() const
    {
        auto engine = owner.getWorld().getAudioEngine();
        if (! engine)
            return 0;
        const int currentIndex = engine->getActiveGraph();
        if (currentIndex >= 0)
            for (auto* h : graphs)
                if (auto* root = h->getRootGraph())
                    if (currentIndex == root->getEngineIndex())
                        return h;
        return 0;
    }

    /** Returns the active graph according to the session */
    RootGraphHolder* findActive() const
    {
        if (auto session = owner.getWorld().getSession())
            if (auto* h = findFor (session->getActiveGraph()))
                return h;
        return 0;
    }

    /** This returns a GraphManager for the provided node. The
        passed in node is expected to have type="graph" 
        
        NOTE: this is a recursive operation
     */
    GraphManager* findGraphManagerFor (const Node& graph)
    {
        for (const auto* h : graphs)
        {
            if (auto* m1 = h->controller.get())
                if (auto* m2 = m1->findGraphManagerForGraph (graph))
                    return m2;
        }

        return nullptr;
    }

    RootGraphManager* findActiveRootGraphManager() const
    {
        if (auto* h = findActive())
            return h->controller.get();
        return 0;
    }

    void attachAll()
    {
        engine = owner.getWorld().getAudioEngine();
        for (auto* g : graphs)
            g->attach (engine);
    }

    void detachAll()
    {
        engine = owner.getWorld().getAudioEngine();
        for (auto* g : graphs)
            g->detach (engine);
    }

    // remove the holder, this will also delete it!
    void remove (RootGraphHolder* g)
    {
        graphs.removeObject (g, true);
    }

    const OwnedArray<RootGraphHolder>& getGraphs() const { return graphs; }

private:
    EngineService& owner;
    SessionPtr session;
    AudioEnginePtr engine;
    OwnedArray<RootGraphHolder> graphs;
};

EngineService::EngineService()
    : Service()
{
    graphs = std::make_unique<RootGraphs> (*this);
}

EngineService::~EngineService()
{
    graphs = nullptr;
}

void EngineService::addConnection (const uint32 s, const uint32 sp, const uint32 d, const uint32 dp)
{
    if (auto session = getWorld().getSession())
        if (auto* h = graphs->findFor (session->getCurrentGraph()))
            if (auto* c = h->getController())
                c->addConnection (s, sp, d, dp);
}

void EngineService::addConnection (const uint32 s, const uint32 sp, const uint32 d, const uint32 dp, const Node& graph)
{
    if (auto* controller = graphs->findGraphManagerFor (graph))
        controller->addConnection (s, sp, d, dp);
}

void EngineService::addGraph()
{
    auto& world = getWorld();
    auto engine = world.getAudioEngine();
    auto session = world.getSession();

    Node node (Node::createDefaultGraph ("Graph " + String (session->getNumGraphs() + 1)));
    addGraph (node);

    findSibling<GuiService>()->stabilizeContent();
}

void EngineService::addGraph (const Node& newGraph)
{
    jassert (newGraph.isGraph());

    Node node = newGraph.getValueTree().getParent().isValid() ? newGraph
                                                              : Node (newGraph.getValueTree().createCopy(), false);
    auto engine = getWorld().getAudioEngine();
    auto session = getWorld().getSession();
    String err = node.isGraph() ? String() : "Not a graph";

    if (err.isNotEmpty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Audio Engine", err);
        return;
    }

    if (auto* holder = graphs->add (new RootGraphHolder (node, getWorld())))
    {
        if (holder->attach (engine))
        {
            session->addGraph (node, true);
            setRootNode (node);
        }
        else
        {
            err = "Could not attach new graph to engine.";
        }
    }
    else
    {
        err = "Could not create new graph.";
    }

    if (err.isNotEmpty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Audio Engine", err);
    }

    findSibling<GuiService>()->stabilizeContent();
}

void EngineService::duplicateGraph (const Node& graph)
{
    Node duplicate (graph.getValueTree().createCopy());
    duplicate.savePluginState(); // need objects present to update processor states
    Node::sanitizeRuntimeProperties (duplicate.getValueTree());
    // reset UUIDs to avoid compilcations with undoable actions
    duplicate.forEach ([] (const ValueTree& tree) {
        if (! tree.hasType (Tags::node))
            return;
        auto nodeRef = tree;
        nodeRef.setProperty (Tags::uuid, Uuid().toString(), nullptr);
    });

    duplicate.setProperty (Tags::name, duplicate.getName().replace ("(copy)", "").trim() + String (" (copy)"));
    addGraph (duplicate);
}

void EngineService::duplicateGraph()
{
    auto& world = getWorld();
    auto engine = world.getAudioEngine();
    auto session = world.getSession();
    const Node current (session->getCurrentGraph());
    duplicateGraph (current);
}

void EngineService::removeGraph (int index)
{
    auto& world = getWorld();
    auto engine = world.getAudioEngine();
    auto session = world.getSession();

    if (index < 0)
        index = session->getActiveGraphIndex();

    if (auto* holder = graphs->findByEngineIndex (index))
    {
        bool removeIt = false;
        if (holder->detach (engine))
        {
            ValueTree sgraphs = session->getValueTree().getChildWithName (Tags::graphs);
            sgraphs.removeChild (holder->model.getValueTree(), nullptr);
            removeIt = true;
        }
        else
        {
            DBG ("[EL] could not detach root graph");
        }

        if (removeIt)
        {
            graphs->remove (holder);
            DBG ("[EL] graph removed: index: " << index);
            ValueTree sgraphs = session->getValueTree().getChildWithName (Tags::graphs);

            if (index < 0 || index >= session->getNumGraphs())
                index = session->getNumGraphs() - 1;

            sgraphs.setProperty (Tags::active, index, 0);
            const Node nextGraph = session->getCurrentGraph();

            if (nextGraph.isRootGraph())
            {
                DBG ("[EL] setting new graph: " << nextGraph.getName());
                setRootNode (nextGraph);
            }
            else if (session->getNumGraphs() > 0)
            {
                DBG ("[EL] failed to find appropriate index.");
                sgraphs.setProperty (Tags::active, 0, 0);
                setRootNode (session->getActiveGraph());
            }
        }
    }
    else
    {
        DBG ("[EL] could not find root graph index: " << index);
    }

    findSibling<GuiService>()->stabilizeContent();
}

void EngineService::connectChannels (const uint32 s, const int sc, const uint32 d, const int dc)
{
    if (auto* root = graphs->findActiveRootGraphManager())
    {
        auto src = root->getNodeForId (s);
        auto dst = root->getNodeForId (d);
        if (! src || ! dst)
            return;
        addConnection (src->nodeId, src->getPortForChannel (PortType::Audio, sc, false), dst->nodeId, dst->getPortForChannel (PortType::Audio, dc, true));
    }
}

void EngineService::removeConnection (const uint32 s, const uint32 sp, const uint32 d, const uint32 dp)
{
    if (auto* root = graphs->findActiveRootGraphManager())
        root->removeConnection (s, sp, d, dp);
}

void EngineService::removeConnection (const uint32 s, const uint32 sp, const uint32 d, const uint32 dp, const Node& target)
{
    if (auto* controller = graphs->findGraphManagerFor (target))
        controller->removeConnection (s, sp, d, dp);
}

Node EngineService::addNode (const Node& node, const Node& target, const ConnectionBuilder& builder)
{
    if (auto* controller = graphs->findGraphManagerFor (target))
    {
        const uint32 nodeId = controller->addNode (node);
        Node referencedNode (controller->getNodeModelForId (nodeId));
        if (referencedNode.isValid())
        {
            builder.addConnections (*controller, nodeId);
            return referencedNode;
        }
    }

    return Node();
}

void EngineService::addNode (const Node& node)
{
    auto* root = graphs->findActiveRootGraphManager();
    const uint32 nodeId = (root != nullptr) ? root->addNode (node) : EL_INVALID_NODE;
    if (EL_INVALID_NODE != nodeId)
    {
        const Node actual (root->getNodeModelForId (nodeId));
        if (getWorld().getSettings().showPluginWindowsWhenAdded())
            findSibling<GuiService>()->presentPluginWindow (actual);
    }
    else
    {
        AlertWindow::showMessageBox (AlertWindow::InfoIcon,
                                     "Audio Engine",
                                     String ("Could not add node: ") + node.getName());
    }
}

void EngineService::addPlugin (const PluginDescription& desc, const bool verified, const float rx, const float ry)
{
    auto* root = graphs->findActiveRootGraphManager();
    if (! root)
        return;

    OwnedArray<PluginDescription> plugs;
    if (! verified)
    {
        auto* format = getWorld().getPluginManager().getAudioPluginFormat (desc.pluginFormatName);
        jassert (format != nullptr);
        auto& list (getWorld().getPluginManager().getKnownPlugins());
        list.removeFromBlacklist (desc.fileOrIdentifier);
        if (list.scanAndAddFile (desc.fileOrIdentifier, false, plugs, *format))
        {
            getWorld().getPluginManager().saveUserPlugins (getWorld().getSettings());
        }
    }
    else
    {
        plugs.add (new PluginDescription (desc));
    }

    if (plugs.size() > 0)
    {
        const auto nodeId = root->addNode (plugs.getFirst(), nullptr, "", rx, ry);
        if (EL_INVALID_NODE != nodeId)
        {
            const Node node (root->getNodeModelForId (nodeId));
            if (getWorld().getSettings().showPluginWindowsWhenAdded())
                findSibling<GuiService>()->presentPluginWindow (node);
        }
    }
    else
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::NoIcon, "Add Plugin", String ("Could not add ") + desc.name + " for an unknown reason");
    }
}

void EngineService::removeNode (const Node& node)
{
    const Node graph (node.getParentGraph());
    if (! graph.isGraph())
        return;

    auto* const gui = findSibling<GuiService>();
    if (auto* manager = graphs->findGraphManagerFor (graph))
    {
        jassert (manager->contains (node.getNodeId()));
        gui->closePluginWindowsFor (node, true);
        if (gui->getSelectedNode() == node)
            gui->selectNode (Node());
        manager->removeNode (node.getNodeId());
        nodeRemoved (node);
    }
}

void EngineService::removeNode (const Uuid& uuid)
{
    auto session = getWorld().getSession();
    const auto node = session->findNodeById (uuid);
    if (node.isValid())
    {
        removeNode (node);
    }
    else
    {
        DBG ("[EL] node not found: " << uuid.toString());
    }
}

void EngineService::removeNode (const uint32 nodeId)
{
    auto* root = graphs->findActiveRootGraphManager();
    if (! root)
        return;
    if (auto* gui = findSibling<GuiService>())
        gui->closePluginWindowsFor (nodeId, true);
    root->removeNode (nodeId);
}

void EngineService::disconnectNode (const Node& node, const bool inputs, const bool outputs, const bool audio, const bool midi)
{
    const auto graph (node.getParentGraph());
    if (auto* controller = graphs->findGraphManagerFor (graph))
        controller->disconnectNode (node.getNodeId(), inputs, outputs, audio, midi);
}

void EngineService::activate()
{
    Service::activate();

    auto& globals (getWorld());
    auto& devices (globals.getDeviceManager());
    auto engine (globals.getAudioEngine());
    auto session (globals.getSession());
    engine->setSession (session);
    engine->activate();

    sessionReloaded();
    devices.addChangeListener (this);
}

void EngineService::deactivate()
{
    Service::deactivate();
    auto& globals (getWorld());
    auto& devices (globals.getDeviceManager());
    auto engine (globals.getAudioEngine());
    auto session (globals.getSession());

    if (auto* gui = findSibling<GuiService>())
    {
        // gui might not deactivate before the engine, so
        // close the windows here
        gui->closeAllPluginWindows();
    }

    session->saveGraphState();
    graphs->clear();

    engine->deactivate();
    engine->setSession (nullptr);
    devices.removeChangeListener (this);
}

void EngineService::clear()
{
    graphs->clear();
}

void EngineService::setRootNode (const Node& newRootNode)
{
    if (! newRootNode.isRootGraph())
    {
        jassertfalse; // needs to be a graph
        return;
    }

    auto* holder = graphs->findFor (newRootNode);
    if (! holder)
    {
        jassertfalse; // you should have a root graph registered before calling this.
        holder = graphs->add (new RootGraphHolder (newRootNode, getWorld()));
    }

    if (! holder)
    {
        DBG ("[EL] failed to find root graph for node: " << newRootNode.getName());
        return;
    }

    auto engine = getWorld().getAudioEngine();
    auto session = getWorld().getSession();
    auto& devices = getWorld().getDeviceManager();

    if (! holder->attached())
        holder->attach (engine);
    const int index = holder->getRootGraph()->getEngineIndex();

#if 0
    // saving this for reference. graphs will need to be
    // explicitly de-activated by users to unload them
    // moving forward - MRF
    
    /* Unload the active graph if necessary */
    auto* active = graphs->findActiveInEngine();
    if (active && active != holder)
    {
        if (auto* gui = findSibling<GuiService>())
            gui->closeAllPluginWindows();
        
        if (! (bool) active->model.getProperty(Tags::persistent) && active->attached())
        {
            active->controller->savePluginStates();
            active->controller->unloadGraph();
            DBG("[EL] graph unloaded: " << active->model.getName());
        }
    }
#endif

    if (auto* proc = holder->getRootGraph())
    {
        proc->setMidiChannels (newRootNode.getMidiChannels().get());
        proc->setVelocityCurveMode ((VelocityCurve::Mode) (int) newRootNode.getProperty (
            Tags::velocityCurveMode, (int) VelocityCurve::Linear));
    }
    else
    {
        DBG ("[EL] couldn't find graph processor for node.");
    }

    if (auto* const r = holder->getController())
    {
        if (! r->isLoaded())
        {
            r->getRootGraph().setPlayConfigFor (devices);
            r->setNodeModel (newRootNode);
        }

        engine->setCurrentGraph (index);
    }
    else
    {
        DBG ("[EL] no graph controller for node: " << newRootNode.getName());
    }

    engine->refreshSession();
}

void EngineService::changeListenerCallback (ChangeBroadcaster* cb)
{
    auto& devices (getWorld().getDeviceManager());
    if (getRunMode() == RunMode::Plugin || cb != &devices)
        return;

    for (auto* const root : graphs->getGraphs())
    {
        auto* manager = root->getController();
        auto* processor = root->getRootGraph();
        if (! processor || ! manager)
            continue;

        const bool wasSuspended = processor->isSuspended();
        processor->suspendProcessing (true);
        processor->setPlayConfigFor (devices);

        for (int i = processor->getNumNodes(); --i >= 0;)
        {
            auto node = processor->getNode (i);
            if (node->isAudioIONode() || node->isMidiIONode())
                node->refreshPorts();
        }

        manager->syncArcsModel();
        processor->suspendProcessing (wasSuspended);
    }
}

void EngineService::syncModels()
{
    for (auto* holder : graphs->getGraphs())
    {
        Node graph (holder->model);
        for (int i = 0; i < graph.getNumNodes(); ++i)
        {
            Node node (graph.getNode (i));
            if (! node.isIONode())
                continue;
            node.resetPorts();
        }

        if (auto* controller = holder->getController())
        {
            controller->syncArcsModel();
        }
    }
}

void EngineService::addPlugin (const Node& graph, const PluginDescription& desc)
{
    if (! graph.isGraph())
        return;

    if (auto* controller = graphs->findGraphManagerFor (graph))
    {
        const Node node (addPlugin (*controller, desc, nullptr, ""));
    }
}

Node EngineService::addPlugin (const Node& graph, const PluginDescription& desc, std::unique_ptr<AudioPluginInstance> plugin, const String& pluginErrorMessage, const ConnectionBuilder& builder, const bool verified)
{
    if (! graph.isGraph())
        return Node();

    OwnedArray<PluginDescription> plugs;
    if (! plugin && ! verified)
    {
        // AUv3 will always be pre-verified so don't need to worry about async here
        auto* format = getWorld().getPluginManager().getAudioPluginFormat (desc.pluginFormatName);
        jassert (format != nullptr);
        auto& list (getWorld().getPluginManager().getKnownPlugins());
        list.removeFromBlacklist (desc.fileOrIdentifier);
        if (list.scanAndAddFile (desc.fileOrIdentifier, false, plugs, *format))
        {
            getWorld().getPluginManager().saveUserPlugins (getWorld().getSettings());
        }
    }
    else
    {
        plugs.add (new PluginDescription (desc));
    }

    const PluginDescription descToLoad = (plugs.size() > 0) ? *plugs.getFirst() : desc;

    if (auto* controller = graphs->findGraphManagerFor (graph))
    {
        const Node node (addPlugin (*controller, descToLoad, std::move(plugin), pluginErrorMessage));
        if (node.isValid())
        {
            builder.addConnections (*controller, node.getNodeId());
            jassert (! node.getUuid().isNull());
        }
        return node;
    }

    return Node();
}

void EngineService::sessionReloaded()
{
    graphs->clear();

    auto session = getWorld().getSession();
    auto engine = getWorld().getAudioEngine();

    if (session->getNumGraphs() > 0)
    {
        for (int i = 0; i < session->getNumGraphs(); ++i)
        {
            Node rootGraph (session->getGraph (i));
            if (auto* holder = graphs->add (new RootGraphHolder (rootGraph, getWorld())))
            {
                holder->attach (engine);
                if (auto* const controller = holder->getController())
                {
                    // noop: saving this logical block
                }
            }
        }

        setRootNode (session->getCurrentGraph());
    }
}

Node EngineService::addPlugin (GraphManager& c, const PluginDescription& desc, std::unique_ptr<AudioPluginInstance> plugin, const String& pluginErrorMessage)
{
    auto& plugins (getWorld().getPluginManager());
    const auto nodeId = c.addNode (&desc, std::move(plugin), pluginErrorMessage, 0.5f, 0.5f, 0);

    if (EL_INVALID_NODE != nodeId)
    {

        const Node node (c.getNodeModelForId (nodeId));
        if (getWorld().getSettings().showPluginWindowsWhenAdded())
            findSibling<GuiService>()->presentPluginWindow (node);
        if (! node.isValid())
        {
            jassertfalse; // fatal, but continue
        }

        if (node.getUuid().isNull())
        {
            jassertfalse;
            ValueTree nodeData = node.getValueTree();
            nodeData.setProperty (Tags::uuid, Uuid().toString(), 0);
        }
        return node;
    }

    return Node();
}

void EngineService::addMidiDeviceNode (const String& device, const bool isInput)
{
    NodeObjectPtr ptr;
    Node graph;
    if (auto s = getWorld().getSession())
        graph = s->getActiveGraph();
    if (auto* const root = graphs->findActiveRootGraphManager())
    {
        PluginDescription desc;
        desc.pluginFormatName = "Internal";
        desc.fileOrIdentifier = isInput ? "element.midiInputDevice" : "element.midiOutputDevice";
        ptr = root->getNodeForId (root->addNode (&desc, nullptr, "",    0.5, 0.5));
    }

    MidiDeviceProcessor* const proc = (ptr == nullptr) ? nullptr
                                                       : dynamic_cast<MidiDeviceProcessor*> (ptr->getAudioProcessor());

    if (proc != nullptr)
    {
        proc->setCurrentDevice (device);
        for (int i = 0; i < graph.getNumNodes(); ++i)
        {
            auto node (graph.getNode (i));
            if (node.getObject() == ptr.get())
            {
                node.setProperty (Tags::name, proc->getCurrentDevice());
                node.resetPorts();
                break;
            }
        }
    }
    else
    {
        // noop
    }
}

void EngineService::changeBusesLayout (const Node& n, const AudioProcessor::BusesLayout& layout)
{
    Node node = n;
    Node graph = node.getParentGraph();
    NodeObjectPtr ptr = node.getObject();
    auto* controller = graphs->findGraphManagerFor (graph);
    if (! controller)
        return;

    if (AudioProcessor* proc = ptr ? ptr->getAudioProcessor() : nullptr)
    {
        NodeObjectPtr ptr2 = graph.getObject();
        if (auto* gp = dynamic_cast<GraphNode*> (ptr2.get()))
        {
            if (proc->checkBusesLayoutSupported (layout))
            {
                gp->suspendProcessing (true);
                gp->releaseResources();

                const bool wasNotSuspended = ! proc->isSuspended();
                proc->suspendProcessing (true);
                proc->releaseResources();
                proc->setBusesLayoutWithoutEnabling (layout);
                node.resetPorts();
                if (wasNotSuspended)
                    proc->suspendProcessing (false);

                gp->prepareToRender (gp->getSampleRate(), gp->getBlockSize());
                gp->suspendProcessing (false);

                controller->removeIllegalConnections();
                controller->syncArcsModel();

                findSibling<GuiService>()->stabilizeViews();
            }
        }
    }
}

void EngineService::replace (const Node& node, const PluginDescription& desc)
{
    const auto graph (node.getParentGraph());
    if (! graph.isGraph())
        return;

    if (auto* ctl = graphs->findGraphManagerFor (graph))
    {
        double x = 0.0, y = 0.0;
        node.getPosition (x, y);
        const auto oldNodeId = node.getNodeId();
        const auto wasWindowOpen = (bool) node.getProperty ("windowVisible");
        const auto nodeId = ctl->addNode (&desc, nullptr, "", x, y);
        if (nodeId != EL_INVALID_NODE)
        {
            NodeObjectPtr newptr = ctl->getNodeForId (nodeId);
            const NodeObjectPtr oldptr = node.getObject();
            jassert (newptr && oldptr);
            // attempt to retain connections from the replaced node
            for (int i = ctl->getNumConnections(); --i >= 0;)
            {
                const auto* arc = ctl->getConnection (i);
                if (oldNodeId == arc->sourceNode)
                {
                    ctl->addConnection (
                        nodeId,
                        newptr->getPortForChannel (
                            oldptr->getPortType (arc->sourcePort),
                            oldptr->getChannelPort (arc->sourcePort),
                            oldptr->isPortInput (arc->sourcePort)),
                        arc->destNode,
                        arc->destPort);
                }
                else if (oldNodeId == arc->destNode)
                {
                    ctl->addConnection (
                        arc->sourceNode,
                        arc->sourcePort,
                        nodeId,
                        newptr->getPortForChannel (
                            oldptr->getPortType (arc->destPort),
                            oldptr->getChannelPort (arc->destPort),
                            oldptr->isPortInput (arc->destPort)));
                }
            }

            auto newNode (ctl->getNodeModelForId (nodeId));
            newNode.setPosition (x, y); // TODO: GraphManager should handle these
            newNode.setProperty ("windowX", (int) node.getProperty ("windowX"))
                .setProperty ("windowY", (int) node.getProperty ("windowY"));

            removeNode (node);
            if (wasWindowOpen)
                findSibling<GuiService>()->presentPluginWindow (newNode);
        }
    }

    findSibling<GuiService>()->stabilizeViews();
}

} // namespace element
