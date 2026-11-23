cmake_minimum_required(VERSION 3.16)

project(GuitarTabCreator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES GuitarTabCreator_es_CO.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        tipos.h
        acordes.h
        acordes.cpp
        audio_player.h
        audio_player.cpp
        sintetizador.h
        sintetizador_aditivo.h
        karplus_strong.h
        csv_parser.h
        csv_parser.cpp
        tab_generator.h
        tab_generator.cpp
        mapper/mapper.h
        mapper/mapper.cpp
        mapper/tipos_mapper.h
        mapper/viterbi.h
        mapper/motor_melodico.h
        mapper/analizador_patrones.h
        mapper/candidatos.h
        mapper/formas_conocidas.h
        mapper/merger.h
        ${TS_FILES}
        resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(GuitarTabCreator
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(GuitarTabCreator SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(GuitarTabCreator
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(GuitarTabCreator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mapper
)

target_link_libraries(GuitarTabCreator PRIVATE Qt${QT_VERSION_MAJOR}::Widgets winmm)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.GuitarTabCreator)
endif()
set_target_properties(GuitarTabCreator PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS GuitarTabCreator
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(GuitarTabCreator)
endif()
