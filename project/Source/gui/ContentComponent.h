/*
    ContentComponent.h - This file is part of Element
    Copyright (c) 2016-2017 Kushview, LLC.  All rights reserved.
*/

#ifndef EL_CONTENT_COMPONENT_H
#define EL_CONTENT_COMPONENT_H

#include "engine/GraphNode.h"
#include "session/Session.h"

namespace Element {

class AppController;
class ContentContainer;
class Globals;
class GuiApp;
class GraphEditorView;
class NavigationConcertinaPanel;
class Node;
class RackView;
class TransportBar;

class ContentView : public Component {
    
};

class ContentComponent :  public Component,
                          public DragAndDropContainer,
                          public FileDragAndDropTarget
{
public:
    ContentComponent (AppController& app, GuiApp& gui);
    ~ContentComponent();

    void childBoundsChanged (Component* child) override;
    void mouseDown (const MouseEvent&) override;
    void paint (Graphics &g) override;
    void resized() override;

    void setCurrentNode (const Node& node);
    void setRackViewComponent (Component* comp);
    void setRackViewNode (GraphNodePtr node);
    void stabilize();
    
    void post (Message*);
    Globals& getGlobals();
    SessionRef getSession() const { return SessionRef(); }
    
    bool isInterestedInFileDrag (const StringArray &files) override;
    void filesDropped (const StringArray &files, int x, int y) override;
    JUCE_DEPRECATED(GuiApp& app());

private:
    AppController& controller;
    GuiApp& gui;
    ScopedPointer<NavigationConcertinaPanel> nav;
    ScopedPointer<ContentContainer> container;
    ScopedPointer<TooltipWindow> toolTips;
    StretchableLayoutManager layout;
    
    class Resizer; friend class Resizer;
    ScopedPointer<Resizer> bar1;
    
    class Toolbar; friend class Toolbar;
    ScopedPointer<Toolbar> toolBar;
    
    class StatusBar; friend class StatusBar;
    ScopedPointer<StatusBar> statusBar;
    
    bool statusBarVisible;
    int statusBarSize;
    bool toolBarVisible;
    int toolBarSize;
    
    void resizerMouseDown();
    void resizerMouseUp();
    void updateLayout();
};

}

#endif // ELEMENT_CONTENT_COMPONENT_H
