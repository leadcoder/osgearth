IF(OSGEARTH_BUILD_SHARED_LIBS)
    ADD_DEFINITIONS(-DOSGEARTHPBR_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DOSGEARTHPBR_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME osgEarthPBR)

# set(TARGET_GLSL
    # Procedural.Vegetation.CS.glsl
    # Procedural.Vegetation.Types.glsl
    # Procedural.Trees.glsl
    # Procedural.Grass.glsl
    # Procedural.TextureSplatting.glsl)

# set(SHADERS_CPP "${CMAKE_CURRENT_BINARY_DIR}/AutoGenShaders.cpp")

# configure_shaders(
    # PBRShaders.cpp.in
    # ${SHADERS_CPP}
    # ${TARGET_GLSL} )

# set(TARGET_IN    
    # PBRShaders.cpp.in)


set(TARGET_SRC
    ReaderWriterGLTF.cpp
	PBRMaterial.cpp
    ${SHADERS_CPP})
	
set(LIB_PUBLIC_HEADERS )
	
SET(TARGET_H
	${LIB_PUBLIC_HEADERS}
	GLTFReader.h
    GLTFWriter.h
    B3DMReader.h
    B3DMWriter.h
	PBRMaterial.h)

ADD_LIBRARY(
    ${LIB_NAME}
    ${OSGEARTH_DYNAMIC_OR_STATIC}
    ${LIB_PUBLIC_HEADERS}
    ${TARGET_SRC}
	${TARGET_H}
#    ${TARGET_GLSL}
#    ${TARGET_IN}
)

INCLUDE_DIRECTORIES(
    ${OSGEARTH_EMBEDDED_THIRD_PARTY_DIR}/tinygltf 
    ${OSGEARTH_EMBEDDED_THIRD_PARTY_DIR}/rapidjson/include/rapidjson
)

#IF(draco_FOUND)
#    INCLUDE_DIRECTORIES( ${draco_INCLUDE_DIRS} )
#    SET(TARGET_LIBRARIES_VARS draco_LIBRARIES )
#ENDIF(draco_FOUND)


LINK_EXTERNAL(
    ${LIB_NAME} 
    ${TARGET_EXTERNAL_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT} 
    ${MATH_LIBRARY} )

LINK_INTERNAL(
    ${LIB_NAME}
    osgEarth
)

LINK_WITH_VARIABLES(
    ${LIB_NAME}
    OSG_LIBRARY
    OSGUTIL_LIBRARY
    OSGDB_LIBRARY
	draco_LIBRARIES)

LINK_CORELIB_DEFAULT(
    ${LIB_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARY} )

# Add this project to the NodeKits solution folder
set_property( TARGET ${LIB_NAME} PROPERTY FOLDER "NodeKits" )

INCLUDE(ModuleInstall OPTIONAL)