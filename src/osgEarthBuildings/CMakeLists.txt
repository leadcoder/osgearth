
IF(OSGEARTH_BUILD_SHARED_LIBS)
    ADD_DEFINITIONS(-DOSGEARTHBUILDINGS_LIBRARY)
ELSE ()
    ADD_DEFINITIONS(-DOSGEARTHBUILDINGS_LIBRARY_STATIC)
ENDIF()

set(LIB_NAME osgEarthBuildings)

# set(HEADER_PATH ${OSGEARTH_SOURCE_DIR}/include/${LIB_NAME})

set(LIB_PUBLIC_HEADERS
    Analyzer
    BuildContext
    Building
    BuildingCatalog
    BuildingCompiler
    BuildingFactory
    BuildingLayer
    BuildingLayerOptions
    BuildingPager
    BuildingSymbol
    BuildingVisitor
    Common
    Compiler
    CompilerOutput
    CompilerSettings
    InstancedBuildingCompiler
    InstancedRoofCompiler
    Elevation
    ElevationCompiler
    Export
    FlatRoofCompiler
    GableRoofCompiler
    Parapet
    Roof
    Zoning
	ExternalModelNode
	InstancedModelNode
	ElevationsLodNode
	FilterUsage
)

set(LIB_COMMON_FILES
    Analyzer.cpp
    Building.cpp
    BuildingCatalog.cpp
    BuildingCompiler.cpp
    BuildingFactory.cpp
    BuildingLayer.cpp
    BuildingPager.cpp
    BuildingSymbol.cpp
    BuildingVisitor.cpp
    Compiler.cpp
    CompilerOutput.cpp
    CompilerSettings.cpp
    InstancedBuildingCompiler.cpp
    InstancedRoofCompiler.cpp
    Elevation.cpp
    ElevationCompiler.cpp
    FeaturePlugin.cpp
    FlatRoofCompiler.cpp
    GableRoofCompiler.cpp
    Parapet.cpp
    Roof.cpp
    ExternalModelNode.cpp
    InstancedModelNode.cpp
    ElevationsLodNode.cpp
)

add_osgearth_library(
    TARGET ${LIB_NAME}
    SOURCES ${LIB_COMMON_FILES}
    PUBLIC_HEADERS ${LIB_PUBLIC_HEADERS}
    SHADERS ${TARGET_GLSL}
    TEMPLATES ${TARGET_IN}
    FOLDER "NodeKits")
