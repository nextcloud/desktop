# - MACRO_ADD_COMPILE_FLAGS(target_name flag1 ... flagN)

# SPDX-FileCopyrightText: 2006 Oswald Buddenhagen, <ossi@kde.org>
# SPDX-FileCopyrightText: 2006 Andreas Schneider, <asn@cryptomilk.org>
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


macro (MACRO_ADD_COMPILE_FLAGS _target)

  get_target_property(_flags ${_target} COMPILE_FLAGS)
  if (_flags)
    set(_flags ${_flags} ${ARGN})
  else (_flags)
    set(_flags ${ARGN})
  endif (_flags)

  set_target_properties(${_target} PROPERTIES COMPILE_FLAGS ${_flags})

endmacro (MACRO_ADD_COMPILE_FLAGS)
