# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate typed Windows dispatch for the standalone Vulkan 1.0 probe."""
import pathlib
import re
import sys
src=pathlib.Path(sys.argv[1]).read_text(encoding="utf8")
names=sorted(set(re.findall(r"\b(vk[A-Z][A-Za-z0-9_]*)\s*\(",src)))
if not names or "vkCreateInstance" not in names:
    raise ValueError("missing Vulkan commands")
lines=["/* Generated; Vulkan 1.0 core loader exports only. */",
       "static HMODULE world_vk_loader;"]
lines.extend(f"static PFN_{n} {n};" for n in names)
lines += ["static int load_world_vk(void){",
          '    world_vk_loader=LoadLibraryExW(L"vulkan-1.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);',
          '    if(!world_vk_loader){fprintf(stderr,"Vulkan system loader unavailable: %lu\\n",(unsigned long)GetLastError());return 0;}']
for n in names:
    lines += [f'    {n}=(PFN_{n})GetProcAddress(world_vk_loader,"{n}");',
              f'    if(!{n}){{fprintf(stderr,"Missing Vulkan core command: {n}\\n");FreeLibrary(world_vk_loader);return 0;}}']
lines += ["    return 1;", "}"]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines)+"\n",encoding="ascii")
