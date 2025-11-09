{
    "target_defaults": {
        "include_dirs": ["<(module_root_dir)/dist/include"],
        "link_settings": {
            "library_dirs": ["<(module_root_dir)/dist/lib/<(OS)-<(target_arch)"]
        },
        "conditions": [
            [
                "OS!='win'",
                {
                    "link_settings": {
                        "libraries": [
                            "-Wl,-rpath,<(module_root_dir)/dist/lib/<(OS)-<(target_arch)"
                        ]
                    }
                }
            ],
            [
                "OS=='win'",
                {
                    "copies": [
                        {
                            "destination": "<(module_root_dir)/build/Release",
                            "files": [
                                "<(module_root_dir)/dist/lib/<(OS)-<(target_arch)/greener_servermock.dll"
                            ]
                        }
                    ]
                }
            ]
        ]
    },
    "targets": [
        {
            "target_name": "greener_servermock_addon",
            "sources": ["addon/addon.cpp", "addon/servermock.cpp"],
            "libraries": ["-lgreener_servermock"]
        }
    ]
}
