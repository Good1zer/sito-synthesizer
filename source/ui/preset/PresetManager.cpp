#include "PresetManager.h"

PresetManager::PresetManager (juce::AudioProcessor& proc)
    : processor (proc)
{
    initializeDirectories();
}

void PresetManager::initializeDirectories()
{
    // User presets: Documents/SITO/Presets
    auto documentsDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    userPresetsDir = documentsDir.getChildFile ("SITO").getChildFile ("Presets");
    
    // Factory presets: Application data (could be bundled with plugin)
    auto appDataDir = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);
    factoryPresetsDir = appDataDir.getChildFile ("SITO").getChildFile ("Factory Presets");
    
    // Create user presets directory if it doesn't exist
    if (! userPresetsDir.exists())
        userPresetsDir.createDirectory();
}

bool PresetManager::savePreset (const juce::String& presetName)
{
    if (! isValidPresetName (presetName))
        return false;
    
    const auto sanitized = sanitizePresetName (presetName);
    const auto presetFile = userPresetsDir.getChildFile (sanitized).withFileExtension ("sitopreset");
    
    // Get current state from processor
    juce::MemoryBlock stateData;
    processor.getStateInformation (stateData);
    
    // Save to file
    if (presetFile.replaceWithData (stateData.getData(), stateData.getSize()))
    {
        setCurrentPreset (presetFile, presetName);
        notifyPresetListChanged();
        return true;
    }
    
    return false;
}

bool PresetManager::loadPreset (const juce::File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;
    
    juce::MemoryBlock stateData;
    if (! presetFile.loadFileAsData (stateData))
        return false;
    
    processor.setStateInformation (stateData.getData(), static_cast<int> (stateData.getSize()));
    
    setCurrentPreset (presetFile, presetFile.getFileNameWithoutExtension());
    return true;
}

bool PresetManager::deletePreset (const juce::File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;
    
    // Don't allow deleting factory presets
    if (presetFile.isAChildOf (factoryPresetsDir))
        return false;
    
    const auto wasCurrentPreset = (presetFile == currentPresetFile);
    
    if (presetFile.deleteFile())
    {
        if (wasCurrentPreset)
            clearCurrentPreset();
        
        notifyPresetListChanged();
        return true;
    }
    
    return false;
}

bool PresetManager::renamePreset (const juce::File& oldFile, const juce::String& newName)
{
    if (! oldFile.existsAsFile() || ! isValidPresetName (newName))
        return false;
    
    // Don't allow renaming factory presets
    if (oldFile.isAChildOf (factoryPresetsDir))
        return false;
    
    const auto sanitized = sanitizePresetName (newName);
    const auto newFile = oldFile.getParentDirectory().getChildFile (sanitized).withFileExtension ("sitopreset");
    
    if (newFile.existsAsFile() && newFile != oldFile)
        return false; // Name conflict
    
    if (oldFile.moveFileTo (newFile))
    {
        if (oldFile == currentPresetFile)
            setCurrentPreset (newFile, newName);
        
        notifyPresetListChanged();
        return true;
    }
    
    return false;
}

std::vector<PresetManager::PresetInfo> PresetManager::getAllPresets() const
{
    auto presets = getUserPresets();
    auto factory = getFactoryPresets();
    presets.insert (presets.end(), factory.begin(), factory.end());
    return presets;
}

std::vector<PresetManager::PresetInfo> PresetManager::getUserPresets() const
{
    std::vector<PresetInfo> presets;
    
    if (! userPresetsDir.exists())
        return presets;
    
    auto files = userPresetsDir.findChildFiles (juce::File::findFiles, false, "*.sitopreset");
    
    for (const auto& file : files)
    {
        PresetInfo info;
        info.name = file.getFileNameWithoutExtension();
        info.file = file;
        info.isFactory = false;
        info.modifiedTime = file.getLastModificationTime();
        presets.push_back (info);
    }
    
    // Sort by name
    std::sort (presets.begin(), presets.end(),
               [] (const PresetInfo& a, const PresetInfo& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });
    
    return presets;
}

std::vector<PresetManager::PresetInfo> PresetManager::getFactoryPresets() const
{
    std::vector<PresetInfo> presets;
    
    if (! factoryPresetsDir.exists())
        return presets;
    
    auto files = factoryPresetsDir.findChildFiles (juce::File::findFiles, false, "*.sitopreset");
    
    for (const auto& file : files)
    {
        PresetInfo info;
        info.name = file.getFileNameWithoutExtension();
        info.file = file;
        info.isFactory = true;
        info.modifiedTime = file.getLastModificationTime();
        presets.push_back (info);
    }
    
    // Sort by name
    std::sort (presets.begin(), presets.end(),
               [] (const PresetInfo& a, const PresetInfo& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });
    
    return presets;
}

void PresetManager::setCurrentPreset (const juce::File& file, const juce::String& name)
{
    currentPresetFile = file;
    currentPresetName = name;
    notifyCurrentPresetChanged();
}

void PresetManager::clearCurrentPreset()
{
    currentPresetFile = juce::File();
    currentPresetName = juce::String();
    notifyCurrentPresetChanged();
}

juce::String PresetManager::generateUniquePresetName (const juce::String& baseName) const
{
    auto presets = getUserPresets();
    
    // Check if base name is available
    bool nameExists = false;
    for (const auto& preset : presets)
    {
        if (preset.name.equalsIgnoreCase (baseName))
        {
            nameExists = true;
            break;
        }
    }
    
    if (! nameExists)
        return baseName;
    
    // Generate numbered variant
    int counter = 1;
    juce::String candidateName;
    
    do
    {
        candidateName = baseName + " " + juce::String (counter).paddedLeft ('0', 2);
        nameExists = false;
        
        for (const auto& preset : presets)
        {
            if (preset.name.equalsIgnoreCase (candidateName))
            {
                nameExists = true;
                break;
            }
        }
        
        ++counter;
    }
    while (nameExists && counter < 1000);
    
    return candidateName;
}

bool PresetManager::isValidPresetName (const juce::String& name) const
{
    if (name.isEmpty() || name.trim().isEmpty())
        return false;
    
    // Check for invalid filename characters
    const auto invalidChars = juce::String ("<>:\"/\\|?*");
    for (int i = 0; i < invalidChars.length(); ++i)
        if (name.containsChar (invalidChars[i]))
            return false;
    
    return true;
}

juce::String PresetManager::sanitizePresetName (const juce::String& name) const
{
    auto sanitized = name.trim();
    
    // Replace invalid characters with underscores
    const auto invalidChars = juce::String ("<>:\"/\\|?*");
    for (int i = 0; i < invalidChars.length(); ++i)
        sanitized = sanitized.replaceCharacter (invalidChars[i], '_');
    
    return sanitized;
}

void PresetManager::addListener (Listener* listener)
{
    listeners.add (listener);
}

void PresetManager::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void PresetManager::notifyPresetListChanged()
{
    listeners.call ([this] (Listener& l) { l.presetListChanged(); });
}

void PresetManager::notifyCurrentPresetChanged()
{
    listeners.call ([this] (Listener& l) { l.currentPresetChanged (currentPresetName); });
}
