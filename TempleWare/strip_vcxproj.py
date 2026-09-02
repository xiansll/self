import re

p = r'TempleWare-CS2\TempleWare-CS2.vcxproj'
src = open(p, encoding='utf-8-sig').read()

minimal = (
    '  <ItemGroup>\n'
    '    <ClCompile Include="external\\imgui\\imgui.cpp" />\n'
    '    <ClCompile Include="external\\imgui\\imgui_draw.cpp" />\n'
    '    <ClCompile Include="external\\imgui\\imgui_impl_dx11.cpp" />\n'
    '    <ClCompile Include="external\\imgui\\imgui_impl_win32.cpp" />\n'
    '    <ClCompile Include="external\\imgui\\imgui_widgets.cpp" />\n'
    '    <ClCompile Include="external\\kiero\\kiero.cpp" />\n'
    '    <ClCompile Include="external\\kiero\\minhook\\src\\buffer.c" />\n'
    '    <ClCompile Include="external\\kiero\\minhook\\src\\hde\\hde32.c" />\n'
    '    <ClCompile Include="external\\kiero\\minhook\\src\\hde\\hde64.c" />\n'
    '    <ClCompile Include="external\\kiero\\minhook\\src\\hook.c" />\n'
    '    <ClCompile Include="external\\kiero\\minhook\\src\\trampoline.c" />\n'
    '    <ClCompile Include="source\\main.cpp" />\n'
    '  </ItemGroup>'
)

start = src.find('  <ItemGroup>\n    <ClCompile Include="external')
if start == -1:
    raise SystemExit('ClCompile ItemGroup not found')
end = src.find('</ItemGroup>', start) + len('</ItemGroup>')
src = src[:start] + minimal + src[end:]

src = src.replace(
    'd3d11.lib;dxgi.lib;freetype.lib;libprotobuf.lib;%(AdditionalDependencies)',
    'd3d11.lib;dxgi.lib;%(AdditionalDependencies)')
src = src.replace(
    'd3d11.lib;dxgi.lib;freetype.lib;libprotobufd.lib;%(AdditionalDependencies)',
    'd3d11.lib;dxgi.lib;%(AdditionalDependencies)')

open(p, 'w', encoding='utf-8-sig', newline='\r\n').write(src)
print('done')
