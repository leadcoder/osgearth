add_osgearth_app(
    TARGET osgearth_cubemap
    SOURCES osgearth_cubemap.cpp EnvironmentMap.cpp
    LIBRARIES osgEarthImGui
    FOLDER Tools )

if(OSGEARTH_BUILD_PROCEDURAL_NODEKIT)
    target_link_libraries(osgearth_cubemap PRIVATE osgEarthProcedural)
endif()

if(OSGEARTH_BUILD_CESIUM_NODEKIT)
    target_link_libraries(osgearth_cubemap PRIVATE osgEarthCesium)
endif()
