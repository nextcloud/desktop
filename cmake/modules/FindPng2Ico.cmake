#.rst:
# FindPng2Ico
# -----------
#
# Try to find png2ico.
#
# If the png2ico executable is not in your PATH, you can provide
# an alternative name or full path location with the ``Png2Ico_EXECUTABLE``
# variable.
#
# This will define the following variables:
#
# ``Png2Ico_FOUND``
#     True if png2ico is available.
#
# ``Png2Ico_EXECUTABLE``
#     The png2ico executable.
#
# If ``Png2Ico_FOUND`` is TRUE, it will also define the following imported
# target:
#
# ``Png2Ico::Png2Ico``
#     The png2ico executable.
#
# and the following variables:
#
# ``Png2Ico_HAS_COLORS_ARGUMENT``
#     Whether png2ico accepts a ``--colors`` argument. `Matthias Benkmann's
#     tool <http://www.winterdrache.de/freeware/png2ico/>`_ does, while the
#     version of png2ico from the `"KDE On Windows" (kdewin)
#     <https://projects.kde.org/projects/kdesupport/kdewin>`_ project does not.
#
# ``Png2Ico_HAS_RCFILE_ARGUMENT``
#     Whether png2ico accepts an ``--rcfile`` argument. The version of png2ico
#     from the `"KDE On Windows" (kdewin)
#     <https://projects.kde.org/projects/kdesupport/kdewin>`_ project does,
#     while `Matthias Benkmann's tool
#     <http://www.winterdrache.de/freeware/png2ico/>`_ does not.
#
# Since 1.7.0.

#=============================================================================
# Copyright 2014 Alex Merry <alex.merry@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/ECMFindModuleHelpersStub.cmake)

ecm_find_package_version_check(Png2Ico)

# Find png2ico
find_program(Png2Ico_EXECUTABLE NAMES png2ico)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Png2Ico
    FOUND_VAR
        Png2Ico_FOUND
    REQUIRED_VARS
        Png2Ico_EXECUTABLE
)

mark_as_advanced(Png2Ico_EXECUTABLE)

if (Png2Ico_FOUND)
    execute_process(
        COMMAND "${Png2Ico_EXECUTABLE}" --help
        OUTPUT_VARIABLE _png2ico_help_text
        ERROR_VARIABLE _png2ico_help_text
    )
    if (_png2ico_help_text MATCHES ".*--rcfile .*")
        set(Png2Ico_HAS_RCFILE_ARGUMENT TRUE)
    else()
        set(Png2Ico_HAS_RCFILE_ARGUMENT FALSE)
    endif()
    if (_png2ico_help_text MATCHES ".*--colors .*")
        set(Png2Ico_HAS_COLORS_ARGUMENT TRUE)
    else()
        set(Png2Ico_HAS_COLORS_ARGUMENT FALSE)
    endif()
    unset(_png2ico_help_text)

    if (NOT TARGET Png2Ico::Png2Ico)
        add_executable(Png2Ico::Png2Ico IMPORTED)
        set_target_properties(Png2Ico::Png2Ico PROPERTIES
            IMPORTED_LOCATION "${Png2Ico_EXECUTABLE}"
        )
    endif()
endif()

include(FeatureSummary)
set_package_properties(Png2Ico PROPERTIES
    URL "http://www.winterdrache.de/freeware/png2ico/ or https://projects.kde.org/projects/kdesupport/kdewin"
    DESCRIPTION "Executable that converts a collection of PNG files into a Windows icon file"
)

