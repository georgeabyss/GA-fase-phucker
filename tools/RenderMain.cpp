// Entry point of the DSP test tool. The work happens in Render.cpp, which is compiled into the
// plug-in's shared code so it sees exactly the same JUCE configuration as the plug-in.
int fphRender (int argc, char** argv);

int main (int argc, char** argv)
{
    return fphRender (argc, argv);
}
