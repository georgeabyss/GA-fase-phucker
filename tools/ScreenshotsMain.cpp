// Entry point of the screenshot tool. The work happens in Screenshots.cpp, which is compiled
// into the plug-in's shared code so it sees exactly the same JUCE configuration as the plug-in.
int fphRenderScreenshots (int argc, char** argv);

int main (int argc, char** argv)
{
    return fphRenderScreenshots (argc, argv);
}
