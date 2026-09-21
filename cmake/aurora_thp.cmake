add_library(aurora_thp STATIC
        lib/dolphin/thp/THPAudio.cpp
        lib/dolphin/thp/THPDec.cpp
)
add_library(aurora::thp ALIAS aurora_thp)
set_target_properties(aurora_thp PROPERTIES FOLDER "aurora")
target_link_libraries(aurora_thp PUBLIC aurora::core)

set_property(TARGET aurora_thp PROPERTY MSVC_RUNTIME_CHECKS "")
if (MSVC)
  set_source_files_properties(lib/dolphin/thp/THPAudio.cpp lib/dolphin/thp/THPDec.cpp
    PROPERTIES VS_SETTINGS "BasicRuntimeChecks=Default")
  target_compile_options(aurora_thp PRIVATE
    $<$<CONFIG:Debug>:/O2>
    $<$<CONFIG:Debug>:/Ob2>
    $<$<CONFIG:Debug>:/RTC->
  )
else ()
  target_compile_options(aurora_thp PRIVATE $<$<CONFIG:Debug>:-O2>)
endif ()
