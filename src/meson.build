
libelement_sources = '''
    arc.cpp
    bindings.cpp
    commands.cpp
    context.cpp
    datapath.cpp
    matrixstate.cpp
    messages.cpp
    module.cpp
    ringbuffer.cpp
    scripting.cpp
    semaphore.cpp
    settings.cpp
    services.cpp
    slaveprocess.cpp
    strings.cpp
    timescale.cpp
    utils.cpp
    version.cpp

    services/deviceservice.cpp
    services/engineservice.cpp
    services/graphservice.cpp
    services/guiservice.cpp
    services/mappingservice.cpp
    services/oscservice.cpp
    services/presetservice.cpp
    services/sessionservice.cpp
    
    documents/graphdocument.cpp
    documents/sessiondocument.cpp

    engine/ionode.cpp
    engine/oversampler.cpp
    engine/graphmanager.cpp
    engine/internalformat.cpp
    engine/midiengine.cpp
    engine/mappingengine.cpp
    engine/nodeobject.cpp
    engine/midipipe.cpp
    engine/nodes/ScriptNode.cpp
    engine/nodes/MidiProgramMapNode.cpp
    engine/nodes/AudioRouterNode.cpp
    engine/nodes/MidiRouterNode.cpp
    engine/nodes/MediaPlayerProcessor.cpp
    engine/nodes/OSCReceiverNode.cpp
    engine/nodes/LuaNode.cpp
    engine/nodes/MidiMonitorNode.cpp
    engine/nodes/MidiFilterNode.cpp
    engine/nodes/MidiChannelSplitterNode.cpp
    engine/nodes/CompressorProcessor.cpp
    engine/nodes/MidiDeviceProcessor.cpp
    engine/nodes/AudioMixerProcessor.cpp
    engine/nodes/EQFilterProcessor.cpp
    engine/nodes/AudioFilePlayerNode.cpp
    engine/nodes/OSCSenderNode.cpp
    engine/nodes/AudioProcessorNode.cpp
    engine/graphnode.cpp
    engine/transport.cpp
    engine/graphbuilder.cpp
    engine/parameter.cpp
    engine/midiclock.cpp
    engine/nodefactory.cpp
    engine/audioengine.cpp
    engine/rootgraph.cpp
    engine/shuttle.cpp
    
    gui/AboutComponent.cpp
    gui/AudioIOPanelView.cpp
    gui/BlockComponent.cpp
    gui/Buttons.cpp
    gui/capslock.cpp
    gui/ChannelStripComponent.cpp
    gui/ConnectionGrid.cpp
    gui/ContentComponent.cpp
    gui/ContextMenus.cpp
    gui/DecibelScaleComponent.cpp
    gui/DigitalMeter.cpp
    gui/Dock.cpp
    gui/DockArea.cpp
    gui/DockContainer.cpp
    gui/DockItem.cpp
    gui/DockItemTabs.cpp
    gui/DockLayout.cpp
    gui/DockPanel.cpp
    gui/DockWindow.cpp
    gui/FileComboBox.cpp
    gui/GraphEditorComponent.cpp
    gui/Icons.cpp
    gui/LookAndFeel_KV1.cpp
    gui/LookAndFeel.cpp
    gui/LuaTokeniser.cpp
    gui/MainMenu.cpp
    gui/MainWindow.cpp
    gui/NavigationConcertinaPanel.cpp
    gui/NodeEditorFactory.cpp
    gui/NodeIOConfiguration.cpp
    gui/PatchMatrixComponent.cpp
    gui/nodes/AudioRouterEditor.cpp
    gui/nodes/CompressorNodeEditor.cpp
    gui/nodes/EQFilterNodeEditor.cpp
    gui/nodes/GenericNodeEditor.cpp
    gui/nodes/KnobsComponent.cpp
    gui/nodes/LuaNodeEditor.cpp
    gui/nodes/MidiMonitorNodeEditor.cpp
    gui/nodes/MidiProgramMapEditor.cpp
    gui/nodes/MidiRouterEditor.cpp
    gui/nodes/NodeEditorComponent.cpp
    gui/nodes/OSCReceiverNodeEditor.cpp
    gui/nodes/OSCSenderNodeEditor.cpp
    gui/nodes/ScriptNodeEditor.cpp
    gui/nodes/VolumeNodeEditor.cpp
    gui/PluginManagerComponent.cpp
    gui/PluginWindow.cpp
    gui/PreferencesComponent.cpp
    gui/properties/NodeProperties.cpp
    gui/properties/NodePropertyPanel.cpp
    gui/RackContentView.cpp
    gui/SequencerClipItem.cpp
    gui/SequencerComponent.cpp
    gui/SessionImportWizard.cpp
    gui/SessionTreePanel.cpp
    gui/StandardContentComponent.cpp
    gui/SystemTray.cpp
    gui/TransportBar.cpp
    gui/TreeviewBase.cpp
    gui/ViewHelpers.cpp
    gui/views/ContentView.cpp
    gui/views/ControllerDevicesView.cpp
    gui/views/ControllerMapsView.cpp
    gui/views/GraphEditorView.cpp
    gui/views/GraphMixerView.cpp
    gui/views/GraphSettingsView.cpp
    gui/views/KeymapEditorView.cpp
    gui/views/LuaConsoleView.cpp
    gui/views/NavigationView.cpp
    gui/views/NodeChannelStripView.cpp
    gui/views/NodeEditorContentView.cpp
    gui/views/NodeMidiContentView.cpp
    gui/views/NodePortsTable.cpp
    gui/views/PluginsPanelView.cpp
    gui/views/ScriptEditorView.cpp
    gui/views/SessionSettingsView.cpp
    gui/views/SessionTreeContentView.cpp
    gui/views/VirtualKeyboardView.cpp
    gui/widgets/AudioDeviceSelectorComponent.cpp
    gui/widgets/Console.cpp
    gui/widgets/FileTreeView.cpp
    gui/widgets/HorizontalListBox.cpp
    gui/widgets/LuaConsole.cpp
    gui/widgets/MidiBlinker.cpp
    gui/widgets/NodeMidiProgramComponent.cpp
    gui/widgets/SessionGraphsListBox.cpp
    gui/Window.cpp
    gui/WindowManager.cpp
    gui/workspace/PanelTypes.cpp
    gui/workspace/PluginsPanel.cpp
    gui/Workspace.cpp
    gui/WorkspacesContentComponent.cpp
    
    plugins/plugineditor.cpp
    plugins/pluginprocessor.cpp
    
    scripting/dspscript.cpp
    scripting/dspuiscript.cpp
    scripting/bindings.cpp
    scripting/script.cpp
    scripting/scriptdescription.cpp
    scripting/scriptmanager.cpp

    session/controllerdevice.cpp
    session/devicemanager.cpp
    session/model.cpp
    session/node.cpp
    session/pluginmanager.cpp
    session/session.cpp

    el/audio.c
    el/AudioBuffer32.cpp
    el/AudioBuffer64.cpp
    el/Bounds.cpp
    el/bytes.c
    el/CommandManager.cpp
    el/Desktop.cpp
    el/DocumentWindow.cpp
    el/File.cpp
    el/Context.cpp
    el/Graphics.cpp
    el/midi.c
    el/MidiBuffer.cpp
    el/MidiMessage.cpp
    el/MouseEvent.cpp
    el/Node.cpp
    el/Parameter.cpp
    el/Point.cpp
    el/Range.cpp
    el/Rectangle.cpp
    el/round.c
    el/Session.cpp
    el/Slider.cpp
    el/TextButton.cpp
    el/Widget.cpp

    ../libs/compat/BinaryData.cpp
'''.split()

libelement_includes =  [ 
    include_directories (
        '../libs/sol',
        '../include',
        '../scripts',
        'el',
        '.'
    ),
    lua_includes, 
    juce_includes, 
    vst3_includes, 
    vst2_includes, 
    lv2_includes 
]

host_cpp_args = []
if host_machine.system() == 'linux'
    absdatadir = get_option('prefix') / get_option('datadir')
    host_cpp_args += [
        '-DEL_LUADIR="@0@"'.format (absdatadir / 'element' / 'lua'),
        '-DEL_SCRIPTSDIR="@0@"'.format (absdatadir / 'element' / 'scripts')
    ]
endif

if host_machine.system() == 'windows'
    libelement_sources += [ 'dlfcn-win32.c' ]
endif

luael_gen = generator (jrc,
    output : [ '@PLAINNAME@.cpp', '@PLAINNAME@.h'],
    arguments : [ '@INPUT@', '@BUILD_DIR@', 'BinaryData' ])
libelement_sources += luael_gen.process (
    'el/AudioBuffer.lua',
    'el/command.lua',
    'el/object.lua',
    'el/script.lua',
    'el/slug.lua'
)

libelement_includes += scripts_includes
libelement_sources  += scripts_sources
libelement_cpp_args  = host_cpp_args

libelement = static_library ('element', 
    libelement_sources, 
    include_directories : libelement_includes,
    gnu_symbol_visibility : 'hidden',
    dependencies : deps,
    cpp_args : libelement_cpp_args,
    install : false,
    objects : [ libelement_lua.extract_all_objects (recursive : false),
                libelement_juce.extract_all_objects (recursive : false) ])

if meson.is_subproject()
    element_dep = declare_dependency (
        include_directories : libelement_includes,
        link_with : libelement)
endif
