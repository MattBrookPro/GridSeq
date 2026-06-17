# A tiny headless CLI that renders a pattern through the engine to a WAV. It is
# another consumer of the SAME engine library the plugin and tests use - more
# evidence that the core is genuinely decoupled and reusable.
add_executable(gridseq_render render_cli.cpp)
target_link_libraries(gridseq_render PRIVATE gridseq_engine)
target_compile_features(gridseq_render PRIVATE cxx_std_17)
