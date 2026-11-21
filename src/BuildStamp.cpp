#include "BuildStamp.h"

#include "BuildStampGen.h"

namespace BuildStamp
{
    const char* Text()   { return CHATMSG_BUILD_TEXT; }
    const char* Commit() { return CHATMSG_BUILD_COMMIT; }
    const char* Time()   { return CHATMSG_BUILD_TIME; }
}
