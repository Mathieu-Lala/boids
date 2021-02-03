add_executable(boids main.cpp)
target_link_libraries(boids PRIVATE project_warnings project_options CONAN_PKG::imgui-sfml CONAN_PKG::entt
                                    CONAN_PKG::glm)
target_compile_definitions(boids PRIVATE GLM_CONFIG_XYZW_ONLY)
