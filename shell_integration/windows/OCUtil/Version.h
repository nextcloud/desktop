#pragma once

// This is the number that will end up in the version window of the DLLs.
// Increment this version before committing a new build if you are today's shell_integration build master.
#define OCEXT_BUILD_NUM 43

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#define OCEXT_VERSION 1,0,0,OCEXT_BUILD_NUM
#define OCEXT_VERSION_STRING STRINGIZE(OCEXT_VERSION)
