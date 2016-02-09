{
    'targets': [
        {
            'target_name': 'mpadec',
            'type': 'static_library',
            'sources': [
                'src/bitstream.c',
                'src/dct64.c',
                'src/decode.c',
                'src/layer1.c',
                'src/layer2.c',
                'src/layer3.c',
                'src/mpadec.c',
                'src/mpadec_interface.c',
                'src/tabinit.c',
                'src/vbrtag.c'

            ],
            'include_dirs': [
                './include'
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                  'include/'
                ]
            }
        }
    ]
}