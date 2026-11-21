#pragma once

// The build stamp: which commit a DLL was built from and when (UTC), e.g.
// "db84a1e+changes, 2026-11-21 19:20 UTC". Generated before every build by
// tools\build_stamp.ps1 (BuildStampGen.h under build\obj\, never committed), so test builds that
// share one release version can still be told apart. Only BuildStamp.cpp and the .rc include the
// generated header, so a new stamp recompiles just those.
namespace BuildStamp
{
    const char* Text();   // "db84a1e, 2026-11-21 19:20 UTC"
    const char* Commit(); // "db84a1e" or "db84a1e+changes"
    const char* Time();   // "2026-11-21 19:20 UTC"
}
