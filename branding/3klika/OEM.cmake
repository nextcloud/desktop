set(APPLICATION_NAME "3klika Oblak" CACHE STRING "" FORCE)
set(APPLICATION_REV_DOMAIN "rs.3klika.oblak" CACHE STRING "" FORCE)

# Ako imaš PNG, koristi putanju ispod; ako nema – može ostati prazno.
set(OEM_THEME_DIR "${CMAKE_SOURCE_DIR}/branding/3klika" CACHE STRING "" FORCE)
set(APPLICATION_ICON "${CMAKE_SOURCE_DIR}/branding/3klika/icons/3klika.png" CACHE STRING "" FORCE)

# Isključenja
set(BUILD_UPDATER OFF CACHE BOOL "" FORCE)
set(BUILD_SHELL_INTEGRATION OFF CACHE BOOL "" FORCE)
set(WITH_CRASHREPORTER OFF CACHE BOOL "" FORCE)
