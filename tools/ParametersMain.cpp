// Entry point of the parameter tool. The work happens in Parameters.cpp, which is compiled into the
// plug-in's shared code so it sees exactly the same JUCE configuration as the plug-in.
int fphParameters (int argc, char** argv);

int main (int argc, char** argv)
{
    return fphParameters (argc, argv);
}
