{
    "variables": {
        # "freenect_libname%":"freenect",
        "freenect_libname%":"fakenect"
    },
    "targets": [{
        "target_name": "freenect",
        "sources": [ "src/freenect.cc" ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        #"cflags_cc": ["-fexceptions"],
        "conditions": [
            ["OS == 'linux'", {
                "include_dirs": [
                    # "/usr/local/include/lib<(freenect_libname)",
                    "/usr/local/include/libfreenect,"
                    "/usr/local/include/libusb-1.0" ],
                "library_dirs": [
                    "/usr/local/lib"
                ],
                "libraries": [
                    #"-l<(freenect_libname)",
                    "-lfreenect",
                    # "/usr/local/lib/fakenect/libfakenect.so"
                ]
        }]
      ]
  }]
}
