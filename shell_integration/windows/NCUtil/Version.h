#pragma once

// This is the number that will end up in the version window of the DLLs.
// Increment this version before committing a new build if you are today's shell_integration build master.
#define NCEXT_BUILD_NUM 47

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#define NCEXT_VERSION 3,0,0,NCEXT_BUILD_NUM
#define NCEXT_VERSION_STRING STRINGIZE(NCEXT_VERSION)
