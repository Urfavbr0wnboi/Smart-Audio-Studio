#include <juce_core/juce_core.h>

#include <iostream>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Smart Audio Studio");

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    if (runner.getNumResults() == 0)
    {
        std::cerr << "No tests were run." << std::endl;
        return 1;
    }

    return failures > 0 ? 1 : 0;
}
