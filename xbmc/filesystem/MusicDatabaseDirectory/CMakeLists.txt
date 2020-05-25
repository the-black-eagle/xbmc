set(SOURCES DirectoryNodeAlbum.cpp
            DirectoryNodeArtist.cpp
            DirectoryNodeDiscs.cpp
            DirectoryNode.cpp
            DirectoryNodeGrouped.cpp
            DirectoryNodeOverview.cpp
            DirectoryNodeRoot.cpp
            DirectoryNodeSingles.cpp
            DirectoryNodeSong.cpp
            QueryParams.cpp)

set(HEADERS DirectoryNode.h
            DirectoryNodeAlbum.h
            DirectoryNodeArtist.h
            DirectoryNodeDiscs.h
            DirectoryNodeGrouped.h
            DirectoryNodeOverview.h
            DirectoryNodeRoot.h
            DirectoryNodeSingles.h
            DirectoryNodeSong.h
            QueryParams.h)

core_add_library(musicdatabasedirectory)
