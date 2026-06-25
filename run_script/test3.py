#!/usr/bin/env python3

import anlnext # ANL Next library
import EricaPy as th # Python extension library using ANL Next

### chain definition
def setup(module):
    module.chain(th.SaveFile)
    module.with_parameters({
        "filename": "output_3.root"
    })

    module.chain(th.ReadData)
    module.with_parameters({
      
        "data_dir": "/mnt/d/data/0625/run_20260625T053901Z",
       #"data_dir": "/mnt/d/DAQ/20260616/run_20260616T070422Z",
       #"data_dir": "/mnt/d/DAQ/20260607/run_20260607T160702Z",
       # "data_dir": "/mnt/d/DAQ/20260606/run_20260606T123236Z",
       # "data_dir": "/mnt/d/DAQ/20260605/run_20260605T145825Z",# "started_at": "2026-06-05T23:58:25.876Z",
       # "data_dir": "/mnt/d/DAQ/20260605/run_20260605T144841Z", #"started_at": "2026-06-05T23:48:41.066Z",
       # "data_dir": "/mnt/d/DAQ/20260605/run_20260605T141812Z", # "started_at": "2026-06-05T23:18:12.306Z",
       # "data_dir": "/mnt/d/DAQ/20260605/run_20260605T084014Z", #"started_at": "2026-06-05T17:40:14.929Z",
       # "data_dir": "/mnt/d/DAQ/20260605/run_20260605T024025Z",# "started_at": "2026-06-05T11:40:25.869Z", "stopped_at": "2026-06-05T11:56:12.776Z",
       # "data_dir": "/mnt/d/DAQ/20260604/run4/run_20260604T141103Z", #"started_at": "2026-06-04T23:11:03.351Z","stopped_at": "2026-06-04T23:22:58.290Z",
       # "data_dir": "/mnt/d/DAQ/20260604/run4/run_20260604T124059Z",#"started_at": "2026-06-04T21:40:59.199Z"
       #"data_dir": "/mnt/d/DAQ/20260604/run4/run_20260604T124423Z", #"started_at": "2026-06-04T21:44:23.291Z"
       # "data_dir": "/mnt/d/DAQ/20260604/run3/run_20260604T123134Z",
       # "data_dir": "/mnt/d/DAQ/20260604/run0/run_20260604T092347Z",
       # "data_dir": "/mnt/d/DAQ/20260604/run2/run_20260604T095036Z",
        "reverse_TPC" : False,
        "TPC_Only" : True,
        "event_id" : True,
        "Image_A_C" : False,
        "FADC_TH" : False,
        "save_track_png" : False,
        "save_fadc_png" : False,
        "save_fadc_a_filter" : False,
        "fadc_a_filter_min" : 50,
        "fadc_a_filter_max" : 800,
        "save_fadc_c_filter" : False,
        "fadc_c_filter_min" : 200,
        "fadc_c_filter_max" : 950
    })
   
    module.chain(th.CalcFADC)
    module.with_parameters({
        "base_window": 30.0,
        "threshold": 20.0, #20.0
        "save_TH_plots": True #20260618 小野田修正
    })
    
    module.chain(th.TPC_Calibration)
    module.with_parameters({
        ##"ParameterFile": "/home/onoda/ANLNext/SMILEsoft/src/data/Calibration20190130.dat",
        "NoCalData": True
    })
    
    module.chain(th.TOT_Skewness)
    module.with_parameters({
        "pitch_x_mm_per_strip": 0.4,
        "pitch_y_mm_per_strip": 0.4,
        "pitch_z_mm_per_clock": 0.5,
        "clock_offset":         90.0,
        "flag_pixel_cut":       False,
        "flag_save_cloud":      True
    })


### run analysis chain
a = anlnext.AnalysisChain()
setup(a)
#a.run(3000)
a.run(-1)
