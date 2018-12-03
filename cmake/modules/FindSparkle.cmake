# Find Sparkle.framework
#
# Once done this will define
#  SPARKLE_FOUND - system has Sparkle
#  SPARKLE_LIBRARY - The framework needed to use Sparkle
# Copyright (c) 2009, Vittorio Giovara <vittorio.giovara@gmail.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.

include(FindPackageHandleStandardArgs)

find_library(SPARKLE_LIBRARY NAMES Sparkle)

find_package_handle_standard_args(Sparkle DEFAULT_MSG SPARKLE_LIBRARY)
mark_as_advanced(SPARKLE_LIBRARY)

