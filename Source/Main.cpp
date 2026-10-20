#include "MainComponent.h"
#include "UI/StudioLookAndFeel.h"

namespace sas
{

class SmartAudioStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        openFiles (commandLine);
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        openFiles (commandLine);
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (auto* content = mainWindow != nullptr ? dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) : nullptr)
            content->requestQuit ([] { JUCEApplication::quit(); });
        else
            quit();
    }

private:
    /** Opens any projects or audio files named on the command line. */
    void openFiles (const juce::String& commandLine)
    {
        juce::Array<juce::File> files;

        for (const auto& argument : juce::StringArray::fromTokens (commandLine, true))
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argument.unquoted());

            if (file.existsAsFile())
                files.add (file);
        }

        if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()); content != nullptr && ! files.isEmpty())
            content->openFiles (files);
    }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, Theme::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            auto content = std::make_unique<MainComponent>();
            content->onTitleChanged = [this] (const juce::String& title) { setName (title); };
            content->refreshTitle();
            setContentOwned (content.release(), true);
            setResizable (true, true);
            setResizeLimits (900, 600, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            if (auto* mainComponent = getContentComponent())
                mainComponent->grabKeyboardFocus();
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    StudioLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace sas

START_JUCE_APPLICATION (sas::SmartAudioStudioApplication)
