{
    "variables": {
        "freenect_libname%":"freenect",
    },
    "targets": [{
        "target_name": "nkinect",
        "sources": [ "src/addon/nkinect.cc" ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "cflags_cc": ["-fexceptions"],
        "conditions": [
            ["OS == 'linux'", {
                "include_dirs": [
                    "/usr/local/include/lib<(freenect_libname),"
                    "/usr/local/include/libusb-1.0" ],
                "library_dirs": [
                    "/usr/local/lib"
                ],
                # "libraries": [
                #     "lib<(freenect_libname).so"
                # ],
                "ldflags": [
                  "-l<(freenect_libname)",
                ]
            }],
            ["OS == 'mac'", {
                "include_dirs": [
                    "/usr/local/include/lib<(freenect_libname),"
                    "/usr/local/include/libusb-1.0" ],
                "library_dirs": [
                    "/usr/local/lib"
                ],
                "libraries": [
                    "lib<(freenect_libname).dylib"
                ]
            }]
      ]
  }]
}
