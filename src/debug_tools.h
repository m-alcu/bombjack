#pragma once
// Command-line debug tools, run instead of the game:
//
//   --selftest [steps]            headless simulation smoke test
//   --rendertest [outdir]         draw every state headless (BMPs to outdir)
//   --shot <round> <out.bmp>      render a round's PLAYING view to a BMP
//   --demotrace <round> <cycle>   replay a demo and print Jack's path (CSV)
//   --enemytrace <round> <frames> print every enemy's sprite per frame (CSV)
//
// All of them seed the RNG so their output is reproducible. The SDL ones run
// headless with SDL_VIDEODRIVER=dummy.

// Run the tool named on the command line and return its exit code, or -1 if
// none was named.
int runDebugTool(int argc, char** argv);
