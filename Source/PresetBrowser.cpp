#include "PresetBrowser.h"
#include <iostream>

PresetBrowser::PresetBrowser(const juce::File& root)
    : rootFolder(root), currentFolder(root)
{
    setVisible(true);
    setWantsKeyboardFocus(false);

    folderSelector.onChange = [this]()
    {
        int idx = folderSelector.getSelectedItemIndex();
        if (idx >= 0 && idx < folderPaths.size())
        {
            currentFolder = folderPaths[idx];
            scanFolder();
        }
    };
    addAndMakeVisible(folderSelector);

    presetList.setModel(this);
    presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    presetList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff3a3a3a));
    presetList.setRowHeight(24);
    presetList.setMouseClickGrabsKeyboardFocus(true);
    presetList.setWantsKeyboardFocus(true);
    addAndMakeVisible(presetList);

    std::cerr << "[PresetBrowser] ListBox created and configured" << std::endl;

    loadButton.addListener(this);
    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff44aa44));
    addAndMakeVisible(loadButton);

    deleteButton.addListener(this);
    deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffaa3333));
    addAndMakeVisible(deleteButton);

    saveButton.addListener(this);
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4466aa));
    addAndMakeVisible(saveButton);

    newFolderButton.addListener(this);
    newFolderButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(newFolderButton);

    openFolderButton.addListener(this);
    openFolderButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(openFolderButton);

    presetNameEditor.setTextToShowWhenEmpty("Preset name...", juce::Colours::grey);
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a3a));
    presetNameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    presetNameEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff4466aa));
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff5588cc));
    presetNameEditor.setColour(juce::CaretComponent::caretColourId, juce::Colours::white);
    presetNameEditor.setMultiLine(false);
    presetNameEditor.setReturnKeyStartsNewLine(false);
    presetNameEditor.setSelectAllWhenFocused(true);
    presetNameEditor.setWantsKeyboardFocus(true);
    presetNameEditor.setInputRestrictions(100);
    addAndMakeVisible(presetNameEditor);

    // Notes section - display only, use button to edit via popup
    notesLabel.setText("Notes:", juce::dontSendNotification);
    notesLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(notesLabel);

    editNotesButton.addListener(this);
    editNotesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(editNotesButton);

    notesEditor.setTextToShowWhenEmpty("Select a preset to add notes...", juce::Colours::grey);
    notesEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    notesEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a3a));
    notesEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    notesEditor.setReadOnly(true);
    notesEditor.setMultiLine(true);
    addAndMakeVisible(notesEditor);

    refresh();
}

PresetBrowser::~PresetBrowser()
{
    loadButton.removeListener(this);
    deleteButton.removeListener(this);
    saveButton.removeListener(this);
    newFolderButton.removeListener(this);
    openFolderButton.removeListener(this);
    editNotesButton.removeListener(this);
}

void PresetBrowser::setRootFolder(const juce::File& folder)
{
    rootFolder = folder;
    currentFolder = folder;
    refresh();
}

void PresetBrowser::refresh()
{
    // Build folder list
    folderNames.clear();
    folderPaths.clear();

    folderNames.add("/ (Root)");
    folderPaths.add(rootFolder);

    scanSubfolders(rootFolder, folderNames, "");

    folderSelector.clear();
    int id = 1;
    for (const auto& name : folderNames)
        folderSelector.addItem(name, id++);

    // Select current folder
    int idx = folderPaths.indexOf(currentFolder);
    if (idx >= 0)
        folderSelector.setSelectedItemIndex(idx, juce::dontSendNotification);
    else
        folderSelector.setSelectedItemIndex(0, juce::dontSendNotification);

    scanFolder();
}

void PresetBrowser::scanSubfolders(const juce::File& folder, juce::StringArray& folders, const juce::String& prefix)
{
    for (const auto& child : folder.findChildFiles(juce::File::findDirectories, false))
    {
        juce::String displayName = prefix + "  " + child.getFileName();
        folders.add(displayName);
        folderPaths.add(child);
        scanSubfolders(child, folders, prefix + "  ");
    }
}

void PresetBrowser::scanFolder()
{
    presetFiles.clear();

    std::cerr << "[PresetBrowser] Scanning folder: " << currentFolder.getFullPathName() << std::endl;

    if (currentFolder.exists())
    {
        for (const auto& file : currentFolder.findChildFiles(juce::File::findFiles, false, "*.uhbikchain"))
        {
            presetFiles.add(file);
            std::cerr << "[PresetBrowser] Found preset: " << file.getFileName() << std::endl;
        }
    }

    std::cerr << "[PresetBrowser] Total presets found: " << presetFiles.size() << std::endl;

    presetList.updateContent();
    presetList.repaint();
}

void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Header
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), 30);

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("PRESETS", 10, 0, 100, 30, juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto bounds = getLocalBounds();

    // Header
    bounds.removeFromTop(30);

    // Folder selector row with open button
    auto folderArea = bounds.removeFromTop(28);
    openFolderButton.setBounds(folderArea.removeFromRight(50).reduced(2, 2));
    folderSelector.setBounds(folderArea.reduced(4, 2));

    // Load/Delete button row
    auto loadArea = bounds.removeFromBottom(32);
    auto loadBounds = loadArea.reduced(4, 2);
    loadButton.setBounds(loadBounds.removeFromLeft(loadBounds.getWidth() * 2 / 3 - 2));
    loadBounds.removeFromLeft(4);
    deleteButton.setBounds(loadBounds);

    // Save area at bottom
    auto saveArea = bounds.removeFromBottom(60);
    presetNameEditor.setBounds(saveArea.removeFromTop(28).reduced(4, 2));
    auto buttonArea = saveArea.reduced(4, 2);
    saveButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2 - 2));
    buttonArea.removeFromLeft(4);
    newFolderButton.setBounds(buttonArea);

    // Notes area at bottom (above save)
    auto notesArea = bounds.removeFromBottom(100);
    auto notesHeader = notesArea.removeFromTop(20);
    notesLabel.setBounds(notesHeader.removeFromLeft(50).reduced(4, 0));
    editNotesButton.setBounds(notesHeader.removeFromRight(40).reduced(2, 0));
    notesEditor.setBounds(notesArea.reduced(4, 2));

    // Preset list
    presetList.setBounds(bounds.reduced(4));
}

int PresetBrowser::getNumRows()
{
    return presetFiles.size();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= presetFiles.size())
        return;

    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3a3a3a));

    g.setColour(juce::Colours::white);
    g.setFont(13.0f);

    auto presetName = presetFiles[rowNumber].getFileNameWithoutExtension();
    g.drawText(presetName, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    std::cerr << "[PresetBrowser] List item clicked: row=" << row << ", presetFiles.size()=" << presetFiles.size() << std::endl;

    if (row >= 0 && row < presetFiles.size())
    {
        // Save notes for previously selected preset
        if (selectedPreset.exists())
            saveNotesForPreset(selectedPreset);

        selectedPreset = presetFiles[row];
        presetNameEditor.setText(presetFiles[row].getFileNameWithoutExtension());
        loadNotesForPreset(selectedPreset);
        std::cerr << "[PresetBrowser] Selected preset: " << selectedPreset.getFileName() << std::endl;
    }
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    std::cerr << "[PresetBrowser] List item DOUBLE clicked: row=" << row << std::endl;

    if (row >= 0 && row < presetFiles.size() && listener != nullptr)
    {
        std::cerr << "[PresetBrowser] Loading preset: " << presetFiles[row].getFileName() << std::endl;
        listener->presetSelected(presetFiles[row]);
    }
}

void PresetBrowser::selectedRowsChanged(int lastRowSelected)
{
    std::cerr << "[PresetBrowser] selectedRowsChanged: row=" << lastRowSelected << std::endl;

    if (lastRowSelected >= 0 && lastRowSelected < presetFiles.size())
    {
        // Save notes for previously selected preset
        if (selectedPreset.exists())
            saveNotesForPreset(selectedPreset);

        selectedPreset = presetFiles[lastRowSelected];
        presetNameEditor.setText(presetFiles[lastRowSelected].getFileNameWithoutExtension());
        loadNotesForPreset(selectedPreset);
        std::cerr << "[PresetBrowser] Selected preset via row change: " << selectedPreset.getFileName() << std::endl;
    }
}

void PresetBrowser::buttonClicked(juce::Button* button)
{
    if (button == &loadButton)
    {
        std::cerr << "[PresetBrowser] Load button clicked, selectedPreset: " << selectedPreset.getFullPathName() << std::endl;
        if (selectedPreset.exists() && listener != nullptr)
        {
            std::cerr << "[PresetBrowser] Loading preset via button: " << selectedPreset.getFileName() << std::endl;
            listener->presetSelected(selectedPreset);
        }
    }
    else if (button == &deleteButton)
    {
        if (selectedPreset.exists())
        {
            // Delete preset with confirmation
            auto* alertWindow = new juce::AlertWindow("Delete Preset",
                "Are you sure you want to delete \"" + selectedPreset.getFileNameWithoutExtension() + "\"?",
                juce::MessageBoxIconType::WarningIcon);
            alertWindow->addButton("Delete", 1);
            alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, alertWindow](int result)
                {
                    if (result == 1)
                    {
                        // Delete the preset file
                        if (selectedPreset.deleteFile())
                        {
                            // Also delete notes file if it exists
                            getNotesFile(selectedPreset).deleteFile();
                            std::cerr << "[PresetBrowser] Deleted preset: " << selectedPreset.getFileName() << std::endl;
                            selectedPreset = juce::File();
                            refresh();
                        }
                    }
                    delete alertWindow;
                }), true);
        }
        else if (currentFolder.exists() && currentFolder != rootFolder)
        {
            // Delete current folder with confirmation
            auto* alertWindow = new juce::AlertWindow("Delete Folder",
                "Are you sure you want to delete folder \"" + currentFolder.getFileName() + "\" and all its contents?",
                juce::MessageBoxIconType::WarningIcon);
            alertWindow->addButton("Delete", 1);
            alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, alertWindow](int result)
                {
                    if (result == 1)
                    {
                        juce::File folderToDelete = currentFolder;
                        currentFolder = rootFolder;
                        if (folderToDelete.deleteRecursively())
                        {
                            std::cerr << "[PresetBrowser] Deleted folder: " << folderToDelete.getFileName() << std::endl;
                            refresh();
                        }
                    }
                    delete alertWindow;
                }), true);
        }
    }
    else if (button == &openFolderButton)
    {
        // Open current folder in file manager
        if (currentFolder.exists())
        {
            currentFolder.revealToUser();
        }
    }
    else if (button == &editNotesButton)
    {
        if (selectedPreset.exists())
            showNotesEditor();
    }
    else if (button == &saveButton)
    {
        // Use popup dialog for better keyboard focus handling
        auto* alertWindow = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::MessageBoxIconType::NoIcon);
        alertWindow->addTextEditor("name", presetNameEditor.getText(), "Name:");
        alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        // Give focus to the text editor
        if (auto* te = alertWindow->getTextEditor("name"))
            te->grabKeyboardFocus();

        alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, alertWindow](int result)
            {
                if (result == 1)
                {
                    auto name = alertWindow->getTextEditorContents("name").trim();
                    if (name.isNotEmpty() && listener != nullptr)
                    {
                        listener->savePresetRequested(currentFolder, name);
                        presetNameEditor.clear();
                        refresh();
                    }
                }
                delete alertWindow;
            }), true);
    }
    else if (button == &newFolderButton)
    {
        auto* alertWindow = new juce::AlertWindow("New Folder", "Enter folder name:", juce::MessageBoxIconType::NoIcon);
        alertWindow->addTextEditor("name", "", "Name:");
        alertWindow->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        if (auto* te = alertWindow->getTextEditor("name"))
            te->grabKeyboardFocus();

        alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, alertWindow](int result)
            {
                if (result == 1)
                {
                    auto folderName = alertWindow->getTextEditorContents("name").trim();
                    if (folderName.isNotEmpty())
                    {
                        auto newFolder = currentFolder.getChildFile(folderName);
                        if (newFolder.createDirectory())
                        {
                            refresh();
                            int idx = folderPaths.indexOf(newFolder);
                            if (idx >= 0)
                                folderSelector.setSelectedItemIndex(idx);
                        }
                    }
                }
                delete alertWindow;
            }), true);
    }
}

juce::File PresetBrowser::getNotesFile(const juce::File& presetFile)
{
    return presetFile.withFileExtension(".notes");
}

void PresetBrowser::loadNotesForPreset(const juce::File& presetFile)
{
    auto notesFile = getNotesFile(presetFile);
    if (notesFile.existsAsFile())
    {
        notesEditor.setText(notesFile.loadFileAsString());
    }
    else
    {
        notesEditor.clear();
    }
}

void PresetBrowser::saveNotesForPreset(const juce::File& presetFile)
{
    auto notesFile = getNotesFile(presetFile);
    auto text = notesEditor.getText();

    if (text.isEmpty())
    {
        notesFile.deleteFile();
    }
    else
    {
        notesFile.replaceWithText(text);
    }
}

void PresetBrowser::showNotesEditor()
{
    auto* alertWindow = new juce::AlertWindow("Edit Notes", "Notes for: " + selectedPreset.getFileNameWithoutExtension(), juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("notes", notesEditor.getText(), "");

    if (auto* te = alertWindow->getTextEditor("notes"))
    {
        te->setMultiLine(true);
        te->setReturnKeyStartsNewLine(true);
        te->setSize(300, 150);
        te->grabKeyboardFocus();
    }

    alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result)
        {
            if (result == 1)
            {
                auto notes = alertWindow->getTextEditorContents("notes");
                notesEditor.setText(notes);
                if (selectedPreset.exists())
                    saveNotesForPreset(selectedPreset);
            }
            delete alertWindow;
        }), true);
}
