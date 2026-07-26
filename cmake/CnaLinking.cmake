# CNA and some graphics backend static libraries currently contain genuine circular
# references. CNA's own examples use a GNU/Clang linker group on Linux. Keep the same
# consumer-side rule here until CNA exports a single cycle-safe facade target.
function(iron_shadows_link_final_target target)
    set(_backend_target "")
    if(CNA_GRAPHICS_BACKEND STREQUAL "EASYGL")
        set(_backend_target cna_backend_graphics_easygl)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "VULKAN")
        set(_backend_target cna_backend_graphics_vulkan)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "SOFTWARE")
        set(_backend_target cna_backend_graphics_software)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "HEADLESS")
        set(_backend_target cna_backend_graphics_headless)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "SDL_RENDERER")
        set(_backend_target cna_backend_graphics_sdl_renderer)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "BGFX")
        set(_backend_target cna_backend_graphics_bgfx)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "WEBGPU")
        set(_backend_target cna_backend_graphics_webgpu)
    elseif(CNA_GRAPHICS_BACKEND STREQUAL "SDL_GPU")
        set(_backend_target cna_backend_graphics_sdl_gpu)
    endif()

    if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND _backend_target AND TARGET ${_backend_target})
        target_link_libraries(${target} PRIVATE
            -Wl,--start-group
            CNA
            ${_backend_target}
            -Wl,--end-group
            SHARP_RUNTIME)
    else()
        target_link_libraries(${target} PRIVATE CNA SHARP_RUNTIME)
    endif()
endfunction()
