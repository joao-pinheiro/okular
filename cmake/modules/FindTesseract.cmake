# SPDX-FileCopyrightText: 2026 Tunahan Yıldıran <tunahanyrd@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

#.rst:
# FindTesseract
# -------------
#
# Try to find the Tesseract OCR library.
#
# This will define the following variables:
#
# ``Tesseract_FOUND``
#     True if Tesseract was found.
# ``Tesseract_INCLUDE_DIRS``
#     The Tesseract include directories.
# ``Tesseract_LIBRARIES``
#     The libraries needed to use Tesseract.
#
# If Tesseract is found, the following imported target will also be defined:
#
# ``Tesseract::Tesseract``
#     The Tesseract library target.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Tesseract QUIET tesseract)
endif()

find_path(Tesseract_INCLUDE_DIR
    NAMES tesseract/baseapi.h
    HINTS ${PC_Tesseract_INCLUDE_DIRS}
)

find_library(Tesseract_LIBRARY
    NAMES tesseract libtesseract
    HINTS ${PC_Tesseract_LIBRARY_DIRS}
)

set(Tesseract_VERSION ${PC_Tesseract_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tesseract
    FOUND_VAR Tesseract_FOUND
    REQUIRED_VARS Tesseract_LIBRARY Tesseract_INCLUDE_DIR
    VERSION_VAR Tesseract_VERSION
)

if(Tesseract_FOUND)
    set(Tesseract_LIBRARIES ${Tesseract_LIBRARY})
    set(Tesseract_INCLUDE_DIRS ${Tesseract_INCLUDE_DIR})

    if(NOT TARGET Tesseract::Tesseract)
        add_library(Tesseract::Tesseract UNKNOWN IMPORTED)
        set_target_properties(Tesseract::Tesseract PROPERTIES
            IMPORTED_LOCATION "${Tesseract_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Tesseract_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Tesseract_INCLUDE_DIR Tesseract_LIBRARY)
