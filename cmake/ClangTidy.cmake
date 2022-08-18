
set (WITH_CLANG_TIDY FALSE CACHE BOOL "Use Clang tidy")
mark_as_advanced(WITH_CLANG_TIDY)
if (WITH_CORE)
  if(WITH_CLANG_TIDY)
    find_program(
      CLANG_TIDY_EXE
      NAMES "clang-tidy"
      DOC "Path to clang-tidy executable"
      )
    if(NOT CLANG_TIDY_EXE)
      message(FATAL_ERROR "clang-tidy not found.")
    else()

      message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
      string(CONCAT CLANG_TIDY_ARGS "-checks=bugprone-*,"

        # TODO: Fix those
        # "-bugprone-branch-clone,"

        # Won't fix those
        "-bugprone-easily-swappable-parameters,"
        "-bugprone-narrowing-conversions,"
        "-bugprone-virtual-near-miss"
        )

      file(GLOB EXTERNAL_FILES RELATIVE ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/*/*.cpp
        ${CMAKE_SOURCE_DIR}/external/*/*.hpp
        ${CMAKE_SOURCE_DIR}/external/*/*.c
        ${CMAKE_SOURCE_DIR}/external/*/*.h)

      list(APPEND EXTERNAL_FILES "qgsmeshcalcparser.cpp" "qgsmeshcalclexer.cpp"
        "qgsexpressionparser.cpp" "qgsexpressionlexer.cpp"
        "qgssqlstatementparser.cpp" "qgssqlstatementlexer.cpp"
        "mocs_compilation.cpp")

      # line-filter is actually the file/line you WANT to get warning for!!!
      # so when we filter we need to specify some arbitrary and big line number to filter all...
      set(CLANG_TIDY_LINE_FILTER "[")
      foreach(EXTERNAL_FILE ${EXTERNAL_FILES})
        string(APPEND CLANG_TIDY_LINE_FILTER "{\"name\":\"${EXTERNAL_FILE}\",\"lines\":[[10000000,10000001]]},")
      endforeach()

      string(APPEND CLANG_TIDY_LINE_FILTER "{\"name\":\".cpp\"},{\"name\":\".h\"}]")

      set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "${CLANG_TIDY_ARGS}" "-line-filter=${CLANG_TIDY_LINE_FILTER}" "--export-fixes=/tmp/fixes.yaml")
    endif()
  endif()
endif()
