cmake_minimum_required(VERSION 3.16)
project(rgTorrent VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
        Core Widgets LinguistTools
)

find_package(OpenSSL REQUIRED)
find_package(uriparser REQUIRED)
find_package(Threads REQUIRED)

set(C_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rgTorrent-c"
        CACHE PATH "Path to the rgTorrent C backend source root")

set(C_SOURCES
        ${C_BACKEND_DIR}/torrent_session.c
        ${C_BACKEND_DIR}/bencoding/bencoder.c
        ${C_BACKEND_DIR}/bencoding/bencode_parser.c
        ${C_BACKEND_DIR}/helpers/helpers.c
        ${C_BACKEND_DIR}/connectivity/announce_connector.c
        ${C_BACKEND_DIR}/helpers/request_helpers.c
        ${C_BACKEND_DIR}/connectivity/handshake/handshake.c
        ${C_BACKEND_DIR}/downloader/downloader.c
        ${C_BACKEND_DIR}/downloader/file_saver.c
        ${C_BACKEND_DIR}/swarm/swarm.c
        # main.c is intentionally excluded - Qt's main() replaces it.
)

set(C_INCLUDE_DIRS
        ${C_BACKEND_DIR}
        ${C_BACKEND_DIR}/helpers
        ${C_BACKEND_DIR}/bencoding
        ${C_BACKEND_DIR}/connectivity
        ${C_BACKEND_DIR}/connectivity/handshake
        ${C_BACKEND_DIR}/downloader
        ${C_BACKEND_DIR}/swarm
)

set(TS_FILES
        i18n/rgTorrent_en.ts
        i18n/rgTorrent_uk.ts
)

set(QT_SOURCES
        src/main.cpp

        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/TorrentListWidget.cpp
        src/ui/TorrentListWidget.h
        src/ui/TorrentDetailsPanel.cpp
        src/ui/TorrentDetailsPanel.h
        src/ui/CreateTorrentDialog.cpp
        src/ui/CreateTorrentDialog.h
        src/ui/TorrentContentDialog.cpp
        src/ui/TorrentContentDialog.h
        src/ui/ThemeManager.cpp
        src/ui/ThemeManager.h

        src/backend/TorrentBackend.cpp
        src/backend/TorrentBackend.h
        src/backend/TorrentItem.h

        resources/resources.qrc
)

qt_add_executable(rgTorrent ${QT_SOURCES} ${C_SOURCES})

qt_add_translations(rgTorrent
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
)

target_include_directories(rgTorrent PRIVATE
        src
        ${C_INCLUDE_DIRS}
)

target_link_libraries(rgTorrent PRIVATE
        Qt6::Core
        Qt6::Widgets
        OpenSSL::SSL
        OpenSSL::Crypto
        uriparser::uriparser
        Threads::Threads
)

install(TARGETS rgTorrent DESTINATION bin)