{
  "targets": [
    {
      "target_name": "acr122u",
      "sources": [ "src/acr122u.cpp" ],
      'cflags': [
        '<!@(pkg-config --cflags libnfc)'
      ],
      'ldflags': [
        '<!@(pkg-config --libs-only-L --libs-only-other libnfc)'
      ],
      'libraries': [
        '<!@(pkg-config --libs-only-l libnfc)'
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
