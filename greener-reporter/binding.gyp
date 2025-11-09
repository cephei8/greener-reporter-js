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
                                "<(module_root_dir)/dist/lib/<(OS)-<(target_arch)/greener_reporter.dll"
                            ]
                        }
                    ]
                }
            ]
        ]
    },
    "targets": [
        {
            "target_name": "greener_reporter_addon",
            "sources": ["addon/addon.cpp", "addon/reporter.cpp"],
            "libraries": ["-lgreener_reporter"]
        }
    ]
}
