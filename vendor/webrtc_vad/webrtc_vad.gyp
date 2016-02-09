{
    'targets': [
        {
            'target_name': 'webrtc_vad',
            'type':    'static_library',
            'sources': [
                'spl/complex_fft.c',
                'spl/ilbc_specific_functions.c',
                'spl/levinson_durbin.c',
                'spl/min_max_operations.c',
                'spl/randomization_functions.c',
                'spl/real_fft.c',
                'spl/resample.c',
                'spl/resample_48khz.c',
                'spl/resample_by_2.c',
                'spl/resample_by_2_internal.c',
                'spl/resample_fractional.c',
                'spl/spl_core.c',
                'spl/spl_init.c',
                'spl/splitting_filter.c',
                'spl/spl_sqrt.c',
                'spl/spl_sqrt_floor.c',
                'vad/vad_core.c',
                'vad/vad_filterbank.c',
                'vad/vad_gmm.c',
                'vad/vad_sp.c',
                'vad/webrtc_vad.c'
            ],
            'include_dirs': ['./include'],
            'direct_dependent_settings': {
                'include_dirs': ['./include'],
            }
          }
    ]
}
