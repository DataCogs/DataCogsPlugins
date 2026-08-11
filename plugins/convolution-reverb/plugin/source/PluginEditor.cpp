#include "PluginEditor.h"
#include "DataCogsAssets.h"

namespace
{
constexpr int kKnobSize = 94;   // house-standard knob cell, same as the compressor
constexpr int kKnobsPerRow = 5;
constexpr int kKnobRows = 2;
constexpr int kIconRowHeight = 34;  // pictogram strip between label and knob
constexpr int kKnobRowHeight = 150 + kIconRowHeight;
constexpr int kPhotoPanelWidth = 200;
constexpr int kHeaderHeight = 40; // house standard, incl. separator line
constexpr int kEditorWidth = kKnobsPerRow * (kKnobSize + 14) + 28 + kPhotoPanelWidth + 14;
constexpr int kEditorHeight = kHeaderHeight + 10 + 30 + 8 + kKnobRows * kKnobRowHeight + 10;

// Jokey engraved pictograms, one per parameter: mix = a cocktail, pre-delay
// = a snail, decay = a tooth with a cavity, early = the bird that got the
// worm, output = a wall outlet. Drawn on a 24x24 grid and scaled into
// place; stroke-only "etched faceplate" style so the gags whisper rather
// than shout. Keyed by parameter ID.
void drawParameterIcon (juce::Graphics& g, const juce::String& icon, juce::Rectangle<float> area)
{
    const float scale = juce::jmin (area.getWidth(), area.getHeight()) / 24.0f;

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale (scale).translated (
        area.getCentreX() - 12.0f * scale, area.getCentreY() - 12.0f * scale));
    g.setColour (DataCogsLookAndFeel::textDim);

    const juce::PathStrokeType stroke (1.5f, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);
    juce::Path p;

    if (icon == "mix")                  // shaken, not stirred
    {
        p.startNewSubPath (4.0f, 5.0f);  p.lineTo (20.0f, 5.0f);    // martini bowl
        p.lineTo (12.0f, 14.5f);         p.closeSubPath();
        p.startNewSubPath (12.0f, 14.5f); p.lineTo (12.0f, 20.0f);  // stem
        p.startNewSubPath (7.5f, 21.0f);  p.lineTo (16.5f, 21.0f);  // base
        p.startNewSubPath (10.5f, 6.8f);  p.lineTo (15.5f, 1.5f);   // cocktail pick
        g.strokePath (p, stroke);
        g.fillEllipse (8.6f, 6.4f, 3.4f, 3.4f);                     // the olive
    }
    else if (icon == "preDelay")        // it'll get there eventually
    {
        p.startNewSubPath (3.0f, 20.5f);                            // foot along the ground
        p.lineTo (17.0f, 20.5f);
        p.quadraticTo (19.6f, 20.5f, 19.8f, 17.5f);                 // neck curving up
        p.lineTo (19.8f, 13.5f);
        p.startNewSubPath (19.0f, 12.0f); p.lineTo (17.2f, 8.6f);   // eyestalks
        p.startNewSubPath (20.6f, 12.0f); p.lineTo (22.2f, 8.6f);
        g.strokePath (p, stroke);
        g.fillEllipse (16.4f, 7.4f, 1.6f, 1.6f);                    // eyes on stalks
        g.fillEllipse (21.4f, 7.4f, 1.6f, 1.6f);
        g.fillEllipse (4.0f, 8.0f, 11.5f, 11.5f);                   // the shell
        juce::Path spiral;                                          // what sells the snail
        spiral.addCentredArc (9.75f, 13.75f, 3.4f, 3.4f, 0.0f,
                              0.0f, juce::MathConstants<float>::pi * 1.6f, true);
        g.setColour (DataCogsLookAndFeel::background);
        g.strokePath (spiral, juce::PathStrokeType (1.4f));
    }
    else if (icon == "size")            // measure twice, reverb once
    {
        juce::Path body;                                            // tape-measure case
        body.addRoundedRectangle (2.5f, 5.5f, 12.5f, 12.5f, 3.0f);
        g.fillPath (body);
        g.setColour (DataCogsLookAndFeel::background);
        g.fillEllipse (7.0f, 10.0f, 3.5f, 3.5f);                    // the reel hub
        g.setColour (DataCogsLookAndFeel::textDim);
        p.startNewSubPath (15.0f, 14.8f); p.lineTo (21.2f, 14.8f);  // tape pulled out
        p.startNewSubPath (15.0f, 18.0f); p.lineTo (21.2f, 18.0f);
        p.startNewSubPath (21.2f, 13.6f); p.lineTo (21.2f, 19.2f);  // end hook
        p.startNewSubPath (17.2f, 14.8f); p.lineTo (17.2f, 16.4f);  // tick marks
        p.startNewSubPath (19.2f, 14.8f); p.lineTo (19.2f, 16.4f);
        g.strokePath (p, stroke);
    }
    else if (icon == "decay")           // should have flossed
    {
        p.startNewSubPath (5.5f, 8.0f);                             // molar crown
        p.quadraticTo (5.0f, 3.0f, 9.0f, 3.5f);
        p.quadraticTo (12.0f, 4.8f, 15.0f, 3.5f);
        p.quadraticTo (19.0f, 3.0f, 18.5f, 8.0f);
        p.quadraticTo (18.2f, 12.0f, 16.5f, 15.0f);                 // down into two roots
        p.quadraticTo (15.7f, 20.6f, 14.0f, 20.2f);
        p.quadraticTo (12.9f, 19.6f, 12.9f, 16.5f);
        p.quadraticTo (12.0f, 14.4f, 11.1f, 16.5f);
        p.quadraticTo (11.1f, 19.6f, 10.0f, 20.2f);
        p.quadraticTo (8.3f, 20.6f, 7.5f, 15.0f);
        p.quadraticTo (5.8f, 12.0f, 5.5f, 8.0f);
        p.closeSubPath();
        g.strokePath (p, stroke);
        g.fillEllipse (13.0f, 6.2f, 3.4f, 3.0f);                    // the cavity
    }
    else if (icon == "early")           // the bird got the worm
    {
        g.fillEllipse (4.5f, 10.5f, 10.0f, 7.0f);                   // plump body
        g.fillEllipse (12.0f, 6.0f, 5.0f, 5.0f);                    // head
        juce::Path beak;
        beak.addTriangle (16.6f, 7.6f, 20.2f, 8.6f, 16.6f, 9.6f);
        g.fillPath (beak);
        p.startNewSubPath (19.4f, 9.2f);                            // the worm, dangling
        p.quadraticTo (20.4f, 12.0f, 18.6f, 13.6f);
        p.quadraticTo (16.8f, 15.2f, 18.0f, 17.0f);
        p.startNewSubPath (5.2f, 12.0f); p.lineTo (1.8f, 10.2f);    // tail feathers
        p.startNewSubPath (5.2f, 13.6f); p.lineTo (1.8f, 13.2f);
        p.startNewSubPath (8.5f, 17.4f); p.lineTo (8.5f, 21.0f);    // legs, up early
        p.startNewSubPath (11.5f, 17.4f); p.lineTo (11.5f, 21.0f);
        p.startNewSubPath (3.0f, 21.0f); p.lineTo (21.0f, 21.0f);   // the ground
        g.strokePath (p, stroke);
        g.setColour (DataCogsLookAndFeel::background);
        g.fillEllipse (14.6f, 7.2f, 1.4f, 1.4f);                    // eye
    }
    else if (icon == "tail")            // happy to see you
    {
        p.addRoundedRectangle (5.5f, 11.0f, 10.0f, 6.5f, 2.5f);     // dog body
        p.addEllipse (13.5f, 5.5f, 5.5f, 5.5f);                     // head
        p.startNewSubPath (17.8f, 6.4f); p.lineTo (19.6f, 3.8f);    // ear
        p.startNewSubPath (7.5f, 17.5f);  p.lineTo (7.5f, 21.0f);   // legs
        p.startNewSubPath (13.5f, 17.5f); p.lineTo (13.5f, 21.0f);
        g.strokePath (p, stroke);
        juce::Path tail;                                            // the star of the show
        tail.startNewSubPath (5.8f, 12.5f);
        tail.quadraticTo (1.6f, 9.5f, 3.0f, 4.5f);
        tail.quadraticTo (4.6f, 8.0f, 7.0f, 10.8f);
        tail.closeSubPath();
        g.fillPath (tail);
        p.clear();                                                  // wag marks
        p.startNewSubPath (0.8f, 3.6f); p.lineTo (1.9f, 2.3f);
        p.startNewSubPath (4.6f, 2.6f); p.lineTo (5.3f, 1.1f);
        g.strokePath (p, stroke);
    }
    else if (icon == "width")           // wide stance
    {
        p.addEllipse (5.0f, 8.0f, 14.0f, 11.0f);                    // sumo body
        p.addEllipse (10.2f, 3.0f, 3.6f, 3.6f);                     // head
        p.startNewSubPath (5.4f, 10.5f);  p.lineTo (1.2f, 9.0f);    // arms out wide
        p.startNewSubPath (18.6f, 10.5f); p.lineTo (22.8f, 9.0f);
        p.startNewSubPath (8.5f, 18.6f);  p.lineTo (6.5f, 22.5f);   // wide stance
        p.startNewSubPath (15.5f, 18.6f); p.lineTo (17.5f, 22.5f);
        g.strokePath (p, stroke);
        g.fillRoundedRectangle (5.2f, 12.6f, 13.6f, 3.2f, 1.5f);    // the mawashi
        g.fillEllipse (12.6f, 2.4f, 1.6f, 1.6f);                    // topknot
    }
    else if (icon == "lowCut")          // daring neckline
    {
        juce::Path dress;
        dress.startNewSubPath (6.5f, 3.5f);                         // left shoulder strap
        dress.quadraticTo (7.0f, 9.0f, 8.0f, 12.0f);                // in at the waist
        dress.quadraticTo (5.0f, 17.5f, 4.5f, 21.0f);               // out to the hem
        dress.lineTo (19.5f, 21.0f);
        dress.quadraticTo (19.0f, 17.5f, 16.0f, 12.0f);
        dress.quadraticTo (17.0f, 9.0f, 17.5f, 3.5f);               // right shoulder strap
        dress.lineTo (12.0f, 16.0f);                                // the LOW cut
        dress.closeSubPath();
        g.fillPath (dress);
        p.startNewSubPath (20.6f, 4.6f); p.lineTo (22.0f, 3.2f);    // sparkle
        p.startNewSubPath (21.9f, 4.7f); p.lineTo (20.7f, 3.1f);
        g.strokePath (p, stroke);
    }
    else if (icon == "highCut")         // topiary, the brutal kind
    {
        juce::Path tree;                                            // what's left of the pine
        tree.addTriangle (4.5f, 17.0f, 17.5f, 17.0f, 11.0f, 7.0f);
        g.fillPath (tree);
        p.startNewSubPath (11.0f, 17.0f); p.lineTo (11.0f, 21.0f);  // trunk
        p.startNewSubPath (6.5f, 21.0f);  p.lineTo (15.5f, 21.0f);  // ground
        p.startNewSubPath (13.5f, 2.0f);                            // the lopped-off top,
        p.lineTo (17.4f, 5.2f);                                     // tumbling away
        p.lineTo (12.4f, 5.6f);
        p.closeSubPath();
        p.startNewSubPath (9.2f, 4.4f);  p.lineTo (10.4f, 5.6f);    // snip marks
        p.startNewSubPath (19.0f, 7.0f); p.lineTo (20.4f, 8.2f);
        g.strokePath (p, stroke);
    }
    else if (icon == "outGain")         // plug in here
    {
        juce::Path plate;
        plate.addRoundedRectangle (5.0f, 4.0f, 14.0f, 16.0f, 2.5f); // faceplate
        g.fillPath (plate);
        g.setColour (DataCogsLookAndFeel::background);
        g.fillRoundedRectangle (8.9f, 8.0f, 1.9f, 4.2f, 0.9f);      // the slots
        g.fillRoundedRectangle (13.2f, 8.0f, 1.9f, 4.2f, 0.9f);
        g.fillEllipse (10.8f, 14.2f, 2.4f, 2.4f);                   // earth pin hole
    }
}

// Hover text for the icons: what the parameter actually does, in plain
// words - the icons carry the joke, the tooltips carry the manual.
juce::String parameterTooltip (const juce::String& parameterID)
{
    if (parameterID == "mix")
        return "Mix: dry/wet balance. 0 % is the untouched signal, 100 % is "
               "reverb only - keep it low for a subtle room, high for a wash.";
    if (parameterID == "preDelay")
        return "Pre-Delay: a gap between the dry sound and the first reflection. "
               "A little keeps the source upfront while the room blooms behind it.";
    if (parameterID == "size")
        return "Size: stretches or shrinks the impulse response, scaling the whole "
               "space - below 100 % tightens the room, above it the hall grows.";
    if (parameterID == "decay")
        return "Decay: rescales the reverb time (RT60) of the loaded space. "
               "100 % is the room as captured; lower dries it up, higher lets it ring.";
    if (parameterID == "early")
        return "Early: level of the early reflections - the first bounces that tell "
               "you how close the walls are. Up for presence, down for distance.";
    if (parameterID == "tail")
        return "Tail: level of the late reverb tail - the long wash after the first "
               "bounces. Down for clarity, up for lushness.";
    if (parameterID == "width")
        return "Width: stereo width of the wet signal. 0 % collapses the reverb "
               "to mono; 200 % pushes it out beyond the speakers.";
    if (parameterID == "lowCut")
        return "Low Cut: high-pass filter on the wet signal only. Raise it to keep "
               "the reverb from muddying the low end.";
    if (parameterID == "highCut")
        return "High Cut: low-pass filter (damping) on the wet signal. Lower it "
               "for a darker, warmer space.";
    if (parameterID == "outGain")
        return "Output: final level trim after the dry/wet mix, to match the "
               "bypassed level.";
    return {};
}
} // namespace

void ConvolutionReverbAudioProcessorEditor::ParameterIcon::setIcon (const juce::String& parameterID)
{
    icon = parameterID;
    setTooltip (parameterTooltip (parameterID));
}

void ConvolutionReverbAudioProcessorEditor::ParameterIcon::paint (juce::Graphics& g)
{
    drawParameterIcon (g, icon, getLocalBounds().toFloat().reduced (0.0f, 1.0f));
}

//==============================================================================
ConvolutionReverbAudioProcessorEditor::ConvolutionReverbAudioProcessorEditor (
    ConvolutionReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (DataCogsAssets::datacogs_logo_png,
                                            DataCogsAssets::datacogs_logo_pngSize);

    setupKnob (mixSlider,      mixLabel,      mixIcon,      "Mix",       "mix",      mixAttachment);
    setupKnob (preDelaySlider, preDelayLabel, preDelayIcon, "Pre-Delay", "preDelay", preDelayAttachment);
    setupKnob (sizeSlider,     sizeLabel,     sizeIcon,     "Size",      "size",     sizeAttachment);
    setupKnob (decaySlider,    decayLabel,    decayIcon,    "Decay",     "decay",    decayAttachment);
    setupKnob (widthSlider,    widthLabel,    widthIcon,    "Width",     "width",    widthAttachment);
    setupKnob (earlySlider,    earlyLabel,    earlyIcon,    "Early",     "early",    earlyAttachment);
    setupKnob (tailSlider,     tailLabel,     tailIcon,     "Tail",      "tail",     tailAttachment);
    setupKnob (lowCutSlider,   lowCutLabel,   lowCutIcon,   "Low Cut",   "lowCut",   lowCutAttachment);
    setupKnob (highCutSlider,  highCutLabel,  highCutIcon,  "High Cut",  "highCut",  highCutAttachment);
    setupKnob (gainSlider,     gainLabel,     gainIcon,     "Output",    "outGain",  gainAttachment);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getValueTreeState(), "bypass", bypassButton);
    addAndMakeVisible (bypassButton);
    reverseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getValueTreeState(), "reverse", reverseButton);
    addAndMakeVisible (reverseButton);

    resetButton.setTooltip ("Set all dials back to their defaults (keeps the loaded IR)");
    resetButton.onClick = [this]
    {
        processorRef.resetParametersToDefaults();
        presetBox.setSelectedId (0, juce::dontSendNotification); // no preset active now
    };
    addAndMakeVisible (resetButton);

    irBox.setTextWhenNothingSelected ("Select impulse response...");
    refreshIrList();
    irBox.onChange = [this]
    {
        const int id = irBox.getSelectedId();
        if (id == 1)
            processorRef.loadBuiltInImpulseResponse();
        else if (id >= 2 && static_cast<size_t> (id - 2) < irFiles.size())
            processorRef.loadImpulseResponseFile (irFiles[static_cast<size_t> (id - 2)]);
        updateIrPhoto();
        updateFavouriteButton();
    };
    addAndMakeVisible (irBox);

    loadIrButton.onClick = [this] { openIrFileChooser(); };
    addAndMakeVisible (loadIrButton);

    favouriteButton.onClick = [this] { toggleFavouriteForCurrentIr(); };
    addAndMakeVisible (favouriteButton);
    updateFavouriteButton();

    presetBox.setTextWhenNothingSelected ("Presets...");
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index < 0)
            return;
        processorRef.setCurrentProgram (index);
        refreshIrList();
        updateIrPhoto();
        updateFavouriteButton();
    };
    addAndMakeVisible (presetBox);

    irPhoto.setImagePlacement (juce::RectanglePlacement::centred
                               | juce::RectanglePlacement::onlyReduceInSize);
    addAndMakeVisible (irPhoto);
    noPhotoLabel.setText ("no photo", juce::dontSendNotification);
    noPhotoLabel.setJustificationType (juce::Justification::centred);
    noPhotoLabel.setColour (juce::Label::textColourId,
                            noPhotoLabel.findColour (juce::Label::textColourId).withAlpha (0.4f));
    addAndMakeVisible (noPhotoLabel);
    updateIrPhoto();

    setSize (kEditorWidth, kEditorHeight);
}

ConvolutionReverbAudioProcessorEditor::~ConvolutionReverbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::File ConvolutionReverbAudioProcessorEditor::getIrLibraryFolder()
{
    return ConvolutionReverbAudioProcessor::getIrLibraryFolder();
}

juce::String ConvolutionReverbAudioProcessorEditor::libraryRelativePath (const juce::File& file)
{
    // Relative to whichever library root actually contains the file - so
    // favourites and display names work for both the user's own library
    // and the installer's system-wide one.
    for (const auto& root : ConvolutionReverbAudioProcessor::getIrLibraryRoots())
    {
        const auto rel = file.getRelativePathFrom (root);
        if (! rel.startsWith ("..") && ! juce::File::isAbsolutePath (rel))
            return rel;
    }
    return {};
}

juce::StringArray ConvolutionReverbAudioProcessorEditor::loadFavourites()
{
    juce::StringArray lines;
    const auto file = getIrLibraryFolder().getChildFile ("favourites.txt");
    if (file.existsAsFile())
        file.readLines (lines);
    lines.removeEmptyStrings (true);
    return lines;
}

void ConvolutionReverbAudioProcessorEditor::saveFavourites (const juce::StringArray& relPaths)
{
    // replaceWithText, NOT FileOutputStream: the latter opens in append
    // mode and silently grows the file on every save.
    getIrLibraryFolder().getChildFile ("favourites.txt")
        .replaceWithText (relPaths.joinIntoString ("\n") + "\n");
}

void ConvolutionReverbAudioProcessorEditor::refreshIrList()
{
    irBox.clear (juce::dontSendNotification);
    irFiles.clear();

    irBox.addItem ("Built-in Hall", 1);

    int nextId = 2;
    auto addEntry = [&] (const juce::File& file, const juce::String& display)
    {
        irBox.addItem (display, nextId++);
        irFiles.push_back (file);
    };

    // Favourites first, in their own section. Entries whose file has since
    // vanished are skipped (but kept in the file, in case a pack returns).
    const auto favourites = loadFavourites();
    bool anyFavourite = false;
    for (const auto& rel : favourites)
    {
        const auto file = ConvolutionReverbAudioProcessor::resolveIrLibraryFile (rel);
        if (! file.existsAsFile())
            continue;
        if (! anyFavourite)
        {
            irBox.addSectionHeading (juce::String::fromUTF8 ("\xE2\x98\x85 Favourites"));
            anyFavourite = true;
        }
        addEntry (file, rel.upToLastOccurrenceOf (".", false, false));
    }
    if (anyFavourite)
        irBox.addSectionHeading ("Library");

    // Recursive so IR packs can keep their folder structure; the item shows
    // "pack/room" for anything nested one level down. Both library roots
    // are scanned; a user-library file shadows a system one at the same
    // relative path.
    juce::StringArray seenRelPaths;
    for (const auto& root : ConvolutionReverbAudioProcessor::getIrLibraryRoots())
    {
        if (! root.isDirectory())
            continue;
        for (const auto& entry : juce::RangedDirectoryIterator (
                 root, true, "*.wav;*.aif;*.aiff;*.flac", juce::File::findFiles))
        {
            const auto file = entry.getFile();
            const auto rel = file.getRelativePathFrom (root);
            if (seenRelPaths.contains (rel))
                continue;
            seenRelPaths.add (rel);
            addEntry (file, rel.dropLastCharacters (file.getFileExtension().length()));
        }
    }

    // Reflect what the processor currently has loaded.
    const auto current = processorRef.getCurrentIrFile();
    if (current == juce::File())
    {
        irBox.setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        for (size_t i = 0; i < irFiles.size(); ++i)
            if (irFiles[i] == current)
                irBox.setSelectedId (static_cast<int> (i) + 2, juce::dontSendNotification);
    }
}

void ConvolutionReverbAudioProcessorEditor::openIrFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select an impulse response",
        getIrLibraryFolder().exists() ? getIrLibraryFolder()
                                      : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            if (processorRef.loadImpulseResponseFile (file))
            {
                refreshIrList();
                updateIrPhoto();
            }
            else
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Convolution Reverb",
                    "Couldn't read \"" + file.getFileName() + "\" as an audio file.");
        });
}

void ConvolutionReverbAudioProcessorEditor::toggleFavouriteForCurrentIr()
{
    const auto current = processorRef.getCurrentIrFile();
    if (current == juce::File())
        return; // the built-in hall needs no bookmark

    const auto rel = libraryRelativePath (current);
    if (rel.isEmpty())
        return; // outside the library - nothing stable to reference

    auto favourites = loadFavourites();
    if (favourites.contains (rel))
        favourites.removeString (rel);
    else
        favourites.add (rel);

    saveFavourites (favourites);
    refreshIrList();
    updateFavouriteButton();
}

void ConvolutionReverbAudioProcessorEditor::updateFavouriteButton()
{
    const auto current = processorRef.getCurrentIrFile();
    const auto rel = current == juce::File()
                         ? juce::String()
                         : libraryRelativePath (current);

    const bool inLibrary = rel.isNotEmpty();
    const bool isFavourite = inLibrary && loadFavourites().contains (rel);

    favouriteButton.setEnabled (inLibrary);
    favouriteButton.setButtonText (juce::String::fromUTF8 (isFavourite ? "\xE2\x98\x85"    // filled star
                                                                       : "\xE2\x98\x86")); // outline star
    favouriteButton.setTooltip (isFavourite ? "Remove from favourites" : "Add to favourites");
}

void ConvolutionReverbAudioProcessorEditor::updateIrPhoto()
{
    const auto irFile = processorRef.getCurrentIrFile();
    juce::Image image;

    if (irFile != juce::File())
    {
        for (const char* ext : { ".jpg", ".jpeg", ".png" })
        {
            const auto candidate = irFile.withFileExtension (ext);
            if (candidate.existsAsFile())
            {
                image = juce::ImageFileFormat::loadFrom (candidate);
                if (image.isValid())
                    break;
            }
        }
    }

    irPhoto.setImage (image); // invalid image just clears the component
    noPhotoLabel.setVisible (! image.isValid());
}

//==============================================================================
void ConvolutionReverbAudioProcessorEditor::setupKnob (
    juce::Slider& slider, juce::Label& label, ParameterIcon& icon,
    const juce::String& text, const juce::String& paramID,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment)
{
    icon.setIcon (paramID);
    addAndMakeVisible (icon);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // 22px tall so the in-place editor's text isn't crushed against the
    // plate edges when a value is double-clicked for editing.
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kKnobSize, 22);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getValueTreeState(), paramID, slider);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, DataCogsLookAndFeel::text);
    addAndMakeVisible (label);
}

void ConvolutionReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (DataCogsLookAndFeel::background);

    auto header = getLocalBounds().removeFromTop (kHeaderHeight);

    // Gears logo, then the wordmark: bright "DATACOGS", dim "CONVOLUTION REVERB".
    auto brand = header.withTrimmedLeft (14);
    if (logo.isValid())
    {
        const int logoHeight = 28;
        const int logoWidth = logoHeight * logo.getWidth() / logo.getHeight();
        g.drawImageWithin (logo, brand.getX(), header.getCentreY() - logoHeight / 2,
                           logoWidth, logoHeight, juce::RectanglePlacement::centred);
        brand.removeFromLeft (logoWidth + 10);
    }

    const juce::Font brandFont { juce::FontOptions (15.0f, juce::Font::bold) };
    g.setFont (brandFont);
    g.setColour (DataCogsLookAndFeel::text);
    g.drawText ("DATACOGS", brand, juce::Justification::centredLeft);

    juce::GlyphArrangement measure;
    measure.addLineOfText (brandFont, "DATACOGS", 0.0f, 0.0f);
    brand.removeFromLeft ((int) std::ceil (measure.getBoundingBox (0, -1, true).getWidth()) + 8);
    g.setColour (DataCogsLookAndFeel::textDim);
    g.drawText ("CONVOLUTION REVERB", brand, juce::Justification::centredLeft);

    // House-standard header separator (all DataCogs plugins).
    g.setColour (DataCogsLookAndFeel::outline);
    g.drawHorizontalLine (kHeaderHeight, 0.0f, static_cast<float> (getWidth()));

    // Panel behind the photo, so "no photo" still reads as a deliberate
    // frame rather than an empty corner.
    auto photoArea = irPhoto.getBounds().toFloat().expanded (4.0f);
    g.setColour (DataCogsLookAndFeel::panel);
    g.fillRoundedRectangle (photoArea, 4.0f);
    g.setColour (DataCogsLookAndFeel::outline);
    g.drawRoundedRectangle (photoArea.reduced (0.5f), 4.0f, 1.0f);
}

void ConvolutionReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // House-standard header: 40 px band (brand and separator line painted
    // in paint()), title left, right-aligned trio, then a consistent 10 px
    // gap before content.
    auto topRow = area.removeFromTop (kHeaderHeight).reduced (14, 7);
    bypassButton.setBounds (topRow.removeFromRight (80));
    topRow.removeFromRight (8);
    resetButton.setBounds (topRow.removeFromRight (64));
    topRow.removeFromRight (8);
    presetBox.setBounds (topRow.removeFromRight (170));

    area.removeFromTop (10);
    area = area.reduced (14, 0).withTrimmedBottom (10);

    auto photoPanel = area.removeFromRight (kPhotoPanelWidth);
    area.removeFromRight (14);
    photoPanel.removeFromTop (4);
    irPhoto.setBounds (photoPanel);
    noPhotoLabel.setBounds (photoPanel);

    // Reverse lives with the IR row - it's an IR-domain control.
    auto irRow = area.removeFromTop (30).reduced (0, 2);
    reverseButton.setBounds (irRow.removeFromLeft (90));
    irRow.removeFromLeft (8);
    loadIrButton.setBounds (irRow.removeFromRight (90));
    irRow.removeFromRight (8);
    favouriteButton.setBounds (irRow.removeFromRight (30));
    irRow.removeFromRight (8);
    irBox.setBounds (irRow);

    area.removeFromTop (8);

    juce::Slider* sliders[] = { &mixSlider, &preDelaySlider, &sizeSlider, &decaySlider, &widthSlider,
                                &earlySlider, &tailSlider, &lowCutSlider, &highCutSlider, &gainSlider };
    juce::Label* labels[]   = { &mixLabel, &preDelayLabel, &sizeLabel, &decayLabel, &widthLabel,
                                &earlyLabel, &tailLabel, &lowCutLabel, &highCutLabel, &gainLabel };
    ParameterIcon* icons[]  = { &mixIcon, &preDelayIcon, &sizeIcon, &decayIcon, &widthIcon,
                                &earlyIcon, &tailIcon, &lowCutIcon, &highCutIcon, &gainIcon };

    const int cell = area.getWidth() / kKnobsPerRow;
    for (int row = 0; row < kKnobRows; ++row)
    {
        auto knobRow = area.removeFromTop (kKnobRowHeight);
        for (int col = 0; col < kKnobsPerRow; ++col)
        {
            const int i = row * kKnobsPerRow + col;
            auto cellArea = knobRow.removeFromLeft (cell);
            labels[i]->setBounds (cellArea.removeFromTop (18));
            icons[i]->setBounds (cellArea.removeFromTop (kIconRowHeight));
            sliders[i]->setBounds (cellArea.withSizeKeepingCentre (kKnobSize, cellArea.getHeight()));
        }
    }
}
