#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PresetBrowser : public juce::Component,
                      public juce::ListBoxModel,
                      public juce::Button::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void presetSelected(const juce::File& presetFile) = 0;
        virtual void savePresetRequested(const juce::File& folder, const juce::String& name,
                                         const juce::String& author, const juce::String& tags,
                                         const juce::String& notes) = 0;
    };

    PresetBrowser(const juce::File& rootFolder);
    ~PresetBrowser() override;

    void setListener(Listener* l) { listener = l; }
    void refresh();
    void setRootFolder(const juce::File& folder);

    // Component
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // Button::Listener
    void buttonClicked(juce::Button* button) override;

private:
    void scanFolder();
    void scanSubfolders(const juce::File& folder, juce::StringArray& folders, const juce::String& prefix);
    void loadNotesForPreset(const juce::File& presetFile);
    void loadMetadataForPreset(const juce::File& presetFile);
    void saveNotesForPreset(const juce::File& presetFile);
    juce::File getNotesFile(const juce::File& presetFile);
    void showNotesEditor();

    juce::File rootFolder;
    juce::File currentFolder;
    juce::File selectedPreset;
    juce::Array<juce::File> presetFiles;
    juce::StringArray folderNames;
    juce::Array<juce::File> folderPaths;

    juce::ComboBox folderSelector;
    juce::ListBox presetList;
    juce::TextButton loadButton{"Load"};
    juce::TextButton deleteButton{"Del"};
    juce::TextButton saveButton{"Save"};
    juce::TextButton newFolderButton{"New Folder"};
    juce::TextButton openFolderButton{"Open"};
    juce::TextButton editNotesButton{"Edit"};
    juce::TextEditor presetNameEditor;
    juce::TextEditor notesEditor;
    juce::Label notesLabel;
    juce::Label pluginsLabel;
    juce::TextEditor pluginsDisplay;

    Listener* listener = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
